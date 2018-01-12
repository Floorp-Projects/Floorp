/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object representing rectangle values in DOM computed style */

#include "nsDOMCSSRect.h"

#include "mozilla/dom/RectBinding.h"
#include "nsROCSSPrimitiveValue.h"

using namespace mozilla;

nsDOMCSSRect::nsDOMCSSRect(nsROCSSPrimitiveValue* aTop,
                           nsROCSSPrimitiveValue* aRight,
                           nsROCSSPrimitiveValue* aBottom,
                           nsROCSSPrimitiveValue* aLeft)
  : mTop(aTop), mRight(aRight), mBottom(aBottom), mLeft(aLeft)
{
}

nsDOMCSSRect::~nsDOMCSSRect(void)
{
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMCSSRect)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMCSSRect)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMCSSRect)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(nsDOMCSSRect, mTop, mBottom, mLeft, mRight)

JSObject*
nsDOMCSSRect::WrapObject(JSContext* cx, JS::Handle<JSObject*> aGivenProto)
{
 return dom::RectBinding::Wrap(cx, this, aGivenProto);
}
