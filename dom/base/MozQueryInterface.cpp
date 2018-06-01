/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromeUtils.h"
#include "MozQueryInterface.h"

#include <string.h>

#include "jsapi.h"

#include "xpcpublic.h"
#include "xpcjsid.h"

namespace mozilla {
namespace dom {

constexpr size_t IID_SIZE = sizeof(nsIID);

static_assert(IID_SIZE == 16,
              "Size of nsID struct changed. Please ensure this code is still valid.");

static int
CompareIIDs(const nsIID& aA, const nsIID &aB)
{
  return memcmp((void*)&aA.m0, (void*)&aB.m0, IID_SIZE);
}

struct IIDComparator
{
  bool
  LessThan(const nsIID& aA, const nsIID &aB) const
  {
    return CompareIIDs(aA, aB) < 0;
  }

  bool
  Equals(const nsIID& aA, const nsIID &aB) const
  {
    return aA.Equals(aB);
  }
};

/* static */
MozQueryInterface*
ChromeUtils::GenerateQI(const GlobalObject& aGlobal, const Sequence<OwningStringOrIID>& aInterfaces, ErrorResult& aRv)
{
  JSContext* cx = aGlobal.Context();
  JS::RootedObject xpcIfaces(cx);

  nsTArray<nsIID> ifaces;

  JS::RootedValue val(cx);
  for (auto& iface : aInterfaces) {
    if (iface.IsIID()) {
      ifaces.AppendElement(*iface.GetAsIID()->GetID());
      continue;
    }

    // If we have a string value, we need to look up the interface name. The
    // simplest and most efficient way to do this is to just grab the "Ci"
    // object from the global scope.
    if (!xpcIfaces) {
      JS::RootedObject global(cx, aGlobal.Get());
      if (!JS_GetProperty(cx, global, "Ci", &val)) {
        aRv.NoteJSContextException(cx);
        return nullptr;
      }
      if (!val.isObject()) {
        aRv.Throw(NS_ERROR_UNEXPECTED);
        return nullptr;
      }
      xpcIfaces = &val.toObject();
    }

    auto& name = iface.GetAsString();
    if (!JS_GetUCProperty(cx, xpcIfaces, name.get(), name.Length(), &val)) {
      aRv.NoteJSContextException(cx);
      return nullptr;
    }

    if (val.isNullOrUndefined()) {
      continue;
    }
    if (!val.isObject()) {
      aRv.Throw(NS_ERROR_INVALID_ARG);
      return nullptr;
    }

    nsCOMPtr<nsISupports> base = xpc::UnwrapReflectorToISupports(&val.toObject());
    nsCOMPtr<nsIJSID> iid = do_QueryInterface(base);
    if (!iid) {
      aRv.Throw(NS_ERROR_INVALID_ARG);
      return nullptr;
    }
    ifaces.AppendElement(*iid->GetID());
  }

  MOZ_ASSERT(!ifaces.Contains(NS_GET_IID(nsISupports), IIDComparator()));
  ifaces.AppendElement(NS_GET_IID(nsISupports));

  ifaces.Sort(IIDComparator());

  return new MozQueryInterface(std::move(ifaces));
}

bool
MozQueryInterface::QueriesTo(const nsIID& aIID) const
{
  // We use BinarySearchIf here because nsTArray::ContainsSorted requires
  // twice as many comparisons.
  size_t result;
  return BinarySearchIf(mInterfaces, 0, mInterfaces.Length(),
                        [&] (const nsIID& aOther) { return CompareIIDs(aIID, aOther); },
                        &result);
}

void
MozQueryInterface::LegacyCall(JSContext* cx, JS::Handle<JS::Value> thisv,
                              nsIJSID* aIID,
                              JS::MutableHandle<JS::Value> aResult,
                              ErrorResult& aRv) const
{
  if (!QueriesTo(*aIID->GetID())) {
    aRv.Throw(NS_ERROR_NO_INTERFACE);
  } else {
    aResult.set(thisv);
  }
}

bool
MozQueryInterface::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto, JS::MutableHandle<JSObject*> aReflector)
{
  return MozQueryInterfaceBinding::Wrap(aCx, this, aGivenProto, aReflector);
}

} // namespace dom
} // namespace mozilla

