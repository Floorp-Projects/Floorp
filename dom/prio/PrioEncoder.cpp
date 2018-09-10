/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Preferences.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Services.h"
#include "mozilla/TextUtils.h"

#include "mozilla/dom/ToJSValue.h"

#include "PrioEncoder.h"

namespace mozilla {
namespace dom {

/* static */ StaticRefPtr<PrioEncoder> PrioEncoder::sSingleton;

/* static */ PublicKey PrioEncoder::sPublicKeyA = nullptr;
/* static */ PublicKey PrioEncoder::sPublicKeyB = nullptr;

PrioEncoder::PrioEncoder() = default;
PrioEncoder::~PrioEncoder()
{
  if (sPublicKeyA) {
    PublicKey_clear(sPublicKeyA);
    sPublicKeyA = nullptr;
  }

  if (sPublicKeyB) {
    PublicKey_clear(sPublicKeyB);
    sPublicKeyB = nullptr;
  }

  Prio_clear();
}

/* static */ void
PrioEncoder::Encode(GlobalObject& aGlobal,
                    const nsCString& aBatchID,
                    const PrioParams& aPrioParams,
                    RootedDictionary<PrioEncodedData>& aData,
                    ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  SECStatus prio_rv = SECSuccess;

  if (!sSingleton) {
    sSingleton = new PrioEncoder();

    ClearOnShutdown(&sSingleton);

    Prio_init();

    nsresult rv;

    nsAutoCStringN<CURVE25519_KEY_LEN_HEX + 1> prioKeyA;
    rv = Preferences::GetCString("prio.publicKeyA", prioKeyA);
    if (NS_FAILED(rv)) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return;
    }

    nsAutoCStringN<CURVE25519_KEY_LEN_HEX + 1> prioKeyB;
    rv = Preferences::GetCString("prio.publicKeyB", prioKeyB);
    if (NS_FAILED(rv)) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return;
    }

    // Check that both public keys are of the right length
    // and contain only hex digits 0-9a-fA-f
    if (!PrioEncoder::IsValidHexPublicKey(prioKeyA)
        || !PrioEncoder::IsValidHexPublicKey(prioKeyB))  {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return;
    }

    prio_rv = PublicKey_import_hex(&sPublicKeyA,
                                   reinterpret_cast<const unsigned char*>(prioKeyA.BeginReading()),
                                   CURVE25519_KEY_LEN_HEX);
    if (prio_rv != SECSuccess) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return;
    }

    prio_rv = PublicKey_import_hex(&sPublicKeyB,
              reinterpret_cast<const unsigned char*>(prioKeyB.BeginReading()),
              CURVE25519_KEY_LEN_HEX);
    if (prio_rv != SECSuccess) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return;
    }
  }

  bool dataItems[] = {
    aPrioParams.mBrowserIsUserDefault,
    aPrioParams.mNewTabPageEnabled,
    aPrioParams.mPdfViewerUsed,
  };

  PrioConfig prioConfig = PrioConfig_new(mozilla::ArrayLength(dataItems),
                                         sPublicKeyA,
                                         sPublicKeyB,
                                         reinterpret_cast<const unsigned char*>(aBatchID.BeginReading()),
                                         aBatchID.Length());

  if (!prioConfig) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  auto configGuard = MakeScopeExit([&] {
    PrioConfig_clear(prioConfig);
  });

  unsigned char* forServerA = nullptr;
  unsigned int lenA = 0;
  unsigned char* forServerB = nullptr;
  unsigned int lenB = 0;

  prio_rv = PrioClient_encode(prioConfig,
                              dataItems,
                              &forServerA,
                              &lenA,
                              &forServerB,
                              &lenB);

  nsTArray<uint8_t> arrayForServerA;
  nsTArray<uint8_t> arrayForServerB;

  if (!arrayForServerA.AppendElements(reinterpret_cast<uint8_t*>(forServerA),
                                      lenA,
                                      fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  free(forServerA);

  if (!arrayForServerB.AppendElements(reinterpret_cast<uint8_t*>(forServerB),
                                      lenB,
                                      fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return ;
  }

  free(forServerB);

  if (prio_rv != SECSuccess) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  JS::Rooted<JS::Value> valueA(aGlobal.Context());
  if (!ToJSValue(aGlobal.Context(), TypedArrayCreator<Uint8Array>(arrayForServerA), &valueA)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  aData.mA.Construct().Init(&valueA.toObject());

  JS::Rooted<JS::Value> valueB(aGlobal.Context());
  if (!ToJSValue(aGlobal.Context(), TypedArrayCreator<Uint8Array>(arrayForServerB), &valueB)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  aData.mB.Construct().Init(&valueB.toObject());
}

bool
PrioEncoder::IsValidHexPublicKey(mozilla::Span<const char> aStr)
{
  if (aStr.Length() != CURVE25519_KEY_LEN_HEX) {
    return false;
  }

  for (auto c : aStr) {
    if (!IsAsciiHexDigit(c)) {
      return false;
    }
  }

  return true;
}

} // dom namespace
} // mozilla namespace
