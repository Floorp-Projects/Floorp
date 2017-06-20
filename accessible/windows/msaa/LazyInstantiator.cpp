/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LazyInstantiator.h"

#include "MainThreadUtils.h"
#include "mozilla/a11y/Accessible.h"
#include "mozilla/Assertions.h"
#include "mozilla/mscom/MainThreadRuntime.h"
#include "mozilla/mscom/Ptr.h"
#include "mozilla/mscom/Registration.h"
#include "mozilla/UniquePtr.h"
#include "nsAccessibilityService.h"
#include "nsWindowsHelpers.h"
#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsXPCOM.h"
#include "RootAccessibleWrap.h"
#include "WinUtils.h"

#if defined(MOZ_TELEMETRY_REPORTING)
#include "mozilla/Telemetry.h"
#endif // defined(MOZ_TELEMETRY_REPORTING)

#include <oaidl.h>

#if !defined(STATE_SYSTEM_NORMAL)
#define STATE_SYSTEM_NORMAL (0)
#endif // !defined(STATE_SYSTEM_NORMAL)

/**
 * Because our wrapped accessible is cycle-collected, we can't safely AddRef()
 * or Release() ourselves off the main thread. This template specialization
 * forces NewRunnableMethod() to use STAUniquePtr instead of RefPtr for managing
 * a runnable's lifetime. Once the runnable has completed, the STAUniquePtr will
 * post a runnable to the main thread to release ourselves from there.
 */
template<>
struct nsRunnableMethodReceiver<mozilla::a11y::LazyInstantiator, true, false>
{
  mozilla::mscom::STAUniquePtr<mozilla::a11y::LazyInstantiator> mObj;
  explicit nsRunnableMethodReceiver(mozilla::a11y::LazyInstantiator* aObj)
    : mObj(aObj)
  {
    MOZ_ASSERT(NS_IsMainThread());
    // STAUniquePtr does not implicitly AddRef(), so we must explicitly do so
    // here.
    aObj->AddRef();
  }
  ~nsRunnableMethodReceiver() { Revoke(); }
  mozilla::a11y::LazyInstantiator* Get() const { return mObj.get(); }
  void Revoke() { mObj = nullptr; }
  void SetDeadline(mozilla::TimeStamp aDeadline) {}
};

namespace mozilla {
namespace a11y {

static const wchar_t kLazyInstantiatorProp[] = L"mozilla::a11y::LazyInstantiator";

/* static */
already_AddRefed<IAccessible>
LazyInstantiator::GetRootAccessible(HWND aHwnd)
{
  // There must only be one LazyInstantiator per HWND.
  // To track this, we set the kLazyInstantiatorProp on the HWND with a pointer
  // to an existing instance. We only create a new LazyInstatiator if that prop
  // has not already been set.
  LazyInstantiator* existingInstantiator =
    reinterpret_cast<LazyInstantiator*>(::GetProp(aHwnd, kLazyInstantiatorProp));

  RefPtr<IAccessible> result;
  if (existingInstantiator) {
    // Temporarily disable blind aggregation until we know that we have been
    // marshaled. See EnableBlindAggregation for more information.
    existingInstantiator->mAllowBlindAggregation = false;
    result = existingInstantiator;
    return result.forget();
  }

  // At this time we only want to check whether the acc service is running; We
  // don't actually want to create the acc service yet.
  if (!GetAccService()) {
    // a11y is not running yet, there are no existing LazyInstantiators for this
    // HWND, so create a new one and return it as a surrogate for the root
    // accessible.
    result = new LazyInstantiator(aHwnd);
    return result.forget();
  }

  // a11y is running, so we just resolve the real root accessible.
  a11y::Accessible* rootAcc = widget::WinUtils::GetRootAccessibleForHWND(aHwnd);
  if (!rootAcc || !rootAcc->IsRoot()) {
    return nullptr;
  }

  // Subtle: rootAcc might still be wrapped by a LazyInstantiator, but we
  // don't need LazyInstantiator's capabilities anymore (since a11y is already
  // running). We can bypass LazyInstantiator by retrieving the internal
  // unknown (which is not wrapped by the LazyInstantiator) and then querying
  // that for IID_IAccessible.
  a11y::RootAccessibleWrap* rootWrap =
    static_cast<a11y::RootAccessibleWrap*>(rootAcc);
  RefPtr<IUnknown> punk(rootWrap->GetInternalUnknown());

  MOZ_ASSERT(punk);
  if (!punk) {
    return nullptr;
  }

  punk->QueryInterface(IID_IAccessible, getter_AddRefs(result));
  return result.forget();
}

/**
 * When marshaling an interface, COM makes a whole bunch of QueryInterface
 * calls to determine what kind of marshaling the interface supports. We need
 * to handle those queries without instantiating a11y, so we temporarily
 * disable passing through of QueryInterface calls to a11y. Once we know that
 * COM is finished marshaling, we call EnableBlindAggregation to re-enable
 * QueryInterface passthrough.
 */
/* static */
void
LazyInstantiator::EnableBlindAggregation(HWND aHwnd)
{
  LazyInstantiator* existingInstantiator =
    reinterpret_cast<LazyInstantiator*>(::GetProp(aHwnd, kLazyInstantiatorProp));

  if (!existingInstantiator) {
    return;
  }

  existingInstantiator->mAllowBlindAggregation = true;
}

LazyInstantiator::LazyInstantiator(HWND aHwnd)
  : mHwnd(aHwnd)
  , mAllowBlindAggregation(false)
  , mWeakRootAccWrap(nullptr)
  , mWeakAccessible(nullptr)
  , mWeakDispatch(nullptr)
{
  MOZ_ASSERT(aHwnd);
  // Assign ourselves as the designated LazyInstantiator for aHwnd
  DebugOnly<BOOL> setPropOk = ::SetProp(aHwnd, kLazyInstantiatorProp,
                                        reinterpret_cast<HANDLE>(this));
  MOZ_ASSERT(setPropOk);
}

LazyInstantiator::~LazyInstantiator()
{
  if (mRealRootUnk) {
    // Disconnect ourselves from the root accessible.
    RefPtr<IUnknown> dummy(mWeakRootAccWrap->Aggregate(nullptr));
  }

  ClearProp();
}

void
LazyInstantiator::ClearProp()
{
  // Remove ourselves as the designated LazyInstantiator for mHwnd
  DebugOnly<HANDLE> removedProp = ::RemoveProp(mHwnd, kLazyInstantiatorProp);
  MOZ_ASSERT(!removedProp ||
             reinterpret_cast<LazyInstantiator*>(removedProp.value) == this);
}

/**
 * Given the remote client's thread ID, resolve its execuatable image name.
 */
bool
LazyInstantiator::GetClientExecutableName(const DWORD aClientTid,
                                          nsIFile** aOutClientExe)
{
  nsAutoHandle callingThread(::OpenThread(THREAD_QUERY_LIMITED_INFORMATION,
                                          FALSE, aClientTid));
  if (!callingThread) {
    return false;
  }

  DWORD callingPid = ::GetProcessIdOfThread(callingThread);

  nsAutoHandle callingProcess(::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                                            FALSE, callingPid));
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
 * Given a remote client's thread ID, determine whether we should proceed with
 * a11y instantiation. This is where telemetry should be gathered and any
 * potential blocking of unwanted a11y clients should occur.
 *
 * @return true if we should instantiate a11y
 */
bool
LazyInstantiator::ShouldInstantiate(const DWORD aClientTid)
{
  if (!aClientTid) {
    // aClientTid == 0 implies that this is either an in-process call, or else
    // we failed to retrieve information about the remote caller.
    // We should always default to instantiating a11y in this case.
    return true;
  }

  nsCOMPtr<nsIFile> clientExe;
  GetClientExecutableName(aClientTid, getter_AddRefs(clientExe));

  // Blocklist checks should go here. return false if we should not instantiate.
  /*
  if (ClientShouldBeBlocked(clientExe)) {
    return false;
  }
  */

#if defined(MOZ_TELEMETRY_REPORTING)
  if (!mTelemetryThread) {
    // Call GatherTelemetry on a background thread because it does I/O on
    // the executable file to retrieve version information.
    nsCOMPtr<nsIRunnable> runnable(
        NewRunnableMethod<nsCOMPtr<nsIFile>>(this,
                                             &LazyInstantiator::GatherTelemetry,
                                             clientExe));
    NS_NewThread(getter_AddRefs(mTelemetryThread), runnable);
  }
#endif // defined(MOZ_TELEMETRY_REPORTING)
  return true;
}

#if defined(MOZ_TELEMETRY_REPORTING)
/**
 * Appends version information in the format "|a.b.c.d".
 * If there is no version information, we append nothing.
 */
void
LazyInstantiator::AppendVersionInfo(nsIFile* aClientExe,
                                    nsAString& aStrToAppend)
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

void
LazyInstantiator::GatherTelemetry(nsIFile* aClientExe)
{
  MOZ_ASSERT(!NS_IsMainThread());

  nsAutoString value;
  nsresult rv = aClientExe->GetLeafName(value);
  if (NS_SUCCEEDED(rv)) {
    AppendVersionInfo(aClientExe, value);
  }

  // Now that we've (possibly) obtained version info, send the resulting
  // string back to the main thread to accumulate in telemetry.
  NS_DispatchToMainThread(NewNonOwningRunnableMethod<nsString>(this,
        &LazyInstantiator::AccumulateTelemetry, value));
}

void
LazyInstantiator::AccumulateTelemetry(const nsString& aValue)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!aValue.IsEmpty()) {
    Telemetry::ScalarSet(Telemetry::ScalarID::A11Y_INSTANTIATORS, aValue);
  }

  mTelemetryThread->Shutdown();
  mTelemetryThread = nullptr;
}
#endif // defined(MOZ_TELEMETRY_REPORTING)

RootAccessibleWrap*
LazyInstantiator::ResolveRootAccWrap()
{
  Accessible* acc = widget::WinUtils::GetRootAccessibleForHWND(mHwnd);
  if (!acc || !acc->IsRoot()) {
    return nullptr;
  }

  return static_cast<RootAccessibleWrap*>(acc);
}

/**
 * With COM aggregation, the aggregated inner object usually delegates its
 * reference counting to the outer object. In other words, we would expect
 * mRealRootUnk to delegate its AddRef() and Release() to this LazyInstantiator.
 *
 * This scheme will not work in our case because the RootAccessibleWrap is
 * cycle-collected!
 *
 * Instead, once a LazyInstantiator aggregates a RootAccessibleWrap, we transfer
 * our strong references into mRealRootUnk. Any future calls to AddRef or
 * Release now operate on mRealRootUnk instead of our intrinsic reference
 * count. This is a bit strange, but it is the only way for these objects to
 * share their reference count in a way that is safe for cycle collection.
 *
 * How do we know when it is safe to destroy ourselves? In
 * LazyInstantiator::Release, we examine the result of mRealRootUnk->Release().
 * If mRealRootUnk's resulting refcount is 1, then we know that the only
 * remaining reference to mRealRootUnk is the mRealRootUnk reference itself (and
 * thus nobody else holds references to either this or mRealRootUnk). Therefore
 * we may now delete ourselves.
 */
void
LazyInstantiator::TransplantRefCnt()
{
  MOZ_ASSERT(mRefCnt > 0);
  MOZ_ASSERT(mRealRootUnk);

  while (mRefCnt > 0) {
    mRealRootUnk.get()->AddRef();
    --mRefCnt;
  }
}

HRESULT
LazyInstantiator::MaybeResolveRoot()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mWeakAccessible) {
    return S_OK;
  }

  if (GetAccService() ||
      ShouldInstantiate(mscom::MainThreadRuntime::GetClientThreadId())) {
    mWeakRootAccWrap = ResolveRootAccWrap();
    if (!mWeakRootAccWrap) {
      return E_POINTER;
    }

    // Wrap ourselves around the root accessible wrap
    mRealRootUnk = mWeakRootAccWrap->Aggregate(static_cast<IAccessible*>(this));
    if (!mRealRootUnk) {
      return E_FAIL;
    }

    // Move our strong references into the root accessible (see the comments
    // above TransplantRefCnt for explanation).
    TransplantRefCnt();

    // Now obtain mWeakAccessible which we use to forward our incoming calls
    // to the real accesssible.
    HRESULT hr = mRealRootUnk->QueryInterface(IID_IAccessible,
                                              (void**) &mWeakAccessible);
    if (FAILED(hr)) {
      return hr;
    }

    // mWeakAccessible is weak, so don't hold a strong ref
    mWeakAccessible->Release();

    // Now that a11y is running, we don't need to remain registered with our
    // HWND anymore.
    ClearProp();

    return S_OK;
  }

  // If we don't want a real root, let's resolve a fake one.

  const WPARAM flags = 0xFFFFFFFFUL;
  // Synthesize a WM_GETOBJECT request to obtain a system-implemented
  // IAccessible object from DefWindowProc
  LRESULT lresult = ::DefWindowProc(mHwnd, WM_GETOBJECT, flags,
                                    static_cast<LPARAM>(OBJID_CLIENT));

  HRESULT hr = ObjectFromLresult(lresult, IID_IAccessible, flags,
                                 getter_AddRefs(mRealRootUnk));
  if (FAILED(hr)) {
    return hr;
  }

  if (!mRealRootUnk) {
    return E_NOTIMPL;
  }

  hr = mRealRootUnk->QueryInterface(IID_IAccessible,
                                    (void**) &mWeakAccessible);
  if (FAILED(hr)) {
    return hr;
  }

  // mWeakAccessible is weak, so don't hold a strong ref
  mWeakAccessible->Release();

  return S_OK;
}

#define RESOLVE_ROOT \
  { \
    HRESULT hr = MaybeResolveRoot(); \
    if (FAILED(hr)) { \
      return hr; \
    } \
  }

IMPL_IUNKNOWN_QUERY_HEAD(LazyInstantiator)
IMPL_IUNKNOWN_QUERY_IFACE_AMBIGIOUS(IUnknown, IAccessible)
IMPL_IUNKNOWN_QUERY_IFACE(IAccessible)
IMPL_IUNKNOWN_QUERY_IFACE(IDispatch)
IMPL_IUNKNOWN_QUERY_IFACE(IServiceProvider)
  // See EnableBlindAggregation for comments.
  if (!mAllowBlindAggregation) {
    return E_NOINTERFACE;
  }
// If the client queries for an interface that LazyInstantiator does not
// intrinsically support, then we must resolve the root accessible and pass
// on the QueryInterface call to mRealRootUnk.
RESOLVE_ROOT
IMPL_IUNKNOWN_QUERY_TAIL_AGGREGATED(mRealRootUnk)

ULONG
LazyInstantiator::AddRef()
{
  // Always delegate refcounting to mRealRootUnk when it exists
  if (mRealRootUnk) {
    return mRealRootUnk.get()->AddRef();
  }

  return ++mRefCnt;
}

ULONG
LazyInstantiator::Release()
{
  ULONG result;

  // Always delegate refcounting to mRealRootUnk when it exists
  if (mRealRootUnk) {
    result = mRealRootUnk.get()->Release();
    if (result == 1) {
      // mRealRootUnk is the only strong reference left, so nothing else holds
      // a strong reference to us. Drop result to zero so that we destroy
      // ourselves (See the comments above LazyInstantiator::TransplantRefCnt
      // for more info).
      --result;
    }
  } else {
    result = --mRefCnt;
  }

  if (!result) {
    delete this;
  }
  return result;
}

/**
 * Create a standard IDispatch implementation. mStdDispatch will translate any
 * IDispatch::Invoke calls into real IAccessible calls.
 */
HRESULT
LazyInstantiator::ResolveDispatch()
{
  if (mWeakDispatch) {
    return S_OK;
  }

  // The IAccessible typelib is embedded in oleacc.dll's resources.
  auto typelib = mscom::RegisterTypelib(L"oleacc.dll",
                                        mscom::RegistrationFlags::eUseSystemDirectory);
  if (!typelib) {
    return E_UNEXPECTED;
  }

  // Extract IAccessible's type info
  RefPtr<ITypeInfo> accTypeInfo;
  HRESULT hr = typelib->GetTypeInfoForGuid(IID_IAccessible,
                                           getter_AddRefs(accTypeInfo));
  if (FAILED(hr)) {
    return hr;
  }

  // Now create the standard IDispatch for IAccessible
  hr = ::CreateStdDispatch(static_cast<IAccessible*>(this),
                           static_cast<IAccessible*>(this),
                           accTypeInfo, getter_AddRefs(mStdDispatch));
  if (FAILED(hr)) {
    return hr;
  }

  hr = mStdDispatch->QueryInterface(IID_IDispatch, (void**)&mWeakDispatch);
  if (FAILED(hr)) {
    return hr;
  }

  // WEAK reference
  mWeakDispatch->Release();
  return S_OK;
}

#define RESOLVE_IDISPATCH \
  { \
    HRESULT hr = ResolveDispatch(); \
    if (FAILED(hr)) { \
      return hr; \
    } \
  }

/**
 * The remaining methods implement IDispatch, IAccessible, and IServiceProvider,
 * lazily resolving the real a11y objects and passing the call through.
 */

HRESULT
LazyInstantiator::GetTypeInfoCount(UINT* pctinfo)
{
  RESOLVE_IDISPATCH;
  return mWeakDispatch->GetTypeInfoCount(pctinfo);
}

HRESULT
LazyInstantiator::GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo **ppTInfo)
{
  RESOLVE_IDISPATCH;
  return mWeakDispatch->GetTypeInfo(iTInfo, lcid, ppTInfo);
}

HRESULT
LazyInstantiator::GetIDsOfNames(REFIID riid, LPOLESTR *rgszNames, UINT cNames,
                                LCID lcid, DISPID* rgDispId)
{
  RESOLVE_IDISPATCH;
  return mWeakDispatch->GetIDsOfNames(riid, rgszNames, cNames, lcid, rgDispId);
}

HRESULT
LazyInstantiator::Invoke(DISPID dispIdMember, REFIID riid, LCID lcid,
                         WORD wFlags, DISPPARAMS* pDispParams,
                         VARIANT* pVarResult, EXCEPINFO* pExcepInfo,
                         UINT* puArgErr)
{
  RESOLVE_IDISPATCH;
  return mWeakDispatch->Invoke(dispIdMember, riid, lcid, wFlags, pDispParams,
                               pVarResult, pExcepInfo, puArgErr);
}

HRESULT
LazyInstantiator::get_accParent(IDispatch **ppdispParent)
{
  RESOLVE_ROOT;
  return mWeakAccessible->get_accParent(ppdispParent);
}

HRESULT
LazyInstantiator::get_accChildCount(long *pcountChildren)
{
  if (!pcountChildren) {
    return E_INVALIDARG;
  }

  RESOLVE_ROOT;
  return mWeakAccessible->get_accChildCount(pcountChildren);
}

HRESULT
LazyInstantiator::get_accChild(VARIANT varChild, IDispatch **ppdispChild)
{
  if (!ppdispChild) {
    return E_INVALIDARG;
  }

  RESOLVE_ROOT;
  return mWeakAccessible->get_accChild(varChild, ppdispChild);
}

HRESULT
LazyInstantiator::get_accName(VARIANT varChild, BSTR *pszName)
{
  if (!pszName) {
    return E_INVALIDARG;
  }

  RESOLVE_ROOT;
  return mWeakAccessible->get_accName(varChild, pszName);
}

HRESULT
LazyInstantiator::get_accValue(VARIANT varChild, BSTR *pszValue)
{
  if (!pszValue) {
    return E_INVALIDARG;
  }

  RESOLVE_ROOT;
  return mWeakAccessible->get_accValue(varChild, pszValue);
}

HRESULT
LazyInstantiator::get_accDescription(VARIANT varChild, BSTR *pszDescription)
{
  if (!pszDescription) {
    return E_INVALIDARG;
  }

  RESOLVE_ROOT;
  return mWeakAccessible->get_accDescription(varChild, pszDescription);
}

HRESULT
LazyInstantiator::get_accRole(VARIANT varChild, VARIANT *pvarRole)
{
  if (!pvarRole) {
    return E_INVALIDARG;
  }

  RESOLVE_ROOT;
  return mWeakAccessible->get_accRole(varChild, pvarRole);
}

HRESULT
LazyInstantiator::get_accState(VARIANT varChild, VARIANT *pvarState)
{
  if (!pvarState) {
    return E_INVALIDARG;
  }

  RESOLVE_ROOT;
  return mWeakAccessible->get_accState(varChild, pvarState);
}

HRESULT
LazyInstantiator::get_accHelp(VARIANT varChild, BSTR *pszHelp)
{
  return E_NOTIMPL;
}

HRESULT
LazyInstantiator::get_accHelpTopic(BSTR *pszHelpFile, VARIANT varChild,
                                   long *pidTopic)
{
  return E_NOTIMPL;
}

HRESULT
LazyInstantiator::get_accKeyboardShortcut(VARIANT varChild,
                                          BSTR *pszKeyboardShortcut)
{
  if (!pszKeyboardShortcut) {
    return E_INVALIDARG;
  }

  RESOLVE_ROOT;
  return mWeakAccessible->get_accKeyboardShortcut(varChild, pszKeyboardShortcut);
}

HRESULT
LazyInstantiator::get_accFocus(VARIANT *pvarChild)
{
  if (!pvarChild) {
    return E_INVALIDARG;
  }

  RESOLVE_ROOT;
  return mWeakAccessible->get_accFocus(pvarChild);
}

HRESULT
LazyInstantiator::get_accSelection(VARIANT *pvarChildren)
{
  if (!pvarChildren) {
    return E_INVALIDARG;
  }

  RESOLVE_ROOT;
  return mWeakAccessible->get_accSelection(pvarChildren);
}

HRESULT
LazyInstantiator::get_accDefaultAction(VARIANT varChild, BSTR *pszDefaultAction)
{
  if (!pszDefaultAction) {
    return E_INVALIDARG;
  }

  RESOLVE_ROOT;
  return mWeakAccessible->get_accDefaultAction(varChild, pszDefaultAction);
}

HRESULT
LazyInstantiator::accSelect(long flagsSelect, VARIANT varChild)
{
  RESOLVE_ROOT;
  return mWeakAccessible->accSelect(flagsSelect, varChild);
}

HRESULT
LazyInstantiator::accLocation(long *pxLeft, long *pyTop, long *pcxWidth,
                              long *pcyHeight, VARIANT varChild)
{
  RESOLVE_ROOT;
  return mWeakAccessible->accLocation(pxLeft, pyTop, pcxWidth, pcyHeight, varChild);
}

HRESULT
LazyInstantiator::accNavigate(long navDir, VARIANT varStart,
                              VARIANT *pvarEndUpAt)
{
  return E_NOTIMPL;
}

HRESULT
LazyInstantiator::accHitTest(long xLeft, long yTop, VARIANT *pvarChild)
{
  if (!pvarChild) {
    return E_INVALIDARG;
  }

  RESOLVE_ROOT;
  return mWeakAccessible->accHitTest(xLeft, yTop, pvarChild);
}

HRESULT
LazyInstantiator::accDoDefaultAction(VARIANT varChild)
{
  RESOLVE_ROOT;
  return mWeakAccessible->accDoDefaultAction(varChild);
}

HRESULT
LazyInstantiator::put_accName(VARIANT varChild, BSTR szName)
{
  return E_NOTIMPL;
}

HRESULT
LazyInstantiator::put_accValue(VARIANT varChild, BSTR szValue)
{
  return E_NOTIMPL;
}

HRESULT
LazyInstantiator::QueryService(REFGUID aServiceId, REFIID aServiceIid,
                               void** aOutInterface)
{
  if (!aOutInterface) {
    return E_INVALIDARG;
  }

  *aOutInterface = nullptr;

  RESOLVE_ROOT;

  RefPtr<IServiceProvider> servProv;
  HRESULT hr = mRealRootUnk->QueryInterface(IID_IServiceProvider,
                                            getter_AddRefs(servProv));
  if (FAILED(hr)) {
    return hr;
  }

  return servProv->QueryService(aServiceId, aServiceIid, aOutInterface);
}

} // namespace a11y
} // namespace mozilla

