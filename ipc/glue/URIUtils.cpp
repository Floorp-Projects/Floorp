/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "URIUtils.h"

#include "nsIIPCSerializableURI.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "nsComponentManagerUtils.h"
#include "nsDebug.h"
#include "nsID.h"
#include "nsJARURI.h"
#include "nsIIconURI.h"
#include "nsHostObjectURI.h"
#include "NullPrincipalURI.h"
#include "nsJSProtocolHandler.h"
#include "nsNetCID.h"
#include "nsSimpleNestedURI.h"
#include "nsThreadUtils.h"

using namespace mozilla::ipc;
using mozilla::ArrayLength;

namespace {

NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);
NS_DEFINE_CID(kStandardURLCID, NS_STANDARDURL_CID);
NS_DEFINE_CID(kJARURICID, NS_JARURI_CID);
NS_DEFINE_CID(kIconURICID, NS_MOZICONURI_CID);

} // namespace

namespace mozilla {
namespace ipc {

void
SerializeURI(nsIURI* aURI,
             URIParams& aParams)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aURI);

  nsCOMPtr<nsIIPCSerializableURI> serializable = do_QueryInterface(aURI);
  if (!serializable) {
    MOZ_CRASH("All IPDL URIs must be serializable!");
  }

  serializable->Serialize(aParams);
  if (aParams.type() == URIParams::T__None) {
    MOZ_CRASH("Serialize failed!");
  }
}

void
SerializeURI(nsIURI* aURI,
             OptionalURIParams& aParams)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (aURI) {
    URIParams params;
    SerializeURI(aURI, params);
    aParams = params;
  }
  else {
    aParams = mozilla::void_t();
  }
}

already_AddRefed<nsIURI>
DeserializeURI(const URIParams& aParams)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIIPCSerializableURI> serializable;

  switch (aParams.type()) {
    case URIParams::TSimpleURIParams:
      serializable = do_CreateInstance(kSimpleURICID);
      break;

    case URIParams::TStandardURLParams:
      serializable = do_CreateInstance(kStandardURLCID);
      break;

    case URIParams::TJARURIParams:
      serializable = do_CreateInstance(kJARURICID);
      break;

    case URIParams::TJSURIParams:
      serializable = new nsJSURI();
      break;

    case URIParams::TIconURIParams:
      serializable = do_CreateInstance(kIconURICID);
      break;

    case URIParams::TNullPrincipalURIParams:
      serializable = new NullPrincipalURI();
      break;

    case URIParams::TSimpleNestedURIParams:
      serializable = new nsSimpleNestedURI();
      break;

    case URIParams::THostObjectURIParams:
      serializable = new nsHostObjectURI();
      break;

    default:
      MOZ_CRASH("Unknown params!");
  }

  MOZ_ASSERT(serializable);

  if (!serializable->Deserialize(aParams)) {
    MOZ_ASSERT(false, "Deserialize failed!");
    return nullptr;
  }

  nsCOMPtr<nsIURI> uri = do_QueryInterface(serializable);
  MOZ_ASSERT(uri);

  return uri.forget();
}

already_AddRefed<nsIURI>
DeserializeURI(const OptionalURIParams& aParams)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIURI> uri;

  switch (aParams.type()) {
    case OptionalURIParams::Tvoid_t:
      break;

    case OptionalURIParams::TURIParams:
      uri = DeserializeURI(aParams.get_URIParams());
      break;

    default:
      MOZ_CRASH("Unknown params!");
  }

  return uri.forget();
}

} // namespace ipc
} // namespace mozilla
