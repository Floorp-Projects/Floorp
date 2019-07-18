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

class ProxyAccessibleWrap : public AccessibleWrap {
 public:
  explicit ProxyAccessibleWrap(ProxyAccessible* aProxy)
      : AccessibleWrap(nullptr, nullptr) {
    mType = eProxyType;
    mBits.proxy = aProxy;
  }

  virtual void Shutdown() override {
    mBits.proxy = nullptr;
    mStateFlags |= eIsDefunct;
  }

  virtual void GetNativeInterface(void** aOutAccessible) override {
    mBits.proxy->GetCOMInterface(aOutAccessible);
  }
};

class HyperTextProxyAccessibleWrap : public HyperTextAccessibleWrap {
 public:
  explicit HyperTextProxyAccessibleWrap(ProxyAccessible* aProxy)
      : HyperTextAccessibleWrap(nullptr, nullptr) {
    mType = eProxyType;
    mBits.proxy = aProxy;
  }

  virtual void Shutdown() override {
    mBits.proxy = nullptr;
    mStateFlags |= eIsDefunct;
  }

  virtual void GetNativeInterface(void** aOutAccessible) override {
    mBits.proxy->GetCOMInterface(aOutAccessible);
  }
};

class DocProxyAccessibleWrap : public HyperTextProxyAccessibleWrap {
 public:
  explicit DocProxyAccessibleWrap(ProxyAccessible* aProxy)
      : HyperTextProxyAccessibleWrap(aProxy) {
    mGenericTypes |= eDocument;
  }

  void AddID(uint32_t aID, AccessibleWrap* aAcc) {
    mIDToAccessibleMap.Put(aID, aAcc);
  }
  void RemoveID(uint32_t aID) { mIDToAccessibleMap.Remove(aID); }
  AccessibleWrap* GetAccessibleByID(uint32_t aID) const {
    return mIDToAccessibleMap.Get(aID);
  }

  virtual nsIntRect Bounds() const override {
    // OuterDocAccessible can return a DocProxyAccessibleWrap as a child.
    // Accessible::ChildAtPoint on an ancestor might retrieve this proxy and
    // call Bounds() on it. This will crash on a proxy, so we override it to do
    // nothing here.
    return nsIntRect();
  }

 private:
  /*
   * This provides a mapping from 32 bit id to accessible objects.
   */
  nsDataHashtable<nsUint32HashKey, AccessibleWrap*> mIDToAccessibleMap;
};

template <typename T>
inline ProxyAccessible* HyperTextProxyFor(T* aWrapper) {
  static_assert(mozilla::IsBaseOf<IUnknown, T>::value,
                "only IAccessible* should be passed in");
  auto wrapper = static_cast<HyperTextProxyAccessibleWrap*>(aWrapper);
  return wrapper->IsProxy() ? wrapper->Proxy() : nullptr;
}

/**
 * Stub AccessibleWrap used in a content process for an embedded document
 * residing in another content process.
 * There is no ProxyAccessible here, since those only exist in the parent
 * process. However, like ProxyAccessibleWrap, the only real method that
 * gets called is GetNativeInterface, which returns a COM proxy for the
 * document.
 */
class RemoteIframeDocProxyAccessibleWrap : public HyperTextAccessibleWrap {
 public:
  explicit RemoteIframeDocProxyAccessibleWrap(IDispatch* aCOMProxy)
      : HyperTextAccessibleWrap(nullptr, nullptr), mCOMProxy(aCOMProxy) {
    mType = eProxyType;
    mBits.proxy = nullptr;
  }

  virtual void Shutdown() override {
    mStateFlags |= eIsDefunct;
    mCOMProxy = nullptr;
  }

  virtual void GetNativeInterface(void** aOutAccessible) override {
    RefPtr<IDispatch> addRefed = mCOMProxy;
    addRefed.forget(aOutAccessible);
  }

  virtual nsIntRect Bounds() const override {
    // OuterDocAccessible can return a RemoteIframeDocProxyAccessibleWrap as a
    // child. Accessible::ChildAtPoint on an ancestor might retrieve this proxy
    // and call Bounds() on it. This will crash on a proxy, so we override it
    // to do nothing here.
    return nsIntRect();
  }

 private:
  RefPtr<IDispatch> mCOMProxy;
};

}  // namespace a11y
}  // namespace mozilla

#endif
