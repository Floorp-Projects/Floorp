/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ImageEncoder_h
#define ImageEncoder_h

#include "imgIEncoder.h"
#include "nsDOMFile.h"
#include "nsError.h"
#include "nsIDOMHTMLCanvasElement.h"
#include "nsLayoutUtils.h"
#include "nsNetUtil.h"
#include "nsSize.h"

class nsICanvasRenderingContextInternal;

namespace mozilla {
namespace dom {

class EncodingRunnable;

class ImageEncoder
{
public:
  static nsresult ExtractData(const nsAString& aType,
                              const nsAString& aOptions,
                              uint8_t* aImageBuffer,
                              const nsIntSize aSize,
                              nsICanvasRenderingContextInternal* aContext,
                              nsIInputStream** aStream,
                              bool& aFellBackToPNG);

  static nsresult ExtractDataAsync(const nsAString& aType,
                                   const nsAString& aOptions,
                                   uint8_t* aImageBuffer,
                                   const nsIntSize aSize,
                                   nsICanvasRenderingContextInternal* aContext,
                                   JSContext* aJSContext,
                                   nsIFileCallback* aCallback);

private:
  static nsresult
  ExtractDataInternal(const nsAString& aType,
                      const nsAString& aOptions,
                      uint8_t* aImageBuffer,
                      const nsIntSize aSize,
                      nsICanvasRenderingContextInternal* aContext,
                      nsIInputStream** aStream,
                      bool& aFellBackToPNG,
                      imgIEncoder* aEncoder,
                      imgIEncoder* aFallbackEncoder);

  static nsresult GetImageEncoders(const nsAString& aType,
                                   imgIEncoder** aEncoder,
                                   imgIEncoder** aFallbackEncoder);

  friend class EncodingRunnable;
};

} // namespace dom
} // namespace mozilla

#endif // ImageEncoder_h