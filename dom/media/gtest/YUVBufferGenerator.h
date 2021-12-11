/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef YUVBufferGenerator_h
#define YUVBufferGenerator_h

#include "ImageContainer.h"
#include "mozilla/AlreadyAddRefed.h"
#include "nsTArray.h"
#include "Point.h"  // mozilla::gfx::IntSize

// A helper object to generate of different YUV planes.
class YUVBufferGenerator {
 public:
  void Init(const mozilla::gfx::IntSize& aSize);
  mozilla::gfx::IntSize GetSize() const;
  already_AddRefed<mozilla::layers::Image> GenerateI420Image();
  already_AddRefed<mozilla::layers::Image> GenerateNV12Image();
  already_AddRefed<mozilla::layers::Image> GenerateNV21Image();

 private:
  mozilla::layers::Image* CreateI420Image();
  mozilla::layers::Image* CreateNV12Image();
  mozilla::layers::Image* CreateNV21Image();
  mozilla::gfx::IntSize mImageSize;
  nsTArray<uint8_t> mSourceBuffer;
};

#endif  // YUVBufferGenerator_h
