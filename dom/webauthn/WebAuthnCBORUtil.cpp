/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cbor-cpp/src/cbor.h"
#include "mozilla/dom/WebAuthnCBORUtil.h"
#include "mozilla/dom/WebAuthnUtil.h"

namespace mozilla {
namespace dom {

nsresult
CBOREncodePublicKeyObj(const CryptoBuffer& aPubKeyBuf,
                       /* out */ CryptoBuffer& aPubKeyObj)
{
  mozilla::dom::CryptoBuffer xBuf, yBuf;
  nsresult rv = U2FDecomposeECKey(aPubKeyBuf, xBuf, yBuf);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // COSE_Key object. See https://tools.ietf.org/html/rfc8152#section-7
  cbor::output_dynamic cborPubKeyOut;
  cbor::encoder encoder(cborPubKeyOut);
  encoder.write_map(5);
  {
    encoder.write_int(1);   // kty
    encoder.write_int(2);   // EC2
    encoder.write_int(3);   // alg
    encoder.write_int(-7);  // ES256

    // See https://tools.ietf.org/html/rfc8152#section-13.1
    encoder.write_int(-1);  // crv
    encoder.write_int(1);   // P-256
    encoder.write_int(-2);  // x
    encoder.write_bytes(xBuf.Elements(), xBuf.Length());
    encoder.write_int(-3);  // y
    encoder.write_bytes(yBuf.Elements(), yBuf.Length());
  }

  if (!aPubKeyObj.Assign(cborPubKeyOut.data(), cborPubKeyOut.size())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsresult
CBOREncodeAttestationObj(const CryptoBuffer& aAuthDataBuf,
                         const CryptoBuffer& aAttestationCertBuf,
                         const CryptoBuffer& aSignatureBuf,
                         /* out */ CryptoBuffer& aAttestationObj)
{
  /*
  Attestation Object, encoded in CBOR (description is CDDL)

  attObj = {
              authData: bytes,
              $$attStmtType
           }
  $$attStmtType //= (
                        fmt: "fido-u2f",
                        attStmt: u2fStmtFormat
                    )
  u2fStmtFormat = {
                      x5c: [ attestnCert: bytes, * (caCert: bytes) ],
                      sig: bytes
                  }
  */
  cbor::output_dynamic cborAttOut;
  cbor::encoder encoder(cborAttOut);
  encoder.write_map(3);
  {
    encoder.write_string("fmt");
    encoder.write_string("fido-u2f");

    encoder.write_string("attStmt");
    encoder.write_map(2);
    {
      encoder.write_string("sig");
      encoder.write_bytes(aSignatureBuf.Elements(), aSignatureBuf.Length());

      encoder.write_string("x5c");
      // U2F wire protocol can only deliver 1 certificate, so it's never a chain
      encoder.write_array(1);
      encoder.write_bytes(aAttestationCertBuf.Elements(), aAttestationCertBuf.Length());
    }

    encoder.write_string("authData");
    encoder.write_bytes(aAuthDataBuf.Elements(), aAuthDataBuf.Length());
  }

  if (!aAttestationObj.Assign(cborAttOut.data(), cborAttOut.size())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

}
}
