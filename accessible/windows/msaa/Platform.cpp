/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Platform.h"

#include "AccEvent.h"
#include "Compatibility.h"
#include "HyperTextAccessibleWrap.h"
#include "nsIWindowsRegKey.h"
#include "nsWinUtils.h"
#include "mozilla/a11y/DocAccessibleParent.h"
#include "mozilla/a11y/RemoteAccessible.h"
#include "mozilla/mscom/ActivationContext.h"
#include "mozilla/mscom/InterceptorLog.h"
#include "mozilla/mscom/Registration.h"
#include "mozilla/mscom/Utils.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/WindowsVersion.h"
#include "mozilla/WinHeaderOnlyUtils.h"
#include "nsAccessibilityService.h"
#include "nsComponentManagerUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "ProxyWrappers.h"

#if defined(MOZ_TELEMETRY_REPORTING)
#  include "mozilla/Telemetry.h"
#endif  // defined(MOZ_TELEMETRY_REPORTING)

using namespace mozilla;
using namespace mozilla::a11y;
using namespace mozilla::mscom;

static StaticAutoPtr<RegisteredProxy> gRegCustomProxy;
static StaticAutoPtr<RegisteredProxy> gRegProxy;
static StaticAutoPtr<RegisteredProxy> gRegAccTlb;
static StaticAutoPtr<RegisteredProxy> gRegMiscTlb;
static StaticRefPtr<nsIFile> gInstantiator;

void a11y::PlatformInit() {
  nsWinUtils::MaybeStartWindowEmulation();
  ia2AccessibleText::InitTextChangeData();

  mscom::InterceptorLog::Init();
  UniquePtr<RegisteredProxy> regCustomProxy(mscom::RegisterProxy());
  gRegCustomProxy = regCustomProxy.release();
  UniquePtr<RegisteredProxy> regProxy(mscom::RegisterProxy(L"ia2marshal.dll"));
  gRegProxy = regProxy.release();
  UniquePtr<RegisteredProxy> regAccTlb(mscom::RegisterTypelib(
      L"oleacc.dll", RegistrationFlags::eUseSystemDirectory));
  gRegAccTlb = regAccTlb.release();
  UniquePtr<RegisteredProxy> regMiscTlb(
      mscom::RegisterTypelib(L"Accessible.tlb"));
  gRegMiscTlb = regMiscTlb.release();
}

void a11y::PlatformShutdown() {
  ::DestroyCaret();

  nsWinUtils::ShutdownWindowEmulation();
  gRegCustomProxy = nullptr;
  gRegProxy = nullptr;
  gRegAccTlb = nullptr;
  gRegMiscTlb = nullptr;

  if (gInstantiator) {
    gInstantiator = nullptr;
  }
}

void a11y::ProxyCreated(RemoteAccessible* aProxy) {
  AccessibleWrap* wrapper = nullptr;
  if (aProxy->IsDoc()) {
    wrapper = new DocRemoteAccessibleWrap(aProxy);
  } else if (aProxy->IsHyperText()) {
    wrapper = new HyperTextRemoteAccessibleWrap(aProxy);
  } else {
    wrapper = new RemoteAccessibleWrap(aProxy);
  }

  wrapper->AddRef();
  aProxy->SetWrapper(reinterpret_cast<uintptr_t>(wrapper));
}

void a11y::ProxyDestroyed(RemoteAccessible* aProxy) {
  AccessibleWrap* wrapper =
      reinterpret_cast<AccessibleWrap*>(aProxy->GetWrapper());

  // If aProxy is a document that was created, but
  // RecvPDocAccessibleConstructor failed then aProxy->GetWrapper() will be
  // null.
  if (!wrapper) return;

  if (aProxy->IsDoc() && nsWinUtils::IsWindowEmulationStarted()) {
    aProxy->AsDoc()->SetEmulatedWindowHandle(nullptr);
  }

  wrapper->Shutdown();
  aProxy->SetWrapper(0);
  wrapper->Release();
}

void a11y::ProxyEvent(RemoteAccessible* aTarget, uint32_t aEventType) {
  MsaaAccessible::FireWinEvent(WrapperFor(aTarget), aEventType);
}

void a11y::ProxyStateChangeEvent(RemoteAccessible* aTarget, uint64_t, bool) {
  MsaaAccessible::FireWinEvent(WrapperFor(aTarget),
                               nsIAccessibleEvent::EVENT_STATE_CHANGE);
}

void a11y::ProxyFocusEvent(RemoteAccessible* aTarget,
                           const LayoutDeviceIntRect& aCaretRect) {
  FocusManager* focusMgr = FocusMgr();
  if (focusMgr && focusMgr->FocusedAccessible()) {
    // This is a focus event from a remote document, but focus has moved out
    // of that document into the chrome since that event was sent. For example,
    // this can happen when choosing File menu -> New Tab. See bug 1471466.
    // Note that this does not handle the case where a focus event is sent from
    // one remote document, but focus moved into a second remote document
    // since that event was sent. However, this isn't something anyone has been
    // able to trigger.
    return;
  }

  AccessibleWrap::UpdateSystemCaretFor(aTarget, aCaretRect);
  MsaaAccessible::FireWinEvent(WrapperFor(aTarget),
                               nsIAccessibleEvent::EVENT_FOCUS);
}

void a11y::ProxyCaretMoveEvent(RemoteAccessible* aTarget,
                               const LayoutDeviceIntRect& aCaretRect) {
  AccessibleWrap::UpdateSystemCaretFor(aTarget, aCaretRect);
  MsaaAccessible::FireWinEvent(WrapperFor(aTarget),
                               nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED);
}

void a11y::ProxyTextChangeEvent(RemoteAccessible* aText, const nsString& aStr,
                                int32_t aStart, uint32_t aLen, bool aInsert,
                                bool) {
  AccessibleWrap* wrapper = WrapperFor(aText);
  MOZ_ASSERT(wrapper);
  if (!wrapper) {
    return;
  }

  static const bool useHandler =
      Preferences::GetBool("accessibility.handler.enabled", false) &&
      IsHandlerRegistered();

  if (useHandler) {
    wrapper->DispatchTextChangeToHandler(aInsert, aStr, aStart, aLen);
    return;
  }

  auto text = static_cast<HyperTextAccessibleWrap*>(wrapper->AsHyperText());
  if (text) {
    ia2AccessibleText::UpdateTextChangeData(text, aInsert, aStr, aStart, aLen);
  }

  uint32_t eventType = aInsert ? nsIAccessibleEvent::EVENT_TEXT_INSERTED
                               : nsIAccessibleEvent::EVENT_TEXT_REMOVED;
  MsaaAccessible::FireWinEvent(wrapper, eventType);
}

void a11y::ProxyShowHideEvent(RemoteAccessible* aTarget, RemoteAccessible*,
                              bool aInsert, bool) {
  uint32_t event =
      aInsert ? nsIAccessibleEvent::EVENT_SHOW : nsIAccessibleEvent::EVENT_HIDE;
  AccessibleWrap* wrapper = WrapperFor(aTarget);
  MsaaAccessible::FireWinEvent(wrapper, event);
}

void a11y::ProxySelectionEvent(RemoteAccessible* aTarget, RemoteAccessible*,
                               uint32_t aType) {
  AccessibleWrap* wrapper = WrapperFor(aTarget);
  MsaaAccessible::FireWinEvent(wrapper, aType);
}

bool a11y::IsHandlerRegistered() {
  nsresult rv;
  nsCOMPtr<nsIWindowsRegKey> regKey =
      do_CreateInstance("@mozilla.org/windows-registry-key;1", &rv);
  if (NS_FAILED(rv)) {
    return false;
  }

  nsAutoString clsid;
  GUIDToString(CLSID_AccessibleHandler, clsid);

  nsAutoString subKey;
  subKey.AppendLiteral(u"SOFTWARE\\Classes\\CLSID\\");
  subKey.Append(clsid);
  subKey.AppendLiteral(u"\\InprocHandler32");

  rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_LOCAL_MACHINE, subKey,
                    nsIWindowsRegKey::ACCESS_READ);
  if (NS_FAILED(rv)) {
    return false;
  }

  nsAutoString handlerPath;
  rv = regKey->ReadStringValue(nsAutoString(), handlerPath);
  if (NS_FAILED(rv)) {
    return false;
  }

  nsCOMPtr<nsIFile> actualHandler;
  rv = NS_NewLocalFile(handlerPath, false, getter_AddRefs(actualHandler));
  if (NS_FAILED(rv)) {
    return false;
  }

  nsCOMPtr<nsIFile> expectedHandler;
  rv = NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(expectedHandler));
  if (NS_FAILED(rv)) {
    return false;
  }

  rv = expectedHandler->Append(u"AccessibleHandler.dll"_ns);
  if (NS_FAILED(rv)) {
    return false;
  }

  bool equal;
  rv = expectedHandler->Equals(actualHandler, &equal);
  return NS_SUCCEEDED(rv) && equal;
}

static bool GetInstantiatorExecutable(const DWORD aPid,
                                      nsIFile** aOutClientExe) {
  nsAutoHandle callingProcess(
      ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, aPid));
  if (!callingProcess) {
    return false;
  }

  DWORD bufLen = MAX_PATH;
  UniquePtr<wchar_t[]> buf;

  while (true) {
    buf = MakeUnique<wchar_t[]>(bufLen);
    if (::QueryFullProcessImageName(callingProcess, 0, buf.get(), &bufLen)) {
      break;
    }

    DWORD lastError = ::GetLastError();
    MOZ_ASSERT(lastError == ERROR_INSUFFICIENT_BUFFER);
    if (lastError != ERROR_INSUFFICIENT_BUFFER) {
      return false;
    }

    bufLen *= 2;
  }

  nsCOMPtr<nsIFile> file;
  nsresult rv = NS_NewLocalFile(nsDependentString(buf.get(), bufLen), false,
                                getter_AddRefs(file));
  if (NS_FAILED(rv)) {
    return false;
  }

  file.forget(aOutClientExe);
  return NS_SUCCEEDED(rv);
}

/**
 * Appends version information in the format "|a.b.c.d".
 * If there is no version information, we append nothing.
 */
static void AppendVersionInfo(nsIFile* aClientExe, nsAString& aStrToAppend) {
  MOZ_ASSERT(!NS_IsMainThread());

  LauncherResult<ModuleVersion> version = GetModuleVersion(aClientExe);
  if (version.isErr()) {
    return;
  }

  uint16_t major, minor, patch, build;
  Tie(major, minor, patch, build) = version.unwrap().AsTuple();

  aStrToAppend.AppendLiteral(u"|");

  constexpr auto dot = u"."_ns;

  aStrToAppend.AppendInt(major);
  aStrToAppend.Append(dot);
  aStrToAppend.AppendInt(minor);
  aStrToAppend.Append(dot);
  aStrToAppend.AppendInt(patch);
  aStrToAppend.Append(dot);
  aStrToAppend.AppendInt(build);
}

static void AccumulateInstantiatorTelemetry(const nsAString& aValue) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!aValue.IsEmpty()) {
#if defined(MOZ_TELEMETRY_REPORTING)
    Telemetry::ScalarSet(Telemetry::ScalarID::A11Y_INSTANTIATORS, aValue);
#endif  // defined(MOZ_TELEMETRY_REPORTING)
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::AccessibilityClient,
        NS_ConvertUTF16toUTF8(aValue));
  }
}

static void GatherInstantiatorTelemetry(nsIFile* aClientExe) {
  MOZ_ASSERT(!NS_IsMainThread());

  nsString value;
  nsresult rv = aClientExe->GetLeafName(value);
  if (NS_SUCCEEDED(rv)) {
    AppendVersionInfo(aClientExe, value);
  }

  nsCOMPtr<nsIRunnable> runnable(
      NS_NewRunnableFunction("a11y::AccumulateInstantiatorTelemetry",
                             [value = std::move(value)]() -> void {
                               AccumulateInstantiatorTelemetry(value);
                             }));

  // Now that we've (possibly) obtained version info, send the resulting
  // string back to the main thread to accumulate in telemetry.
  NS_DispatchToMainThread(runnable.forget());
}

void a11y::SetInstantiator(const uint32_t aPid) {
  nsCOMPtr<nsIFile> clientExe;
  if (!GetInstantiatorExecutable(aPid, getter_AddRefs(clientExe))) {
    AccumulateInstantiatorTelemetry(
        u"(Failed to retrieve client image name)"_ns);
    return;
  }

  // Only record the instantiator if it is the first instantiator, or if it does
  // not match the previous one. Some blocked clients are repeatedly requesting
  // a11y over and over so we don't want to be spawning countless telemetry
  // threads.
  if (gInstantiator) {
    bool equal;
    nsresult rv = gInstantiator->Equals(clientExe, &equal);
    if (NS_SUCCEEDED(rv) && equal) {
      return;
    }
  }

  gInstantiator = clientExe;

  nsCOMPtr<nsIRunnable> runnable(
      NS_NewRunnableFunction("a11y::GatherInstantiatorTelemetry",
                             [clientExe = std::move(clientExe)]() -> void {
                               GatherInstantiatorTelemetry(clientExe);
                             }));

  DebugOnly<nsresult> rv =
      NS_DispatchBackgroundTask(runnable.forget(), NS_DISPATCH_EVENT_MAY_BLOCK);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

bool a11y::GetInstantiator(nsIFile** aOutInstantiator) {
  if (!gInstantiator) {
    return false;
  }

  return NS_SUCCEEDED(gInstantiator->Clone(aOutInstantiator));
}
