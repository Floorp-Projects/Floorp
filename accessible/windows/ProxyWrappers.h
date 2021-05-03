/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. *
 */

#ifndef MOZILLA_A11Y_ProxyWrappers_h
#define MOZILLA_A11Y_ProxyWrappers_h

#include "HyperTextAccessibleWrap.h"

namespace mozilla {
namespace a11y {

class RemoteAccessibleWrap : public AccessibleWrap {
 public:
  explicit RemoteAccessibleWrap(RemoteAccessible* aProxy)
      : AccessibleWrap(nullptr, nullptr) {
    mType = eProxyType;
    mBits.proxy = aProxy;
  }

  virtual void Shutdown() override {
    if (mMsaa) {
      mMsaa->MsaaShutdown();
    }
    mBits.proxy = nullptr;
    mStateFlags |= eIsDefunct;
  }

  virtual void GetNativeInterface(void** aOutAccessible) override {
    mBits.proxy->GetCOMInterface(aOutAccessible);
  }
};

class HyperTextRemoteAccessibleWrap : public HyperTextAccessibleWrap {
 public:
  explicit HyperTextRemoteAccessibleWrap(RemoteAccessible* aProxy)
      : HyperTextAccessibleWrap(nullptr, nullptr) {
    mType = eProxyType;
    mBits.proxy = aProxy;
  }

  virtual void Shutdown() override {
    if (mMsaa) {
      mMsaa->MsaaShutdown();
    }
    mBits.proxy = nullptr;
    mStateFlags |= eIsDefunct;
  }

  virtual void GetNativeInterface(void** aOutAccessible) override {
    mBits.proxy->GetCOMInterface(aOutAccessible);
  }
};

class DocRemoteAccessibleWrap : public HyperTextRemoteAccessibleWrap {
 public:
  explicit DocRemoteAccessibleWrap(RemoteAccessible* aProxy)
      : HyperTextRemoteAccessibleWrap(aProxy) {
    mGenericTypes |= eDocument;
  }

  void AddID(uint32_t aID, AccessibleWrap* aAcc) {
    mIDToAccessibleMap.InsertOrUpdate(aID, aAcc);
  }
  void RemoveID(uint32_t aID) { mIDToAccessibleMap.Remove(aID); }
  AccessibleWrap* GetAccessibleByID(uint32_t aID) const {
    return mIDToAccessibleMap.Get(aID);
  }

  virtual nsIntRect Bounds() const override {
    // OuterDocAccessible can return a DocRemoteAccessibleWrap as a child.
    // LocalAccessible::ChildAtPoint on an ancestor might retrieve this proxy
    // and call Bounds() on it. This will crash on a proxy, so we override it to
    // do nothing here.
    return nsIntRect();
  }

 private:
  /*
   * This provides a mapping from 32 bit id to accessible objects.
   */
  nsTHashMap<nsUint32HashKey, AccessibleWrap*> mIDToAccessibleMap;
};

/**
 * Stub AccessibleWrap used in a content process for an embedded document
 * residing in another content process.
 * There is no RemoteAccessible here, since those only exist in the parent
 * process. However, like RemoteAccessibleWrap, the only real method that
 * gets called is GetNativeInterface, which returns a COM proxy for the
 * document.
 */
class RemoteIframeDocRemoteAccessibleWrap : public HyperTextAccessibleWrap {
 public:
  explicit RemoteIframeDocRemoteAccessibleWrap(IDispatch* aCOMProxy)
      : HyperTextAccessibleWrap(nullptr, nullptr), mCOMProxy(aCOMProxy) {
    mType = eProxyType;
    mBits.proxy = nullptr;
  }

  virtual void Shutdown() override {
    MOZ_ASSERT(!mMsaa);
    mStateFlags |= eIsDefunct;
    mCOMProxy = nullptr;
  }

  virtual void GetNativeInterface(void** aOutAccessible) override {
    RefPtr<IDispatch> addRefed = mCOMProxy;
    addRefed.forget(aOutAccessible);
  }

  virtual nsIntRect Bounds() const override {
    // OuterDocAccessible can return a RemoteIframeDocRemoteAccessibleWrap as a
    // child. LocalAccessible::ChildAtPoint on an ancestor might retrieve this
    // proxy and call Bounds() on it. This will crash on a proxy, so we override
    // it to do nothing here.
    return nsIntRect();
  }

 private:
  RefPtr<IDispatch> mCOMProxy;
};

}  // namespace a11y
}  // namespace mozilla

#endif
