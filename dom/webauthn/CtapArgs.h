/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CtapArgs_h
#define CtapArgs_h

#include "mozilla/dom/WebAuthnTransactionChild.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "nsIWebAuthnController.h"

namespace mozilla::dom {

// These classes provide an FFI between C++ and Rust for the getters of IPC
// objects (WebAuthnMakeCredentialInfo and WebAuthnGetAssertionInfo).  They hold
// non-owning references to IPC objects, and must only be used within the
// lifetime of the IPC transaction that created them. There are runtime
// assertions to ensure that these types are created and used on the IPC
// background thread, but that alone does not guarantee safety.

class CtapRegisterArgs final : public nsICtapRegisterArgs {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICTAPREGISTERARGS

  explicit CtapRegisterArgs(const WebAuthnMakeCredentialInfo& aInfo,
                            bool aForceNoneAttestation)
      : mInfo(aInfo), mForceNoneAttestation(aForceNoneAttestation) {
    mozilla::ipc::AssertIsOnBackgroundThread();
  }

 private:
  ~CtapRegisterArgs() = default;

  const WebAuthnMakeCredentialInfo& mInfo;
  const bool mForceNoneAttestation;
};

class CtapSignArgs final : public nsICtapSignArgs {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICTAPSIGNARGS

  explicit CtapSignArgs(const WebAuthnGetAssertionInfo& aInfo) : mInfo(aInfo) {
    mozilla::ipc::AssertIsOnBackgroundThread();
  }

 private:
  ~CtapSignArgs() = default;

  const WebAuthnGetAssertionInfo& mInfo;
};

}  // namespace mozilla::dom

#endif  // CtapArgs_h
