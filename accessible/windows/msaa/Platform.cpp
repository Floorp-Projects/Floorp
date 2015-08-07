/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Platform.h"

#include "AccEvent.h"
#include "Compatibility.h"
#include "HyperTextAccessibleWrap.h"
#include "ia2AccessibleText.h"
#include "nsWinUtils.h"
#include "mozilla/a11y/ProxyAccessible.h"
#include "ProxyWrappers.h"

using namespace mozilla;
using namespace mozilla::a11y;

void
a11y::PlatformInit()
{
  Compatibility::Init();

  nsWinUtils::MaybeStartWindowEmulation();
  ia2AccessibleText::InitTextChangeData();
}

void
a11y::PlatformShutdown()
{
  ::DestroyCaret();

  nsWinUtils::ShutdownWindowEmulation();
}

void
a11y::ProxyCreated(ProxyAccessible* aProxy, uint32_t aInterfaces)
{
  AccessibleWrap* wrapper = nullptr;
  if (aInterfaces & Interfaces::DOCUMENT) {
    wrapper = new DocProxyAccessibleWrap(aProxy);
  } else if (aInterfaces & Interfaces::HYPERTEXT) {
    wrapper = new HyperTextProxyAccessibleWrap(aProxy);
  } else {
    wrapper = new ProxyAccessibleWrap(aProxy);
  }

  wrapper->AddRef();
  aProxy->SetWrapper(reinterpret_cast<uintptr_t>(wrapper));
}

void
a11y::ProxyDestroyed(ProxyAccessible* aProxy)
{
  AccessibleWrap* wrapper =
    reinterpret_cast<AccessibleWrap*>(aProxy->GetWrapper());
  MOZ_ASSERT(wrapper);
  if (!wrapper)
    return;

  wrapper->Shutdown();
  aProxy->SetWrapper(0);
  wrapper->Release();
}

void
a11y::ProxyEvent(ProxyAccessible*, uint32_t)
{
}

void
a11y::ProxyStateChangeEvent(ProxyAccessible*, uint64_t, bool)
{
}

void
a11y::ProxyCaretMoveEvent(ProxyAccessible* aTarget, int32_t aOffset)
{
}

void
a11y::ProxyTextChangeEvent(ProxyAccessible* aText, const nsString& aStr,
                           int32_t aStart, uint32_t aLen, bool aInsert, bool)
{
  AccessibleWrap* wrapper = WrapperFor(aText);
  MOZ_ASSERT(wrapper);
  if (!wrapper) {
    return;
  }

  auto text = static_cast<HyperTextAccessibleWrap*>(wrapper->AsHyperText());
  if (text) {
    ia2AccessibleText::UpdateTextChangeData(text, aInsert, aStr, aStart, aLen);
  }
}
