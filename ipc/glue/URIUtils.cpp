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
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"

using namespace mozilla::ipc;
using mozilla::ArrayLength;

namespace {

NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);
NS_DEFINE_CID(kStandardURLCID, NS_STANDARDURL_CID);
NS_DEFINE_CID(kJARURICID, NS_JARURI_CID);

struct StringWithLengh
{
  const char* string;
  size_t length;
};

#define STRING_WITH_LENGTH(_str) \
  { _str, ArrayLength(_str) - 1 }

const StringWithLengh kGenericURIAllowedSchemes[] = {
  STRING_WITH_LENGTH("about:"),
  STRING_WITH_LENGTH("javascript:"),
  STRING_WITH_LENGTH("javascript")
};

#undef STRING_WITH_LENGTH

} // anonymous namespace

namespace mozilla {
namespace ipc {

void
SerializeURI(nsIURI* aURI,
             URIParams& aParams)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aURI);

  nsCOMPtr<nsIIPCSerializableURI> serializable = do_QueryInterface(aURI);
  if (serializable) {
    serializable->Serialize(aParams);
    if (aParams.type() == URIParams::T__None) {
      MOZ_CRASH("Serialize failed!");
    }
    return;
  }

  nsCString scheme;
  if (NS_FAILED(aURI->GetScheme(scheme))) {
    MOZ_CRASH("This must never fail!");
  }

  bool allowed = false;

  for (size_t i = 0; i < ArrayLength(kGenericURIAllowedSchemes); i++) {
    const StringWithLengh& entry = kGenericURIAllowedSchemes[i];
    if (scheme.EqualsASCII(entry.string, entry.length)) {
      allowed = true;
      break;
    }
  }

  if (!allowed) {
    MOZ_CRASH("All IPDL URIs must be serializable or an allowed "
              "scheme!");
  }

  GenericURIParams params;
  if (NS_FAILED(aURI->GetSpec(params.spec())) ||
      NS_FAILED(aURI->GetOriginCharset(params.charset()))) {
    MOZ_CRASH("This must never fail!");
  }

  aParams = params;
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

  nsCOMPtr<nsIURI> uri;

  if (aParams.type() != URIParams::TGenericURIParams) {
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

      default:
        MOZ_CRASH("Unknown params!");
    }

    MOZ_ASSERT(serializable);

    if (!serializable->Deserialize(aParams)) {
      MOZ_ASSERT(false, "Deserialize failed!");
      return nullptr;
    }

    uri = do_QueryInterface(serializable);
    MOZ_ASSERT(uri);

    return uri.forget();
  }

  MOZ_ASSERT(aParams.type() == URIParams::TGenericURIParams);

  const GenericURIParams& params = aParams.get_GenericURIParams();

  if (NS_FAILED(NS_NewURI(getter_AddRefs(uri), params.spec(),
                          params.charset().get()))) {
    NS_WARNING("Failed to make new URI!");
    return nullptr;
  }

  nsCString scheme;
  if (NS_FAILED(uri->GetScheme(scheme))) {
    MOZ_CRASH("This must never fail!");
  }

  bool allowed = false;

  for (size_t i = 0; i < ArrayLength(kGenericURIAllowedSchemes); i++) {
    const StringWithLengh& entry = kGenericURIAllowedSchemes[i];
    if (scheme.EqualsASCII(entry.string, entry.length)) {
      allowed = true;
      break;
    }
  }

  if (!allowed) {
    MOZ_ASSERT(false, "This type of URI is not allowed!");
    return nullptr;
  }

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
