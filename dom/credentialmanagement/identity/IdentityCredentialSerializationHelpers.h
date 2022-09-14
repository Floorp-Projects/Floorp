/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_identitycredentialserializationhelpers_h__
#define mozilla_dom_identitycredentialserializationhelpers_h__

#include "mozilla/dom/IdentityCredential.h"
#include "mozilla/dom/IdentityCredentialBinding.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::dom::IdentityProvider> {
  typedef mozilla::dom::IdentityProvider paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mConfigURL);
    WriteParam(aWriter, aParam.mClientId);
    WriteParam(aWriter, aParam.mNonce);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mConfigURL) &&
           ReadParam(aReader, &aResult->mClientId) &&
           ReadParam(aReader, &aResult->mNonce);
  }
};

template <>
struct ParamTraits<mozilla::dom::IdentityCredentialRequestOptions> {
  typedef mozilla::dom::IdentityCredentialRequestOptions paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mProviders);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mProviders);
    ;
  }
};

}  // namespace IPC

#endif  // mozilla_dom_identitycredentialserializationhelpers_h__
