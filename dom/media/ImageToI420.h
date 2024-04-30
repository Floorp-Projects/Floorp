/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ImageToI420Converter_h
#define ImageToI420Converter_h

#include "nsError.h"

namespace mozilla {

namespace layers {
class Image;
}  // namespace layers

/**
 * Converts aImage to an I420 image and writes it to the given buffers.
 */
nsresult ConvertToI420(layers::Image* aImage, uint8_t* aDestY, int aDestStrideY,
                       uint8_t* aDestU, int aDestStrideU, uint8_t* aDestV,
                       int aDestStrideV);

}  // namespace mozilla

#endif /* ImageToI420Converter_h */
