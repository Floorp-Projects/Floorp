/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebAuthnUtil_h
#define mozilla_dom_WebAuthnUtil_h

/*
 * Utility functions used by both WebAuthnManager and U2FTokenManager.
 */

#include "mozilla/dom/WebAuthenticationBinding.h"
#include "ipc/IPCMessageUtils.h"

namespace mozilla::dom {

bool EvaluateAppID(nsPIDOMWindowInner* aParent, const nsString& aOrigin,
                   /* in/out */ nsString& aAppId);

nsresult HashCString(const nsACString& aIn, /* out */ nsTArray<uint8_t>& aOut);

}  // namespace mozilla::dom

#endif  // mozilla_dom_WebAuthnUtil_h
