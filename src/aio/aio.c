/*
 * Author: Nandkishor Sonar
 * Copyright (c) 2014 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#include "aio.h"
#include "mraa_internal.h"

#define DEFAULT_BITS 10

static int raw_bits;

static mraa_result_t
aio_get_valid_fp(mraa_aio_context dev)
{
    if (advance_func->aio_get_valid_fp != NULL)
        return advance_func->aio_get_valid_fp(dev);

    char file_path[64]= "";

    //Open file Analog device input channel raw voltage file for reading.
    snprintf(file_path, 64, "/sys/bus/iio/devices/iio:device0/in_voltage%d_raw",
            dev->channel );

    dev->adc_in_fp = open(file_path, O_RDONLY);
    if (dev->adc_in_fp == -1) {
        syslog(LOG_ERR, "Failed to open Analog input raw file %s for "
                "reading!", file_path);
        return MRAA_ERROR_INVALID_RESOURCE;
    }

    return MRAA_SUCCESS;
}

mraa_aio_context
mraa_aio_init(unsigned int aio_channel)
{
    if (advance_func->aio_init_pre != NULL) {
        mraa_result_t pre_ret = (advance_func->aio_init_pre(aio_channel));
        if (pre_ret != MRAA_SUCCESS)
            return NULL;
    }

    int checked_pin = mraa_setup_aio(aio_channel);
    if (checked_pin < 0) {
        switch (checked_pin) {
            case MRAA_NO_SUCH_IO:
                syslog(LOG_ERR, "Invalid analog input channel %d specified",
                        aio_channel);
                return NULL;
            case MRAA_IO_SETUP_FAILURE:
                syslog(LOG_ERR, "Failed to set-up analog input channel %d "
                        "multiplexer", aio_channel);
                return NULL;
            case MRAA_PLATFORM_NO_INIT:
                syslog(LOG_ERR, "Platform not initialised");
                return NULL;
            default:
                return NULL;
        }
    }

    //Create ADC device connected to specified channel
    mraa_aio_context dev = malloc(sizeof(struct _aio));
    if (dev == NULL) {
        syslog(LOG_ERR, "Insufficient memory for specified Analog input channel "
                "%d\n", aio_channel);
        return NULL;
    }
    dev->channel = checked_pin;
    dev->value_bit = DEFAULT_BITS;

    //Open valid  analog input file and get the pointer.
    if (MRAA_SUCCESS != aio_get_valid_fp(dev)) {
        free(dev);
        return NULL;
    }
    raw_bits = mraa_adc_raw_bits();

    if (advance_func->aio_init_post != NULL) {
        mraa_result_t ret = advance_func->aio_init_post(dev);
        if (ret != MRAA_SUCCESS) {
            free(dev);
            return NULL;
        }
    }

    return dev;
}

unsigned int
mraa_aio_read(mraa_aio_context dev)
{
    char buffer[17];
    unsigned int shifter_value = 0;

    if (dev->adc_in_fp == -1) {
        aio_get_valid_fp(dev);
    }

    lseek(dev->adc_in_fp, 0, SEEK_SET);
    if (read(dev->adc_in_fp, buffer, sizeof(buffer)) < 1) {
        syslog(LOG_ERR, "Failed to read a sensible value");
    }
    // force NULL termination of string
    buffer[16] = '\0';
    lseek(dev->adc_in_fp, 0, SEEK_SET);

    errno = 0;
    char *end;
    unsigned int analog_value = (unsigned int) strtoul(buffer, &end, 10);
    if (end == &buffer[0]) {
        syslog(LOG_ERR, "value is not a decimal number");
    }
    else if (errno != 0) {
        syslog(LOG_ERR, "errno was set");
    }

    if (dev->value_bit != raw_bits) {
        /* Adjust the raw analog input reading to supported resolution value*/
        if (raw_bits > dev->value_bit) {
            shifter_value = raw_bits - dev->value_bit;
            analog_value =  analog_value >> shifter_value;
        } else {
            shifter_value = dev->value_bit - raw_bits;
            analog_value = analog_value << shifter_value;
        }
    }

    return analog_value;
}

mraa_result_t
mraa_aio_close(mraa_aio_context dev)
{
    if (NULL != dev)
        free(dev);

    return(MRAA_SUCCESS);
}

mraa_result_t
mraa_aio_set_bit(mraa_aio_context dev, int bits)
{
    if (dev == NULL) {
        syslog(LOG_ERR, "AIO Device not valid");
        return MRAA_ERROR_INVALID_RESOURCE;
    }
    if (bits < 1) {
        syslog(LOG_ERR, "AIO Device not valid");
        return MRAA_ERROR_INVALID_PARAMETER;
    }
    dev->value_bit = bits;
    return MRAA_SUCCESS;
}

int
mraa_aio_get_bit(mraa_aio_context dev)
{
    if (dev == NULL) {
        syslog(LOG_ERR, "AIO Device not valid");
        return 0;
    }
    return dev->value_bit;
}
