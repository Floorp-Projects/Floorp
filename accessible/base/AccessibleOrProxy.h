/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_AccessibleOrProxy_h
#define mozilla_a11y_AccessibleOrProxy_h

#include "mozilla/a11y/Accessible.h"
#include "mozilla/a11y/ProxyAccessible.h"

#include <stdint.h>

namespace mozilla {
namespace a11y {

/**
 * This class stores an Accessible* or a ProxyAccessible* in a safe manner
 * with size sizeof(void*).
 */
class AccessibleOrProxy
{
public:
  MOZ_IMPLICIT AccessibleOrProxy(Accessible* aAcc) :
    mBits(reinterpret_cast<uintptr_t>(aAcc)) {}
  MOZ_IMPLICIT AccessibleOrProxy(ProxyAccessible* aProxy) :
    mBits(reinterpret_cast<uintptr_t>(aProxy) | IS_PROXY) {}
  MOZ_IMPLICIT AccessibleOrProxy(decltype(nullptr)) : mBits(0) {}

  bool IsProxy() const { return mBits & IS_PROXY; }
  ProxyAccessible* AsProxy() const
  {
    if (IsProxy()) {
      return reinterpret_cast<ProxyAccessible*>(mBits & ~IS_PROXY);
    }

    return nullptr;
  }

  bool IsAccessible() const { return !IsProxy(); }
  Accessible* AsAccessible() const
  {
    if (IsAccessible()) {
      return reinterpret_cast<Accessible*>(mBits);
    }

    return nullptr;
  }

  bool IsNull() const { return mBits == 0; }

  // XXX these are implementation details that ideally would not be exposed.
  uintptr_t Bits() const { return mBits; }
  void SetBits(uintptr_t aBits) { mBits = aBits; }

private:
  uintptr_t mBits;
  static const uintptr_t IS_PROXY = 0x1;
};

}
}

#endif
