/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ImageCaptureError.h"
#include "mozilla/dom/ImageCaptureErrorEventBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ImageCaptureError, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(ImageCaptureError)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ImageCaptureError)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ImageCaptureError)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

ImageCaptureError::ImageCaptureError(nsISupports* aParent,
                                     uint16_t aCode,
                                     const nsAString& aMessage)
  : mParent(aParent)
  , mMessage(aMessage)
  , mCode(aCode)
{
}

ImageCaptureError::~ImageCaptureError()
{
}

nsISupports*
ImageCaptureError::GetParentObject() const
{
  return mParent;
}

JSObject*
ImageCaptureError::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ImageCaptureError_Binding::Wrap(aCx, this, aGivenProto);
}

uint16_t
ImageCaptureError::Code() const
{
  return mCode;
}

void
ImageCaptureError::GetMessage(nsAString& retval) const
{
  retval = mMessage;
}

} // namespace dom
} // namespace mozilla
