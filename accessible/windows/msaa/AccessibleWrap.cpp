/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessibleWrap.h"
#include "Accessible-inl.h"

#include "Compatibility.h"
#include "DocAccessible-inl.h"
#include "mozilla/a11y/DocAccessibleChild.h"
#include "mozilla/a11y/DocAccessibleParent.h"
#include "EnumVariant.h"
#include "nsAccUtils.h"
#include "nsCoreUtils.h"
#include "nsIAccessibleEvent.h"
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
#include "nsIServiceManager.h"
#include "nsNameSpaceManager.h"
#include "nsTextFormatter.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsEventMap.h"
#include "nsArrayUtils.h"
#include "mozilla/Preferences.h"

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

#ifdef _WIN64
IDSet AccessibleWrap::sIDGen;
#endif

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
#ifdef _WIN64
  if (mID != kNoID)
    sIDGen.ReleaseID(mID);
#endif
}

ITypeInfo* AccessibleWrap::gTypeInfo = nullptr;

NS_IMPL_ISUPPORTS_INHERITED0(AccessibleWrap, Accessible)

void
AccessibleWrap::Shutdown()
{
  if (mID != kNoID) {
    auto doc = static_cast<DocAccessibleWrap*>(mDoc);
    MOZ_ASSERT(doc);
    if (doc) {
      doc->RemoveID(mID);
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
  A11Y_TRYBLOCK_BEGIN

  if (!ppv)
    return E_INVALIDARG;

  *ppv = nullptr;

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

    *ppv = static_cast<ISimpleDOMNode*>(new sdnAccessible(GetNode()));
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

  if (nullptr == *ppv)
    return E_NOINTERFACE;

  (reinterpret_cast<IUnknown*>(*ppv))->AddRef();
  return S_OK;

  A11Y_TRYBLOCK_END
}

//-----------------------------------------------------
// IAccessible methods
//-----------------------------------------------------

STDMETHODIMP
AccessibleWrap::get_accParent( IDispatch __RPC_FAR *__RPC_FAR *ppdispParent)
{
  A11Y_TRYBLOCK_BEGIN

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

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleWrap::get_accChildCount( long __RPC_FAR *pcountChildren)
{
  A11Y_TRYBLOCK_BEGIN

  if (!pcountChildren)
    return E_INVALIDARG;

  *pcountChildren = 0;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  if (nsAccUtils::MustPrune(this))
    return S_OK;

  *pcountChildren = ChildCount();
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleWrap::get_accChild(
      /* [in] */ VARIANT varChild,
      /* [retval][out] */ IDispatch __RPC_FAR *__RPC_FAR *ppdispChild)
{
  A11Y_TRYBLOCK_BEGIN

  if (!ppdispChild)
    return E_INVALIDARG;

  *ppdispChild = nullptr;
  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  // IAccessible::accChild is used to return this accessible or child accessible
  // at the given index or to get an accessible by child ID in the case of
  // document accessible (it's handled by overriden GetXPAccessibleFor method
  // on the document accessible). The getting an accessible by child ID is used
  // by AccessibleObjectFromEvent() called by AT when AT handles our MSAA event.
  Accessible* child = GetXPAccessibleFor(varChild);
  if (!child)
    return E_INVALIDARG;

  if (child->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  *ppdispChild = NativeAccessible(child);
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleWrap::get_accName(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszName)
{
  A11Y_TRYBLOCK_BEGIN

  if (!pszName)
    return E_INVALIDARG;

  *pszName = nullptr;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  Accessible* xpAccessible = GetXPAccessibleFor(varChild);
  if (!xpAccessible)
    return E_INVALIDARG;

  if (xpAccessible->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsAutoString name;
  xpAccessible->Name(name);

  // The name was not provided, e.g. no alt attribute for an image. A screen
  // reader may choose to invent its own accessible name, e.g. from an image src
  // attribute. Refer to eNoNameOnPurpose return value.
  if (name.IsVoid())
    return S_FALSE;

  *pszName = ::SysAllocStringLen(name.get(), name.Length());
  if (!*pszName)
    return E_OUTOFMEMORY;
  return S_OK;

  A11Y_TRYBLOCK_END
}


STDMETHODIMP
AccessibleWrap::get_accValue(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszValue)
{
  A11Y_TRYBLOCK_BEGIN

  if (!pszValue)
    return E_INVALIDARG;

  *pszValue = nullptr;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  Accessible* xpAccessible = GetXPAccessibleFor(varChild);
  if (!xpAccessible)
    return E_INVALIDARG;

  if (xpAccessible->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsAutoString value;
  xpAccessible->Value(value);

  // See bug 438784: need to expose URL on doc's value attribute. For this,
  // reverting part of fix for bug 425693 to make this MSAA method behave
  // IAccessible2-style.
  if (value.IsEmpty())
    return S_FALSE;

  *pszValue = ::SysAllocStringLen(value.get(), value.Length());
  if (!*pszValue)
    return E_OUTOFMEMORY;
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleWrap::get_accDescription(VARIANT varChild,
                                   BSTR __RPC_FAR *pszDescription)
{
  A11Y_TRYBLOCK_BEGIN

  if (!pszDescription)
    return E_INVALIDARG;

  *pszDescription = nullptr;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  Accessible* xpAccessible = GetXPAccessibleFor(varChild);
  if (!xpAccessible)
    return E_INVALIDARG;

  if (xpAccessible->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsAutoString description;
  xpAccessible->Description(description);

  *pszDescription = ::SysAllocStringLen(description.get(),
                                        description.Length());
  return *pszDescription ? S_OK : E_OUTOFMEMORY;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleWrap::get_accRole(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ VARIANT __RPC_FAR *pvarRole)
{
  A11Y_TRYBLOCK_BEGIN

  if (!pvarRole)
    return E_INVALIDARG;

  VariantInit(pvarRole);

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  Accessible* xpAccessible = GetXPAccessibleFor(varChild);
  if (!xpAccessible)
    return E_INVALIDARG;

  if (xpAccessible->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  a11y::role geckoRole;
#ifdef DEBUG
  NS_ASSERTION(nsAccUtils::IsTextInterfaceSupportCorrect(xpAccessible),
               "Does not support Text when it should");
#endif

  geckoRole = xpAccessible->Role();

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
  nsIContent *content = xpAccessible->GetContent();
  if (!content)
    return E_FAIL;

  if (content->IsElement()) {
    nsAutoString roleString;
    if (msaaRole != ROLE_SYSTEM_CLIENT &&
        !content->GetAttr(kNameSpaceID_None, nsGkAtoms::role, roleString)) {
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

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleWrap::get_accState(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ VARIANT __RPC_FAR *pvarState)
{
  A11Y_TRYBLOCK_BEGIN

  if (!pvarState)
    return E_INVALIDARG;

  VariantInit(pvarState);
  pvarState->vt = VT_I4;
  pvarState->lVal = 0;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  Accessible* xpAccessible = GetXPAccessibleFor(varChild);
  if (!xpAccessible)
    return E_INVALIDARG;

  if (xpAccessible->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

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

  A11Y_TRYBLOCK_END
}


STDMETHODIMP
AccessibleWrap::get_accHelp(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszHelp)
{
  A11Y_TRYBLOCK_BEGIN

  if (!pszHelp)
    return E_INVALIDARG;

  *pszHelp = nullptr;
  return S_FALSE;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleWrap::get_accHelpTopic(
      /* [out] */ BSTR __RPC_FAR *pszHelpFile,
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ long __RPC_FAR *pidTopic)
{
  A11Y_TRYBLOCK_BEGIN

  if (!pszHelpFile || !pidTopic)
    return E_INVALIDARG;

  *pszHelpFile = nullptr;
  *pidTopic = 0;
  return S_FALSE;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleWrap::get_accKeyboardShortcut(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszKeyboardShortcut)
{
  A11Y_TRYBLOCK_BEGIN

  if (!pszKeyboardShortcut)
    return E_INVALIDARG;
  *pszKeyboardShortcut = nullptr;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  Accessible* acc = GetXPAccessibleFor(varChild);
  if (!acc)
    return E_INVALIDARG;

  if (acc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  KeyBinding keyBinding = acc->AccessKey();
  if (keyBinding.IsEmpty())
    keyBinding = acc->KeyboardShortcut();

  nsAutoString shortcut;
  keyBinding.ToString(shortcut);

  *pszKeyboardShortcut = ::SysAllocStringLen(shortcut.get(),
                                             shortcut.Length());
  return *pszKeyboardShortcut ? S_OK : E_OUTOFMEMORY;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleWrap::get_accFocus(
      /* [retval][out] */ VARIANT __RPC_FAR *pvarChild)
{
  A11Y_TRYBLOCK_BEGIN

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

  A11Y_TRYBLOCK_END
}

/**
 * This helper class implements IEnumVARIANT for a nsTArray containing
 * accessible objects.
 */
class AccessibleEnumerator final : public IEnumVARIANT
{
public:
  AccessibleEnumerator(const nsTArray<Accessible*>& aArray) :
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
  A11Y_TRYBLOCK_BEGIN

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

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleEnumerator::Next(unsigned long celt, VARIANT FAR* rgvar, unsigned long FAR* pceltFetched)
{
  A11Y_TRYBLOCK_BEGIN

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

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleEnumerator::Clone(IEnumVARIANT FAR* FAR* ppenum)
{
  A11Y_TRYBLOCK_BEGIN

  *ppenum = new AccessibleEnumerator(*this);
  if (!*ppenum)
    return E_OUTOFMEMORY;
  NS_ADDREF(*ppenum);
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleEnumerator::Skip(unsigned long celt)
{
  A11Y_TRYBLOCK_BEGIN

  uint32_t length = mArray.Length();
  // Check if we can skip the requested number of elements
  if (celt > length - mCurIndex) {
    mCurIndex = length;
    return S_FALSE;
  }
  mCurIndex += celt;
  return S_OK;

  A11Y_TRYBLOCK_END
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
  A11Y_TRYBLOCK_BEGIN

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

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleWrap::get_accDefaultAction(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszDefaultAction)
{
  A11Y_TRYBLOCK_BEGIN

  if (!pszDefaultAction)
    return E_INVALIDARG;

  *pszDefaultAction = nullptr;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  Accessible* xpAccessible = GetXPAccessibleFor(varChild);
  if (!xpAccessible)
    return E_INVALIDARG;

  if (xpAccessible->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsAutoString defaultAction;
  xpAccessible->ActionNameAt(0, defaultAction);

  *pszDefaultAction = ::SysAllocStringLen(defaultAction.get(),
                                          defaultAction.Length());
  return *pszDefaultAction ? S_OK : E_OUTOFMEMORY;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleWrap::accSelect(
      /* [in] */ long flagsSelect,
      /* [optional][in] */ VARIANT varChild)
{
  A11Y_TRYBLOCK_BEGIN

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  // currently only handle focus and selection
  Accessible* xpAccessible = GetXPAccessibleFor(varChild);
  if (!xpAccessible)
    return E_INVALIDARG;

  if (xpAccessible->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  if (flagsSelect & SELFLAG_TAKEFOCUS) {
    if (XRE_IsContentProcess()) {
      // In this case we might have been invoked while the IPC MessageChannel is
      // waiting on a sync reply. We cannot dispatch additional IPC while that
      // is happening, so we dispatch TakeFocus from the main thread to
      // guarantee that we are outside any IPC.
      nsCOMPtr<nsIRunnable> runnable =
        mozilla::NewRunnableMethod(xpAccessible, &Accessible::TakeFocus);
      NS_DispatchToMainThread(runnable, NS_DISPATCH_NORMAL);
      return S_OK;
    }
    xpAccessible->TakeFocus();
    return S_OK;
  }

  if (flagsSelect & SELFLAG_TAKESELECTION) {
    xpAccessible->TakeSelection();
    return S_OK;
  }

  if (flagsSelect & SELFLAG_ADDSELECTION) {
    xpAccessible->SetSelected(true);
    return S_OK;
  }

  if (flagsSelect & SELFLAG_REMOVESELECTION) {
    xpAccessible->SetSelected(false);
    return S_OK;
  }

  return E_FAIL;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleWrap::accLocation(
      /* [out] */ long __RPC_FAR *pxLeft,
      /* [out] */ long __RPC_FAR *pyTop,
      /* [out] */ long __RPC_FAR *pcxWidth,
      /* [out] */ long __RPC_FAR *pcyHeight,
      /* [optional][in] */ VARIANT varChild)
{
  A11Y_TRYBLOCK_BEGIN

  if (!pxLeft || !pyTop || !pcxWidth || !pcyHeight)
    return E_INVALIDARG;

  *pxLeft = 0;
  *pyTop = 0;
  *pcxWidth = 0;
  *pcyHeight = 0;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  Accessible* xpAccessible = GetXPAccessibleFor(varChild);
  if (!xpAccessible)
    return E_INVALIDARG;

  if (xpAccessible->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsIntRect rect = xpAccessible->Bounds();

  *pxLeft = rect.x;
  *pyTop = rect.y;
  *pcxWidth = rect.width;
  *pcyHeight = rect.height;
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleWrap::accNavigate(
      /* [in] */ long navDir,
      /* [optional][in] */ VARIANT varStart,
      /* [retval][out] */ VARIANT __RPC_FAR *pvarEndUpAt)
{
  A11Y_TRYBLOCK_BEGIN

  if (!pvarEndUpAt)
    return E_INVALIDARG;

  VariantInit(pvarEndUpAt);

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  Accessible* accessible = GetXPAccessibleFor(varStart);
  if (!accessible)
    return E_INVALIDARG;

  if (accessible->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  Accessible* navAccessible = nullptr;
  Maybe<RelationType> xpRelation;

#define RELATIONTYPE(geckoType, stringType, atkType, msaaType, ia2Type) \
  case msaaType: \
    xpRelation.emplace(RelationType::geckoType); \
    break;

  switch(navDir) {
    case NAVDIR_FIRSTCHILD:
      if (accessible->IsProxy()) {
        if (!accessible->Proxy()->MustPruneChildren()) {
          navAccessible = WrapperFor(accessible->Proxy()->FirstChild());
        }
      } else {
        if (!nsAccUtils::MustPrune(accessible))
          navAccessible = accessible->FirstChild();
      }
      break;
    case NAVDIR_LASTCHILD:
      if (accessible->IsProxy()) {
        if (!accessible->Proxy()->MustPruneChildren()) {
          navAccessible = WrapperFor(accessible->Proxy()->LastChild());
        }
      } else {
        if (!nsAccUtils::MustPrune(accessible))
          navAccessible = accessible->LastChild();
      }
      break;
    case NAVDIR_NEXT:
      navAccessible = accessible->IsProxy()
        ? WrapperFor(accessible->Proxy()->NextSibling())
        : accessible->NextSibling();
      break;
    case NAVDIR_PREVIOUS:
      navAccessible = accessible->IsProxy()
        ? WrapperFor(accessible->Proxy()->PrevSibling())
        : accessible->PrevSibling();
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

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleWrap::accHitTest(
      /* [in] */ long xLeft,
      /* [in] */ long yTop,
      /* [retval][out] */ VARIANT __RPC_FAR *pvarChild)
{
  A11Y_TRYBLOCK_BEGIN

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

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleWrap::accDoDefaultAction(
      /* [optional][in] */ VARIANT varChild)
{
  A11Y_TRYBLOCK_BEGIN

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  Accessible* xpAccessible = GetXPAccessibleFor(varChild);
  if (!xpAccessible)
    return E_INVALIDARG;

  if (xpAccessible->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  return xpAccessible->DoAction(0) ? S_OK : E_INVALIDARG;

  A11Y_TRYBLOCK_END
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
  MOZ_ASSERT(XRE_IsContentProcess());
  mID = aID;
  DocAccessibleWrap* doc = static_cast<DocAccessibleWrap*>(Document());
  DebugOnly<AccessibleWrap*> checkAcc = nullptr;
  MOZ_ASSERT(!(checkAcc = doc->GetAccessibleByID(aID)) ||
             checkAcc->GetExistingID() == aID);
  doc->AddID(aID, this);
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

  // Content should use mID which has been generated by the chrome process.
  if (XRE_IsContentProcess() && !aAccessible->IsApplication()) {
    const uint32_t id = static_cast<AccessibleWrap*>(aAccessible)->mID;
    MOZ_ASSERT(id != kNoID);
    return id;
  }

#ifdef _WIN64
  if (!aAccessible->Document() && !aAccessible->IsProxy())
    return 0;

  uint32_t* id = & static_cast<AccessibleWrap*>(aAccessible)->mID;
  if (*id != kNoID)
    return *id;

  *id = sIDGen.GetID();

  if (aAccessible->IsProxy()) {
    DocProxyAccessibleWrap* doc =
      static_cast<AccessibleWrap*>(aAccessible)->DocProxyWrapper();
    doc->AddID(*id, static_cast<AccessibleWrap*>(aAccessible));
  } else {
    DocAccessibleWrap* doc =
      static_cast<DocAccessibleWrap*>(aAccessible->Document());
    doc->AddID(*id, static_cast<AccessibleWrap*>(aAccessible));
  }

  return *id;
#else
  int32_t id = - reinterpret_cast<intptr_t>(aAccessible);
  if (aAccessible->IsProxy()) {
    DocProxyAccessibleWrap* doc =
      static_cast<AccessibleWrap*>(aAccessible)->DocProxyWrapper();
    doc->AddID(id, static_cast<AccessibleWrap*>(aAccessible));
  }

  return id;
#endif
}

HWND
AccessibleWrap::GetHWNDFor(Accessible* aAccessible)
{
  if (!aAccessible) {
    return nullptr;
  }

  // Accessibles in child processes are said to have the HWND of the window
  // their tab is within.  Popups are always in the parent process, and so
  // never proxied, which means this is basically correct.
  if (aAccessible->IsProxy()) {
    ProxyAccessible* proxy = aAccessible->Proxy();
    if (!proxy) {
      return nullptr;
    }

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

static AccessibleWrap*
GetProxiedAccessibleInSubtree(const DocAccessibleParent* aDoc, uint32_t aID)
{
  auto wrapper = static_cast<DocProxyAccessibleWrap*>(WrapperFor(aDoc));
  AccessibleWrap* child = wrapper->GetAccessibleByID(aID);
  if (child) {
    return child;
  }

  size_t childDocs = aDoc->ChildDocCount();
  for (size_t i = 0; i < childDocs; i++) {
    const DocAccessibleParent* childDoc = aDoc->ChildDocAt(i);
    child = GetProxiedAccessibleInSubtree(childDoc, aID);
    if (child) {
      return child;
    }
  }

  return nullptr;
}

Accessible*
AccessibleWrap::GetXPAccessibleFor(const VARIANT& aVarChild)
{
  if (aVarChild.vt != VT_I4)
    return nullptr;

  // if its us real easy - this seems to always be the case
  if (aVarChild.lVal == CHILDID_SELF)
    return this;

  if (IsProxy() ? Proxy()->MustPruneChildren() : nsAccUtils::MustPrune(this)) {
    return nullptr;
  }

  if (aVarChild.lVal > 0) {
    // Gecko child indices are 0-based in contrast to indices used in MSAA.
    if (IsProxy()) {
      if (static_cast<uint32_t>(aVarChild.lVal) > Proxy()->ChildrenCount()) {
        return nullptr;
      }

      return WrapperFor(Proxy()->ChildAt(aVarChild.lVal - 1));
    } else {
      return GetChildAt(aVarChild.lVal - 1);
    }
  }

  // If lVal negative then it is treated as child ID and we should look for
  // accessible through whole accessible subtree including subdocuments.
  // Otherwise we treat lVal as index in parent.
  // Convert child ID to unique ID.
  // First handle the case that both this accessible and the id'd one are in
  // this process.
  if (!IsProxy()) {
    void* uniqueID = reinterpret_cast<void*>(intptr_t(-aVarChild.lVal));

    DocAccessible* document = Document();
    Accessible* child =
#ifdef _WIN64
      GetAccessibleInSubtree(document, static_cast<uint32_t>(aVarChild.lVal));
#else
      XRE_IsContentProcess() ?
        GetAccessibleInSubtree(document, static_cast<uint32_t>(aVarChild.lVal)) :
        document->GetAccessibleByUniqueIDInSubtree(uniqueID);
#endif

    // If it is a document then just return an accessible.
    if (child && IsDoc())
      return child;

    // Otherwise check whether the accessible is a child (this path works for
    // ARIA documents and popups).
    Accessible* parent = child;
    while (parent && parent != document) {
      if (parent == this)
        return child;

      parent = parent->Parent();
    }
  }

  // Now see about the case that both this accessible and the target one are
  // proxied.
  uint32_t id = aVarChild.lVal;
  if (IsProxy()) {
    DocAccessibleParent* proxyDoc = Proxy()->Document();
    AccessibleWrap* wrapper = GetProxiedAccessibleInSubtree(proxyDoc, id);
    if (!wrapper)
      return nullptr;

    MOZ_ASSERT(wrapper->IsProxy());

    if (proxyDoc == this->Proxy()) {
      return wrapper;
    }

    ProxyAccessible* parent = wrapper->Proxy();
    while (parent && parent != proxyDoc) {
      if (parent == this->Proxy()) {
        return wrapper;
      }

      parent = parent->Parent();
    }

    return nullptr;
  }

  // Finally we need to handle the case that this accessible is in the main
  // process, but the target is proxied.  This is the case when the target
  // accessible is in a child document of this one.
  DocAccessibleParent* proxyDoc = nullptr;
  DocAccessible* doc = Document();
  const nsTArray<DocAccessibleParent*>* remoteDocs =
    DocManager::TopLevelRemoteDocs();
  if (!remoteDocs) {
    return nullptr;
  }

  size_t docCount = remoteDocs->Length();
  for (size_t i = 0; i < docCount; i++) {
    Accessible* outerDoc = remoteDocs->ElementAt(i)->OuterDocOfRemoteBrowser();
    if (!outerDoc) {
      continue;
    }

    if (outerDoc->Document() != doc) {
      continue;
    }

    if (doc == this) {
      AccessibleWrap* proxyWrapper =
        GetProxiedAccessibleInSubtree(remoteDocs->ElementAt(i), id);
      if (proxyWrapper) {
        return proxyWrapper;
      }

      continue;
    }

    Accessible* parent = outerDoc;
    while (parent && parent != doc) {
      if (parent == this) {
        AccessibleWrap* proxyWrapper =
          GetProxiedAccessibleInSubtree(remoteDocs->ElementAt(i), id);
        if (proxyWrapper) {
          return proxyWrapper;
        }
      }

      parent = parent->Parent();
    }
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
  HWND caretWnd;
  if (caretRect.IsEmpty() || !(caretWnd = (HWND)widget->GetNativeData(NS_NATIVE_WINDOW))) {
    return;
  }

  // Create invisible bitmap for caret, otherwise its appearance interferes
  // with Gecko caret
  HBITMAP caretBitMap = CreateBitmap(1, caretRect.height, 1, 1, nullptr);
  if (::CreateCaret(caretWnd, caretBitMap, 1, caretRect.height)) {  // Also destroys the last caret
    ::ShowCaret(caretWnd);
    RECT windowRect;
    ::GetWindowRect(caretWnd, &windowRect);
    ::SetCaretPos(caretRect.x - windowRect.left, caretRect.y - windowRect.top);
    ::DeleteObject(caretBitMap);
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
