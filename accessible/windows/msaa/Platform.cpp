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
#include "nsIWindowsRegKey.h"
#include "nsIXULRuntime.h"
#include "nsWinUtils.h"
#include "mozilla/a11y/ProxyAccessible.h"
#include "mozilla/mscom/ActivationContext.h"
#include "mozilla/mscom/InterceptorLog.h"
#include "mozilla/mscom/Registration.h"
#include "mozilla/mscom/Utils.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/WindowsVersion.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "ProxyWrappers.h"

#if defined(MOZ_TELEMETRY_REPORTING)
#include "mozilla/Telemetry.h"
#endif // defined(MOZ_TELEMETRY_REPORTING)

using namespace mozilla;
using namespace mozilla::a11y;
using namespace mozilla::mscom;

static StaticAutoPtr<RegisteredProxy> gRegCustomProxy;
static StaticAutoPtr<RegisteredProxy> gRegProxy;
static StaticAutoPtr<RegisteredProxy> gRegAccTlb;
static StaticAutoPtr<RegisteredProxy> gRegMiscTlb;
static StaticRefPtr<nsIFile> gInstantiator;

void
a11y::PlatformInit()
{
  nsWinUtils::MaybeStartWindowEmulation();
  ia2AccessibleText::InitTextChangeData();

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

void
a11y::PlatformShutdown()
{
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

bool
a11y::IsHandlerRegistered()
{
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

  rv = expectedHandler->Append(NS_LITERAL_STRING("AccessibleHandler.dll"));
  if (NS_FAILED(rv)) {
    return false;
  }

  bool equal;
  rv = expectedHandler->Equals(actualHandler, &equal);
  return NS_SUCCEEDED(rv) && equal;
}

static bool
GetInstantiatorExecutable(const DWORD aPid, nsIFile** aOutClientExe)
{
  nsAutoHandle callingProcess(::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                                            FALSE, aPid));
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
static void
AppendVersionInfo(nsIFile* aClientExe, nsAString& aStrToAppend)
{
  MOZ_ASSERT(!NS_IsMainThread());

  nsAutoString fullPath;
  nsresult rv = aClientExe->GetPath(fullPath);
  if (NS_FAILED(rv)) {
    return;
  }

  DWORD verInfoSize = ::GetFileVersionInfoSize(fullPath.get(), nullptr);
  if (!verInfoSize) {
    return;
  }

  auto verInfoBuf = MakeUnique<BYTE[]>(verInfoSize);

  if (!::GetFileVersionInfo(fullPath.get(), 0, verInfoSize, verInfoBuf.get())) {
    return;
  }

  VS_FIXEDFILEINFO* fixedInfo = nullptr;
  UINT fixedInfoLen = 0;

  if (!::VerQueryValue(verInfoBuf.get(), L"\\", (LPVOID*) &fixedInfo,
                       &fixedInfoLen)) {
    return;
  }

  uint32_t major = HIWORD(fixedInfo->dwFileVersionMS);
  uint32_t minor = LOWORD(fixedInfo->dwFileVersionMS);
  uint32_t patch = HIWORD(fixedInfo->dwFileVersionLS);
  uint32_t build = LOWORD(fixedInfo->dwFileVersionLS);

  aStrToAppend.AppendLiteral(u"|");

  NS_NAMED_LITERAL_STRING(dot, ".");

  aStrToAppend.AppendInt(major);
  aStrToAppend.Append(dot);
  aStrToAppend.AppendInt(minor);
  aStrToAppend.Append(dot);
  aStrToAppend.AppendInt(patch);
  aStrToAppend.Append(dot);
  aStrToAppend.AppendInt(build);
}

static void
AccumulateInstantiatorTelemetry(nsIFile* aClientExe, const nsAString& aValue)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!aValue.IsEmpty()) {
#if defined(MOZ_TELEMETRY_REPORTING)
    Telemetry::ScalarSet(Telemetry::ScalarID::A11Y_INSTANTIATORS,
                         aValue);
#endif // defined(MOZ_TELEMETRY_REPORTING)
    CrashReporter::
      AnnotateCrashReport(NS_LITERAL_CSTRING("AccessibilityClient"),
                          NS_ConvertUTF16toUTF8(aValue));
  }
}

static void
GatherInstantiatorTelemetry(nsIFile* aClientExe)
{
  MOZ_ASSERT(!NS_IsMainThread());

  nsString value;
  nsresult rv = aClientExe->GetLeafName(value);
  if (NS_SUCCEEDED(rv)) {
    AppendVersionInfo(aClientExe, value);
  }

  nsCOMPtr<nsIFile> ref(aClientExe);
  nsCOMPtr<nsIRunnable> runnable(
    NS_NewRunnableFunction("a11y::AccumulateInstantiatorTelemetry",
                           [ref, value]() -> void {
                             AccumulateInstantiatorTelemetry(ref, value);
                           }));

  // Now that we've (possibly) obtained version info, send the resulting
  // string back to the main thread to accumulate in telemetry.
  NS_DispatchToMainThread(runnable);
}

void
a11y::SetInstantiator(const uint32_t aPid)
{
  nsCOMPtr<nsIFile> clientExe;
  if (!GetInstantiatorExecutable(aPid, getter_AddRefs(clientExe))) {
#if defined(MOZ_TELEMETRY_REPORTING)
    AccumulateInstantiatorTelemetry(nullptr, NS_LITERAL_STRING("(Failed to retrieve client image name)"));
#endif // defined(MOZ_TELEMETRY_REPORTING)
    return;
  }

  gInstantiator = clientExe;

  nsCOMPtr<nsIRunnable> runnable(
    NS_NewRunnableFunction("a11y::GatherInstantiatorTelemetry",
                           [clientExe]() -> void {
                             GatherInstantiatorTelemetry(clientExe);
                           }));

  nsCOMPtr<nsIThread> telemetryThread;
  NS_NewThread(getter_AddRefs(telemetryThread), runnable);
}

bool
a11y::GetInstantiator(nsIFile** aOutInstantiator)
{
  if (!gInstantiator) {
    return false;
  }

  return NS_SUCCEEDED(gInstantiator->Clone(aOutInstantiator));
}
