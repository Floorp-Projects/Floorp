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
#include "nsIURIMutator.h"

using namespace mozilla::ipc;
using mozilla::ArrayLength;

namespace {

NS_DEFINE_CID(kSimpleURIMutatorCID, NS_SIMPLEURIMUTATOR_CID);
NS_DEFINE_CID(kStandardURLMutatorCID, NS_STANDARDURLMUTATOR_CID);
NS_DEFINE_CID(kJARURIMutatorCID, NS_JARURIMUTATOR_CID);
NS_DEFINE_CID(kIconURIMutatorCID, NS_MOZICONURIMUTATOR_CID);

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

  nsCOMPtr<nsIURIMutator> mutator;

  switch (aParams.type()) {
    case URIParams::TSimpleURIParams:
      mutator = do_CreateInstance(kSimpleURIMutatorCID);
      break;

    case URIParams::TStandardURLParams:
      mutator = do_CreateInstance(kStandardURLMutatorCID);
      break;

    case URIParams::TJARURIParams:
      mutator = do_CreateInstance(kJARURIMutatorCID);
      break;

    case URIParams::TJSURIParams:
      mutator = new nsJSURI::Mutator();
      break;

    case URIParams::TIconURIParams:
      mutator = do_CreateInstance(kIconURIMutatorCID);
      break;

    case URIParams::TNullPrincipalURIParams:
      mutator = new NullPrincipalURI::Mutator();
      break;

    case URIParams::TSimpleNestedURIParams:
      mutator = new net::nsSimpleNestedURI::Mutator();
      break;

    case URIParams::THostObjectURIParams:
      mutator = new nsHostObjectURI::Mutator();
      break;

    default:
      MOZ_CRASH("Unknown params!");
  }

  MOZ_ASSERT(mutator);

  nsresult rv = mutator->Deserialize(aParams);
  if (NS_FAILED(rv)) {
    MOZ_ASSERT(false, "Deserialize failed!");
    return nullptr;
  }

  nsCOMPtr<nsIURI> uri;
  DebugOnly<nsresult> rv2 = mutator->Finalize(getter_AddRefs(uri));
  MOZ_ASSERT(uri);
  MOZ_ASSERT(NS_SUCCEEDED(rv2));

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
