/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ImageData.h"

#include "mozilla/CheckedInt.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/ImageDataBinding.h"

#include "jsapi.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTING_ADDREF(ImageData)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ImageData)

NS_IMPL_CYCLE_COLLECTION_CLASS(ImageData)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ImageData)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(ImageData)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mData)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(ImageData)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ImageData)
  tmp->DropData();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

//static
ImageData*
ImageData::Constructor(const GlobalObject& aGlobal,
                       const uint32_t aWidth,
                       const uint32_t aHeight,
                       ErrorResult& aRv)
{
  if (aWidth == 0 || aHeight == 0) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }
  CheckedInt<uint32_t> length = CheckedInt<uint32_t>(aWidth) * aHeight * 4;
  if (!length.isValid()) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }
  js::AssertSameCompartment(aGlobal.GetContext(), aGlobal.Get());
  JSObject* data = Uint8ClampedArray::Create(aGlobal.GetContext(),
                                             length.value());
  if (!data) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }
  return new ImageData(aWidth, aHeight, *data);
}

//static
ImageData*
ImageData::Constructor(const GlobalObject& aGlobal,
                       const Uint8ClampedArray& aData,
                       const uint32_t aWidth,
                       const Optional<uint32_t>& aHeight,
                       ErrorResult& aRv)
{
  aData.ComputeLengthAndData();

  uint32_t length = aData.Length();
  if (length == 0 || length % 4) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }
  length /= 4;
  if (aWidth == 0) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }
  uint32_t height = length / aWidth;
  if (length != aWidth * height ||
      (aHeight.WasPassed() && aHeight.Value() != height)) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }
  return new ImageData(aWidth, height, *aData.Obj());
}

void
ImageData::HoldData()
{
  mozilla::HoldJSObjects(this);
}

void
ImageData::DropData()
{
  if (mData) {
    mData = nullptr;
    mozilla::DropJSObjects(this);
  }
}

JSObject*
ImageData::WrapObject(JSContext* cx)
{
  return ImageDataBinding::Wrap(cx, this);
}

} // namespace dom
} // namespace mozilla
