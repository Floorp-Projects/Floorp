/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebAuthnArgs_H_
#define mozilla_dom_WebAuthnArgs_H_

#include "mozilla/dom/WebAuthnTransactionChild.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "nsIWebAuthnArgs.h"

namespace mozilla::dom {

class WebAuthnRegisterArgs final : public nsIWebAuthnRegisterArgs {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIWEBAUTHNREGISTERARGS

  explicit WebAuthnRegisterArgs(const WebAuthnMakeCredentialInfo& aInfo)
      : mInfo(aInfo),
        mCredProps(false),
        mHmacCreateSecret(false),
        mMinPinLength(false) {
    for (const WebAuthnExtension& ext : mInfo.Extensions()) {
      switch (ext.type()) {
        case WebAuthnExtension::TWebAuthnExtensionCredProps:
          mCredProps = ext.get_WebAuthnExtensionCredProps().credProps();
          break;
        case WebAuthnExtension::TWebAuthnExtensionHmacSecret:
          mHmacCreateSecret =
              ext.get_WebAuthnExtensionHmacSecret().hmacCreateSecret();
          break;
        case WebAuthnExtension::TWebAuthnExtensionMinPinLength:
          mMinPinLength =
              ext.get_WebAuthnExtensionMinPinLength().minPinLength();
          break;
        case WebAuthnExtension::TWebAuthnExtensionAppId:
          break;
        case WebAuthnExtension::T__None:
          break;
      }
    }
  }

 private:
  ~WebAuthnRegisterArgs() = default;

  const WebAuthnMakeCredentialInfo mInfo;

  // Flags to indicate whether an extension is being requested.
  bool mCredProps;
  bool mHmacCreateSecret;
  bool mMinPinLength;
};

class WebAuthnSignArgs final : public nsIWebAuthnSignArgs {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIWEBAUTHNSIGNARGS

  explicit WebAuthnSignArgs(const WebAuthnGetAssertionInfo& aInfo)
      : mInfo(aInfo) {
    for (const WebAuthnExtension& ext : mInfo.Extensions()) {
      switch (ext.type()) {
        case WebAuthnExtension::TWebAuthnExtensionAppId:
          mAppId = Some(ext.get_WebAuthnExtensionAppId().appIdentifier());
          break;
        case WebAuthnExtension::TWebAuthnExtensionCredProps:
          break;
        case WebAuthnExtension::TWebAuthnExtensionHmacSecret:
          break;
        case WebAuthnExtension::TWebAuthnExtensionMinPinLength:
          break;
        case WebAuthnExtension::T__None:
          break;
      }
    }
  }

 private:
  ~WebAuthnSignArgs() = default;

  const WebAuthnGetAssertionInfo mInfo;
  Maybe<nsString> mAppId;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_WebAuthnArgs_H_
