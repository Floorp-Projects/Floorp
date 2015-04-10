/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 ci et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef simple_image_buffer_h
#define simple_image_buffer_h

#include "mozilla/NullPtr.h"
#include "nsISupportsImpl.h"

namespace mozilla {

class SimpleImageBuffer {
NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SimpleImageBuffer)
public:
  SimpleImageBuffer() : mBuffer(nullptr), mBufferSize(0),  mSize(0), mWidth(0), mHeight(0) {}
  void SetImage(const unsigned char* frame, unsigned int size, int width, int height);
  void Copy(const SimpleImageBuffer* aImage);
  const unsigned char* GetImage(unsigned int* size) const;
  void GetWidthAndHeight(int* width, int* height) const
  {
    if (width) {
      *width = mWidth;
    }
    if (height) {
      *height = mHeight;
    }
  }

protected:
  ~SimpleImageBuffer()
  {
    delete[] mBuffer;
  }
  const unsigned char* mBuffer;
  unsigned int mBufferSize;
  unsigned int mSize;
  int mWidth;
  int mHeight;

private:
  SimpleImageBuffer(const SimpleImageBuffer& aImage);
  SimpleImageBuffer& operator=(const SimpleImageBuffer& aImage);
};

} // namespace mozilla

#endif //  simple_image_buffer_h
