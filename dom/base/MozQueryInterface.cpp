/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromeUtils.h"
#include "MozQueryInterface.h"
#include "xptinfo.h"

#include <string.h>

#include "jsapi.h"

#include "mozilla/ErrorResult.h"
#include "xpcpublic.h"

namespace mozilla::dom {

constexpr size_t IID_SIZE = sizeof(nsIID);

static_assert(
    IID_SIZE == 16,
    "Size of nsID struct changed. Please ensure this code is still valid.");

static int CompareIIDs(const nsIID& aA, const nsIID& aB) {
  return memcmp((void*)&aA.m0, (void*)&aB.m0, IID_SIZE);
}

/* static */
UniquePtr<MozQueryInterface> ChromeUtils::GenerateQI(
    const GlobalObject& aGlobal, const Sequence<JS::Value>& aInterfaces) {
  JSContext* cx = aGlobal.Context();

  nsTArray<nsIID> ifaces;

  JS::Rooted<JS::Value> iface(cx);
  for (uint32_t idx = 0; idx < aInterfaces.Length(); ++idx) {
    iface = aInterfaces[idx];

    // Handle ID objects
    if (Maybe<nsID> id = xpc::JSValue2ID(cx, iface)) {
      ifaces.AppendElement(*id);
      continue;
    }

    // Accept string valued names
    if (iface.isString()) {
      JS::UniqueChars name = JS_EncodeStringToLatin1(cx, iface.toString());

      const nsXPTInterfaceInfo* iinfo = nsXPTInterfaceInfo::ByName(name.get());
      if (iinfo) {
        ifaces.AppendElement(iinfo->IID());
        continue;
      }
    }

    // NOTE: We ignore unknown interfaces here because in some cases we try to
    // pass them in to support multiple platforms.
  }

  MOZ_ASSERT(!ifaces.Contains(NS_GET_IID(nsISupports), CompareIIDs));
  ifaces.AppendElement(NS_GET_IID(nsISupports));

  ifaces.Sort(CompareIIDs);

  return MakeUnique<MozQueryInterface>(std::move(ifaces));
}

bool MozQueryInterface::QueriesTo(const nsIID& aIID) const {
  return mInterfaces.ContainsSorted(aIID, CompareIIDs);
}

void MozQueryInterface::LegacyCall(JSContext* cx, JS::Handle<JS::Value> thisv,
                                   JS::Handle<JS::Value> aIID,
                                   JS::MutableHandle<JS::Value> aResult,
                                   ErrorResult& aRv) const {
  Maybe<nsID> id = xpc::JSValue2ID(cx, aIID);
  if (id && QueriesTo(*id)) {
    aResult.set(thisv);
  } else {
    aRv.Throw(NS_ERROR_NO_INTERFACE);
  }
}

bool MozQueryInterface::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto,
                                   JS::MutableHandle<JSObject*> aReflector) {
  return MozQueryInterface_Binding::Wrap(aCx, this, aGivenProto, aReflector);
}

}  // namespace mozilla::dom
