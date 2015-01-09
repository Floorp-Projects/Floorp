/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef I420_COLOR_CONVERTER_HELPER_H
#define I420_COLOR_CONVERTER_HELPER_H

#include <utils/RWLock.h>
#include <media/editor/II420ColorConverter.h>

#include <mozilla/Attributes.h>

namespace android {

class I420ColorConverterHelper {
public:
  I420ColorConverterHelper();
  ~I420ColorConverterHelper();

  int getDecoderOutputFormat();

  int convertDecoderOutputToI420(void* aDecoderBits,
                                 int aDecoderWidth,
                                 int aDecoderHeight,
                                 ARect aDecoderRect,
                                 void* aDstBits);

  int getEncoderInputFormat();

  int convertI420ToEncoderInput(void* aSrcBits,
                                int aSrcWidth,
                                int aSrcHeight,
                                int aEncoderWidth,
                                int aEncoderHeight,
                                ARect aEncoderRect,
                                void* aEncoderBits);

  int getEncoderInputBufferInfo(int aSrcWidth,
                                int aSrcHeight,
                                int* aEncoderWidth,
                                int* aEncoderHeight,
                                ARect* aEncoderRect,
                                int* aEncoderBufferSize);

private:
  mutable RWLock mLock;
  void *mHandle;
  II420ColorConverter mConverter;

  bool loadLocked();
  bool loadedLocked() const;
  void unloadLocked();

  bool ensureLoaded();

  I420ColorConverterHelper(const I420ColorConverterHelper &) = delete;
  const I420ColorConverterHelper &operator=(const I420ColorConverterHelper &) = delete;
};

} // namespace android

#endif // I420_COLOR_CONVERTER_HELPER_H
