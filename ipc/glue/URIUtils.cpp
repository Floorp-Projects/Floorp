/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "URIUtils.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/BlobURL.h"
#include "mozilla/net/DefaultURI.h"
#include "mozilla/net/SubstitutingJARURI.h"
#include "mozilla/net/SubstitutingURL.h"
#include "nsAboutProtocolHandler.h"
#include "nsComponentManagerUtils.h"
#include "nsDebug.h"
#include "nsID.h"
#include "nsJARURI.h"
#include "nsIIconURI.h"
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

}  // namespace

namespace mozilla {
namespace ipc {

void SerializeURI(nsIURI* aURI, URIParams& aParams) {
  MOZ_ASSERT(aURI);

  aURI->Serialize(aParams);
  if (aParams.type() == URIParams::T__None) {
    MOZ_CRASH("Serialize failed!");
  }
}

void SerializeURI(nsIURI* aURI, Maybe<URIParams>& aParams) {
  if (aURI) {
    URIParams params;
    SerializeURI(aURI, params);
    aParams = Some(std::move(params));
  } else {
    aParams = Nothing();
  }
}

already_AddRefed<nsIURI> DeserializeURI(const URIParams& aParams) {
  nsCOMPtr<nsIURIMutator> mutator;

  switch (aParams.type()) {
    case URIParams::TSimpleURIParams:
      mutator = do_CreateInstance(kSimpleURIMutatorCID);
      break;

    case URIParams::TStandardURLParams:
      if (aParams.get_StandardURLParams().isSubstituting()) {
        mutator = new net::SubstitutingURL::Mutator();
      } else {
        mutator = do_CreateInstance(kStandardURLMutatorCID);
      }
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

    case URIParams::TSimpleNestedURIParams:
      mutator = new net::nsSimpleNestedURI::Mutator();
      break;

    case URIParams::THostObjectURIParams:
      mutator = new mozilla::dom::BlobURL::Mutator();
      break;

    case URIParams::TDefaultURIParams:
      mutator = new mozilla::net::DefaultURI::Mutator();
      break;

    case URIParams::TNestedAboutURIParams:
      mutator = new net::nsNestedAboutURI::Mutator();
      break;

    case URIParams::TSubstitutingJARURIParams:
      mutator = new net::SubstitutingJARURI::Mutator();
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

already_AddRefed<nsIURI> DeserializeURI(const Maybe<URIParams>& aParams) {
  nsCOMPtr<nsIURI> uri;

  if (aParams.isSome()) {
    uri = DeserializeURI(aParams.ref());
  }

  return uri.forget();
}

}  // namespace ipc
}  // namespace mozilla
