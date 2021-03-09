/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_AccessibleOrProxy_h
#define mozilla_a11y_AccessibleOrProxy_h

#include "mozilla/a11y/LocalAccessible.h"
#include "mozilla/a11y/RemoteAccessible.h"
#include "mozilla/a11y/Role.h"

#include <stdint.h>

namespace mozilla {
namespace a11y {

/**
 * This class stores a LocalAccessible* or a RemoteAccessible* in a safe manner
 * with size sizeof(void*).
 */
class AccessibleOrProxy {
 public:
  // XXX: A temporary constructor to aid in the transition to
  // an Accessible base class.
  MOZ_IMPLICIT AccessibleOrProxy(Accessible* aAcc)
      : mBits(aAcc ? (aAcc->IsRemote()
                          ? reinterpret_cast<uintptr_t>(aAcc->AsRemote()) |
                                IS_PROXY
                          : reinterpret_cast<uintptr_t>(aAcc->AsLocal()))
                   : 0) {}
  MOZ_IMPLICIT AccessibleOrProxy(LocalAccessible* aAcc)
      : mBits(reinterpret_cast<uintptr_t>(aAcc)) {}
  MOZ_IMPLICIT AccessibleOrProxy(RemoteAccessible* aProxy)
      : mBits(aProxy ? (reinterpret_cast<uintptr_t>(aProxy) | IS_PROXY) : 0) {}
  MOZ_IMPLICIT AccessibleOrProxy(decltype(nullptr)) : mBits(0) {}
  MOZ_IMPLICIT AccessibleOrProxy() : mBits(0) {}

  bool IsProxy() const { return mBits & IS_PROXY; }
  RemoteAccessible* AsProxy() const {
    if (IsProxy()) {
      return reinterpret_cast<RemoteAccessible*>(mBits & ~IS_PROXY);
    }

    return nullptr;
  }

  bool IsAccessible() const { return !IsProxy(); }
  LocalAccessible* AsAccessible() const {
    if (IsAccessible()) {
      return reinterpret_cast<LocalAccessible*>(mBits);
    }

    return nullptr;
  }

  bool IsNull() const { return mBits == 0; }

  uint32_t ChildCount() const {
    if (IsProxy()) {
      return AsProxy()->ChildCount();
    }

    if (RemoteChildDoc()) {
      return 1;
    }

    return AsAccessible()->ChildCount();
  }

  /**
   * Return true if the object has children, false otherwise
   */
  bool HasChildren() const { return ChildCount() > 0; }

  /**
   * Return the child object either an accessible or a proxied accessible at
   * the given index.
   */
  AccessibleOrProxy ChildAt(uint32_t aIdx) const {
    if (IsProxy()) {
      return AsProxy()->RemoteChildAt(aIdx);
    }

    RemoteAccessible* childDoc = RemoteChildDoc();
    if (childDoc && aIdx == 0) {
      return childDoc;
    }

    return AsAccessible()->LocalChildAt(aIdx);
  }

  /**
   * Return the first child object.
   */
  AccessibleOrProxy FirstChild() {
    if (IsProxy()) {
      return AsProxy()->RemoteFirstChild();
    }

    RemoteAccessible* childDoc = RemoteChildDoc();
    if (childDoc) {
      return childDoc;
    }

    return AsAccessible()->LocalFirstChild();
  }

  /**
   * Return the last child object.
   */
  AccessibleOrProxy LastChild() {
    if (IsProxy()) {
      return AsProxy()->RemoteLastChild();
    }

    RemoteAccessible* childDoc = RemoteChildDoc();
    if (childDoc) {
      return childDoc;
    }

    return AsAccessible()->LocalLastChild();
  }

  /**
   * Return the next sibling object.
   */
  AccessibleOrProxy NextSibling() {
    if (IsProxy()) {
      return AsProxy()->RemoteNextSibling();
    }

    return AsAccessible()->LocalNextSibling();
  }

  /**
   * Return the prev sibling object.
   */
  AccessibleOrProxy PrevSibling() {
    if (IsProxy()) {
      return AsProxy()->RemotePrevSibling();
    }

    return AsAccessible()->LocalPrevSibling();
  }

  role Role() const {
    if (IsProxy()) {
      return AsProxy()->Role();
    }

    return AsAccessible()->Role();
  }

  int32_t IndexInParent() const;

  AccessibleOrProxy Parent() const;

  AccessibleOrProxy ChildAtPoint(
      int32_t aX, int32_t aY, LocalAccessible::EWhichChildAtPoint aWhichChild);

  bool operator!=(const AccessibleOrProxy& aOther) const {
    return mBits != aOther.mBits;
  }

  bool operator==(const AccessibleOrProxy& aOther) const {
    return mBits == aOther.mBits;
  }

  // XXX these are implementation details that ideally would not be exposed.
  uintptr_t Bits() const { return mBits; }
  void SetBits(uintptr_t aBits) { mBits = aBits; }

 private:
  /**
   * If this is an OuterDocAccessible, return the remote child document.
   */
  RemoteAccessible* RemoteChildDoc() const;

  uintptr_t mBits;
  static const uintptr_t IS_PROXY = 0x1;
};

}  // namespace a11y
}  // namespace mozilla

#endif
