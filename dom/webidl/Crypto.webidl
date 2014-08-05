/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.w3.org/TR/WebCryptoAPI/
 */

[NoInterfaceObject]
interface RandomSource {
  [Throws]
  ArrayBufferView getRandomValues(ArrayBufferView array);
};

Crypto implements RandomSource;

interface Crypto {
  [Pref="dom.webcrypto.enabled"]
  readonly attribute SubtleCrypto subtle;
};

#ifndef MOZ_DISABLE_CRYPTOLEGACY
[NoInterfaceObject]
interface CryptoLegacy {
  [Pref="dom.unsafe_legacy_crypto.enabled"]
  readonly attribute DOMString version;

  [SetterThrows,Pref="dom.unsafe_legacy_crypto.enabled"]
  attribute boolean enableSmartCardEvents;

  [Throws,NewObject,Pref="dom.unsafe_legacy_crypto.enabled"]
  CRMFObject? generateCRMFRequest(ByteString? reqDN,
                                  ByteString? regToken,
                                  ByteString? authenticator,
                                  ByteString? eaCert,
                                  ByteString? jsCallback,
                                  any... args);

  [Throws,Pref="dom.unsafe_legacy_crypto.enabled"]
  DOMString importUserCertificates(DOMString nickname,
                                   DOMString cmmfResponse,
                                   boolean doForcedBackup);

  [Pref="dom.unsafe_legacy_crypto.enabled"]
  DOMString signText(DOMString stringToSign,
                     DOMString caOption,
                     ByteString... args);

  [Throws,Pref="dom.unsafe_legacy_crypto.enabled"]
  void logout();
};

Crypto implements CryptoLegacy;
#endif // !MOZ_DISABLE_CRYPTOLEGACY

