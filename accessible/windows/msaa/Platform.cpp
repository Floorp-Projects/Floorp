/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Platform.h"

#include "AccEvent.h"
#include "Compatibility.h"
#include "HyperTextAccessible.h"
#include "nsWinUtils.h"
#include "mozilla/a11y/DocAccessibleParent.h"
#include "mozilla/a11y/RemoteAccessible.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/WindowsVersion.h"
#include "mozilla/WinHeaderOnlyUtils.h"
#include "WinUtils.h"
#include "ia2AccessibleText.h"

#include <tuple>

#if defined(MOZ_TELEMETRY_REPORTING)
#  include "mozilla/Telemetry.h"
#endif  // defined(MOZ_TELEMETRY_REPORTING)

using namespace mozilla;
using namespace mozilla::a11y;
using namespace mozilla::mscom;

static StaticRefPtr<nsIFile> gInstantiator;

void a11y::PlatformInit() {
  nsWinUtils::MaybeStartWindowEmulation();
  ia2AccessibleText::InitTextChangeData();
}

void a11y::PlatformShutdown() {
  ::DestroyCaret();

  nsWinUtils::ShutdownWindowEmulation();

  if (gInstantiator) {
    gInstantiator = nullptr;
  }
}

void a11y::ProxyCreated(RemoteAccessible* aProxy) {
  MsaaAccessible* msaa = MsaaAccessible::Create(aProxy);
  msaa->AddRef();
  aProxy->SetWrapper(reinterpret_cast<uintptr_t>(msaa));
}

void a11y::ProxyDestroyed(RemoteAccessible* aProxy) {
  MsaaAccessible* msaa =
      reinterpret_cast<MsaaAccessible*>(aProxy->GetWrapper());
  if (!msaa) {
    return;
  }
  msaa->MsaaShutdown();
  aProxy->SetWrapper(0);
  msaa->Release();

  if (aProxy->IsDoc() && nsWinUtils::IsWindowEmulationStarted()) {
    aProxy->AsDoc()->SetEmulatedWindowHandle(nullptr);
  }
}

void a11y::PlatformEvent(Accessible* aTarget, uint32_t aEventType) {
  MsaaAccessible::FireWinEvent(aTarget, aEventType);
}

void a11y::PlatformStateChangeEvent(Accessible* aTarget, uint64_t, bool) {
  MsaaAccessible::FireWinEvent(aTarget, nsIAccessibleEvent::EVENT_STATE_CHANGE);
}

void a11y::PlatformFocusEvent(Accessible* aTarget,
                              const LayoutDeviceIntRect& aCaretRect) {
  if (aTarget->IsRemote() && FocusMgr() &&
      FocusMgr()->FocusedLocalAccessible()) {
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
  MsaaAccessible::FireWinEvent(aTarget, nsIAccessibleEvent::EVENT_FOCUS);
}

void a11y::PlatformCaretMoveEvent(Accessible* aTarget, int32_t aOffset,
                                  bool aIsSelectionCollapsed,
                                  int32_t aGranularity,
                                  const LayoutDeviceIntRect& aCaretRect) {
  AccessibleWrap::UpdateSystemCaretFor(aTarget, aCaretRect);
  MsaaAccessible::FireWinEvent(aTarget,
                               nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED);
}

void a11y::PlatformTextChangeEvent(Accessible* aText, const nsAString& aStr,
                                   int32_t aStart, uint32_t aLen, bool aInsert,
                                   bool) {
  uint32_t eventType = aInsert ? nsIAccessibleEvent::EVENT_TEXT_INSERTED
                               : nsIAccessibleEvent::EVENT_TEXT_REMOVED;
  MOZ_ASSERT(aText->IsHyperText());
  ia2AccessibleText::UpdateTextChangeData(aText->AsHyperTextBase(), aInsert,
                                          aStr, aStart, aLen);
  MsaaAccessible::FireWinEvent(aText, eventType);
}

void a11y::PlatformShowHideEvent(Accessible* aTarget, Accessible*, bool aInsert,
                                 bool) {
  uint32_t event =
      aInsert ? nsIAccessibleEvent::EVENT_SHOW : nsIAccessibleEvent::EVENT_HIDE;
  MsaaAccessible::FireWinEvent(aTarget, event);
}

void a11y::PlatformSelectionEvent(Accessible* aTarget, Accessible*,
                                  uint32_t aType) {
  MsaaAccessible::FireWinEvent(aTarget, aType);
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

  auto [major, minor, patch, build] = version.unwrap().AsTuple();

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
