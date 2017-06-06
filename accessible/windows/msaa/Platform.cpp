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
#include "nsIXULRuntime.h"
#include "nsWinUtils.h"
#include "mozilla/a11y/ProxyAccessible.h"
#include "mozilla/mscom/ActivationContext.h"
#include "mozilla/mscom/InterceptorLog.h"
#include "mozilla/mscom/Registration.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/WindowsVersion.h"
#include "ProxyWrappers.h"

using namespace mozilla;
using namespace mozilla::a11y;
using namespace mozilla::mscom;

static StaticAutoPtr<RegisteredProxy> gRegCustomProxy;
static StaticAutoPtr<RegisteredProxy> gRegProxy;
static StaticAutoPtr<RegisteredProxy> gRegAccTlb;
static StaticAutoPtr<RegisteredProxy> gRegMiscTlb;
static StaticAutoPtr<ActivationContextRegion> gActCtxRgn;

void
a11y::PlatformInit()
{
  Compatibility::Init();

  nsWinUtils::MaybeStartWindowEmulation();
  ia2AccessibleText::InitTextChangeData();
  if (BrowserTabsRemoteAutostart()) {
    // The manifest for 32-bit Windows is embedded with resource ID 32.
    // The manifest for 64-bit Windows is embedded with resource ID 64.
    // Beginning with Windows 10 Creators Update, 32-bit builds use the 64-bit
    // manifest.
    DWORD actCtxResourceId;
#if defined(HAVE_64BIT_BUILD)
    actCtxResourceId = 64;
#else
    if (IsWin10CreatorsUpdateOrLater()) {
      actCtxResourceId = 64;
    } else {
      actCtxResourceId = 32;
    }
#endif
    gActCtxRgn = new ActivationContextRegion(actCtxResourceId);
    mscom::InterceptorLog::Init();
    UniquePtr<RegisteredProxy> regCustomProxy(
        mscom::RegisterProxy());
    gRegCustomProxy = regCustomProxy.release();
    UniquePtr<RegisteredProxy> regProxy(
        mscom::RegisterProxy(L"ia2marshal.dll"));
    gRegProxy = regProxy.release();
    UniquePtr<RegisteredProxy> regAccTlb(
        mscom::RegisterTypelib(L"oleacc.dll",
                               RegistrationFlags::eUseSystemDirectory));
    gRegAccTlb = regAccTlb.release();
    UniquePtr<RegisteredProxy> regMiscTlb(
        mscom::RegisterTypelib(L"Accessible.tlb"));
    gRegMiscTlb = regMiscTlb.release();
  }
}

void
a11y::PlatformShutdown()
{
  ::DestroyCaret();

  nsWinUtils::ShutdownWindowEmulation();
  gRegCustomProxy = nullptr;
  gRegProxy = nullptr;
  gRegAccTlb = nullptr;
  gRegMiscTlb = nullptr;
  gActCtxRgn = nullptr;
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

  wrapper->SetProxyInterfaces(aInterfaces);
  wrapper->AddRef();
  aProxy->SetWrapper(reinterpret_cast<uintptr_t>(wrapper));
}

void
a11y::ProxyDestroyed(ProxyAccessible* aProxy)
{
  AccessibleWrap* wrapper =
    reinterpret_cast<AccessibleWrap*>(aProxy->GetWrapper());

  // If aProxy is a document that was created, but
  // RecvPDocAccessibleConstructor failed then aProxy->GetWrapper() will be
  // null.
  if (!wrapper)
    return;

  if (aProxy->IsDoc() && nsWinUtils::IsWindowEmulationStarted()) {
    aProxy->AsDoc()->SetEmulatedWindowHandle(nullptr);
  }

  wrapper->Shutdown();
  aProxy->SetWrapper(0);
  wrapper->Release();
}

void
a11y::ProxyEvent(ProxyAccessible* aTarget, uint32_t aEventType)
{
  AccessibleWrap::FireWinEvent(WrapperFor(aTarget), aEventType);
}

void
a11y::ProxyStateChangeEvent(ProxyAccessible* aTarget, uint64_t, bool)
{
  AccessibleWrap::FireWinEvent(WrapperFor(aTarget),
                               nsIAccessibleEvent::EVENT_STATE_CHANGE);
}

void
a11y::ProxyFocusEvent(ProxyAccessible* aTarget,
                      const LayoutDeviceIntRect& aCaretRect)
{
  AccessibleWrap::UpdateSystemCaretFor(aTarget, aCaretRect);
  AccessibleWrap::FireWinEvent(WrapperFor(aTarget),
                               nsIAccessibleEvent::EVENT_FOCUS);
}

void
a11y::ProxyCaretMoveEvent(ProxyAccessible* aTarget,
                          const LayoutDeviceIntRect& aCaretRect)
{
  AccessibleWrap::UpdateSystemCaretFor(aTarget, aCaretRect);
  AccessibleWrap::FireWinEvent(WrapperFor(aTarget),
                               nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED);
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

  static const bool useHandler =
    Preferences::GetBool("accessibility.handler.enabled", false);

  if (useHandler) {
    wrapper->DispatchTextChangeToHandler(aInsert, aStr, aStart, aLen);
    return;
  }

  auto text = static_cast<HyperTextAccessibleWrap*>(wrapper->AsHyperText());
  if (text) {
    ia2AccessibleText::UpdateTextChangeData(text, aInsert, aStr, aStart, aLen);
  }

  uint32_t eventType = aInsert ? nsIAccessibleEvent::EVENT_TEXT_INSERTED :
    nsIAccessibleEvent::EVENT_TEXT_REMOVED;
  AccessibleWrap::FireWinEvent(wrapper, eventType);
}

void
a11y::ProxyShowHideEvent(ProxyAccessible* aTarget, ProxyAccessible*, bool aInsert, bool)
{
  uint32_t event = aInsert ? nsIAccessibleEvent::EVENT_SHOW :
    nsIAccessibleEvent::EVENT_HIDE;
  AccessibleWrap* wrapper = WrapperFor(aTarget);
  AccessibleWrap::FireWinEvent(wrapper, event);
}

void
a11y::ProxySelectionEvent(ProxyAccessible* aTarget, ProxyAccessible*, uint32_t aType)
{
  AccessibleWrap* wrapper = WrapperFor(aTarget);
  AccessibleWrap::FireWinEvent(wrapper, aType);
}
