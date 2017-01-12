/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ImageBitmapFormatUtils_h
#define mozilla_dom_ImageBitmapFormatUtils_h

#include "mozilla/UniquePtr.h"
#include "nsTArrayForwardDeclare.h"

namespace mozilla {

namespace layers {
class Image;
}

class ErrorResult;

namespace dom {

struct ChannelPixelLayout;
enum class ImageBitmapFormat : uint8_t;

typedef nsTArray<ChannelPixelLayout> ImagePixelLayout;

/*
 * ImageUtils is a wrapper around layers::Image. It provides three unified
 * methods to all sub-classes of layers::Image, which are:
 *
 * (1) GetFormat() converts the image's format into ImageBitmapFormat enum.
 * (2) GetBufferLength() returns the number of bytes that are used to store
 *     the image's underlying raw data.
 * (3) MapDataInto() writes the image's underlying raw data into a given
 *     ArrayBuffer in the given format. (If the given format is different from
 *     the existing format, the ImageUtils uses the ImageBitmapFormatUtils to
 *     performa color conversion.)
 *
 * In theory, the functionalities of this class could be merged into the
 * interface of layers::Image. However, this is designed as a isolated wrapper
 * because we don't want to pollute the layers::Image's interface with methods
 * that are only meaningful to the ImageBitmap.
 */
class ImageUtils
{
public:
  class Impl;
  ImageUtils() = delete;
  ImageUtils(const ImageUtils&) = delete;
  ImageUtils(ImageUtils&&) = delete;
  ImageUtils& operator=(const ImageUtils&) = delete;
  ImageUtils& operator=(ImageUtils&&) = delete;

  explicit ImageUtils(layers::Image* aImage);
  ~ImageUtils();

  ImageBitmapFormat GetFormat() const;

  uint32_t GetBufferLength() const;

  UniquePtr<ImagePixelLayout>
  MapDataInto(uint8_t* aBuffer, uint32_t aOffset, uint32_t aBufferLength,
              ImageBitmapFormat aFormat, ErrorResult& aRv) const;

protected:
  Impl* mImpl;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_ImageBitmapFormatUtils_h */
