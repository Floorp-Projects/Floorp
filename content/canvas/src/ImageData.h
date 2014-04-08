/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ImageData_h
#define mozilla_dom_ImageData_h

#include "nsIDOMCanvasRenderingContext2D.h"

#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/TypedArray.h"
#include <stdint.h>

#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"
#include "js/GCAPI.h"

namespace mozilla {
namespace dom {

class ImageData MOZ_FINAL : public nsISupports
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
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ImageData)

  static ImageData* Constructor(const GlobalObject& aGlobal,
                                const uint32_t aWidth,
                                const uint32_t aHeight,
                                ErrorResult& aRv);

  static ImageData* Constructor(const GlobalObject& aGlobal,
                                const Uint8ClampedArray& aData,
                                const uint32_t aWidth,
                                const Optional<uint32_t>& aHeight,
                                ErrorResult& aRv);

  uint32_t Width() const
  {
    return mWidth;
  }
  uint32_t Height() const
  {
    return mHeight;
  }
  JSObject* Data(JSContext* cx) const
  {
    return GetDataObject();
  }
  JSObject* GetDataObject() const
  {
    JS::ExposeObjectToActiveJS(mData);
    return mData;
  }

  JSObject* WrapObject(JSContext* cx);

private:
  void HoldData();
  void DropData();

  ImageData() MOZ_DELETE;

  uint32_t mWidth, mHeight;
  JS::Heap<JSObject*> mData;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ImageData_h
