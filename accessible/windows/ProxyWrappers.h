/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. *
 */

#ifndef MOZILLA_A11Y_ProxyWrappers_h
#define MOZILLA_A11Y_ProxyWrappers_h

#include "HyperTextAccessible.h"

namespace mozilla {
namespace a11y {

class ProxyAccessibleWrap : public AccessibleWrap
{
  public:
  ProxyAccessibleWrap(ProxyAccessible* aProxy) :
    AccessibleWrap(nullptr, nullptr)
  {
    mType = eProxyType;
    mBits.proxy = aProxy;
  }

  virtual void Shutdown() override
  {
    mBits.proxy = nullptr;
  }
};

class HyperTextProxyAccessibleWrap : public HyperTextAccessibleWrap
{
public:
  HyperTextProxyAccessibleWrap(ProxyAccessible* aProxy) :
    HyperTextAccessibleWrap(nullptr, nullptr)
  {
    mType = eProxyType;
    mBits.proxy = aProxy;
  }

  virtual void Shutdown() override { mBits.proxy = nullptr; }
};

class DocProxyAccessibleWrap : public HyperTextProxyAccessibleWrap
{
public:
  DocProxyAccessibleWrap(ProxyAccessible* aProxy) :
    HyperTextProxyAccessibleWrap(aProxy) {}
};

template<typename T>
inline ProxyAccessible*
HyperTextProxyFor(T* aWrapper)
{
  static_assert(mozilla::IsBaseOf<IUnknown, T>::value, "only IAccessible* should be passed in");
  auto wrapper = static_cast<HyperTextProxyAccessibleWrap*>(aWrapper);
  return wrapper->IsProxy() ? wrapper->Proxy() : nullptr;
}

}
}

#endif
