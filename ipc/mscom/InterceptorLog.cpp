/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mscom/InterceptorLog.h"

#include "MainThreadUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/mscom/Registration.h"
#include "mozilla/mscom/Utils.h"
#include "mozilla/Mutex.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Unused.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIFileStreams.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "nsXPCOMPrivate.h"
#include "nsXULAppAPI.h"
#include "prenv.h"

#include <callobj.h>

using mozilla::DebugOnly;
using mozilla::mscom::ArrayData;
using mozilla::mscom::FindArrayData;
using mozilla::mscom::IsValidGUID;
using mozilla::Mutex;
using mozilla::MutexAutoLock;
using mozilla::NewNonOwningRunnableMethod;
using mozilla::services::GetObserverService;
using mozilla::StaticAutoPtr;
using mozilla::TimeDuration;
using mozilla::TimeStamp;
using mozilla::Unused;

namespace {

class ShutdownEvent final : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

private:
  ~ShutdownEvent() {}
};

NS_IMPL_ISUPPORTS(ShutdownEvent, nsIObserver)

class Logger
{
public:
  explicit Logger(const nsACString& aLeafBaseName);
  bool IsValid()
  {
    MutexAutoLock lock(mMutex);
    return !!mThread;
  }
  void LogQI(HRESULT aResult, IUnknown* aTarget, REFIID aIid, IUnknown* aInterface);
  void LogEvent(ICallFrame* aCallFrame, IUnknown* aTargetInterface);
  nsresult Shutdown();

private:
  void OpenFile();
  void Flush();
  void CloseFile();
  void AssertRunningOnLoggerThread();
  bool VariantToString(const VARIANT& aVariant, nsACString& aOut, LONG aIndex = 0);
  bool TryParamAsGuid(REFIID aIid, ICallFrame* aCallFrame,
                      const CALLFRAMEPARAMINFO& aParamInfo, nsACString& aLine);
  static double GetElapsedTime();

  nsCOMPtr<nsIFile>         mLogFileName;
  nsCOMPtr<nsIOutputStream> mLogFile; // Only accessed by mThread
  Mutex                     mMutex; // Guards mThread and mEntries
  nsCOMPtr<nsIThread>       mThread;
  nsTArray<nsCString>       mEntries;
};

Logger::Logger(const nsACString& aLeafBaseName)
  : mMutex("mozilla::com::InterceptorLog::Logger")
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIFile> logFileName;
  GeckoProcessType procType = XRE_GetProcessType();
  nsAutoCString leafName(aLeafBaseName);
  nsresult rv;
  if (procType == GeckoProcessType_Default) {
    leafName.AppendLiteral("-Parent-");
    rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(logFileName));
  } else if (procType == GeckoProcessType_Content) {
    leafName.AppendLiteral("-Content-");
#if defined(MOZ_CONTENT_SANDBOX)
    rv = NS_GetSpecialDirectory(NS_APP_CONTENT_PROCESS_TEMP_DIR,
                                getter_AddRefs(logFileName));
#else
    rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR,
                                getter_AddRefs(logFileName));
#endif // defined(MOZ_CONTENT_SANDBOX)
  } else {
    return;
  }
  if (NS_FAILED(rv)) {
    return;
  }
  DWORD pid = GetCurrentProcessId();
  leafName.AppendPrintf("%u.log", pid);
  // Using AppendNative here because Windows
  rv = logFileName->AppendNative(leafName);
  if (NS_FAILED(rv)) {
    return;
  }
  mLogFileName.swap(logFileName);

  nsCOMPtr<nsIObserverService> obsSvc = GetObserverService();
  nsCOMPtr<nsIObserver> shutdownEvent = new ShutdownEvent();
  rv = obsSvc->AddObserver(shutdownEvent, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID,
                           false);
  if (NS_FAILED(rv)) {
    return;
  }

  nsCOMPtr<nsIRunnable> openRunnable(
      NewNonOwningRunnableMethod(this, &Logger::OpenFile));
  rv = NS_NewNamedThread("COM Intcpt Log", getter_AddRefs(mThread),
                         openRunnable);
  if (NS_FAILED(rv)) {
    obsSvc->RemoveObserver(shutdownEvent, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID);
  }
}

void
Logger::AssertRunningOnLoggerThread()
{
#if defined(DEBUG)
  nsCOMPtr<nsIThread> curThread;
  if (NS_FAILED(NS_GetCurrentThread(getter_AddRefs(curThread)))) {
    return;
  }
  MutexAutoLock lock(mMutex);
  MOZ_ASSERT(curThread == mThread);
#endif
}

void
Logger::OpenFile()
{
  AssertRunningOnLoggerThread();
  MOZ_ASSERT(mLogFileName && !mLogFile);
  NS_NewLocalFileOutputStream(getter_AddRefs(mLogFile), mLogFileName,
                              PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
                              PR_IRUSR | PR_IWUSR | PR_IRGRP);
}

void
Logger::CloseFile()
{
  AssertRunningOnLoggerThread();
  MOZ_ASSERT(mLogFile);
  if (!mLogFile) {
    return;
  }
  Flush();
  mLogFile->Close();
  mLogFile = nullptr;
}

nsresult
Logger::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  nsresult rv = mThread->Dispatch(NewNonOwningRunnableMethod(this,
                                                             &Logger::CloseFile),
                                  NS_DISPATCH_NORMAL);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Dispatch failed");

  rv = mThread->Shutdown();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Shutdown failed");
  return NS_OK;
}

bool
Logger::VariantToString(const VARIANT& aVariant, nsACString& aOut, LONG aIndex)
{
  switch (aVariant.vt) {
    case VT_DISPATCH: {
      aOut.AppendPrintf("(IDispatch*) 0x%0p", aVariant.pdispVal);
      return true;
    }
    case VT_DISPATCH | VT_BYREF: {
      aOut.AppendPrintf("(IDispatch*) 0x%0p", (aVariant.ppdispVal)[aIndex]);
      return true;
    }
    case VT_UNKNOWN: {
      aOut.AppendPrintf("(IUnknown*) 0x%0p", aVariant.punkVal);
      return true;
    }
    case VT_UNKNOWN | VT_BYREF: {
      aOut.AppendPrintf("(IUnknown*) 0x%0p", (aVariant.ppunkVal)[aIndex]);
      return true;
    }
    case VT_VARIANT | VT_BYREF: {
      return VariantToString((aVariant.pvarVal)[aIndex], aOut);
    }
    case VT_I4 | VT_BYREF: {
      aOut.AppendPrintf("%d", aVariant.plVal[aIndex]);
      return true;
    }
    case VT_UI4 | VT_BYREF: {
      aOut.AppendPrintf("%u", aVariant.pulVal[aIndex]);
      return true;
    }
    case VT_I4: {
      aOut.AppendPrintf("%d", aVariant.lVal);
      return true;
    }
    case VT_UI4: {
      aOut.AppendPrintf("%u", aVariant.ulVal);
      return true;
    }
    case VT_EMPTY: {
      aOut.AppendLiteral("(empty VARIANT)");
      return true;
    }
    case VT_NULL: {
      aOut.AppendLiteral("(null VARIANT)");
      return true;
    }
    case VT_BSTR: {
      aOut.AppendPrintf("\"%S\"", aVariant.bstrVal);
      return true;
    }
    case VT_BSTR | VT_BYREF: {
      aOut.AppendPrintf("\"%S\"", *aVariant.pbstrVal);
      return true;
    }
    default: {
      aOut.AppendPrintf("(VariantToString failed, VARTYPE == 0x%04hx)",
                        aVariant.vt);
      return false;
    }
  }
}

/* static */ double
Logger::GetElapsedTime()
{
  TimeStamp ts = TimeStamp::Now();
  TimeDuration duration = ts - TimeStamp::ProcessCreation();
  return duration.ToMicroseconds();
}

void
Logger::LogQI(HRESULT aResult, IUnknown* aTarget, REFIID aIid, IUnknown* aInterface)
{
  if (FAILED(aResult)) {
    return;
  }
  double elapsed = GetElapsedTime();

  nsPrintfCString line("%fus\t0x%0p\tIUnknown::QueryInterface\t([in] ", elapsed,
                       aTarget);

  WCHAR buf[39] = {0};
  if (StringFromGUID2(aIid, buf, mozilla::ArrayLength(buf))) {
    line.AppendPrintf("%S", buf);
  } else {
    line.AppendLiteral("(IID Conversion Failed)");
  }
  line.AppendPrintf(", [out] 0x%p)\t0x%08X\n", aInterface, aResult);

  MutexAutoLock lock(mMutex);
  mEntries.AppendElement(line);
  mThread->Dispatch(NewNonOwningRunnableMethod(this, &Logger::Flush),
                    NS_DISPATCH_NORMAL);
}

bool
Logger::TryParamAsGuid(REFIID aIid, ICallFrame* aCallFrame,
                       const CALLFRAMEPARAMINFO& aParamInfo, nsACString& aLine)
{
  if (aIid != IID_IServiceProvider) {
    return false;
  }

  GUID** guid = reinterpret_cast<GUID**>(
                  static_cast<BYTE*>(aCallFrame->GetStackLocation()) +
                  aParamInfo.stackOffset);

  if (!IsValidGUID(**guid)) {
    return false;
  }

  WCHAR buf[39] = {0};
  if (!StringFromGUID2(**guid, buf, mozilla::ArrayLength(buf))) {
    return false;
  }

  aLine.AppendPrintf("%S", buf);
  return true;
}

void
Logger::LogEvent(ICallFrame* aCallFrame, IUnknown* aTargetInterface)
{
  // (1) Gather info about the call
  double elapsed = GetElapsedTime();

  CALLFRAMEINFO callInfo;
  HRESULT hr = aCallFrame->GetInfo(&callInfo);
  if (FAILED(hr)) {
    return;
  }

  PWSTR interfaceName = nullptr;
  PWSTR methodName = nullptr;
  hr = aCallFrame->GetNames(&interfaceName, &methodName);
  if (FAILED(hr)) {
    return;
  }

  // (2) Serialize the call
  nsPrintfCString line("%fus\t0x%p\t%S::%S\t(", elapsed,
                       aTargetInterface, interfaceName, methodName);

  CoTaskMemFree(interfaceName);
  interfaceName = nullptr;
  CoTaskMemFree(methodName);
  methodName = nullptr;

  // Check for supplemental array data
  const ArrayData* arrayData = FindArrayData(callInfo.iid, callInfo.iMethod);

  for (ULONG paramIndex = 0; paramIndex < callInfo.cParams; ++paramIndex) {
    CALLFRAMEPARAMINFO paramInfo;
    hr = aCallFrame->GetParamInfo(paramIndex, &paramInfo);
    if (SUCCEEDED(hr)) {
      line.AppendLiteral("[");
      if (paramInfo.fIn) {
        line.AppendLiteral("in");
      }
      if (paramInfo.fOut) {
        line.AppendLiteral("out");
      }
      line.AppendLiteral("] ");
    }
    VARIANT paramValue;
    hr = aCallFrame->GetParam(paramIndex, &paramValue);
    if (SUCCEEDED(hr)) {
      if (arrayData && paramIndex == arrayData->mArrayParamIndex) {
        VARIANT lengthParam;
        hr = aCallFrame->GetParam(arrayData->mLengthParamIndex, &lengthParam);
        if (SUCCEEDED(hr)) {
          line.AppendLiteral("{ ");
          for (LONG i = 0; i < *lengthParam.plVal; ++i) {
            VariantToString(paramValue, line, i);
            if (i < *lengthParam.plVal - 1) {
              line.AppendLiteral(", ");
            }
          }
          line.AppendLiteral(" }");
        } else {
          line.AppendPrintf("(GetParam failed with HRESULT 0x%08X)", hr);
        }
      } else {
        VariantToString(paramValue, line);
      }
    } else if (hr != DISP_E_BADVARTYPE ||
               !TryParamAsGuid(callInfo.iid, aCallFrame, paramInfo, line)) {
      line.AppendPrintf("(GetParam failed with HRESULT 0x%08X)", hr);
    }
    if (paramIndex < callInfo.cParams - 1) {
      line.AppendLiteral(", ");
    }
  }
  line.AppendLiteral(")\t");

  HRESULT callResult = aCallFrame->GetReturnValue();
  line.AppendPrintf("0x%08X\n", callResult);

  // (3) Enqueue event for logging
  MutexAutoLock lock(mMutex);
  mEntries.AppendElement(line);
  mThread->Dispatch(NewNonOwningRunnableMethod(this, &Logger::Flush),
                    NS_DISPATCH_NORMAL);
}

void
Logger::Flush()
{
  AssertRunningOnLoggerThread();
  MOZ_ASSERT(mLogFile);
  if (!mLogFile) {
    return;
  }
  nsTArray<nsCString> linesToWrite;
  { // Scope for lock
    MutexAutoLock lock(mMutex);
    linesToWrite.SwapElements(mEntries);
  }

  for (uint32_t i = 0, len = linesToWrite.Length(); i < len; ++i) {
    uint32_t bytesWritten;
    nsCString& line = linesToWrite[i];
    nsresult rv = mLogFile->Write(line.get(), line.Length(), &bytesWritten);
    NS_WARN_IF(NS_FAILED(rv));
  }
}

StaticAutoPtr<Logger> sLogger;

NS_IMETHODIMP
ShutdownEvent::Observe(nsISupports* aSubject, const char* aTopic,
                       const char16_t* aData)
{
  if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID)) {
    MOZ_ASSERT(false);
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  MOZ_ASSERT(sLogger);
  Unused << NS_WARN_IF(NS_FAILED(sLogger->Shutdown()));
  nsCOMPtr<nsIObserver> kungFuDeathGrip(this);
  nsCOMPtr<nsIObserverService> obsSvc = GetObserverService();
  obsSvc->RemoveObserver(this, aTopic);
  return NS_OK;
}
} // anonymous namespace


static bool
MaybeCreateLog(const char* aEnvVarName)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_IsContentProcess() || XRE_IsParentProcess());
  MOZ_ASSERT(!sLogger);
  const char* leafBaseName = PR_GetEnv(aEnvVarName);
  if (!leafBaseName) {
    return false;
  }
  nsDependentCString strLeafBaseName(leafBaseName);
  if (strLeafBaseName.IsEmpty()) {
    return false;
  }
  sLogger = new Logger(strLeafBaseName);
  if (!sLogger->IsValid()) {
    sLogger = nullptr;
    return false;
  }
  ClearOnShutdown(&sLogger);
  return true;
}

namespace mozilla {
namespace mscom {

/* static */ bool
InterceptorLog::Init()
{
  static const bool isEnabled = MaybeCreateLog("MOZ_MSCOM_LOG_BASENAME");
  return isEnabled;
}

/* static */ void
InterceptorLog::QI(HRESULT aResult, IUnknown* aTarget, REFIID aIid, IUnknown* aInterface)
{
  if (!sLogger) {
    return;
  }
  sLogger->LogQI(aResult, aTarget, aIid, aInterface);
}

/* static */ void
InterceptorLog::Event(ICallFrame* aCallFrame, IUnknown* aTargetInterface)
{
  if (!sLogger) {
    return;
  }
  sLogger->LogEvent(aCallFrame, aTargetInterface);
}

} // namespace mscom
} // namespace mozilla

