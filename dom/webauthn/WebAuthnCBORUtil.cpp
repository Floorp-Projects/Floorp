/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cbor-cpp/src/cbor.h"
#include "mozilla/dom/WebAuthnCBORUtil.h"
#include "mozilla/dom/WebAuthnUtil.h"

namespace mozilla::dom {

nsresult CBOREncodeNoneAttestationObj(const CryptoBuffer& aAuthDataBuf,
                                      /* out */ CryptoBuffer& aAttestationObj) {
  /*
  Attestation Object, encoded in CBOR (description is CDDL)

  $$attStmtType //= (
                          fmt: "none",
                          attStmt: emptyMap
                      )

  emptyMap = {}
  */
  cbor::output_dynamic cborAttOut;
  cbor::encoder encoder(cborAttOut);
  encoder.write_map(3);
  {
    encoder.write_string("fmt");
    encoder.write_string("none");

    encoder.write_string("attStmt");
    encoder.write_map(0);

    encoder.write_string("authData");
    encoder.write_bytes(aAuthDataBuf.Elements(), aAuthDataBuf.Length());
  }

  if (!aAttestationObj.Assign(cborAttOut.data(), cborAttOut.size())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

}  // namespace mozilla::dom
