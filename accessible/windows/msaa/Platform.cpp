/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Platform.h"

#include "AccEvent.h"
#include "Compatibility.h"
#include "HyperTextAccessibleWrap.h"
#include "nsWinUtils.h"
#include "mozilla/a11y/ProxyAccessible.h"

#include "mozilla/ClearOnShutdown.h"

using namespace mozilla;
using namespace mozilla::a11y;

void
a11y::PlatformInit()
{
  Compatibility::Init();

  nsWinUtils::MaybeStartWindowEmulation();
  ClearOnShutdown(&HyperTextAccessibleWrap::sLastTextChangeAcc);
  ClearOnShutdown(&HyperTextAccessibleWrap::sLastTextChangeString);
}

void
a11y::PlatformShutdown()
{
  ::DestroyCaret();

  nsWinUtils::ShutdownWindowEmulation();
}

class ProxyAccessibleWrap : public AccessibleWrap
{
  public:
  ProxyAccessibleWrap(ProxyAccessible* aProxy) :
    AccessibleWrap(nullptr, nullptr)
  {
    mType = eProxyType;
    mBits.proxy = aProxy;
  }

  virtual void Shutdown() MOZ_OVERRIDE
  {
    mBits.proxy = nullptr;
  }
};

void
a11y::ProxyCreated(ProxyAccessible* aProxy, uint32_t)
{
  ProxyAccessibleWrap* wrapper = new ProxyAccessibleWrap(aProxy);
  wrapper->AddRef();
  aProxy->SetWrapper(reinterpret_cast<uintptr_t>(wrapper));
}

void
a11y::ProxyDestroyed(ProxyAccessible* aProxy)
{
  ProxyAccessibleWrap* wrapper =
    reinterpret_cast<ProxyAccessibleWrap*>(aProxy->GetWrapper());
  wrapper->Shutdown();
  aProxy->SetWrapper(0);
  wrapper->Release();
}

void
a11y::ProxyEvent(ProxyAccessible*, uint32_t)
{
}
