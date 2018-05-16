/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/StructuredClone.h"

#include "js/StructuredClone.h"
#include "mozilla/dom/ImageData.h"
#include "mozilla/dom/StructuredCloneTags.h"

namespace mozilla {
namespace dom {

JSObject*
ReadStructuredCloneImageData(JSContext* aCx, JSStructuredCloneReader* aReader)
{
  // Read the information out of the stream.
  uint32_t width, height;
  JS::Rooted<JS::Value> dataArray(aCx);
  if (!JS_ReadUint32Pair(aReader, &width, &height) ||
      !JS_ReadTypedArray(aReader, &dataArray)) {
    return nullptr;
  }
  MOZ_ASSERT(dataArray.isObject());

  // Protect the result from a moving GC in ~nsRefPtr.
  JS::Rooted<JSObject*> result(aCx);
  {
    // Construct the ImageData.
    RefPtr<ImageData> imageData = new ImageData(width, height,
                                                  dataArray.toObject());
    // Wrap it in a JS::Value.
    if (!imageData->WrapObject(aCx, nullptr, &result)) {
      return nullptr;
    }
  }
  return result;
}

bool
WriteStructuredCloneImageData(JSContext* aCx, JSStructuredCloneWriter* aWriter,
                              ImageData* aImageData)
{
  uint32_t width = aImageData->Width();
  uint32_t height = aImageData->Height();
  JS::Rooted<JSObject*> dataArray(aCx, aImageData->GetDataObject());

  JSAutoRealm ar(aCx, dataArray);
  JS::Rooted<JS::Value> arrayValue(aCx, JS::ObjectValue(*dataArray));
  return JS_WriteUint32Pair(aWriter, SCTAG_DOM_IMAGEDATA, 0) &&
         JS_WriteUint32Pair(aWriter, width, height) &&
         JS_WriteTypedArray(aWriter, arrayValue);
}

} // namespace dom
} // namespace mozilla
