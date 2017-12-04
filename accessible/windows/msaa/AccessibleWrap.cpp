/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessibleWrap.h"
#include "Accessible-inl.h"

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
#include "mozilla/a11y/ProxyAccessible.h"
#include "ProxyWrappers.h"
#include "ServiceProvider.h"
#include "Relation.h"
#include "Role.h"
#include "RootAccessible.h"
#include "sdnAccessible.h"
#include "States.h"

#ifdef A11Y_LOG
#include "Logging.h"
#endif

#include "nsIMutableArray.h"
#include "nsIFrame.h"
#include "nsIScrollableFrame.h"
#include "mozilla/dom/NodeInfo.h"
#include "mozilla/dom/TabParent.h"
#include "nsIServiceManager.h"
#include "nsNameSpaceManager.h"
#include "nsTextFormatter.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsEventMap.h"
#include "nsArrayUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/ReverseIterator.h"
#include "nsIXULRuntime.h"
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

MsaaIdGenerator AccessibleWrap::sIDGen;
StaticAutoPtr<nsTArray<AccessibleWrap::HandlerControllerData>> AccessibleWrap::sHandlerControllers;

static const VARIANT kVarChildIdSelf = {VT_I4};

static const int32_t kIEnumVariantDisconnected = -1;

////////////////////////////////////////////////////////////////////////////////
// AccessibleWrap
////////////////////////////////////////////////////////////////////////////////
AccessibleWrap::AccessibleWrap(nsIContent* aContent, DocAccessible* aDoc) :
  Accessible(aContent, aDoc)
  , mID(kNoID)
{
}

AccessibleWrap::~AccessibleWrap()
{
  if (mID != kNoID) {
    sIDGen.ReleaseID(WrapNotNull(this));
  }
}

ITypeInfo* AccessibleWrap::gTypeInfo = nullptr;

NS_IMPL_ISUPPORTS_INHERITED0(AccessibleWrap, Accessible)

void
AccessibleWrap::Shutdown()
{
  if (mID != kNoID) {
    auto doc = static_cast<DocAccessibleWrap*>(mDoc.get());
    MOZ_ASSERT(doc);
    if (doc) {
      doc->RemoveID(mID);
      mID = kNoID;
    }
  }

  Accessible::Shutdown();
}

//-----------------------------------------------------
// IUnknown interface methods - see iunknown.h for documentation
//-----------------------------------------------------

// Microsoft COM QueryInterface
STDMETHODIMP
AccessibleWrap::QueryInterface(REFIID iid, void** ppv)
{
  if (!ppv)
    return E_INVALIDARG;

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
    if (!HasChildren() || nsAccUtils::MustPrune(this))
      return E_NOINTERFACE;

    *ppv = static_cast<IEnumVARIANT*>(new ChildrenEnumVariant(this));
  } else if (IID_IServiceProvider == iid)
    *ppv = new ServiceProvider(this);
  else if (IID_ISimpleDOMNode == iid && !IsProxy()) {
    if (IsDefunct() || (!HasOwnContent() && !IsDoc()))
      return E_NOINTERFACE;

    *ppv = static_cast<ISimpleDOMNode*>(new sdnAccessible(WrapNotNull(this)));
  }

  if (nullptr == *ppv) {
    HRESULT hr = ia2Accessible::QueryInterface(iid, ppv);
    if (SUCCEEDED(hr))
      return hr;
  }

  if (nullptr == *ppv && !IsProxy()) {
    HRESULT hr = ia2AccessibleComponent::QueryInterface(iid, ppv);
    if (SUCCEEDED(hr))
      return hr;
  }

  if (nullptr == *ppv) {
    HRESULT hr = ia2AccessibleHyperlink::QueryInterface(iid, ppv);
    if (SUCCEEDED(hr))
      return hr;
  }

  if (nullptr == *ppv && !IsProxy()) {
    HRESULT hr = ia2AccessibleValue::QueryInterface(iid, ppv);
    if (SUCCEEDED(hr))
      return hr;
  }

  if (!*ppv && iid == IID_IGeckoCustom) {
    RefPtr<GeckoCustom> gkCrap = new GeckoCustom(this);
    gkCrap.forget(ppv);
    return S_OK;
  }

  if (nullptr == *ppv)
    return E_NOINTERFACE;

  (reinterpret_cast<IUnknown*>(*ppv))->AddRef();
  return S_OK;
}

//-----------------------------------------------------
// IAccessible methods
//-----------------------------------------------------

STDMETHODIMP
AccessibleWrap::get_accParent( IDispatch __RPC_FAR *__RPC_FAR *ppdispParent)
{
  if (!ppdispParent)
    return E_INVALIDARG;

  *ppdispParent = nullptr;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  DocAccessible* doc = AsDoc();
  if (doc) {
    // Return window system accessible object for root document and tab document
    // accessibles.
    if (!doc->ParentDocument() ||
        (nsWinUtils::IsWindowEmulationStarted() &&
         nsCoreUtils::IsTabDocument(doc->DocumentNode()))) {
      HWND hwnd = static_cast<HWND>(doc->GetNativeWindow());
      if (hwnd && SUCCEEDED(::AccessibleObjectFromWindow(hwnd, OBJID_WINDOW,
                                                         IID_IAccessible,
                                                         (void**)ppdispParent))) {
        return S_OK;
      }
    }
  }

  Accessible* xpParentAcc = Parent();
  if (!xpParentAcc)
    return S_FALSE;

  *ppdispParent = NativeAccessible(xpParentAcc);
  return S_OK;
}

STDMETHODIMP
AccessibleWrap::get_accChildCount( long __RPC_FAR *pcountChildren)
{
  if (!pcountChildren)
    return E_INVALIDARG;

  *pcountChildren = 0;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  if (nsAccUtils::MustPrune(this))
    return S_OK;

  *pcountChildren = ChildCount();
  return S_OK;
}

STDMETHODIMP
AccessibleWrap::get_accChild(
      /* [in] */ VARIANT varChild,
      /* [retval][out] */ IDispatch __RPC_FAR *__RPC_FAR *ppdispChild)
{
  if (!ppdispChild)
    return E_INVALIDARG;

  *ppdispChild = nullptr;
  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  // IAccessible::accChild is used to return this accessible or child accessible
  // at the given index or to get an accessible by child ID in the case of
  // document accessible.
  // The getting an accessible by child ID is used by AccessibleObjectFromEvent()
  // called by AT when AT handles our MSAA event.
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

/**
 * This function is a helper for implementing IAccessible methods that accept
 * a Child ID as a parameter. If the child ID is CHILDID_SELF, the function
 * returns S_OK but a null *aOutInterface. Otherwise, *aOutInterface points
 * to the resolved IAccessible.
 *
 * The CHILDID_SELF case is special because in that case we actually execute
 * the implementation of the IAccessible method, whereas in the non-self case,
 * we delegate the method call to that object for execution.
 *
 * A sample invocation of this would look like:
 *
 *  RefPtr<IAccessible> accessible;
 *  HRESULT hr = ResolveChild(varChild, getter_AddRefs(accessible));
 *  if (FAILED(hr)) {
 *    return hr;
 *  }
 *
 *  if (accessible) {
 *    return accessible->get_accFoo(kVarChildIdSelf, pszName);
 *  }
 *
 *  // Implementation for CHILDID_SELF case goes here
 */
HRESULT
AccessibleWrap::ResolveChild(const VARIANT& aVarChild,
                             IAccessible** aOutInterface)
{
  MOZ_ASSERT(aOutInterface);
  *aOutInterface = nullptr;

  if (aVarChild.vt != VT_I4) {
    return E_INVALIDARG;
  }

  if (IsDefunct()) {
    return CO_E_OBJNOTCONNECTED;
  }

  if (aVarChild.lVal == CHILDID_SELF) {
    return S_OK;
  }

  bool isDefunct = false;
  RefPtr<IAccessible> accessible = GetIAccessibleFor(aVarChild, &isDefunct);
  if (!accessible) {
    return E_INVALIDARG;
  }

  if (isDefunct) {
    return CO_E_OBJNOTCONNECTED;
  }

  accessible.forget(aOutInterface);
  return S_OK;
}

STDMETHODIMP
AccessibleWrap::get_accName(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszName)
{
  if (!pszName || varChild.vt != VT_I4)
    return E_INVALIDARG;

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
  if (name.IsVoid())
    return S_FALSE;

  *pszName = ::SysAllocStringLen(name.get(), name.Length());
  if (!*pszName)
    return E_OUTOFMEMORY;
  return S_OK;
}


STDMETHODIMP
AccessibleWrap::get_accValue(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszValue)
{
  if (!pszValue)
    return E_INVALIDARG;

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
  if (value.IsEmpty())
    return S_FALSE;

  *pszValue = ::SysAllocStringLen(value.get(), value.Length());
  if (!*pszValue)
    return E_OUTOFMEMORY;
  return S_OK;
}

STDMETHODIMP
AccessibleWrap::get_accDescription(VARIANT varChild,
                                   BSTR __RPC_FAR *pszDescription)
{
  if (!pszDescription)
    return E_INVALIDARG;

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

  *pszDescription = ::SysAllocStringLen(description.get(),
                                        description.Length());
  return *pszDescription ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
AccessibleWrap::get_accRole(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ VARIANT __RPC_FAR *pvarRole)
{
  if (!pvarRole)
    return E_INVALIDARG;

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

#define ROLE(_geckoRole, stringRole, atkRole, macRole, \
             _msaaRole, ia2Role, nameRule) \
  case roles::_geckoRole: \
    msaaRole = _msaaRole; \
    break;

  switch (geckoRole) {
#include "RoleMap.h"
    default:
      MOZ_CRASH("Unknown role.");
  }

#undef ROLE

  // Special case, if there is a ROLE_ROW inside of a ROLE_TREE_TABLE, then call the MSAA role
  // a ROLE_OUTLINEITEM for consistency and compatibility.
  // We need this because ARIA has a role of "row" for both grid and treegrid
  if (geckoRole == roles::ROW) {
    Accessible* xpParent = Parent();
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
  nsIContent *content = GetContent();
  if (!content)
    return E_FAIL;

  if (content->IsElement()) {
    nsAutoString roleString;
    // Try the role attribute.
    content->GetAttr(kNameSpaceID_None, nsGkAtoms::role, roleString);

    if (roleString.IsEmpty()) {
      // No role attribute (or it is an empty string).
      // Use the tag name.
      nsIDocument * document = content->GetUncomposedDoc();
      if (!document)
        return E_FAIL;

      dom::NodeInfo *nodeInfo = content->NodeInfo();
      nodeInfo->GetName(roleString);

      // Only append name space if different from that of current document.
      if (!nodeInfo->NamespaceEquals(document->GetDefaultNamespaceID())) {
        nsAutoString nameSpaceURI;
        nodeInfo->GetNamespaceURI(nameSpaceURI);
        roleString += NS_LITERAL_STRING(", ") + nameSpaceURI;
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
      /* [retval][out] */ VARIANT __RPC_FAR *pvarState)
{
  if (!pvarState)
    return E_INVALIDARG;

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
      /* [retval][out] */ BSTR __RPC_FAR *pszHelp)
{
  if (!pszHelp)
    return E_INVALIDARG;

  *pszHelp = nullptr;
  return S_FALSE;
}

STDMETHODIMP
AccessibleWrap::get_accHelpTopic(
      /* [out] */ BSTR __RPC_FAR *pszHelpFile,
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ long __RPC_FAR *pidTopic)
{
  if (!pszHelpFile || !pidTopic)
    return E_INVALIDARG;

  *pszHelpFile = nullptr;
  *pidTopic = 0;
  return S_FALSE;
}

STDMETHODIMP
AccessibleWrap::get_accKeyboardShortcut(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszKeyboardShortcut)
{
  if (!pszKeyboardShortcut)
    return E_INVALIDARG;
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
  if (keyBinding.IsEmpty())
    keyBinding = KeyboardShortcut();

  nsAutoString shortcut;
  keyBinding.ToString(shortcut);

  *pszKeyboardShortcut = ::SysAllocStringLen(shortcut.get(),
                                             shortcut.Length());
  return *pszKeyboardShortcut ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
AccessibleWrap::get_accFocus(
      /* [retval][out] */ VARIANT __RPC_FAR *pvarChild)
{
  if (!pvarChild)
    return E_INVALIDARG;

  VariantInit(pvarChild);

  // VT_EMPTY:    None. This object does not have the keyboard focus itself
  //              and does not contain a child that has the keyboard focus.
  // VT_I4:       lVal is CHILDID_SELF. The object itself has the keyboard focus.
  // VT_I4:       lVal contains the child ID of the child element with the keyboard focus.
  // VT_DISPATCH: pdispVal member is the address of the IDispatch interface
  //              for the child object with the keyboard focus.
  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  // Return the current IAccessible child that has focus
  Accessible* focusedAccessible = FocusedChild();

  if (focusedAccessible == this) {
    pvarChild->vt = VT_I4;
    pvarChild->lVal = CHILDID_SELF;
  }
  else if (focusedAccessible) {
    pvarChild->vt = VT_DISPATCH;
    pvarChild->pdispVal = NativeAccessible(focusedAccessible);
  }
  else {
    pvarChild->vt = VT_EMPTY;   // No focus or focus is not a child
  }

  return S_OK;
}

/**
 * This helper class implements IEnumVARIANT for a nsTArray containing
 * accessible objects.
 */
class AccessibleEnumerator final : public IEnumVARIANT
{
public:
  explicit AccessibleEnumerator(const nsTArray<Accessible*>& aArray) :
    mArray(aArray), mCurIndex(0) { }
  AccessibleEnumerator(const AccessibleEnumerator& toCopy) :
    mArray(toCopy.mArray), mCurIndex(toCopy.mCurIndex) { }
  ~AccessibleEnumerator() { }

  // IUnknown
  DECL_IUNKNOWN

  // IEnumVARIANT
  STDMETHODIMP Next(unsigned long celt, VARIANT FAR* rgvar, unsigned long FAR* pceltFetched);
  STDMETHODIMP Skip(unsigned long celt);
  STDMETHODIMP Reset()
  {
    mCurIndex = 0;
    return S_OK;
  }
  STDMETHODIMP Clone(IEnumVARIANT FAR* FAR* ppenum);

private:
  nsTArray<Accessible*> mArray;
  uint32_t mCurIndex;
};

STDMETHODIMP
AccessibleEnumerator::QueryInterface(REFIID iid, void ** ppvObject)
{
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
AccessibleEnumerator::Next(unsigned long celt, VARIANT FAR* rgvar, unsigned long FAR* pceltFetched)
{
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

  if (pceltFetched)
    *pceltFetched = celt;

  return hr;
}

STDMETHODIMP
AccessibleEnumerator::Clone(IEnumVARIANT FAR* FAR* ppenum)
{
  *ppenum = new AccessibleEnumerator(*this);
  if (!*ppenum)
    return E_OUTOFMEMORY;
  NS_ADDREF(*ppenum);
  return S_OK;
}

STDMETHODIMP
AccessibleEnumerator::Skip(unsigned long celt)
{
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
  * The VARIANT return value arguement is expected to either contain a single IAccessible
  *  or an IEnumVARIANT of IAccessibles. We return the IEnumVARIANT regardless of the number
  *  of children selected, unless there are none selected in which case we return an empty
  *  VARIANT.
  *
  * We get the selected options from the select's accessible object and wrap
  *  those in an AccessibleEnumerator which we then put in the return VARIANT.
  *
  * returns a VT_EMPTY VARIANT if:
  *  - there are no selected children for this object
  *  - the object is not the type that can have children selected
  */
STDMETHODIMP
AccessibleWrap::get_accSelection(VARIANT __RPC_FAR *pvarChildren)
{
  if (!pvarChildren)
    return E_INVALIDARG;

  VariantInit(pvarChildren);
  pvarChildren->vt = VT_EMPTY;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  if (IsSelect()) {
    AutoTArray<Accessible*, 10> selectedItems;
    SelectedItems(&selectedItems);

    // 1) Create and initialize the enumeration
    RefPtr<AccessibleEnumerator> pEnum = new AccessibleEnumerator(selectedItems);
    pvarChildren->vt = VT_UNKNOWN;    // this must be VT_UNKNOWN for an IEnumVARIANT
    NS_ADDREF(pvarChildren->punkVal = pEnum);
  }
  return S_OK;
}

STDMETHODIMP
AccessibleWrap::get_accDefaultAction(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszDefaultAction)
{
  if (!pszDefaultAction)
    return E_INVALIDARG;

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

  *pszDefaultAction = ::SysAllocStringLen(defaultAction.get(),
                                          defaultAction.Length());
  return *pszDefaultAction ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
AccessibleWrap::accSelect(
      /* [in] */ long flagsSelect,
      /* [optional][in] */ VARIANT varChild)
{
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
      nsCOMPtr<nsIRunnable> runnable =
        mozilla::NewRunnableMethod("Accessible::TakeFocus",
                                   this, &Accessible::TakeFocus);
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
      /* [out] */ long __RPC_FAR *pxLeft,
      /* [out] */ long __RPC_FAR *pyTop,
      /* [out] */ long __RPC_FAR *pcxWidth,
      /* [out] */ long __RPC_FAR *pcyHeight,
      /* [optional][in] */ VARIANT varChild)
{
  if (!pxLeft || !pyTop || !pcxWidth || !pcyHeight)
    return E_INVALIDARG;

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

  nsIntRect rect = Bounds();

  *pxLeft = rect.x;
  *pyTop = rect.y;
  *pcxWidth = rect.width;
  *pcyHeight = rect.height;
  return S_OK;
}

STDMETHODIMP
AccessibleWrap::accNavigate(
      /* [in] */ long navDir,
      /* [optional][in] */ VARIANT varStart,
      /* [retval][out] */ VARIANT __RPC_FAR *pvarEndUpAt)
{
  if (!pvarEndUpAt)
    return E_INVALIDARG;

  VariantInit(pvarEndUpAt);

  RefPtr<IAccessible> accessible;
  HRESULT hr = ResolveChild(varStart, getter_AddRefs(accessible));
  if (FAILED(hr)) {
    return hr;
  }

  if (accessible) {
    return accessible->accNavigate(navDir, kVarChildIdSelf, pvarEndUpAt);
  }

  Accessible* navAccessible = nullptr;
  Maybe<RelationType> xpRelation;

#define RELATIONTYPE(geckoType, stringType, atkType, msaaType, ia2Type) \
  case msaaType: \
    xpRelation.emplace(RelationType::geckoType); \
    break;

  switch(navDir) {
    case NAVDIR_FIRSTCHILD:
      if (IsProxy()) {
        if (!Proxy()->MustPruneChildren()) {
          navAccessible = WrapperFor(Proxy()->FirstChild());
        }
      } else {
        if (!nsAccUtils::MustPrune(this))
          navAccessible = FirstChild();
      }
      break;
    case NAVDIR_LASTCHILD:
      if (IsProxy()) {
        if (!Proxy()->MustPruneChildren()) {
          navAccessible = WrapperFor(Proxy()->LastChild());
        }
      } else {
        if (!nsAccUtils::MustPrune(this))
          navAccessible = LastChild();
      }
      break;
    case NAVDIR_NEXT:
      navAccessible = IsProxy()
        ? WrapperFor(Proxy()->NextSibling())
        : NextSibling();
      break;
    case NAVDIR_PREVIOUS:
      navAccessible = IsProxy()
        ? WrapperFor(Proxy()->PrevSibling())
        : PrevSibling();
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

  if (!navAccessible)
    return E_FAIL;

  pvarEndUpAt->pdispVal = NativeAccessible(navAccessible);
  pvarEndUpAt->vt = VT_DISPATCH;
  return S_OK;
}

STDMETHODIMP
AccessibleWrap::accHitTest(
      /* [in] */ long xLeft,
      /* [in] */ long yTop,
      /* [retval][out] */ VARIANT __RPC_FAR *pvarChild)
{
  if (!pvarChild)
    return E_INVALIDARG;

  VariantInit(pvarChild);

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  Accessible* accessible = ChildAtPoint(xLeft, yTop, eDirectChild);

  // if we got a child
  if (accessible) {
    // if the child is us
    if (accessible == this) {
      pvarChild->vt = VT_I4;
      pvarChild->lVal = CHILDID_SELF;
    } else { // its not create an Accessible for it.
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
      /* [optional][in] */ VARIANT varChild)
{
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
      /* [in] */ BSTR szName)
{
  return E_NOTIMPL;
}

STDMETHODIMP
AccessibleWrap::put_accValue(
      /* [optional][in] */ VARIANT varChild,
      /* [in] */ BSTR szValue)
{
  return E_NOTIMPL;
}

////////////////////////////////////////////////////////////////////////////////
// IDispatch

STDMETHODIMP
AccessibleWrap::GetTypeInfoCount(UINT *pctinfo)
{
  if (!pctinfo)
    return E_INVALIDARG;

  *pctinfo = 1;
  return S_OK;
}

STDMETHODIMP
AccessibleWrap::GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo **ppTInfo)
{
  if (!ppTInfo)
    return E_INVALIDARG;

  *ppTInfo = nullptr;

  if (iTInfo != 0)
    return DISP_E_BADINDEX;

  ITypeInfo * typeInfo = GetTI(lcid);
  if (!typeInfo)
    return E_FAIL;

  typeInfo->AddRef();
  *ppTInfo = typeInfo;

  return S_OK;
}

STDMETHODIMP
AccessibleWrap::GetIDsOfNames(REFIID riid, LPOLESTR *rgszNames,
                              UINT cNames, LCID lcid, DISPID *rgDispId)
{
  ITypeInfo *typeInfo = GetTI(lcid);
  if (!typeInfo)
    return E_FAIL;

  HRESULT hr = DispGetIDsOfNames(typeInfo, rgszNames, cNames, rgDispId);
  return hr;
}

STDMETHODIMP
AccessibleWrap::Invoke(DISPID dispIdMember, REFIID riid,
                       LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
                       VARIANT *pVarResult, EXCEPINFO *pExcepInfo,
                       UINT *puArgErr)
{
  ITypeInfo *typeInfo = GetTI(lcid);
  if (!typeInfo)
    return E_FAIL;

  return typeInfo->Invoke(static_cast<IAccessible*>(this), dispIdMember,
                          wFlags, pDispParams, pVarResult, pExcepInfo,
                          puArgErr);
}

void
AccessibleWrap::GetNativeInterface(void** aOutAccessible)
{
  *aOutAccessible = static_cast<IAccessible*>(this);
  NS_ADDREF_THIS();
}

void
AccessibleWrap::SetID(uint32_t aID)
{
  MOZ_ASSERT(XRE_IsParentProcess() && IsProxy());
  mID = aID;
}

static bool
IsHandlerInvalidationNeeded(uint32_t aEvent)
{
  // We want to return true for any events that would indicate that something
  // in the handler cache is out of date.
  switch (aEvent) {
    case EVENT_OBJECT_STATECHANGE:
    case EVENT_OBJECT_LOCATIONCHANGE:
    case EVENT_OBJECT_NAMECHANGE:
    case EVENT_OBJECT_DESCRIPTIONCHANGE:
    case EVENT_OBJECT_VALUECHANGE:
    case IA2_EVENT_ACTION_CHANGED:
    case IA2_EVENT_DOCUMENT_LOAD_COMPLETE:
    case IA2_EVENT_DOCUMENT_LOAD_STOPPED:
    case IA2_EVENT_DOCUMENT_ATTRIBUTE_CHANGED:
    case IA2_EVENT_DOCUMENT_CONTENT_CHANGED:
    case IA2_EVENT_PAGE_CHANGED:
    case IA2_EVENT_TEXT_ATTRIBUTE_CHANGED:
    case IA2_EVENT_TEXT_CHANGED:
    case IA2_EVENT_TEXT_INSERTED:
    case IA2_EVENT_TEXT_REMOVED:
    case IA2_EVENT_TEXT_UPDATED:
    case IA2_EVENT_OBJECT_ATTRIBUTE_CHANGED:
      return true;
    default:
      return false;
  }
}

void
AccessibleWrap::FireWinEvent(Accessible* aTarget, uint32_t aEventType)
{
  MOZ_ASSERT(XRE_IsParentProcess());
  static_assert(sizeof(gWinEventMap)/sizeof(gWinEventMap[0]) == nsIAccessibleEvent::EVENT_LAST_ENTRY,
                "MSAA event map skewed");

  NS_ASSERTION(aEventType > 0 && aEventType < ArrayLength(gWinEventMap), "invalid event type");

  uint32_t winEvent = gWinEventMap[aEventType];
  if (!winEvent)
    return;

  int32_t childID = GetChildIDFor(aTarget);
  if (!childID)
    return; // Can't fire an event without a child ID

  HWND hwnd = GetHWNDFor(aTarget);
  if (!hwnd) {
    return;
  }

  if (IsHandlerInvalidationNeeded(winEvent)) {
    InvalidateHandlers();
  }

  // Fire MSAA event for client area window.
  ::NotifyWinEvent(winEvent, hwnd, OBJID_CLIENT, childID);

  // JAWS announces collapsed combobox navigation based on focus events.
  if (aEventType == nsIAccessibleEvent::EVENT_SELECTION &&
      Compatibility::IsJAWS()) {
    roles::Role role = aTarget->IsProxy() ? aTarget->Proxy()->Role() :
      aTarget->Role();
    if (role == roles::COMBOBOX_OPTION) {
      ::NotifyWinEvent(EVENT_OBJECT_FOCUS, hwnd, OBJID_CLIENT, childID);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Accessible

nsresult
AccessibleWrap::HandleAccEvent(AccEvent* aEvent)
{
  nsresult rv = Accessible::HandleAccEvent(aEvent);
  NS_ENSURE_SUCCESS(rv, rv);

  if (IPCAccessibilityActive()) {
    return NS_OK;
  }

  uint32_t eventType = aEvent->GetEventType();

  // Means we're not active.
  NS_ENSURE_TRUE(!IsDefunct(), NS_ERROR_FAILURE);

  Accessible* accessible = aEvent->GetAccessible();
  if (!accessible)
    return NS_OK;

  if (eventType == nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED ||
      eventType == nsIAccessibleEvent::EVENT_FOCUS) {
    UpdateSystemCaretFor(accessible);
  }

  FireWinEvent(accessible, eventType);

  return NS_OK;
}

DocProxyAccessibleWrap*
AccessibleWrap::DocProxyWrapper() const
{
  MOZ_ASSERT(IsProxy());

  ProxyAccessible* proxy = Proxy();
  if (!proxy) {
    return nullptr;
  }

  AccessibleWrap* acc = WrapperFor(proxy->Document());
  MOZ_ASSERT(acc->IsDoc());

 return static_cast<DocProxyAccessibleWrap*>(acc);
}

////////////////////////////////////////////////////////////////////////////////
// AccessibleWrap

//------- Helper methods ---------

int32_t
AccessibleWrap::GetChildIDFor(Accessible* aAccessible)
{
  // A child ID of the window is required, when we use NotifyWinEvent,
  // so that the 3rd party application can call back and get the IAccessible
  // the event occurred on.

  if (!aAccessible) {
    return 0;
  }

  // Chrome should use mID which has been generated by the content process.
  if (aAccessible->IsProxy()) {
    const uint32_t id = static_cast<AccessibleWrap*>(aAccessible)->mID;
    MOZ_ASSERT(id != kNoID);
    return id;
  }

  if (!aAccessible->Document())
    return 0;

  uint32_t* id = & static_cast<AccessibleWrap*>(aAccessible)->mID;
  if (*id != kNoID)
    return *id;

  *id = sIDGen.GetID();

  MOZ_ASSERT(!aAccessible->IsProxy());
  DocAccessibleWrap* doc =
    static_cast<DocAccessibleWrap*>(aAccessible->Document());
  doc->AddID(*id, static_cast<AccessibleWrap*>(aAccessible));

  return *id;
}

HWND
AccessibleWrap::GetHWNDFor(Accessible* aAccessible)
{
  if (!aAccessible) {
    return nullptr;
  }

  if (aAccessible->IsProxy()) {
    ProxyAccessible* proxy = aAccessible->Proxy();
    if (!proxy) {
      return nullptr;
    }

    // If window emulation is enabled, retrieve the emulated window from the
    // containing document document proxy.
    if (nsWinUtils::IsWindowEmulationStarted()) {
      DocAccessibleParent* doc = proxy->Document();
      HWND hWnd = doc->GetEmulatedWindowHandle();
      if (hWnd) {
        return hWnd;
      }
    }

    // Accessibles in child processes are said to have the HWND of the window
    // their tab is within.  Popups are always in the parent process, and so
    // never proxied, which means this is basically correct.
    Accessible* outerDoc = proxy->OuterDocOfRemoteBrowser();
    NS_ASSERTION(outerDoc, "no outer doc for accessible remote tab!");
    if (!outerDoc) {
      return nullptr;
    }

    return GetHWNDFor(outerDoc);
  }

  DocAccessible* document = aAccessible->Document();
  if(!document)
    return nullptr;

  // Popup lives in own windows, use its HWND until the popup window is
  // hidden to make old JAWS versions work with collapsed comboboxes (see
  // discussion in bug 379678).
  nsIFrame* frame = aAccessible->GetFrame();
  if (frame) {
    nsIWidget* widget = frame->GetNearestWidget();
    if (widget && widget->IsVisible()) {
      nsIPresShell* shell = document->PresShell();
      nsViewManager* vm = shell->GetViewManager();
      if (vm) {
        nsCOMPtr<nsIWidget> rootWidget;
        vm->GetRootWidget(getter_AddRefs(rootWidget));
        // Make sure the accessible belongs to popup. If not then use
        // document HWND (which might be different from root widget in the
        // case of window emulation).
        if (rootWidget != widget)
          return static_cast<HWND>(widget->GetNativeData(NS_NATIVE_WINDOW));
      }
    }
  }

  return static_cast<HWND>(document->GetNativeWindow());
}

IDispatch*
AccessibleWrap::NativeAccessible(Accessible* aAccessible)
{
  if (!aAccessible) {
    NS_WARNING("Not passing in an aAccessible");
    return nullptr;
  }

  IAccessible* msaaAccessible = nullptr;
  aAccessible->GetNativeInterface(reinterpret_cast<void**>(&msaaAccessible));
  return static_cast<IDispatch*>(msaaAccessible);
}

static Accessible*
GetAccessibleInSubtree(DocAccessible* aDoc, uint32_t aID)
{
  Accessible* child = static_cast<DocAccessibleWrap*>(aDoc)->GetAccessibleByID(aID);
  if (child)
    return child;

  uint32_t childDocCount = aDoc->ChildDocumentCount();
  for (uint32_t i = 0; i < childDocCount; i++) {
    child = GetAccessibleInSubtree(aDoc->GetChildDocumentAt(i), aID);
    if (child)
      return child;
  }

    return nullptr;
  }

static already_AddRefed<IDispatch>
GetProxiedAccessibleInSubtree(const DocAccessibleParent* aDoc,
                              const VARIANT& aVarChild)
{
  auto wrapper = static_cast<DocProxyAccessibleWrap*>(WrapperFor(aDoc));
  RefPtr<IAccessible> comProxy;
  int32_t docWrapperChildId = AccessibleWrap::GetChildIDFor(wrapper);
  // Only top level document accessible proxies are created with a pointer to
  // their COM proxy.
  if (aDoc->IsTopLevel()) {
    wrapper->GetNativeInterface(getter_AddRefs(comProxy));
  } else {
    auto tab = static_cast<dom::TabParent*>(aDoc->Manager());
    MOZ_ASSERT(tab);
    DocAccessibleParent* topLevelDoc = tab->GetTopLevelDocAccessible();
    MOZ_ASSERT(topLevelDoc && topLevelDoc->IsTopLevel());
    VARIANT docId = {VT_I4};
    docId.lVal = docWrapperChildId;
    RefPtr<IDispatch> disp = GetProxiedAccessibleInSubtree(topLevelDoc, docId);
    if (!disp) {
      return nullptr;
    }

    DebugOnly<HRESULT> hr = disp->QueryInterface(IID_IAccessible,
                                                 getter_AddRefs(comProxy));
    MOZ_ASSERT(SUCCEEDED(hr));
  }

  MOZ_ASSERT(comProxy);
  if (!comProxy) {
    return nullptr;
  }

  if (docWrapperChildId == aVarChild.lVal) {
    return comProxy.forget();
  }

  RefPtr<IDispatch> disp;
  if (FAILED(comProxy->get_accChild(aVarChild, getter_AddRefs(disp)))) {
    return nullptr;
  }

  return disp.forget();
}

already_AddRefed<IAccessible>
AccessibleWrap::GetIAccessibleFor(const VARIANT& aVarChild, bool* aIsDefunct)
{
  if (aVarChild.vt != VT_I4)
    return nullptr;

  VARIANT varChild = aVarChild;

  MOZ_ASSERT(aIsDefunct);
  *aIsDefunct = false;

  RefPtr<IAccessible> result;

  if (varChild.lVal == CHILDID_SELF) {
    *aIsDefunct = IsDefunct();
    if (*aIsDefunct) {
      return nullptr;
    }
    GetNativeInterface(getter_AddRefs(result));
    if (result) {
      return result.forget();
    }
    // If we're not a proxy, there's nothing more we can do to attempt to
    // resolve the IAccessible, so we just fail.
    if (!IsProxy()) {
      return nullptr;
    }
    // Otherwise, since we're a proxy and we have a null native interface, this
    // indicates that we need to obtain a COM proxy. To do this, we'll replace
    // CHILDID_SELF with our real MSAA ID and continue the search from there.
    varChild.lVal = GetExistingID();
  }

  if (varChild.ulVal != GetExistingID() &&
      (IsProxy() ? Proxy()->MustPruneChildren() : nsAccUtils::MustPrune(this))) {
    // This accessible should have no subtree in platform, return null for its children.
    return nullptr;
  }

  // If the MSAA ID is not a chrome id then we already know that we won't
  // find it here and should look remotely instead. This handles the case when
  // accessible is part of the chrome process and is part of the xul browser
  // window and the child id points in the content documents. Thus we need to
  // make sure that it is never called on proxies.
  if (XRE_IsParentProcess() && !IsProxy() && !sIDGen.IsChromeID(varChild.lVal)) {
    if (!IsRoot()) {
      // Bug 1422201: accChild with a remote id is only valid on the root accessible.
      // Otherwise, we might return remote accessibles which aren't descendants
      // of this accessible. This would confuse clients which use accChild to
      // check whether something is a descendant of a document.
      return nullptr;
    }
    return GetRemoteIAccessibleFor(varChild);
  }

  if (varChild.lVal > 0) {
    // Gecko child indices are 0-based in contrast to indices used in MSAA.
    MOZ_ASSERT(!IsProxy());
    Accessible* xpAcc = GetChildAt(varChild.lVal - 1);
    if (!xpAcc) {
      return nullptr;
    }
    *aIsDefunct = xpAcc->IsDefunct();
    static_cast<AccessibleWrap*>(xpAcc)->GetNativeInterface(getter_AddRefs(result));
    return result.forget();
  }

  // If lVal negative then it is treated as child ID and we should look for
  // accessible through whole accessible subtree including subdocuments.
  // Otherwise we treat lVal as index in parent.
  // First handle the case that both this accessible and the id'd one are in
  // this process.
  if (!IsProxy()) {
    DocAccessible* document = Document();
    Accessible* child =
      GetAccessibleInSubtree(document, static_cast<uint32_t>(varChild.lVal));

    // If it is a document then just return an accessible.
    if (child && IsDoc()) {
      *aIsDefunct = child->IsDefunct();
      static_cast<AccessibleWrap*>(child)->GetNativeInterface(getter_AddRefs(result));
      return result.forget();
    }

    // Otherwise check whether the accessible is a child (this path works for
    // ARIA documents and popups).
    Accessible* parent = child;
    while (parent && parent != document) {
      if (parent == this) {
        *aIsDefunct = child->IsDefunct();
        static_cast<AccessibleWrap*>(child)->GetNativeInterface(getter_AddRefs(result));
        return result.forget();
      }

      parent = parent->Parent();
    }
  }

  // Now see about the case that both this accessible and the target one are
  // proxied.
  if (IsProxy()) {
    DocAccessibleParent* proxyDoc = Proxy()->Document();
    RefPtr<IDispatch> disp = GetProxiedAccessibleInSubtree(proxyDoc, varChild);
    if (!disp) {
      return nullptr;
    }

    MOZ_ASSERT(mscom::IsProxy(disp));
    DebugOnly<HRESULT> hr = disp->QueryInterface(IID_IAccessible,
                                                 getter_AddRefs(result));
    MOZ_ASSERT(SUCCEEDED(hr));
    return result.forget();
  }

  return nullptr;
}

already_AddRefed<IAccessible>
AccessibleWrap::GetRemoteIAccessibleFor(const VARIANT& aVarChild)
{
  a11y::RootAccessible* root = RootAccessible();
  const nsTArray<DocAccessibleParent*>* remoteDocs =
    DocManager::TopLevelRemoteDocs();
  if (!remoteDocs) {
    return nullptr;
  }

  RefPtr<IAccessible> result;

  // We intentionally leave the call to remoteDocs->Length() inside the loop
  // condition because it is possible for reentry to occur in the call to
  // GetProxiedAccessibleInSubtree() such that remoteDocs->Length() is mutated.
  for (size_t i = 0; i < remoteDocs->Length(); i++) {
    DocAccessibleParent* remoteDoc = remoteDocs->ElementAt(i);

    uint32_t remoteDocMsaaId = WrapperFor(remoteDoc)->GetExistingID();
    if (!sIDGen.IsSameContentProcessFor(aVarChild.lVal, remoteDocMsaaId)) {
      continue;
    }

    Accessible* outerDoc = remoteDoc->OuterDocOfRemoteBrowser();
    if (!outerDoc) {
      continue;
    }

    if (outerDoc->RootAccessible() != root) {
      continue;
    }

    RefPtr<IDispatch> disp =
      GetProxiedAccessibleInSubtree(remoteDoc, aVarChild);
    if (!disp) {
      continue;
    }

    DebugOnly<HRESULT> hr = disp->QueryInterface(IID_IAccessible,
                                                 getter_AddRefs(result));
    MOZ_ASSERT(SUCCEEDED(hr));
    return result.forget();
  }

  return nullptr;
}

void
AccessibleWrap::UpdateSystemCaretFor(Accessible* aAccessible)
{
  // Move the system caret so that Windows Tablet Edition and tradional ATs with
  // off-screen model can follow the caret
  ::DestroyCaret();

  HyperTextAccessible* text = aAccessible->AsHyperText();
  if (!text)
    return;

  nsIWidget* widget = nullptr;
  LayoutDeviceIntRect caretRect = text->GetCaretRect(&widget);

  if (!widget) {
    return;
  }

  HWND caretWnd = reinterpret_cast<HWND>(widget->GetNativeData(NS_NATIVE_WINDOW));
  UpdateSystemCaretFor(caretWnd, caretRect);
}

/* static */ void
AccessibleWrap::UpdateSystemCaretFor(ProxyAccessible* aProxy,
                                     const LayoutDeviceIntRect& aCaretRect)
{
  ::DestroyCaret();

  // The HWND should be the real widget HWND, not an emulated HWND.
  // We get the HWND from the proxy's outer doc to bypass window emulation.
  Accessible* outerDoc = aProxy->OuterDocOfRemoteBrowser();
  UpdateSystemCaretFor(GetHWNDFor(outerDoc), aCaretRect);
}

/* static */ void
AccessibleWrap::UpdateSystemCaretFor(HWND aCaretWnd,
                                     const LayoutDeviceIntRect& aCaretRect)
{
  if (!aCaretWnd || aCaretRect.IsEmpty()) {
    return;
  }

  // Create invisible bitmap for caret, otherwise its appearance interferes
  // with Gecko caret
  nsAutoBitmap caretBitMap(CreateBitmap(1, aCaretRect.height, 1, 1, nullptr));
  if (::CreateCaret(aCaretWnd, caretBitMap, 1, aCaretRect.height)) {  // Also destroys the last caret
    ::ShowCaret(aCaretWnd);
    RECT windowRect;
    ::GetWindowRect(aCaretWnd, &windowRect);
    ::SetCaretPos(aCaretRect.x - windowRect.left, aCaretRect.y - windowRect.top);
  }
}

ITypeInfo*
AccessibleWrap::GetTI(LCID lcid)
{
  if (gTypeInfo)
    return gTypeInfo;

  ITypeLib *typeLib = nullptr;
  HRESULT hr = LoadRegTypeLib(LIBID_Accessibility, 1, 0, lcid, &typeLib);
  if (FAILED(hr))
    return nullptr;

  hr = typeLib->GetTypeInfoOfGuid(IID_IAccessible, &gTypeInfo);
  typeLib->Release();

  if (FAILED(hr))
    return nullptr;

  return gTypeInfo;
}

/* static */
uint32_t
AccessibleWrap::GetContentProcessIdFor(dom::ContentParentId aIPCContentId)
{
  return sIDGen.GetContentProcessIDFor(aIPCContentId);
}

/* static */
void
AccessibleWrap::ReleaseContentProcessIdFor(dom::ContentParentId aIPCContentId)
{
  sIDGen.ReleaseContentProcessIDFor(aIPCContentId);
}

/* static */
void
AccessibleWrap::SetHandlerControl(DWORD aPid, RefPtr<IHandlerControl> aCtrl)
{
  MOZ_ASSERT(XRE_IsParentProcess() && NS_IsMainThread());

  if (!sHandlerControllers) {
    sHandlerControllers = new nsTArray<HandlerControllerData>();
    ClearOnShutdown(&sHandlerControllers);
  }

  HandlerControllerData ctrlData(aPid, Move(aCtrl));
  if (sHandlerControllers->Contains(ctrlData)) {
    return;
  }

  sHandlerControllers->AppendElement(Move(ctrlData));
}

/* static */
void
AccessibleWrap::InvalidateHandlers()
{
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

    ASYNC_INVOKER_FOR(IHandlerControl) invoker(controller.mCtrl,
                                               Some(controller.mIsProxy));

    HRESULT hr = ASYNC_INVOKE(invoker, Invalidate);

    if (hr == CO_E_OBJNOTCONNECTED || hr == kErrorServerDied) {
      sHandlerControllers->RemoveElement(controller);
    } else {
      NS_WARN_IF(FAILED(hr));
    }
  }
}

bool
AccessibleWrap::DispatchTextChangeToHandler(bool aIsInsert,
                                            const nsString& aText,
                                            int32_t aStart, uint32_t aLen)
{
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  if (!sHandlerControllers || sHandlerControllers->IsEmpty()) {
    return false;
  }

  HWND hwnd = GetHWNDFor(this);
  MOZ_ASSERT(hwnd);
  if (!hwnd) {
    return false;
  }

  long msaaId = GetChildIDFor(this);

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
                             aStart, static_cast<long>(aLen)};

  ASYNC_INVOKER_FOR(IHandlerControl) invoker(controller.mCtrl,
                                             Some(controller.mIsProxy));

  HRESULT hr = ASYNC_INVOKE(invoker, OnTextChange, PtrToLong(hwnd), msaaId,
                            isInsert, &textSegment);

  ::SysFreeString(textSegment.text);

  return SUCCEEDED(hr);
}

/* static */ void
AccessibleWrap::AssignChildIDTo(NotNull<sdnAccessible*> aSdnAcc)
{
  aSdnAcc->SetUniqueID(sIDGen.GetID());
}

/* static */ void
AccessibleWrap::ReleaseChildID(NotNull<sdnAccessible*> aSdnAcc)
{
  sIDGen.ReleaseID(aSdnAcc);
}
