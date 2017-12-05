
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebAuthnTransactionChildBase_h
#define mozilla_dom_WebAuthnTransactionChildBase_h

#include "mozilla/dom/PWebAuthnTransactionChild.h"

/*
 * A base class to be used by child process IPC implementations for WebAuthn
 * and U2F. This mostly handles refcounting so we can properly dereference
 * in mozilla::ipc::BackgroundChildImpl::DeallocPWebAuthnTransactionChild(),
 * a function that doesn't know which of the two implementations is passed.
 */

namespace mozilla {
namespace dom {

class WebAuthnTransactionChildBase : public PWebAuthnTransactionChild
{
public:
  NS_INLINE_DECL_REFCOUNTING(WebAuthnTransactionChildBase);
  WebAuthnTransactionChildBase();

protected:
  ~WebAuthnTransactionChildBase() = default;
};

}
}

#endif //mozilla_dom_WebAuthnTransactionChildBase_h
