/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LazyInstantiator.h"

#include "MainThreadUtils.h"
#include "mozilla/a11y/LocalAccessible.h"
#include "mozilla/a11y/Compatibility.h"
#include "mozilla/a11y/Platform.h"
#include "mozilla/Assertions.h"
#include "mozilla/mscom/ProcessRuntime.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WinHeaderOnlyUtils.h"
#include "MsaaRootAccessible.h"
#include "nsAccessibilityService.h"
#include "nsWindowsHelpers.h"
#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsXPCOM.h"
#include "WinUtils.h"
#include "prenv.h"

#include <oaidl.h>

#if !defined(STATE_SYSTEM_NORMAL)
#  define STATE_SYSTEM_NORMAL (0)
#endif  // !defined(STATE_SYSTEM_NORMAL)

#define DLL_BLOCKLIST_ENTRY(name, ...) {L##name, __VA_ARGS__},
#define DLL_BLOCKLIST_STRING_TYPE const wchar_t*
#include "mozilla/WindowsDllBlocklistA11yDefs.h"

namespace mozilla {
namespace a11y {

static const wchar_t kLazyInstantiatorProp[] =
    L"mozilla::a11y::LazyInstantiator";

Maybe<bool> LazyInstantiator::sShouldBlockUia;

/* static */
already_AddRefed<IAccessible> LazyInstantiator::GetRootAccessible(HWND aHwnd) {
  RefPtr<IAccessible> result;
  // At this time we only want to check whether the acc service is running. We
  // don't actually want to create the acc service yet.
  if (!GetAccService()) {
    // There must only be one LazyInstantiator per HWND.
    // To track this, we set the kLazyInstantiatorProp on the HWND with a
    // pointer to an existing instance. We only create a new LazyInstatiator if
    // that prop has not already been set.
    LazyInstantiator* existingInstantiator =
        reinterpret_cast<LazyInstantiator*>(
            ::GetProp(aHwnd, kLazyInstantiatorProp));

    if (existingInstantiator) {
      // Temporarily disable blind aggregation until we know that we have been
      // marshaled. See EnableBlindAggregation for more information.
      existingInstantiator->mAllowBlindAggregation = false;
      result = existingInstantiator;
      return result.forget();
    }

    // a11y is not running yet, there are no existing LazyInstantiators for this
    // HWND, so create a new one and return it as a surrogate for the root
    // accessible.
    result = new LazyInstantiator(aHwnd);
    return result.forget();
  }

  // a11y is running, so we just resolve the real root accessible.
  a11y::LocalAccessible* rootAcc =
      widget::WinUtils::GetRootAccessibleForHWND(aHwnd);
  if (!rootAcc) {
    return nullptr;
  }

  if (!rootAcc->IsRoot()) {
    // rootAcc might represent a popup as opposed to a true root accessible.
    // In that case we just use the regular LocalAccessible::GetNativeInterface.
    rootAcc->GetNativeInterface(getter_AddRefs(result));
    return result.forget();
  }

  auto msaaRoot =
      static_cast<MsaaRootAccessible*>(MsaaAccessible::GetFrom(rootAcc));
  // Subtle: msaaRoot might still be wrapped by a LazyInstantiator, but we
  // don't need LazyInstantiator's capabilities anymore (since a11y is already
  // running). We can bypass LazyInstantiator by retrieving the internal
  // unknown (which is not wrapped by the LazyInstantiator) and then querying
  // that for IID_IAccessible.
  RefPtr<IUnknown> punk(msaaRoot->GetInternalUnknown());

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
void LazyInstantiator::EnableBlindAggregation(HWND aHwnd) {
  if (GetAccService()) {
    // The accessibility service is already running. That means that
    // LazyInstantiator::GetRootAccessible returned the real MsaaRootAccessible,
    // rather than returning a LazyInstantiator with blind aggregation disabled.
    // Thus, we have nothing to do here.
    return;
  }

  LazyInstantiator* existingInstantiator = reinterpret_cast<LazyInstantiator*>(
      ::GetProp(aHwnd, kLazyInstantiatorProp));

  if (!existingInstantiator) {
    return;
  }

  existingInstantiator->mAllowBlindAggregation = true;
}

LazyInstantiator::LazyInstantiator(HWND aHwnd)
    : mHwnd(aHwnd),
      mAllowBlindAggregation(false),
      mWeakMsaaRoot(nullptr),
      mWeakAccessible(nullptr),
      mWeakDispatch(nullptr) {
  MOZ_ASSERT(aHwnd);
  // Assign ourselves as the designated LazyInstantiator for aHwnd
  DebugOnly<BOOL> setPropOk =
      ::SetProp(aHwnd, kLazyInstantiatorProp, reinterpret_cast<HANDLE>(this));
  MOZ_ASSERT(setPropOk);
}

LazyInstantiator::~LazyInstantiator() {
  if (mRealRootUnk) {
    // Disconnect ourselves from the root accessible.
    RefPtr<IUnknown> dummy(mWeakMsaaRoot->Aggregate(nullptr));
  }

  ClearProp();
}

void LazyInstantiator::ClearProp() {
  // Remove ourselves as the designated LazyInstantiator for mHwnd
  DebugOnly<HANDLE> removedProp = ::RemoveProp(mHwnd, kLazyInstantiatorProp);
  MOZ_ASSERT(!removedProp ||
             reinterpret_cast<LazyInstantiator*>(removedProp.value) == this);
}

/**
 * Get the process id of a remote (out-of-process) MSAA/IA2 client.
 */
DWORD LazyInstantiator::GetRemoteMsaaClientPid() {
  nsAutoHandle callingThread(
      ::OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE,
                   mscom::ProcessRuntime::GetClientThreadId()));
  if (!callingThread) {
    return 0;
  }
  return ::GetProcessIdOfThread(callingThread);
}

/**
 * This is the blocklist for known "bad" remote clients that instantiate a11y.
 */
static const char* gBlockedRemoteClients[] = {
    "tbnotifier.exe",  // Ask.com Toolbar, bug 1453876
    "flow.exe",        // Conexant Flow causes performance issues, bug 1569712
    "rtop_bg.exe",     // ByteFence Anti-Malware, bug 1713383
    "osk.exe",         // Windows On-Screen Keyboard, bug 1424505
};

/**
 * Check for the presence of any known "bad" injected DLLs that may be trying
 * to instantiate a11y.
 *
 * @return true to block a11y instantiation, otherwise false to continue
 */
bool LazyInstantiator::IsBlockedInjection() {
  // Check debugging options see if we should disable the blocklist.
  if (PR_GetEnv("MOZ_DISABLE_ACCESSIBLE_BLOCKLIST")) {
    return false;
  }

  for (size_t index = 0, len = ArrayLength(gBlockedInprocDlls); index < len;
       ++index) {
    const DllBlockInfo& blockedDll = gBlockedInprocDlls[index];
    HMODULE module = ::GetModuleHandleW(blockedDll.mName);
    if (!module) {
      // This dll isn't loaded.
      continue;
    }

    LauncherResult<ModuleVersion> version = GetModuleVersion(module);
    return version.isOk() && blockedDll.IsVersionBlocked(version.unwrap());
  }

  return false;
}

/**
 * Given a remote client's process ID, determine whether we should proceed with
 * a11y instantiation. This is where telemetry should be gathered and any
 * potential blocking of unwanted a11y clients should occur.
 *
 * @return true if we should instantiate a11y
 */
bool LazyInstantiator::ShouldInstantiate(const DWORD aClientPid) {
  a11y::SetInstantiator(aClientPid);

  nsCOMPtr<nsIFile> clientExe;
  if (!a11y::GetInstantiator(getter_AddRefs(clientExe))) {
    return true;
  }

  nsresult rv;
  if (!PR_GetEnv("MOZ_DISABLE_ACCESSIBLE_BLOCKLIST")) {
    // Debugging option is not present, so check blocklist.
    nsAutoString leafName;
    rv = clientExe->GetLeafName(leafName);
    if (NS_SUCCEEDED(rv)) {
      for (size_t i = 0, len = ArrayLength(gBlockedRemoteClients); i < len;
           ++i) {
        if (leafName.EqualsIgnoreCase(gBlockedRemoteClients[i])) {
          // If client exe is in our blocklist, do not instantiate.
          return false;
        }
      }
    }
  }

  return true;
}

/**
 * Determine whether we should proceed with a11y instantiation, considering the
 * various different types of clients.
 */
bool LazyInstantiator::ShouldInstantiate() {
  if (Compatibility::IsA11ySuppressedForClipboardCopy()) {
    // Bug 1774285: Windows Suggested Actions (introduced in Windows 11 22H2)
    // walks the entire a11y tree using UIA whenever anything is copied to the
    // clipboard. This causes an unacceptable hang, particularly when the cache
    // is disabled. Don't allow a11y to be instantiated by this.
    return false;
  }
  if (DWORD pid = GetRemoteMsaaClientPid()) {
    return ShouldInstantiate(pid);
  }
  if (Compatibility::HasKnownNonUiaConsumer()) {
    // We detected a known in-process client.
    return true;
  }
  // UIA client detection can be expensive, so we cache the result. See the
  // header comment for ResetUiaDetectionCache() for details.
  if (sShouldBlockUia.isNothing()) {
    // Unlike MSAA, we can't tell which specific UIA client is querying us right
    // now. We can only determine which clients have tried querying us.
    // Therefore, we must check all of them.
    AutoTArray<DWORD, 1> uiaPids;
    Compatibility::GetUiaClientPids(uiaPids);
    if (uiaPids.IsEmpty()) {
      // No UIA clients, so don't block UIA. However, we might block for
      // non-UIA clients below.
      sShouldBlockUia = Some(false);
    } else {
      for (const DWORD pid : uiaPids) {
        if (ShouldInstantiate(pid)) {
          sShouldBlockUia = Some(false);
          return true;
        }
      }
      // We didn't return in the loop above, so there are only blocked UIA
      // clients.
      sShouldBlockUia = Some(true);
    }
  }
  if (*sShouldBlockUia) {
    return false;
  }
  if (IsBlockedInjection()) {
    return false;
  }
  return true;
}

MsaaRootAccessible* LazyInstantiator::ResolveMsaaRoot() {
  LocalAccessible* acc = widget::WinUtils::GetRootAccessibleForHWND(mHwnd);
  if (!acc || !acc->IsRoot()) {
    return nullptr;
  }

  RefPtr<IAccessible> ia;
  acc->GetNativeInterface(getter_AddRefs(ia));
  return static_cast<MsaaRootAccessible*>(ia.get());
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
void LazyInstantiator::TransplantRefCnt() {
  MOZ_ASSERT(mRefCnt > 0);
  MOZ_ASSERT(mRealRootUnk);

  while (mRefCnt > 0) {
    mRealRootUnk.get()->AddRef();
    --mRefCnt;
  }
}

HRESULT
LazyInstantiator::MaybeResolveRoot() {
  if (!NS_IsMainThread()) {
    MOZ_ASSERT_UNREACHABLE("Called on a background thread!");
    // Bug 1814780: This should never happen, since a caller should only be able
    // to get this via AccessibleObjectFromWindow/AccessibleObjectFromEvent or
    // WM_GETOBJECT/ObjectFromLresult, which should marshal any calls on
    // a background thread to the main thread. Nevertheless, Windows sometimes
    // calls QueryInterface from a background thread! To avoid crashes, fail
    // gracefully here.
    return RPC_E_WRONG_THREAD;
  }

  if (mWeakAccessible) {
    return S_OK;
  }

  if (GetAccService() || ShouldInstantiate()) {
    mWeakMsaaRoot = ResolveMsaaRoot();
    if (!mWeakMsaaRoot) {
      return E_POINTER;
    }

    // Wrap ourselves around the root accessible wrap
    mRealRootUnk = mWeakMsaaRoot->Aggregate(static_cast<IAccessible*>(this));
    if (!mRealRootUnk) {
      return E_FAIL;
    }

    // Move our strong references into the root accessible (see the comments
    // above TransplantRefCnt for explanation).
    TransplantRefCnt();

    // Now obtain mWeakAccessible which we use to forward our incoming calls
    // to the real accesssible.
    HRESULT hr =
        mRealRootUnk->QueryInterface(IID_IAccessible, (void**)&mWeakAccessible);
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

  return E_FAIL;
}

#define RESOLVE_ROOT                 \
  {                                  \
    HRESULT hr = MaybeResolveRoot(); \
    if (FAILED(hr)) {                \
      return hr;                     \
    }                                \
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

if (aIID == IID_IAccIdentity) {
  return E_NOINTERFACE;
}
// If the client queries for an interface that LazyInstantiator does not
// intrinsically support, then we must resolve the root accessible and pass
// on the QueryInterface call to mRealRootUnk.
RESOLVE_ROOT
IMPL_IUNKNOWN_QUERY_TAIL_AGGREGATED(mRealRootUnk)

ULONG
LazyInstantiator::AddRef() {
  // Always delegate refcounting to mRealRootUnk when it exists
  if (mRealRootUnk) {
    return mRealRootUnk.get()->AddRef();
  }

  return ++mRefCnt;
}

ULONG
LazyInstantiator::Release() {
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
LazyInstantiator::ResolveDispatch() {
  if (mWeakDispatch) {
    return S_OK;
  }

  // Extract IAccessible's type info
  RefPtr<ITypeInfo> accTypeInfo = MsaaAccessible::GetTI(LOCALE_USER_DEFAULT);
  if (!accTypeInfo) {
    return E_UNEXPECTED;
  }

  // Now create the standard IDispatch for IAccessible
  HRESULT hr = ::CreateStdDispatch(static_cast<IAccessible*>(this),
                                   static_cast<IAccessible*>(this), accTypeInfo,
                                   getter_AddRefs(mStdDispatch));
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

#define RESOLVE_IDISPATCH           \
  {                                 \
    HRESULT hr = ResolveDispatch(); \
    if (FAILED(hr)) {               \
      return hr;                    \
    }                               \
  }

/**
 * The remaining methods implement IDispatch, IAccessible, and IServiceProvider,
 * lazily resolving the real a11y objects and passing the call through.
 */

HRESULT
LazyInstantiator::GetTypeInfoCount(UINT* pctinfo) {
  RESOLVE_IDISPATCH;
  return mWeakDispatch->GetTypeInfoCount(pctinfo);
}

HRESULT
LazyInstantiator::GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo** ppTInfo) {
  RESOLVE_IDISPATCH;
  return mWeakDispatch->GetTypeInfo(iTInfo, lcid, ppTInfo);
}

HRESULT
LazyInstantiator::GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, UINT cNames,
                                LCID lcid, DISPID* rgDispId) {
  RESOLVE_IDISPATCH;
  return mWeakDispatch->GetIDsOfNames(riid, rgszNames, cNames, lcid, rgDispId);
}

HRESULT
LazyInstantiator::Invoke(DISPID dispIdMember, REFIID riid, LCID lcid,
                         WORD wFlags, DISPPARAMS* pDispParams,
                         VARIANT* pVarResult, EXCEPINFO* pExcepInfo,
                         UINT* puArgErr) {
  RESOLVE_IDISPATCH;
  return mWeakDispatch->Invoke(dispIdMember, riid, lcid, wFlags, pDispParams,
                               pVarResult, pExcepInfo, puArgErr);
}

HRESULT
LazyInstantiator::get_accParent(IDispatch** ppdispParent) {
  if (!mWeakAccessible) {
    // If we'd resolve the root right now this would be the codepath we'd end
    // up in anyway. So we might as well return it here.
    return ::CreateStdAccessibleObject(mHwnd, OBJID_WINDOW, IID_IAccessible,
                                       (void**)ppdispParent);
  }
  RESOLVE_ROOT;
  return mWeakAccessible->get_accParent(ppdispParent);
}

HRESULT
LazyInstantiator::get_accChildCount(long* pcountChildren) {
  if (!pcountChildren) {
    return E_INVALIDARG;
  }

  RESOLVE_ROOT;
  return mWeakAccessible->get_accChildCount(pcountChildren);
}

HRESULT
LazyInstantiator::get_accChild(VARIANT varChild, IDispatch** ppdispChild) {
  if (!ppdispChild) {
    return E_INVALIDARG;
  }

  if (V_VT(&varChild) == VT_I4 && V_I4(&varChild) == CHILDID_SELF) {
    RefPtr<IDispatch> disp(this);
    disp.forget(ppdispChild);
    return S_OK;
  }

  RESOLVE_ROOT;
  return mWeakAccessible->get_accChild(varChild, ppdispChild);
}

HRESULT
LazyInstantiator::get_accName(VARIANT varChild, BSTR* pszName) {
  if (!pszName) {
    return E_INVALIDARG;
  }

  RESOLVE_ROOT;
  return mWeakAccessible->get_accName(varChild, pszName);
}

HRESULT
LazyInstantiator::get_accValue(VARIANT varChild, BSTR* pszValue) {
  if (!pszValue) {
    return E_INVALIDARG;
  }

  RESOLVE_ROOT;
  return mWeakAccessible->get_accValue(varChild, pszValue);
}

HRESULT
LazyInstantiator::get_accDescription(VARIANT varChild, BSTR* pszDescription) {
  if (!pszDescription) {
    return E_INVALIDARG;
  }

  RESOLVE_ROOT;
  return mWeakAccessible->get_accDescription(varChild, pszDescription);
}

HRESULT
LazyInstantiator::get_accRole(VARIANT varChild, VARIANT* pvarRole) {
  if (!pvarRole) {
    return E_INVALIDARG;
  }

  if (V_VT(&varChild) == VT_I4 && V_I4(&varChild) == CHILDID_SELF) {
    V_VT(pvarRole) = VT_I4;
    V_I4(pvarRole) = ROLE_SYSTEM_APPLICATION;
    return S_OK;
  }

  RESOLVE_ROOT;
  return mWeakAccessible->get_accRole(varChild, pvarRole);
}

HRESULT
LazyInstantiator::get_accState(VARIANT varChild, VARIANT* pvarState) {
  if (!pvarState) {
    return E_INVALIDARG;
  }

  RESOLVE_ROOT;
  return mWeakAccessible->get_accState(varChild, pvarState);
}

HRESULT
LazyInstantiator::get_accHelp(VARIANT varChild, BSTR* pszHelp) {
  return E_NOTIMPL;
}

HRESULT
LazyInstantiator::get_accHelpTopic(BSTR* pszHelpFile, VARIANT varChild,
                                   long* pidTopic) {
  return E_NOTIMPL;
}

HRESULT
LazyInstantiator::get_accKeyboardShortcut(VARIANT varChild,
                                          BSTR* pszKeyboardShortcut) {
  if (!pszKeyboardShortcut) {
    return E_INVALIDARG;
  }

  RESOLVE_ROOT;
  return mWeakAccessible->get_accKeyboardShortcut(varChild,
                                                  pszKeyboardShortcut);
}

HRESULT
LazyInstantiator::get_accFocus(VARIANT* pvarChild) {
  if (!pvarChild) {
    return E_INVALIDARG;
  }

  RESOLVE_ROOT;
  return mWeakAccessible->get_accFocus(pvarChild);
}

HRESULT
LazyInstantiator::get_accSelection(VARIANT* pvarChildren) {
  if (!pvarChildren) {
    return E_INVALIDARG;
  }

  RESOLVE_ROOT;
  return mWeakAccessible->get_accSelection(pvarChildren);
}

HRESULT
LazyInstantiator::get_accDefaultAction(VARIANT varChild,
                                       BSTR* pszDefaultAction) {
  if (!pszDefaultAction) {
    return E_INVALIDARG;
  }

  RESOLVE_ROOT;
  return mWeakAccessible->get_accDefaultAction(varChild, pszDefaultAction);
}

HRESULT
LazyInstantiator::accSelect(long flagsSelect, VARIANT varChild) {
  RESOLVE_ROOT;
  return mWeakAccessible->accSelect(flagsSelect, varChild);
}

HRESULT
LazyInstantiator::accLocation(long* pxLeft, long* pyTop, long* pcxWidth,
                              long* pcyHeight, VARIANT varChild) {
  RESOLVE_ROOT;
  return mWeakAccessible->accLocation(pxLeft, pyTop, pcxWidth, pcyHeight,
                                      varChild);
}

HRESULT
LazyInstantiator::accNavigate(long navDir, VARIANT varStart,
                              VARIANT* pvarEndUpAt) {
  if (!pvarEndUpAt) {
    return E_INVALIDARG;
  }

  RESOLVE_ROOT;
  return mWeakAccessible->accNavigate(navDir, varStart, pvarEndUpAt);
}

HRESULT
LazyInstantiator::accHitTest(long xLeft, long yTop, VARIANT* pvarChild) {
  if (!pvarChild) {
    return E_INVALIDARG;
  }

  RESOLVE_ROOT;
  return mWeakAccessible->accHitTest(xLeft, yTop, pvarChild);
}

HRESULT
LazyInstantiator::accDoDefaultAction(VARIANT varChild) {
  RESOLVE_ROOT;
  return mWeakAccessible->accDoDefaultAction(varChild);
}

HRESULT
LazyInstantiator::put_accName(VARIANT varChild, BSTR szName) {
  return E_NOTIMPL;
}

HRESULT
LazyInstantiator::put_accValue(VARIANT varChild, BSTR szValue) {
  return E_NOTIMPL;
}

static const GUID kUnsupportedServices[] = {
    // clang-format off
  // Unknown, queried by Windows on devices with touch screens or similar devices
  // connected.
  {0x33f139ee, 0xe509, 0x47f7, {0xbf, 0x39, 0x83, 0x76, 0x44, 0xf7, 0x45, 0x76}},
  // Unknown, queried by Windows
  {0xFDA075CF, 0x7C8B, 0x498C, { 0xB5, 0x14, 0xA9, 0xCB, 0x52, 0x1B, 0xBF, 0xB4 }},
  // Unknown, queried by Windows
  {0x8EDAA462, 0x21F4, 0x4C87, { 0xA0, 0x12, 0xB3, 0xCD, 0xA3, 0xAB, 0x01, 0xFC }},
  // Unknown, queried by Windows
  {0xacd46652, 0x829d, 0x41cb, { 0xa5, 0xfc, 0x17, 0xac, 0xf4, 0x36, 0x61, 0xac }},
  // SID_IsUIAutomationObject (undocumented), queried by Windows
  {0xb96fdb85, 0x7204, 0x4724, { 0x84, 0x2b, 0xc7, 0x05, 0x9d, 0xed, 0xb9, 0xd0 }},
  // IIS_IsOleaccProxy (undocumented), queried by Windows
  {0x902697FA, 0x80E4, 0x4560, {0x80, 0x2A, 0xA1, 0x3F, 0x22, 0xA6, 0x47, 0x09}},
  // IID_IHTMLElement, queried by JAWS
  {0x3050F1FF, 0x98B5, 0x11CF, {0xBB, 0x82, 0x00, 0xAA, 0x00, 0xBD, 0xCE, 0x0B}}
    // clang-format on
};

HRESULT
LazyInstantiator::QueryService(REFGUID aServiceId, REFIID aServiceIid,
                               void** aOutInterface) {
  if (!aOutInterface) {
    return E_INVALIDARG;
  }

  for (const GUID& unsupportedService : kUnsupportedServices) {
    if (aServiceId == unsupportedService) {
      return E_NOINTERFACE;
    }
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

}  // namespace a11y
}  // namespace mozilla
