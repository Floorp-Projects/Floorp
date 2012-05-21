/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsError.h"

/**
 * imagelib specific nsresult success and error codes
 */
#define NS_IMAGELIB_SUCCESS_LOAD_FINISHED   NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_IMGLIB, 0)
#define NS_IMAGELIB_CHANGING_OWNER          NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_IMGLIB, 1)

#define NS_IMAGELIB_ERROR_FAILURE           NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_IMGLIB, 5)
#define NS_IMAGELIB_ERROR_NO_DECODER        NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_IMGLIB, 6)
#define NS_IMAGELIB_ERROR_NOT_FINISHED      NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_IMGLIB, 7)
#define NS_IMAGELIB_ERROR_NO_ENCODER        NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_IMGLIB, 9)

