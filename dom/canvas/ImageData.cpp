/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ImageData.h"

#include "ErrorList.h"
#include "js/StructuredClone.h"
#include "js/Value.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/ImageDataBinding.h"
#include "nsCycleCollectionNoteChild.h"

namespace mozilla::dom {

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
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ImageData)
  tmp->DropData();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

// static
already_AddRefed<ImageData> ImageData::Constructor(const GlobalObject& aGlobal,
                                                   const uint32_t aWidth,
                                                   const uint32_t aHeight,
                                                   ErrorResult& aRv) {
  if (aWidth == 0 || aHeight == 0) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  // Restrict the typed array length to INT32_MAX because that's all we support.
  CheckedInt<uint32_t> length = CheckedInt<uint32_t>(aWidth) * aHeight * 4;
  if (!length.isValid() || length.value() > INT32_MAX) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }
  js::AssertSameCompartment(aGlobal.Context(), aGlobal.Get());
  JSObject* data =
      Uint8ClampedArray::Create(aGlobal.Context(), length.value(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  RefPtr<ImageData> imageData = new ImageData(aWidth, aHeight, *data);
  return imageData.forget();
}

// static
already_AddRefed<ImageData> ImageData::Constructor(
    const GlobalObject& aGlobal, const Uint8ClampedArray& aData,
    const uint32_t aWidth, const Optional<uint32_t>& aHeight,
    ErrorResult& aRv) {
  Maybe<uint32_t> maybeLength = aData.ProcessData(
      [&](const Span<uint8_t>& aData, JS::AutoCheckCannotGC&& nogc) {
        return Some(aData.Length());
      });
  uint32_t length = maybeLength.valueOr(0);
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
  RefPtr<ImageData> imageData = new ImageData(aWidth, height, *aData.Obj());
  return imageData.forget();
}

void ImageData::HoldData() { mozilla::HoldJSObjects(this); }

void ImageData::DropData() {
  if (mData) {
    mozilla::DropJSObjects(this);
  }
}

bool ImageData::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto,
                           JS::MutableHandle<JSObject*> aReflector) {
  return ImageData_Binding::Wrap(aCx, this, aGivenProto, aReflector);
}

// static
already_AddRefed<ImageData> ImageData::ReadStructuredClone(
    JSContext* aCx, nsIGlobalObject* aGlobal,
    JSStructuredCloneReader* aReader) {
  // Read the information out of the stream.
  uint32_t width, height;
  JS::Rooted<JS::Value> dataArray(aCx);
  if (!JS_ReadUint32Pair(aReader, &width, &height) ||
      !JS_ReadTypedArray(aReader, &dataArray)) {
    return nullptr;
  }
  MOZ_ASSERT(dataArray.isObject());

  RefPtr<ImageData> imageData =
      new ImageData(width, height, dataArray.toObject());
  return imageData.forget();
}

bool ImageData::WriteStructuredClone(JSContext* aCx,
                                     JSStructuredCloneWriter* aWriter) const {
  JS::Rooted<JS::Value> arrayValue(aCx, JS::ObjectValue(*GetDataObject()));
  if (!JS_WrapValue(aCx, &arrayValue)) {
    return false;
  }

  return JS_WriteUint32Pair(aWriter, Width(), Height()) &&
         JS_WriteTypedArray(aWriter, arrayValue);
}

}  // namespace mozilla::dom
