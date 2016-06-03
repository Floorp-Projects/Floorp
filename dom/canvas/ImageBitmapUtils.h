/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ImageBitmapUtils_h
#define mozilla_dom_ImageBitmapUtils_h

#include "mozilla/UniquePtr.h"
#include "nsTArrayForwardDeclare.h"

namespace mozilla {

namespace layers {
class Image;
struct PlanarYCbCrData;
}

namespace dom {

struct ChannelPixelLayout;
template<typename> class Sequence;

typedef nsTArray<ChannelPixelLayout> ImagePixelLayout;

/*
 * This function creates an ImagePixelLayout object which describes the
 * default layout of the given ImageBitmapFormat with the given width, height
 * and stride.
 */
UniquePtr<ImagePixelLayout>
CreateDefaultPixelLayout(ImageBitmapFormat aFormat,
                         uint32_t aWidth, uint32_t aHeight, uint32_t aStride);

/*
 * This function extracts information from the aImage parameter to customize
 * the ImagePixelLayout object, that is, this function creates a customized
 * ImagePixelLayout object which exactly describes the pixel layout of the
 * given aImage.
 */
UniquePtr<ImagePixelLayout>
CreatePixelLayoutFromPlanarYCbCrData(const layers::PlanarYCbCrData* aData);

/*
 * Get the number of channels of the given ImageBitmapFormat.
 */
uint8_t
GetChannelCountOfImageFormat(ImageBitmapFormat aFormat);

/*
 * Get the needed buffer size to store the image data in the given
 * ImageBitmapFormat with the given width and height.
 */
uint32_t
CalculateImageBufferSize(ImageBitmapFormat aFormat,
                         uint32_t aWidth, uint32_t aHeight);

/*
 * This function always copies the image data in _aSrcBuffer_ into _aDstBuffer_
 * and it also performs color conversion if the _aSrcFormat_ and the
 * _aDstFormat_ are different.
 *
 * The source image is stored in the _aSrcBuffer_ and the corresponding pixel
 * layout is described by the _aSrcLayout_.
 *
 * The copied and converted image will be stored in the _aDstBuffer_, which
 * should be allocated with enough size before invoking this function and the
 * needed size could be found by the CalculateImageBufferSize() method.
 *
 * The returned ImagePixelLayout object describes the pixel layout of the result
 * image and will be null if on failure.
 */
UniquePtr<ImagePixelLayout>
CopyAndConvertImageData(ImageBitmapFormat aSrcFormat,
                        const uint8_t* aSrcBuffer,
                        const ImagePixelLayout* aSrcLayout,
                        ImageBitmapFormat aDstFormat,
                        uint8_t* aDstBuffer);

/*
 * This function tries to find the best ImageBitmapFormat, from the aCandiates,
 * which can be converted from the aSrcFormat. The algorithm now merely returns
 * the FIRST one, from the aCandidates, which can be converted from the
 * aSrcFormat.
 *
 * TODO: The algorithm should be updated after we implement optimizations for
 *       different platforms (different kinds of layers::Image), considering
 *       that some conversion might be cheaper through hardware.
 */
ImageBitmapFormat
FindBestMatchingFromat(ImageBitmapFormat aSrcFormat,
                       const Sequence<ImageBitmapFormat>& aCandidates);

} // namespace dom
} // namespace mozilla


#endif // mozilla_dom_ImageBitmapUtils_h
