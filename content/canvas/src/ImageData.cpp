/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ImageData.h"

#include "nsContentUtils.h" // for NS_HOLD_JS_OBJECTS, NS_DROP_JS_OBJECTS
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

void
ImageData::HoldData()
{
  NS_HOLD_JS_OBJECTS(this, ImageData);
}

void
ImageData::DropData()
{
  if (mData) {
    mData = nullptr;
    NS_DROP_JS_OBJECTS(this, ImageData);
  }
}

JSObject*
ImageData::WrapObject(JSContext* cx, JS::Handle<JSObject*> scope)
{
  return ImageDataBinding::Wrap(cx, scope, this);
}

} // namespace dom
} // namespace mozilla
