/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessibleWrap.h"
#include "LocalAccessible-inl.h"

#include "Compatibility.h"
#include "DocAccessible-inl.h"
#include "mozilla/a11y/DocAccessibleParent.h"
#include "EnumVariant.h"
#include "GeckoCustom.h"
#include "nsAccUtils.h"
#include "nsCoreUtils.h"
#include "nsIAccessibleEvent.h"
#include "nsWindowsHelpers.h"
#include "nsWinUtils.h"
#include "mozilla/a11y/RemoteAccessible.h"
#include "ProxyWrappers.h"
#include "ServiceProvider.h"
#include "Relation.h"
#include "Role.h"
#include "RootAccessible.h"
#include "sdnAccessible.h"
#include "States.h"

#ifdef A11Y_LOG
#  include "Logging.h"
#endif

#include "nsIFrame.h"
#include "nsIScrollableFrame.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/NodeInfo.h"
#include "mozilla/dom/BrowserParent.h"
#include "nsNameSpaceManager.h"
#include "nsTextFormatter.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsArrayUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/ReverseIterator.h"
#include "mozilla/mscom/AsyncInvoker.h"

#include "oleacc.h"

using namespace mozilla;
using namespace mozilla::a11y;

const uint32_t USE_ROLE_STRING = 0;

/* For documentation of the accessibility architecture,
 * see http://lxr.mozilla.org/seamonkey/source/accessible/accessible-docs.html
 */

//#define DEBUG_LEAKS

#ifdef DEBUG_LEAKS
static gAccessibles = 0;
#endif

StaticAutoPtr<nsTArray<AccessibleWrap::HandlerControllerData>>
    AccessibleWrap::sHandlerControllers;

static const VARIANT kVarChildIdSelf = {{{VT_I4}}};

static const int32_t kIEnumVariantDisconnected = -1;

////////////////////////////////////////////////////////////////////////////////
// AccessibleWrap
////////////////////////////////////////////////////////////////////////////////
AccessibleWrap::AccessibleWrap(nsIContent* aContent, DocAccessible* aDoc)
    : LocalAccessible(aContent, aDoc) {}

ITypeInfo* AccessibleWrap::gTypeInfo = nullptr;

NS_IMPL_ISUPPORTS_INHERITED0(AccessibleWrap, LocalAccessible)

void AccessibleWrap::Shutdown() {
  MsaaShutdown();
  LocalAccessible::Shutdown();
}

//-----------------------------------------------------
// IUnknown interface methods - see iunknown.h for documentation
//-----------------------------------------------------

// Microsoft COM QueryInterface
STDMETHODIMP
AccessibleWrap::QueryInterface(REFIID iid, void** ppv) {
  if (!ppv) return E_INVALIDARG;

  *ppv = nullptr;

  if (IID_IClientSecurity == iid) {
    // Some code might QI(IID_IClientSecurity) to detect whether or not we are
    // a proxy. Right now that can potentially happen off the main thread, so we
    // look for this condition immediately so that we don't trigger other code
    // that might not be thread-safe.
    return E_NOINTERFACE;
  }

  if (IID_IUnknown == iid)
    *ppv = static_cast<IAccessible*>(this);
  else if (IID_IDispatch == iid || IID_IAccessible == iid)
    *ppv = static_cast<IAccessible*>(this);
  else if (IID_IEnumVARIANT == iid && !IsProxy()) {
    // Don't support this interface for leaf elements.
    if (!HasChildren() || nsAccUtils::MustPrune(this)) return E_NOINTERFACE;

    *ppv = static_cast<IEnumVARIANT*>(new ChildrenEnumVariant(this));
  } else if (IID_IServiceProvider == iid)
    *ppv = new ServiceProvider(this);
  else if (IID_ISimpleDOMNode == iid && !IsProxy()) {
    if (IsDefunct() || (!HasOwnContent() && !IsDoc())) return E_NOINTERFACE;

    *ppv = static_cast<ISimpleDOMNode*>(new sdnAccessible(WrapNotNull(this)));
  }

  if (nullptr == *ppv) {
    HRESULT hr = ia2Accessible::QueryInterface(iid, ppv);
    if (SUCCEEDED(hr)) return hr;
  }

  if (nullptr == *ppv && !IsProxy()) {
    HRESULT hr = ia2AccessibleComponent::QueryInterface(iid, ppv);
    if (SUCCEEDED(hr)) return hr;
  }

  if (nullptr == *ppv) {
    HRESULT hr = ia2AccessibleHyperlink::QueryInterface(iid, ppv);
    if (SUCCEEDED(hr)) return hr;
  }

  if (nullptr == *ppv && !IsProxy()) {
    HRESULT hr = ia2AccessibleValue::QueryInterface(iid, ppv);
    if (SUCCEEDED(hr)) return hr;
  }

  if (!*ppv && iid == IID_IGeckoCustom) {
    RefPtr<GeckoCustom> gkCrap = new GeckoCustom(this);
    gkCrap.forget(ppv);
    return S_OK;
  }

  if (nullptr == *ppv) return E_NOINTERFACE;

  (reinterpret_cast<IUnknown*>(*ppv))->AddRef();
  return S_OK;
}

//-----------------------------------------------------
// IAccessible methods
//-----------------------------------------------------

STDMETHODIMP
AccessibleWrap::get_accParent(IDispatch __RPC_FAR* __RPC_FAR* ppdispParent) {
  if (!ppdispParent) return E_INVALIDARG;

  *ppdispParent = nullptr;

  if (IsDefunct()) return CO_E_OBJNOTCONNECTED;

  LocalAccessible* xpParentAcc = LocalParent();
  if (!xpParentAcc) return S_FALSE;

  *ppdispParent = NativeAccessible(xpParentAcc);
  return S_OK;
}

STDMETHODIMP
AccessibleWrap::get_accChildCount(long __RPC_FAR* pcountChildren) {
  if (!pcountChildren) return E_INVALIDARG;

  *pcountChildren = 0;

  if (IsDefunct()) return CO_E_OBJNOTCONNECTED;

  if (nsAccUtils::MustPrune(this)) return S_OK;

  *pcountChildren = ChildCount();
  return S_OK;
}

STDMETHODIMP
AccessibleWrap::get_accChild(
    /* [in] */ VARIANT varChild,
    /* [retval][out] */ IDispatch __RPC_FAR* __RPC_FAR* ppdispChild) {
  if (!ppdispChild) return E_INVALIDARG;

  *ppdispChild = nullptr;
  if (IsDefunct()) return CO_E_OBJNOTCONNECTED;

  // IAccessible::accChild is used to return this accessible or child accessible
  // at the given index or to get an accessible by child ID in the case of
  // document accessible.
  // The getting an accessible by child ID is used by
  // AccessibleObjectFromEvent() called by AT when AT handles our MSAA event.
  bool isDefunct = false;
  RefPtr<IAccessible> child = GetIAccessibleFor(varChild, &isDefunct);
  if (!child) {
    return E_INVALIDARG;
  }

  if (isDefunct) {
    return CO_E_OBJNOTCONNECTED;
  }

  child.forget(ppdispChild);
  return S_OK;
}

STDMETHODIMP
AccessibleWrap::get_accName(
    /* [optional][in] */ VARIANT varChild,
    /* [retval][out] */ BSTR __RPC_FAR* pszName) {
  if (!pszName || varChild.vt != VT_I4) return E_INVALIDARG;

  *pszName = nullptr;

  RefPtr<IAccessible> accessible;
  HRESULT hr = ResolveChild(varChild, getter_AddRefs(accessible));
  if (FAILED(hr)) {
    return hr;
  }

  if (accessible) {
    return accessible->get_accName(kVarChildIdSelf, pszName);
  }

  nsAutoString name;
  Name(name);

  // The name was not provided, e.g. no alt attribute for an image. A screen
  // reader may choose to invent its own accessible name, e.g. from an image src
  // attribute. Refer to eNoNameOnPurpose return value.
  if (name.IsVoid()) return S_FALSE;

  *pszName = ::SysAllocStringLen(name.get(), name.Length());
  if (!*pszName) return E_OUTOFMEMORY;
  return S_OK;
}

STDMETHODIMP
AccessibleWrap::get_accValue(
    /* [optional][in] */ VARIANT varChild,
    /* [retval][out] */ BSTR __RPC_FAR* pszValue) {
  if (!pszValue) return E_INVALIDARG;

  *pszValue = nullptr;

  RefPtr<IAccessible> accessible;
  HRESULT hr = ResolveChild(varChild, getter_AddRefs(accessible));
  if (FAILED(hr)) {
    return hr;
  }

  if (accessible) {
    return accessible->get_accValue(kVarChildIdSelf, pszValue);
  }

  nsAutoString value;
  Value(value);

  // See bug 438784: need to expose URL on doc's value attribute. For this,
  // reverting part of fix for bug 425693 to make this MSAA method behave
  // IAccessible2-style.
  if (value.IsEmpty()) return S_FALSE;

  *pszValue = ::SysAllocStringLen(value.get(), value.Length());
  if (!*pszValue) return E_OUTOFMEMORY;
  return S_OK;
}

STDMETHODIMP
AccessibleWrap::get_accDescription(VARIANT varChild,
                                   BSTR __RPC_FAR* pszDescription) {
  if (!pszDescription) return E_INVALIDARG;

  *pszDescription = nullptr;

  RefPtr<IAccessible> accessible;
  HRESULT hr = ResolveChild(varChild, getter_AddRefs(accessible));
  if (FAILED(hr)) {
    return hr;
  }

  if (accessible) {
    return accessible->get_accDescription(kVarChildIdSelf, pszDescription);
  }

  nsAutoString description;
  Description(description);

  *pszDescription =
      ::SysAllocStringLen(description.get(), description.Length());
  return *pszDescription ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
AccessibleWrap::get_accRole(
    /* [optional][in] */ VARIANT varChild,
    /* [retval][out] */ VARIANT __RPC_FAR* pvarRole) {
  if (!pvarRole) return E_INVALIDARG;

  VariantInit(pvarRole);

  RefPtr<IAccessible> accessible;
  HRESULT hr = ResolveChild(varChild, getter_AddRefs(accessible));
  if (FAILED(hr)) {
    return hr;
  }

  if (accessible) {
    return accessible->get_accRole(kVarChildIdSelf, pvarRole);
  }

  a11y::role geckoRole;
#ifdef DEBUG
  NS_ASSERTION(nsAccUtils::IsTextInterfaceSupportCorrect(this),
               "Does not support Text when it should");
#endif

  geckoRole = Role();

  uint32_t msaaRole = 0;

#define ROLE(_geckoRole, stringRole, atkRole, macRole, macSubrole, _msaaRole, \
             ia2Role, androidClass, nameRule)                                 \
  case roles::_geckoRole:                                                     \
    msaaRole = _msaaRole;                                                     \
    break;

  switch (geckoRole) {
#include "RoleMap.h"
    default:
      MOZ_CRASH("Unknown role.");
  }

#undef ROLE

  // Special case, if there is a ROLE_ROW inside of a ROLE_TREE_TABLE, then call
  // the MSAA role a ROLE_OUTLINEITEM for consistency and compatibility. We need
  // this because ARIA has a role of "row" for both grid and treegrid
  if (geckoRole == roles::ROW) {
    LocalAccessible* xpParent = LocalParent();
    if (xpParent && xpParent->Role() == roles::TREE_TABLE)
      msaaRole = ROLE_SYSTEM_OUTLINEITEM;
  }

  // -- Try enumerated role
  if (msaaRole != USE_ROLE_STRING) {
    pvarRole->vt = VT_I4;
    pvarRole->lVal = msaaRole;  // Normal enumerated role
    return S_OK;
  }

  // -- Try BSTR role
  // Could not map to known enumerated MSAA role like ROLE_BUTTON
  // Use BSTR role to expose role attribute or tag name + namespace
  nsIContent* content = GetContent();
  if (!content) return E_FAIL;

  if (content->IsElement()) {
    nsAutoString roleString;
    // Try the role attribute.
    content->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::role,
                                  roleString);

    if (roleString.IsEmpty()) {
      // No role attribute (or it is an empty string).
      // Use the tag name.
      dom::Document* document = content->GetUncomposedDoc();
      if (!document) return E_FAIL;

      dom::NodeInfo* nodeInfo = content->NodeInfo();
      nodeInfo->GetName(roleString);

      // Only append name space if different from that of current document.
      if (!nodeInfo->NamespaceEquals(document->GetDefaultNamespaceID())) {
        nsAutoString nameSpaceURI;
        nodeInfo->GetNamespaceURI(nameSpaceURI);
        roleString += u", "_ns + nameSpaceURI;
      }
    }

    if (!roleString.IsEmpty()) {
      pvarRole->vt = VT_BSTR;
      pvarRole->bstrVal = ::SysAllocString(roleString.get());
      return S_OK;
    }
  }

  return E_FAIL;
}

STDMETHODIMP
AccessibleWrap::get_accState(
    /* [optional][in] */ VARIANT varChild,
    /* [retval][out] */ VARIANT __RPC_FAR* pvarState) {
  if (!pvarState) return E_INVALIDARG;

  VariantInit(pvarState);
  pvarState->vt = VT_I4;
  pvarState->lVal = 0;

  RefPtr<IAccessible> accessible;
  HRESULT hr = ResolveChild(varChild, getter_AddRefs(accessible));
  if (FAILED(hr)) {
    return hr;
  }

  if (accessible) {
    return accessible->get_accState(kVarChildIdSelf, pvarState);
  }

  // MSAA only has 31 states and the lowest 31 bits of our state bit mask
  // are the same states as MSAA.
  // Note: we map the following Gecko states to different MSAA states:
  //   REQUIRED -> ALERT_LOW
  //   ALERT -> ALERT_MEDIUM
  //   INVALID -> ALERT_HIGH
  //   CHECKABLE -> MARQUEED

  uint64_t state = State();

  uint32_t msaaState = 0;
  nsAccUtils::To32States(state, &msaaState, nullptr);
  pvarState->lVal = msaaState;
  return S_OK;
}

STDMETHODIMP
AccessibleWrap::get_accHelp(
    /* [optional][in] */ VARIANT varChild,
    /* [retval][out] */ BSTR __RPC_FAR* pszHelp) {
  if (!pszHelp) return E_INVALIDARG;

  *pszHelp = nullptr;
  return S_FALSE;
}

STDMETHODIMP
AccessibleWrap::get_accHelpTopic(
    /* [out] */ BSTR __RPC_FAR* pszHelpFile,
    /* [optional][in] */ VARIANT varChild,
    /* [retval][out] */ long __RPC_FAR* pidTopic) {
  if (!pszHelpFile || !pidTopic) return E_INVALIDARG;

  *pszHelpFile = nullptr;
  *pidTopic = 0;
  return S_FALSE;
}

STDMETHODIMP
AccessibleWrap::get_accKeyboardShortcut(
    /* [optional][in] */ VARIANT varChild,
    /* [retval][out] */ BSTR __RPC_FAR* pszKeyboardShortcut) {
  if (!pszKeyboardShortcut) return E_INVALIDARG;
  *pszKeyboardShortcut = nullptr;

  RefPtr<IAccessible> accessible;
  HRESULT hr = ResolveChild(varChild, getter_AddRefs(accessible));
  if (FAILED(hr)) {
    return hr;
  }

  if (accessible) {
    return accessible->get_accKeyboardShortcut(kVarChildIdSelf,
                                               pszKeyboardShortcut);
  }

  KeyBinding keyBinding = AccessKey();
  if (keyBinding.IsEmpty()) keyBinding = KeyboardShortcut();

  nsAutoString shortcut;
  keyBinding.ToString(shortcut);

  *pszKeyboardShortcut = ::SysAllocStringLen(shortcut.get(), shortcut.Length());
  return *pszKeyboardShortcut ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
AccessibleWrap::get_accFocus(
    /* [retval][out] */ VARIANT __RPC_FAR* pvarChild) {
  if (!pvarChild) return E_INVALIDARG;

  VariantInit(pvarChild);

  // clang-format off
  // VT_EMPTY:    None. This object does not have the keyboard focus itself
  //              and does not contain a child that has the keyboard focus.
  // VT_I4:       lVal is CHILDID_SELF. The object itself has the keyboard focus.
  // VT_I4:       lVal contains the child ID of the child element with the keyboard focus.
  // VT_DISPATCH: pdispVal member is the address of the IDispatch interface
  //              for the child object with the keyboard focus.
  // clang-format on
  if (IsDefunct()) return CO_E_OBJNOTCONNECTED;

  // Return the current IAccessible child that has focus
  LocalAccessible* focusedAccessible = FocusedChild();

  if (focusedAccessible == this) {
    pvarChild->vt = VT_I4;
    pvarChild->lVal = CHILDID_SELF;
  } else if (focusedAccessible) {
    pvarChild->vt = VT_DISPATCH;
    pvarChild->pdispVal = NativeAccessible(focusedAccessible);
  } else {
    pvarChild->vt = VT_EMPTY;  // No focus or focus is not a child
  }

  return S_OK;
}

/**
 * This helper class implements IEnumVARIANT for a nsTArray containing
 * accessible objects.
 */
class AccessibleEnumerator final : public IEnumVARIANT {
 public:
  explicit AccessibleEnumerator(const nsTArray<LocalAccessible*>& aArray)
      : mArray(aArray.Clone()), mCurIndex(0) {}
  AccessibleEnumerator(const AccessibleEnumerator& toCopy)
      : mArray(toCopy.mArray.Clone()), mCurIndex(toCopy.mCurIndex) {}
  ~AccessibleEnumerator() {}

  // IUnknown
  DECL_IUNKNOWN

  // IEnumVARIANT
  STDMETHODIMP Next(unsigned long celt, VARIANT FAR* rgvar,
                    unsigned long FAR* pceltFetched);
  STDMETHODIMP Skip(unsigned long celt);
  STDMETHODIMP Reset() {
    mCurIndex = 0;
    return S_OK;
  }
  STDMETHODIMP Clone(IEnumVARIANT FAR* FAR* ppenum);

 private:
  nsTArray<LocalAccessible*> mArray;
  uint32_t mCurIndex;
};

STDMETHODIMP
AccessibleEnumerator::QueryInterface(REFIID iid, void** ppvObject) {
  if (iid == IID_IEnumVARIANT) {
    *ppvObject = static_cast<IEnumVARIANT*>(this);
    AddRef();
    return S_OK;
  }
  if (iid == IID_IUnknown) {
    *ppvObject = static_cast<IUnknown*>(this);
    AddRef();
    return S_OK;
  }

  *ppvObject = nullptr;
  return E_NOINTERFACE;
}

STDMETHODIMP
AccessibleEnumerator::Next(unsigned long celt, VARIANT FAR* rgvar,
                           unsigned long FAR* pceltFetched) {
  uint32_t length = mArray.Length();
  HRESULT hr = S_OK;

  // Can't get more elements than there are...
  if (celt > length - mCurIndex) {
    hr = S_FALSE;
    celt = length - mCurIndex;
  }

  // Copy the elements of the array into rgvar.
  for (uint32_t i = 0; i < celt; ++i, ++mCurIndex) {
    rgvar[i].vt = VT_DISPATCH;
    rgvar[i].pdispVal = AccessibleWrap::NativeAccessible(mArray[mCurIndex]);
  }

  if (pceltFetched) *pceltFetched = celt;

  return hr;
}

STDMETHODIMP
AccessibleEnumerator::Clone(IEnumVARIANT FAR* FAR* ppenum) {
  *ppenum = new AccessibleEnumerator(*this);
  NS_ADDREF(*ppenum);
  return S_OK;
}

STDMETHODIMP
AccessibleEnumerator::Skip(unsigned long celt) {
  uint32_t length = mArray.Length();
  // Check if we can skip the requested number of elements
  if (celt > length - mCurIndex) {
    mCurIndex = length;
    return S_FALSE;
  }
  mCurIndex += celt;
  return S_OK;
}

/**
 * This method is called when a client wants to know which children of a node
 *  are selected. Note that this method can only find selected children for
 *  accessible object which implement SelectAccessible.
 *
 * The VARIANT return value arguement is expected to either contain a single
 * IAccessible or an IEnumVARIANT of IAccessibles. We return the IEnumVARIANT
 * regardless of the number of children selected, unless there are none selected
 * in which case we return an empty VARIANT.
 *
 * We get the selected options from the select's accessible object and wrap
 *  those in an AccessibleEnumerator which we then put in the return VARIANT.
 *
 * returns a VT_EMPTY VARIANT if:
 *  - there are no selected children for this object
 *  - the object is not the type that can have children selected
 */
STDMETHODIMP
AccessibleWrap::get_accSelection(VARIANT __RPC_FAR* pvarChildren) {
  if (!pvarChildren) return E_INVALIDARG;

  VariantInit(pvarChildren);
  pvarChildren->vt = VT_EMPTY;

  if (IsDefunct()) return CO_E_OBJNOTCONNECTED;

  if (!IsSelect()) {
    return S_OK;
  }

  AutoTArray<LocalAccessible*, 10> selectedItems;
  SelectedItems(&selectedItems);
  uint32_t count = selectedItems.Length();
  if (count == 1) {
    pvarChildren->vt = VT_DISPATCH;
    pvarChildren->pdispVal = NativeAccessible(selectedItems[0]);
  } else if (count > 1) {
    RefPtr<AccessibleEnumerator> pEnum =
        new AccessibleEnumerator(selectedItems);
    AssociateCOMObjectForDisconnection(pEnum);
    pvarChildren->vt =
        VT_UNKNOWN;  // this must be VT_UNKNOWN for an IEnumVARIANT
    NS_ADDREF(pvarChildren->punkVal = pEnum);
  }
  // If count == 0, vt is already VT_EMPTY, so there's nothing else to do.

  return S_OK;
}

STDMETHODIMP
AccessibleWrap::get_accDefaultAction(
    /* [optional][in] */ VARIANT varChild,
    /* [retval][out] */ BSTR __RPC_FAR* pszDefaultAction) {
  if (!pszDefaultAction) return E_INVALIDARG;

  *pszDefaultAction = nullptr;

  RefPtr<IAccessible> accessible;
  HRESULT hr = ResolveChild(varChild, getter_AddRefs(accessible));
  if (FAILED(hr)) {
    return hr;
  }

  if (accessible) {
    return accessible->get_accDefaultAction(kVarChildIdSelf, pszDefaultAction);
  }

  nsAutoString defaultAction;
  ActionNameAt(0, defaultAction);

  *pszDefaultAction =
      ::SysAllocStringLen(defaultAction.get(), defaultAction.Length());
  return *pszDefaultAction ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
AccessibleWrap::accSelect(
    /* [in] */ long flagsSelect,
    /* [optional][in] */ VARIANT varChild) {
  RefPtr<IAccessible> accessible;
  HRESULT hr = ResolveChild(varChild, getter_AddRefs(accessible));
  if (FAILED(hr)) {
    return hr;
  }

  if (accessible) {
    return accessible->accSelect(flagsSelect, kVarChildIdSelf);
  }

  if (flagsSelect & SELFLAG_TAKEFOCUS) {
    if (XRE_IsContentProcess()) {
      // In this case we might have been invoked while the IPC MessageChannel is
      // waiting on a sync reply. We cannot dispatch additional IPC while that
      // is happening, so we dispatch TakeFocus from the main thread to
      // guarantee that we are outside any IPC.
      nsCOMPtr<nsIRunnable> runnable = mozilla::NewRunnableMethod(
          "LocalAccessible::TakeFocus", this, &LocalAccessible::TakeFocus);
      NS_DispatchToMainThread(runnable, NS_DISPATCH_NORMAL);
      return S_OK;
    }
    TakeFocus();
    return S_OK;
  }

  if (flagsSelect & SELFLAG_TAKESELECTION) {
    TakeSelection();
    return S_OK;
  }

  if (flagsSelect & SELFLAG_ADDSELECTION) {
    SetSelected(true);
    return S_OK;
  }

  if (flagsSelect & SELFLAG_REMOVESELECTION) {
    SetSelected(false);
    return S_OK;
  }

  return E_FAIL;
}

STDMETHODIMP
AccessibleWrap::accLocation(
    /* [out] */ long __RPC_FAR* pxLeft,
    /* [out] */ long __RPC_FAR* pyTop,
    /* [out] */ long __RPC_FAR* pcxWidth,
    /* [out] */ long __RPC_FAR* pcyHeight,
    /* [optional][in] */ VARIANT varChild) {
  if (!pxLeft || !pyTop || !pcxWidth || !pcyHeight) return E_INVALIDARG;

  *pxLeft = 0;
  *pyTop = 0;
  *pcxWidth = 0;
  *pcyHeight = 0;

  RefPtr<IAccessible> accessible;
  HRESULT hr = ResolveChild(varChild, getter_AddRefs(accessible));
  if (FAILED(hr)) {
    return hr;
  }

  if (accessible) {
    return accessible->accLocation(pxLeft, pyTop, pcxWidth, pcyHeight,
                                   kVarChildIdSelf);
  }

  if (IsDefunct()) {
    return CO_E_OBJNOTCONNECTED;
  }

  nsIntRect rect = Bounds();

  *pxLeft = rect.X();
  *pyTop = rect.Y();
  *pcxWidth = rect.Width();
  *pcyHeight = rect.Height();
  return S_OK;
}

STDMETHODIMP
AccessibleWrap::accNavigate(
    /* [in] */ long navDir,
    /* [optional][in] */ VARIANT varStart,
    /* [retval][out] */ VARIANT __RPC_FAR* pvarEndUpAt) {
  if (!pvarEndUpAt) return E_INVALIDARG;

  VariantInit(pvarEndUpAt);

  RefPtr<IAccessible> accessible;
  HRESULT hr = ResolveChild(varStart, getter_AddRefs(accessible));
  if (FAILED(hr)) {
    return hr;
  }

  if (accessible) {
    return accessible->accNavigate(navDir, kVarChildIdSelf, pvarEndUpAt);
  }

  LocalAccessible* navAccessible = nullptr;
  Maybe<RelationType> xpRelation;

#define RELATIONTYPE(geckoType, stringType, atkType, msaaType, ia2Type) \
  case msaaType:                                                        \
    xpRelation.emplace(RelationType::geckoType);                        \
    break;

  switch (navDir) {
    case NAVDIR_FIRSTCHILD:
      if (IsProxy()) {
        if (!nsAccUtils::MustPrune(Proxy())) {
          navAccessible = WrapperFor(Proxy()->RemoteFirstChild());
        }
      } else {
        if (!nsAccUtils::MustPrune(this)) navAccessible = LocalFirstChild();
      }
      break;
    case NAVDIR_LASTCHILD:
      if (IsProxy()) {
        if (!nsAccUtils::MustPrune(Proxy())) {
          navAccessible = WrapperFor(Proxy()->RemoteLastChild());
        }
      } else {
        if (!nsAccUtils::MustPrune(this)) navAccessible = LocalLastChild();
      }
      break;
    case NAVDIR_NEXT:
      navAccessible = IsProxy() ? WrapperFor(Proxy()->RemoteNextSibling())
                                : LocalNextSibling();
      break;
    case NAVDIR_PREVIOUS:
      navAccessible = IsProxy() ? WrapperFor(Proxy()->RemotePrevSibling())
                                : LocalPrevSibling();
      break;
    case NAVDIR_DOWN:
    case NAVDIR_LEFT:
    case NAVDIR_RIGHT:
    case NAVDIR_UP:
      return E_NOTIMPL;

      // MSAA relationship extensions to accNavigate
#include "RelationTypeMap.h"

    default:
      return E_INVALIDARG;
  }

#undef RELATIONTYPE

  pvarEndUpAt->vt = VT_EMPTY;

  if (xpRelation) {
    Relation rel = RelationByType(*xpRelation);
    navAccessible = rel.Next();
  }

  if (!navAccessible) return E_FAIL;

  pvarEndUpAt->pdispVal = NativeAccessible(navAccessible);
  pvarEndUpAt->vt = VT_DISPATCH;
  return S_OK;
}

STDMETHODIMP
AccessibleWrap::accHitTest(
    /* [in] */ long xLeft,
    /* [in] */ long yTop,
    /* [retval][out] */ VARIANT __RPC_FAR* pvarChild) {
  if (!pvarChild) return E_INVALIDARG;

  VariantInit(pvarChild);

  if (IsDefunct()) return CO_E_OBJNOTCONNECTED;

  LocalAccessible* accessible = LocalChildAtPoint(
      xLeft, yTop, Accessible::EWhichChildAtPoint::DirectChild);

  // if we got a child
  if (accessible) {
    // if the child is us
    if (accessible == this) {
      pvarChild->vt = VT_I4;
      pvarChild->lVal = CHILDID_SELF;
    } else {  // its not create a LocalAccessible for it.
      pvarChild->vt = VT_DISPATCH;
      pvarChild->pdispVal = NativeAccessible(accessible);
    }
  } else {
    // no child at that point
    pvarChild->vt = VT_EMPTY;
    return S_FALSE;
  }
  return S_OK;
}

STDMETHODIMP
AccessibleWrap::accDoDefaultAction(
    /* [optional][in] */ VARIANT varChild) {
  RefPtr<IAccessible> accessible;
  HRESULT hr = ResolveChild(varChild, getter_AddRefs(accessible));
  if (FAILED(hr)) {
    return hr;
  }

  if (accessible) {
    return accessible->accDoDefaultAction(kVarChildIdSelf);
  }

  return DoAction(0) ? S_OK : E_INVALIDARG;
}

STDMETHODIMP
AccessibleWrap::put_accName(
    /* [optional][in] */ VARIANT varChild,
    /* [in] */ BSTR szName) {
  return E_NOTIMPL;
}

STDMETHODIMP
AccessibleWrap::put_accValue(
    /* [optional][in] */ VARIANT varChild,
    /* [in] */ BSTR szValue) {
  RefPtr<IAccessible> accessible;
  HRESULT hr = ResolveChild(varChild, getter_AddRefs(accessible));
  if (FAILED(hr)) {
    return hr;
  }

  if (accessible) {
    return accessible->put_accValue(kVarChildIdSelf, szValue);
  }

  HyperTextAccessible* ht = AsHyperText();
  if (!ht) {
    return E_NOTIMPL;
  }

  uint32_t length = ::SysStringLen(szValue);
  nsAutoString text(szValue, length);
  ht->ReplaceText(text);
  return S_OK;
}

////////////////////////////////////////////////////////////////////////////////
// IDispatch

STDMETHODIMP
AccessibleWrap::GetTypeInfoCount(UINT* pctinfo) {
  if (!pctinfo) return E_INVALIDARG;

  *pctinfo = 1;
  return S_OK;
}

STDMETHODIMP
AccessibleWrap::GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo** ppTInfo) {
  if (!ppTInfo) return E_INVALIDARG;

  *ppTInfo = nullptr;

  if (iTInfo != 0) return DISP_E_BADINDEX;

  ITypeInfo* typeInfo = GetTI(lcid);
  if (!typeInfo) return E_FAIL;

  typeInfo->AddRef();
  *ppTInfo = typeInfo;

  return S_OK;
}

STDMETHODIMP
AccessibleWrap::GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, UINT cNames,
                              LCID lcid, DISPID* rgDispId) {
  ITypeInfo* typeInfo = GetTI(lcid);
  if (!typeInfo) return E_FAIL;

  HRESULT hr = DispGetIDsOfNames(typeInfo, rgszNames, cNames, rgDispId);
  return hr;
}

STDMETHODIMP
AccessibleWrap::Invoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags,
                       DISPPARAMS* pDispParams, VARIANT* pVarResult,
                       EXCEPINFO* pExcepInfo, UINT* puArgErr) {
  ITypeInfo* typeInfo = GetTI(lcid);
  if (!typeInfo) return E_FAIL;

  return typeInfo->Invoke(static_cast<IAccessible*>(this), dispIdMember, wFlags,
                          pDispParams, pVarResult, pExcepInfo, puArgErr);
}

void AccessibleWrap::GetNativeInterface(void** aOutAccessible) {
  *aOutAccessible = static_cast<IAccessible*>(this);
  NS_ADDREF_THIS();
}

////////////////////////////////////////////////////////////////////////////////
// LocalAccessible

nsresult AccessibleWrap::HandleAccEvent(AccEvent* aEvent) {
  nsresult rv = LocalAccessible::HandleAccEvent(aEvent);
  NS_ENSURE_SUCCESS(rv, rv);

  if (IPCAccessibilityActive()) {
    return NS_OK;
  }

  uint32_t eventType = aEvent->GetEventType();

  // Means we're not active.
  NS_ENSURE_TRUE(!IsDefunct(), NS_ERROR_FAILURE);

  LocalAccessible* accessible = aEvent->GetAccessible();
  if (!accessible) return NS_OK;

  if (eventType == nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED ||
      eventType == nsIAccessibleEvent::EVENT_FOCUS) {
    UpdateSystemCaretFor(accessible);
  }

  MsaaAccessible::FireWinEvent(accessible, eventType);

  return NS_OK;
}

DocRemoteAccessibleWrap* AccessibleWrap::DocProxyWrapper() const {
  MOZ_ASSERT(IsProxy());

  RemoteAccessible* proxy = Proxy();
  if (!proxy) {
    return nullptr;
  }

  AccessibleWrap* acc = WrapperFor(proxy->Document());
  MOZ_ASSERT(acc->IsDoc());

  return static_cast<DocRemoteAccessibleWrap*>(acc);
}

////////////////////////////////////////////////////////////////////////////////
// AccessibleWrap

//------- Helper methods ---------

IDispatch* AccessibleWrap::NativeAccessible(LocalAccessible* aAccessible) {
  if (!aAccessible) {
    NS_WARNING("Not passing in an aAccessible");
    return nullptr;
  }

  IAccessible* msaaAccessible = nullptr;
  aAccessible->GetNativeInterface(reinterpret_cast<void**>(&msaaAccessible));
  return static_cast<IDispatch*>(msaaAccessible);
}

bool AccessibleWrap::IsRootForHWND() {
  if (IsRoot()) {
    return true;
  }
  HWND thisHwnd = MsaaAccessible::GetHWNDFor(this);
  AccessibleWrap* parent = static_cast<AccessibleWrap*>(LocalParent());
  MOZ_ASSERT(parent);
  HWND parentHwnd = MsaaAccessible::GetHWNDFor(parent);
  return thisHwnd != parentHwnd;
}

void AccessibleWrap::UpdateSystemCaretFor(LocalAccessible* aAccessible) {
  // Move the system caret so that Windows Tablet Edition and tradional ATs with
  // off-screen model can follow the caret
  ::DestroyCaret();

  HyperTextAccessible* text = aAccessible->AsHyperText();
  if (!text) return;

  nsIWidget* widget = nullptr;
  LayoutDeviceIntRect caretRect = text->GetCaretRect(&widget);

  if (!widget) {
    return;
  }

  HWND caretWnd =
      reinterpret_cast<HWND>(widget->GetNativeData(NS_NATIVE_WINDOW));
  UpdateSystemCaretFor(caretWnd, caretRect);
}

/* static */
void AccessibleWrap::UpdateSystemCaretFor(
    RemoteAccessible* aProxy, const LayoutDeviceIntRect& aCaretRect) {
  ::DestroyCaret();

  // The HWND should be the real widget HWND, not an emulated HWND.
  // We get the HWND from the proxy's outer doc to bypass window emulation.
  LocalAccessible* outerDoc = aProxy->OuterDocOfRemoteBrowser();
  UpdateSystemCaretFor(MsaaAccessible::GetHWNDFor(outerDoc), aCaretRect);
}

/* static */
void AccessibleWrap::UpdateSystemCaretFor(
    HWND aCaretWnd, const LayoutDeviceIntRect& aCaretRect) {
  if (!aCaretWnd || aCaretRect.IsEmpty()) {
    return;
  }

  // Create invisible bitmap for caret, otherwise its appearance interferes
  // with Gecko caret
  nsAutoBitmap caretBitMap(CreateBitmap(1, aCaretRect.Height(), 1, 1, nullptr));
  if (::CreateCaret(aCaretWnd, caretBitMap, 1,
                    aCaretRect.Height())) {  // Also destroys the last caret
    ::ShowCaret(aCaretWnd);
    RECT windowRect;
    ::GetWindowRect(aCaretWnd, &windowRect);
    ::SetCaretPos(aCaretRect.X() - windowRect.left,
                  aCaretRect.Y() - windowRect.top);
  }
}

ITypeInfo* AccessibleWrap::GetTI(LCID lcid) {
  if (gTypeInfo) return gTypeInfo;

  ITypeLib* typeLib = nullptr;
  HRESULT hr = LoadRegTypeLib(LIBID_Accessibility, 1, 0, lcid, &typeLib);
  if (FAILED(hr)) return nullptr;

  hr = typeLib->GetTypeInfoOfGuid(IID_IAccessible, &gTypeInfo);
  typeLib->Release();

  if (FAILED(hr)) return nullptr;

  return gTypeInfo;
}

/* static */
void AccessibleWrap::SetHandlerControl(DWORD aPid,
                                       RefPtr<IHandlerControl> aCtrl) {
  MOZ_ASSERT(XRE_IsParentProcess() && NS_IsMainThread());

  if (!sHandlerControllers) {
    sHandlerControllers = new nsTArray<HandlerControllerData>();
    ClearOnShutdown(&sHandlerControllers);
  }

  HandlerControllerData ctrlData(aPid, std::move(aCtrl));
  if (sHandlerControllers->Contains(ctrlData)) {
    return;
  }

  sHandlerControllers->AppendElement(std::move(ctrlData));
}

/* static */
void AccessibleWrap::InvalidateHandlers() {
  static const HRESULT kErrorServerDied =
      HRESULT_FROM_WIN32(RPC_S_SERVER_UNAVAILABLE);

  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  if (!sHandlerControllers || sHandlerControllers->IsEmpty()) {
    return;
  }

  // We iterate in reverse so that we may safely remove defunct elements while
  // executing the loop.
  for (auto& controller : Reversed(*sHandlerControllers)) {
    MOZ_ASSERT(controller.mPid);
    MOZ_ASSERT(controller.mCtrl);

    ASYNC_INVOKER_FOR(IHandlerControl)
    invoker(controller.mCtrl, Some(controller.mIsProxy));

    HRESULT hr = ASYNC_INVOKE(invoker, Invalidate);

    if (hr == CO_E_OBJNOTCONNECTED || hr == kErrorServerDied) {
      sHandlerControllers->RemoveElement(controller);
    } else {
      Unused << NS_WARN_IF(FAILED(hr));
    }
  }
}

bool AccessibleWrap::DispatchTextChangeToHandler(bool aIsInsert,
                                                 const nsString& aText,
                                                 int32_t aStart,
                                                 uint32_t aLen) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  if (!sHandlerControllers || sHandlerControllers->IsEmpty()) {
    return false;
  }

  HWND hwnd = MsaaAccessible::GetHWNDFor(this);
  MOZ_ASSERT(hwnd);
  if (!hwnd) {
    return false;
  }

  long msaaId = MsaaAccessible::GetChildIDFor(this);

  DWORD ourPid = ::GetCurrentProcessId();

  // The handler ends up calling NotifyWinEvent, which should only be done once
  // since it broadcasts the same event to every process who is subscribed.
  // OTOH, if our chrome process contains a handler, we should prefer to
  // broadcast the event from that process, as we want any DLLs injected by ATs
  // to receive the event synchronously. Otherwise we simply choose the first
  // handler in the list, for the lack of a better heuristic.

  nsTArray<HandlerControllerData>::index_type ctrlIndex =
      sHandlerControllers->IndexOf(ourPid);

  if (ctrlIndex == nsTArray<HandlerControllerData>::NoIndex) {
    ctrlIndex = 0;
  }

  HandlerControllerData& controller = sHandlerControllers->ElementAt(ctrlIndex);
  MOZ_ASSERT(controller.mPid);
  MOZ_ASSERT(controller.mCtrl);

  VARIANT_BOOL isInsert = aIsInsert ? VARIANT_TRUE : VARIANT_FALSE;

  IA2TextSegment textSegment{::SysAllocStringLen(aText.get(), aText.Length()),
                             aStart, aStart + static_cast<long>(aLen)};

  ASYNC_INVOKER_FOR(IHandlerControl)
  invoker(controller.mCtrl, Some(controller.mIsProxy));

  HRESULT hr = ASYNC_INVOKE(invoker, OnTextChange, PtrToLong(hwnd), msaaId,
                            isInsert, &textSegment);

  ::SysFreeString(textSegment.text);

  return SUCCEEDED(hr);
}
