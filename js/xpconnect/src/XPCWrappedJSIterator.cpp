/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcprivate.h"

#include "mozilla/ResultExtensions.h"
#include "mozilla/Try.h"
#include "mozilla/dom/IteratorResultBinding.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/ScriptSettings.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace xpc;

NS_IMPL_CYCLE_COLLECTION(XPCWrappedJSIterator, mEnum, mGlobal, mNext)

NS_IMPL_CYCLE_COLLECTING_ADDREF(XPCWrappedJSIterator)
NS_IMPL_CYCLE_COLLECTING_RELEASE(XPCWrappedJSIterator)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(XPCWrappedJSIterator)
  NS_INTERFACE_MAP_ENTRY(nsISimpleEnumerator)
  NS_INTERFACE_MAP_ENTRY(nsISimpleEnumeratorBase)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, XPCWrappedJSIterator)
NS_INTERFACE_MAP_END

XPCWrappedJSIterator::XPCWrappedJSIterator(nsIJSEnumerator* aEnum)
    : mEnum(aEnum) {
  nsCOMPtr<nsIXPConnectWrappedJS> wrapped = do_QueryInterface(aEnum);
  MOZ_ASSERT(wrapped);
  mGlobal = NativeGlobal(wrapped->GetJSObjectGlobal());
}

nsresult XPCWrappedJSIterator::HasMoreElements(bool* aRetVal) {
  if (mHasNext.isNothing()) {
    AutoJSAPI jsapi;
    MOZ_ALWAYS_TRUE(jsapi.Init(mGlobal));

    JSContext* cx = jsapi.cx();

    JS::RootedValue val(cx);
    MOZ_TRY(mEnum->Next(cx, &val));

    RootedDictionary<IteratorResult> result(cx);
    if (!result.Init(cx, val)) {
      return NS_ERROR_FAILURE;
    }

    if (!result.mDone) {
      if (result.mValue.isObject()) {
        JS::RootedObject obj(cx, &result.mValue.toObject());

        nsresult rv;
        if (!XPCConvert::JSObject2NativeInterface(cx, getter_AddRefs(mNext),
                                                  obj, &NS_GET_IID(nsISupports),
                                                  nullptr, &rv)) {
          return rv;
        }
      } else {
        mNext = XPCVariant::newVariant(cx, result.mValue);
      }
    }
    mHasNext = Some(!result.mDone);
  }
  *aRetVal = *mHasNext;
  return NS_OK;
}

nsresult XPCWrappedJSIterator::GetNext(nsISupports** aRetVal) {
  bool hasMore;
  MOZ_TRY(HasMoreElements(&hasMore));
  if (!hasMore) {
    return NS_ERROR_FAILURE;
  }

  mNext.forget(aRetVal);
  mHasNext = Nothing();
  return NS_OK;
}

nsresult XPCWrappedJSIterator::Iterator(nsIJSEnumerator** aRetVal) {
  nsCOMPtr<nsIJSEnumerator> jsEnum = mEnum;
  jsEnum.forget(aRetVal);
  return NS_OK;
}

nsresult XPCWrappedJSIterator::Entries(const nsID&, nsIJSEnumerator** aRetVal) {
  return NS_ERROR_NOT_IMPLEMENTED;
}
