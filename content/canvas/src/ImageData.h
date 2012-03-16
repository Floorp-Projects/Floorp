/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ImageData_h
#define mozilla_dom_ImageData_h

#include "nsIDOMCanvasRenderingContext2D.h"

#include "mozilla/Attributes.h"
#include "mozilla/StandardInteger.h"

#include "nsCycleCollectionParticipant.h"
#include "nsTraceRefcnt.h"
#include "xpcpublic.h"

#include "jsapi.h"

namespace mozilla {
namespace dom {

class ImageData MOZ_FINAL : public nsIDOMImageData
{
public:
  ImageData(uint32_t aWidth, uint32_t aHeight, JSObject& aData)
    : mWidth(aWidth)
    , mHeight(aHeight)
    , mData(&aData)
  {
    MOZ_COUNT_CTOR(ImageData);
    HoldData();
  }

  ~ImageData()
  {
    MOZ_COUNT_DTOR(ImageData);
    DropData();
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIDOMIMAGEDATA
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ImageData)

  uint32_t GetWidth()
  {
    return mWidth;
  }
  uint32_t GetHeight()
  {
    return mHeight;
  }
  JS::Value GetData()
  {
    return JS::ObjectOrNullValue(GetDataObject());
  }
  JSObject* GetDataObject()
  {
    xpc_UnmarkGrayObject(mData);
    return mData;
  }

private:
  void HoldData();
  void DropData();

  ImageData() MOZ_DELETE;

  uint32_t mWidth, mHeight;
  JSObject* mData;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ImageData_h
