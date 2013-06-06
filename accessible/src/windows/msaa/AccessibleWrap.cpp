/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessibleWrap.h"
#include "Accessible-inl.h"

#include "Compatibility.h"
#include "DocAccessible-inl.h"
#include "EnumVariant.h"
#include "ia2AccessibleRelation.h"
#include "nsAccUtils.h"
#include "nsCoreUtils.h"
#include "nsIAccessibleEvent.h"
#include "nsWinUtils.h"
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
#include "nsINameSpaceManager.h"
#include "nsINodeInfo.h"
#include "nsIServiceManager.h"
#include "nsTextFormatter.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsEventMap.h"
#include "nsArrayUtils.h"
#include "mozilla/Preferences.h"

#include "Accessible2_i.c"
#include "AccessibleRole.h"
#include "AccessibleStates.h"
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

static const int32_t kIEnumVariantDisconnected = -1;

////////////////////////////////////////////////////////////////////////////////
// AccessibleWrap
////////////////////////////////////////////////////////////////////////////////

ITypeInfo* AccessibleWrap::gTypeInfo = nullptr;

NS_IMPL_ISUPPORTS_INHERITED0(AccessibleWrap, Accessible)

//-----------------------------------------------------
// IUnknown interface methods - see iunknown.h for documentation
//-----------------------------------------------------

// Microsoft COM QueryInterface
STDMETHODIMP
AccessibleWrap::QueryInterface(REFIID iid, void** ppv)
{
  A11Y_TRYBLOCK_BEGIN

  *ppv = nullptr;

  if (IID_IUnknown == iid || IID_IDispatch == iid || IID_IAccessible == iid)
    *ppv = static_cast<IAccessible*>(this);
  else if (IID_IEnumVARIANT == iid) {
    // Don't support this interface for leaf elements.
    if (!HasChildren() || nsAccUtils::MustPrune(this))
      return E_NOINTERFACE;

    *ppv = static_cast<IEnumVARIANT*>(new ChildrenEnumVariant(this));
  } else if (IID_IServiceProvider == iid)
    *ppv = new ServiceProvider(this);
  else if (IID_IAccessible2 == iid && !Compatibility::IsIA2Off())
    *ppv = static_cast<IAccessible2*>(this);
  else if (IID_ISimpleDOMNode == iid) {
    if (IsDefunct() || (!HasOwnContent() && !IsDoc()))
      return E_NOINTERFACE;

    *ppv = static_cast<ISimpleDOMNode*>(new sdnAccessible(GetNode()));
  }

  if (nullptr == *ppv) {
    HRESULT hr = ia2AccessibleComponent::QueryInterface(iid, ppv);
    if (SUCCEEDED(hr))
      return hr;
  }

  if (nullptr == *ppv) {
    HRESULT hr = ia2AccessibleHyperlink::QueryInterface(iid, ppv);
    if (SUCCEEDED(hr))
      return hr;
  }

  if (nullptr == *ppv) {
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

  *pszValue = nullptr;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  Accessible* xpAccessible = GetXPAccessibleFor(varChild);
  if (!xpAccessible)
    return E_INVALIDARG;

  if (xpAccessible->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  if (xpAccessible->NativeRole() == roles::PASSWORD_TEXT)
    return E_ACCESSDENIED;

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

  VariantInit(pvarRole);

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  Accessible* xpAccessible = GetXPAccessibleFor(varChild);
  if (!xpAccessible)
    return E_INVALIDARG;

  if (xpAccessible->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

#ifdef DEBUG
  NS_ASSERTION(nsAccUtils::IsTextInterfaceSupportCorrect(xpAccessible),
               "Does not support nsIAccessibleText when it should");
#endif

  a11y::role geckoRole = xpAccessible->Role();
  uint32_t msaaRole = 0;

#define ROLE(_geckoRole, stringRole, atkRole, macRole, \
             _msaaRole, ia2Role, nameRule) \
  case roles::_geckoRole: \
    msaaRole = _msaaRole; \
    break;

  switch (geckoRole) {
#include "RoleMap.h"
    default:
      MOZ_NOT_REACHED("Unknown role.");
  };

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
      nsIDocument * document = content->GetCurrentDoc();
      if (!document)
        return E_FAIL;

      nsINodeInfo *nodeInfo = content->NodeInfo();
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

  uint32_t msaaState = 0;
  nsAccUtils::To32States(xpAccessible->State(), &msaaState, nullptr);
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

  // VT_EMPTY:    None. This object does not have the keyboard focus itself
  //              and does not contain a child that has the keyboard focus.
  // VT_I4:       lVal is CHILDID_SELF. The object itself has the keyboard focus.
  // VT_I4:       lVal contains the child ID of the child element with the keyboard focus.
  // VT_DISPATCH: pdispVal member is the address of the IDispatch interface
  //              for the child object with the keyboard focus.
  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  VariantInit(pvarChild);

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

// This helper class implements IEnumVARIANT for a nsIArray containing nsIAccessible objects.

class AccessibleEnumerator MOZ_FINAL : public IEnumVARIANT
{
public:
  AccessibleEnumerator(nsIArray* aArray) : mArray(aArray), mCurIndex(0) { }
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
  nsCOMPtr<nsIArray> mArray;
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

  uint32_t length = 0;
  mArray->GetLength(&length);

  HRESULT hr = S_OK;

  // Can't get more elements than there are...
  if (celt > length - mCurIndex) {
    hr = S_FALSE;
    celt = length - mCurIndex;
  }

  for (uint32_t i = 0; i < celt; ++i, ++mCurIndex) {
    // Copy the elements of the array into rgvar
    nsCOMPtr<nsIAccessible> accel(do_QueryElementAt(mArray, mCurIndex));
    NS_ASSERTION(accel, "Invalid pointer in mArray");

    if (accel) {
      rgvar[i].vt = VT_DISPATCH;
      rgvar[i].pdispVal = AccessibleWrap::NativeAccessible(accel);
    }
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

  uint32_t length = 0;
  mArray->GetLength(&length);
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
  *  nsIAccessible object which implement SelectAccessible.
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

  VariantInit(pvarChildren);
  pvarChildren->vt = VT_EMPTY;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  if (IsSelect()) {
    nsCOMPtr<nsIArray> selectedItems = SelectedItems();
    if (selectedItems) {
      // 1) Create and initialize the enumeration
      nsRefPtr<AccessibleEnumerator> pEnum =
        new AccessibleEnumerator(selectedItems);

      // 2) Put the enumerator in the VARIANT
      if (!pEnum)
        return E_OUTOFMEMORY;
      pvarChildren->vt = VT_UNKNOWN;    // this must be VT_UNKNOWN for an IEnumVARIANT
      NS_ADDREF(pvarChildren->punkVal = pEnum);
    }
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

  *pszDefaultAction = nullptr;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  Accessible* xpAccessible = GetXPAccessibleFor(varChild);
  if (!xpAccessible)
    return E_INVALIDARG;

  if (xpAccessible->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsAutoString defaultAction;
  if (NS_FAILED(xpAccessible->GetActionName(0, defaultAction)))
    return E_FAIL;

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

  if (flagsSelect & (SELFLAG_TAKEFOCUS|SELFLAG_TAKESELECTION|SELFLAG_REMOVESELECTION))
  {
    if (flagsSelect & SELFLAG_TAKEFOCUS)
      xpAccessible->TakeFocus();

    if (flagsSelect & SELFLAG_TAKESELECTION)
      xpAccessible->TakeSelection();

    if (flagsSelect & SELFLAG_ADDSELECTION)
      xpAccessible->SetSelected(true);

    if (flagsSelect & SELFLAG_REMOVESELECTION)
      xpAccessible->SetSelected(false);

    if (flagsSelect & SELFLAG_EXTENDSELECTION)
      xpAccessible->ExtendSelection();

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

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  Accessible* xpAccessible = GetXPAccessibleFor(varChild);
  if (!xpAccessible)
    return E_INVALIDARG;

  if (xpAccessible->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  int32_t x, y, width, height;
  if (NS_FAILED(xpAccessible->GetBounds(&x, &y, &width, &height)))
    return E_FAIL;

  *pxLeft = x;
  *pyTop = y;
  *pcxWidth = width;
  *pcyHeight = height;
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

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  Accessible* accessible = GetXPAccessibleFor(varStart);
  if (!accessible)
    return E_INVALIDARG;

  if (accessible->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  VariantInit(pvarEndUpAt);

  Accessible* navAccessible = nullptr;
  int32_t xpRelation = -1;

  switch(navDir) {
    case NAVDIR_FIRSTCHILD:
      if (!nsAccUtils::MustPrune(accessible))
        navAccessible = accessible->FirstChild();
      break;
    case NAVDIR_LASTCHILD:
      if (!nsAccUtils::MustPrune(accessible))
        navAccessible = accessible->LastChild();
      break;
    case NAVDIR_NEXT:
      navAccessible = accessible->NextSibling();
      break;
    case NAVDIR_PREVIOUS:
      navAccessible = accessible->PrevSibling();
      break;
    case NAVDIR_DOWN:
    case NAVDIR_LEFT:
    case NAVDIR_RIGHT:
    case NAVDIR_UP:
      return E_NOTIMPL;

    // MSAA relationship extensions to accNavigate
    case NAVRELATION_CONTROLLED_BY:
      xpRelation = nsIAccessibleRelation::RELATION_CONTROLLED_BY;
      break;
    case NAVRELATION_CONTROLLER_FOR:
      xpRelation = nsIAccessibleRelation::RELATION_CONTROLLER_FOR;
      break;
    case NAVRELATION_LABEL_FOR:
      xpRelation = nsIAccessibleRelation::RELATION_LABEL_FOR;
      break;
    case NAVRELATION_LABELLED_BY:
      xpRelation = nsIAccessibleRelation::RELATION_LABELLED_BY;
      break;
    case NAVRELATION_MEMBER_OF:
      xpRelation = nsIAccessibleRelation::RELATION_MEMBER_OF;
      break;
    case NAVRELATION_NODE_CHILD_OF:
      xpRelation = nsIAccessibleRelation::RELATION_NODE_CHILD_OF;
      break;
    case NAVRELATION_FLOWS_TO:
      xpRelation = nsIAccessibleRelation::RELATION_FLOWS_TO;
      break;
    case NAVRELATION_FLOWS_FROM:
      xpRelation = nsIAccessibleRelation::RELATION_FLOWS_FROM;
      break;
    case NAVRELATION_SUBWINDOW_OF:
      xpRelation = nsIAccessibleRelation::RELATION_SUBWINDOW_OF;
      break;
    case NAVRELATION_EMBEDS:
      xpRelation = nsIAccessibleRelation::RELATION_EMBEDS;
      break;
    case NAVRELATION_EMBEDDED_BY:
      xpRelation = nsIAccessibleRelation::RELATION_EMBEDDED_BY;
      break;
    case NAVRELATION_POPUP_FOR:
      xpRelation = nsIAccessibleRelation::RELATION_POPUP_FOR;
      break;
    case NAVRELATION_PARENT_WINDOW_OF:
      xpRelation = nsIAccessibleRelation::RELATION_PARENT_WINDOW_OF;
      break;
    case NAVRELATION_DEFAULT_BUTTON:
      xpRelation = nsIAccessibleRelation::RELATION_DEFAULT_BUTTON;
      break;
    case NAVRELATION_DESCRIBED_BY:
      xpRelation = nsIAccessibleRelation::RELATION_DESCRIBED_BY;
      break;
    case NAVRELATION_DESCRIPTION_FOR:
      xpRelation = nsIAccessibleRelation::RELATION_DESCRIPTION_FOR;
      break;
    case NAVRELATION_NODE_PARENT_OF:
      xpRelation = nsIAccessibleRelation::RELATION_NODE_PARENT_OF;
      break;

    default:
      return E_INVALIDARG;
  }

  pvarEndUpAt->vt = VT_EMPTY;

  if (xpRelation >= 0) {
    Relation rel = RelationByType(xpRelation);
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

  return GetHRESULT(xpAccessible->DoAction(0));

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
// AccessibleWrap. IAccessible2

STDMETHODIMP
AccessibleWrap::get_nRelations(long *aNRelations)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aNRelations)
    return E_INVALIDARG;

  *aNRelations = 0;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  for (unsigned int idx = 0; idx < ArrayLength(sRelationTypesForIA2); idx++) {
    Relation rel = RelationByType(sRelationTypesForIA2[idx]);
    if (rel.Next())
      (*aNRelations)++;
  }
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleWrap::get_relation(long aRelationIndex,
                             IAccessibleRelation** aRelation)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aRelation)
    return E_INVALIDARG;

  *aRelation = nullptr;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  long relIdx = 0;
  for (unsigned int idx = 0; idx < ArrayLength(sRelationTypesForIA2); idx++) {
    uint32_t relType = sRelationTypesForIA2[idx];
    Relation rel = RelationByType(relType);
    nsRefPtr<ia2AccessibleRelation> ia2Relation =
      new ia2AccessibleRelation(relType, &rel);
    if (ia2Relation->HasTargets()) {
      if (relIdx == aRelationIndex) {
        ia2Relation.forget(aRelation);
        return S_OK;
      }

      relIdx++;
    }
  }

  return E_INVALIDARG;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleWrap::get_relations(long aMaxRelations,
                                IAccessibleRelation **aRelation,
                                long *aNRelations)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aRelation || !aNRelations)
    return E_INVALIDARG;

  *aNRelations = 0;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  for (unsigned int idx = 0; idx < ArrayLength(sRelationTypesForIA2) &&
       *aNRelations < aMaxRelations; idx++) {
    uint32_t relType = sRelationTypesForIA2[idx];
    Relation rel = RelationByType(relType);
    nsRefPtr<ia2AccessibleRelation> ia2Rel =
      new ia2AccessibleRelation(relType, &rel);
    if (ia2Rel->HasTargets()) {
      ia2Rel.forget(aRelation + (*aNRelations));
      (*aNRelations)++;
    }
  }
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleWrap::role(long *aRole)
{
  A11Y_TRYBLOCK_BEGIN

  *aRole = 0;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

#define ROLE(_geckoRole, stringRole, atkRole, macRole, \
             msaaRole, ia2Role, nameRule) \
  case roles::_geckoRole: \
    *aRole = ia2Role; \
    break;

  a11y::role geckoRole = Role();
  switch (geckoRole) {
#include "RoleMap.h"
    default:
      MOZ_NOT_REACHED("Unknown role.");
  };

#undef ROLE

  // Special case, if there is a ROLE_ROW inside of a ROLE_TREE_TABLE, then call
  // the IA2 role a ROLE_OUTLINEITEM.
  if (geckoRole == roles::ROW) {
    Accessible* xpParent = Parent();
    if (xpParent && xpParent->Role() == roles::TREE_TABLE)
      *aRole = ROLE_SYSTEM_OUTLINEITEM;
  }

  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleWrap::scrollTo(enum IA2ScrollType aScrollType)
{
  A11Y_TRYBLOCK_BEGIN

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsCoreUtils::ScrollTo(mDoc->PresShell(), mContent, aScrollType);
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleWrap::scrollToPoint(enum IA2CoordinateType aCoordType,
                              long aX, long aY)
{
  A11Y_TRYBLOCK_BEGIN

  if (IsDefunct())
      return CO_E_OBJNOTCONNECTED;

  uint32_t geckoCoordType = (aCoordType == IA2_COORDTYPE_SCREEN_RELATIVE) ?
    nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE :
    nsIAccessibleCoordinateType::COORDTYPE_PARENT_RELATIVE;

  nsresult rv = ScrollToPoint(geckoCoordType, aX, aY);
  return GetHRESULT(rv);

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleWrap::get_groupPosition(long *aGroupLevel,
                                  long *aSimilarItemsInGroup,
                                  long *aPositionInGroup)
{
  A11Y_TRYBLOCK_BEGIN

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  GroupPos groupPos = GroupPosition();

  // Group information for accessibles having level only (like html headings
  // elements) isn't exposed by this method. AT should look for 'level' object
  // attribute.
  if (!groupPos.setSize && !groupPos.posInSet)
    return S_FALSE;

  *aGroupLevel = groupPos.level;
  *aSimilarItemsInGroup = groupPos.setSize;
  *aPositionInGroup = groupPos.posInSet;

  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleWrap::get_states(AccessibleStates *aStates)
{
  A11Y_TRYBLOCK_BEGIN

  *aStates = 0;

  // XXX: bug 344674 should come with better approach that we have here.

  uint64_t state = State();

  if (state & states::INVALID)
    *aStates |= IA2_STATE_INVALID_ENTRY;
  if (state & states::REQUIRED)
    *aStates |= IA2_STATE_REQUIRED;

  // The following IA2 states are not supported by Gecko
  // IA2_STATE_ARMED
  // IA2_STATE_MANAGES_DESCENDANTS
  // IA2_STATE_ICONIFIED
  // IA2_STATE_INVALID // This is not a state, it is the absence of a state

  if (state & states::ACTIVE)
    *aStates |= IA2_STATE_ACTIVE;
  if (state & states::DEFUNCT)
    *aStates |= IA2_STATE_DEFUNCT;
  if (state & states::EDITABLE)
    *aStates |= IA2_STATE_EDITABLE;
  if (state & states::HORIZONTAL)
    *aStates |= IA2_STATE_HORIZONTAL;
  if (state & states::MODAL)
    *aStates |= IA2_STATE_MODAL;
  if (state & states::MULTI_LINE)
    *aStates |= IA2_STATE_MULTI_LINE;
  if (state & states::OPAQUE1)
    *aStates |= IA2_STATE_OPAQUE;
  if (state & states::SELECTABLE_TEXT)
    *aStates |= IA2_STATE_SELECTABLE_TEXT;
  if (state & states::SINGLE_LINE)
    *aStates |= IA2_STATE_SINGLE_LINE;
  if (state & states::STALE)
    *aStates |= IA2_STATE_STALE;
  if (state & states::SUPPORTS_AUTOCOMPLETION)
    *aStates |= IA2_STATE_SUPPORTS_AUTOCOMPLETION;
  if (state & states::TRANSIENT)
    *aStates |= IA2_STATE_TRANSIENT;
  if (state & states::VERTICAL)
    *aStates |= IA2_STATE_VERTICAL;
  if (state & states::CHECKED)
    *aStates |= IA2_STATE_CHECKABLE;
  if (state & states::PINNED)
    *aStates |= IA2_STATE_PINNED;

  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleWrap::get_extendedRole(BSTR *aExtendedRole)
{
  A11Y_TRYBLOCK_BEGIN

  *aExtendedRole = nullptr;
  return E_NOTIMPL;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleWrap::get_localizedExtendedRole(BSTR *aLocalizedExtendedRole)
{
  A11Y_TRYBLOCK_BEGIN

  *aLocalizedExtendedRole = nullptr;
  return E_NOTIMPL;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleWrap::get_nExtendedStates(long *aNExtendedStates)
{
  A11Y_TRYBLOCK_BEGIN

  *aNExtendedStates = 0;
  return E_NOTIMPL;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleWrap::get_extendedStates(long aMaxExtendedStates,
                                   BSTR **aExtendedStates,
                                   long *aNExtendedStates)
{
  A11Y_TRYBLOCK_BEGIN

  *aExtendedStates = nullptr;
  *aNExtendedStates = 0;
  return E_NOTIMPL;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleWrap::get_localizedExtendedStates(long aMaxLocalizedExtendedStates,
                                            BSTR** aLocalizedExtendedStates,
                                            long* aNLocalizedExtendedStates)
{
  A11Y_TRYBLOCK_BEGIN

  *aLocalizedExtendedStates = nullptr;
  *aNLocalizedExtendedStates = 0;
  return E_NOTIMPL;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleWrap::get_uniqueID(long *uniqueID)
{
  A11Y_TRYBLOCK_BEGIN

  *uniqueID = - reinterpret_cast<intptr_t>(UniqueID());
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleWrap::get_windowHandle(HWND *aWindowHandle)
{
  A11Y_TRYBLOCK_BEGIN

  *aWindowHandle = 0;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  *aWindowHandle = GetHWNDFor(this);
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleWrap::get_indexInParent(long *aIndexInParent)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aIndexInParent)
    return E_INVALIDARG;

  *aIndexInParent = -1;
  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  *aIndexInParent = IndexInParent();
  if (*aIndexInParent == -1)
    return S_FALSE;

  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleWrap::get_locale(IA2Locale *aLocale)
{
  A11Y_TRYBLOCK_BEGIN

  // Language codes consist of a primary code and a possibly empty series of
  // subcodes: language-code = primary-code ( "-" subcode )*
  // Two-letter primary codes are reserved for [ISO639] language abbreviations.
  // Any two-letter subcode is understood to be a [ISO3166] country code.

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsAutoString lang;
  Language(lang);

  // If primary code consists from two letters then expose it as language.
  int32_t offset = lang.FindChar('-', 0);
  if (offset == -1) {
    if (lang.Length() == 2) {
      aLocale->language = ::SysAllocString(lang.get());
      return S_OK;
    }
  } else if (offset == 2) {
    aLocale->language = ::SysAllocStringLen(lang.get(), 2);

    // If the first subcode consists from two letters then expose it as
    // country.
    offset = lang.FindChar('-', 3);
    if (offset == -1) {
      if (lang.Length() == 5) {
        aLocale->country = ::SysAllocString(lang.get() + 3);
        return S_OK;
      }
    } else if (offset == 5) {
      aLocale->country = ::SysAllocStringLen(lang.get() + 3, 2);
    }
  }

  // Expose as a string if primary code or subcode cannot point to language or
  // country abbreviations or if there are more than one subcode.
  aLocale->variant = ::SysAllocString(lang.get());
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
AccessibleWrap::get_attributes(BSTR *aAttributes)
{
  A11Y_TRYBLOCK_BEGIN

  // The format is name:value;name:value; with \ for escaping these
  // characters ":;=,\".
  *aAttributes = nullptr;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsCOMPtr<nsIPersistentProperties> attributes = Attributes();
  return ConvertToIA2Attributes(attributes, aAttributes);

  A11Y_TRYBLOCK_END
}

////////////////////////////////////////////////////////////////////////////////
// IDispatch

STDMETHODIMP
AccessibleWrap::GetTypeInfoCount(UINT *pctinfo)
{
  *pctinfo = 1;
  return S_OK;
}

STDMETHODIMP
AccessibleWrap::GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo **ppTInfo)
{
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


// nsIAccessible method
NS_IMETHODIMP
AccessibleWrap::GetNativeInterface(void **aOutAccessible)
{
  *aOutAccessible = static_cast<IAccessible*>(this);
  NS_ADDREF_THIS();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// Accessible

nsresult
AccessibleWrap::HandleAccEvent(AccEvent* aEvent)
{
  nsresult rv = Accessible::HandleAccEvent(aEvent);
  NS_ENSURE_SUCCESS(rv, rv);

  // Don't fire native MSAA events or mess with the system caret
  // when running in metro mode. This confuses input focus tracking
  // in metro's UIA implementation.
  if (XRE_GetWindowsEnvironment() == WindowsEnvironmentType_Metro) {
    return NS_OK;
  }

  uint32_t eventType = aEvent->GetEventType();

  MOZ_STATIC_ASSERT(sizeof(gWinEventMap)/sizeof(gWinEventMap[0]) == nsIAccessibleEvent::EVENT_LAST_ENTRY,
                    "MSAA event map skewed");

  NS_ENSURE_TRUE(eventType > 0 && eventType < ArrayLength(gWinEventMap), NS_ERROR_FAILURE);

  uint32_t winEvent = gWinEventMap[eventType];
  if (!winEvent)
    return NS_OK;

  // Means we're not active.
  NS_ENSURE_TRUE(!IsDefunct(), NS_ERROR_FAILURE);

  Accessible* accessible = aEvent->GetAccessible();
  if (!accessible)
    return NS_OK;

  if (eventType == nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED ||
      eventType == nsIAccessibleEvent::EVENT_FOCUS) {
    UpdateSystemCaret();
  }

  int32_t childID = GetChildIDFor(accessible); // get the id for the accessible
  if (!childID)
    return NS_OK; // Can't fire an event without a child ID

  HWND hWnd = GetHWNDFor(accessible);
  NS_ENSURE_TRUE(hWnd, NS_ERROR_FAILURE);

  nsAutoString tag;
  nsAutoCString id;
  nsIContent* cnt = accessible->GetContent();
  if (cnt) {
    cnt->Tag()->ToString(tag);
    nsIAtom* aid = cnt->GetID();
    if (aid)
      aid->ToUTF8String(id);
  }

#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::ePlatforms)) {
    printf("\n\nMSAA event: event: %d, target: %s@id='%s', childid: %d, hwnd: %d\n\n",
           eventType, NS_ConvertUTF16toUTF8(tag).get(), id.get(),
           childID, hWnd);
  }
#endif

  // Fire MSAA event for client area window.
  ::NotifyWinEvent(winEvent, hWnd, OBJID_CLIENT, childID);

  // JAWS announces collapsed combobox navigation based on focus events.
  if (Compatibility::IsJAWS()) {
    if (eventType == nsIAccessibleEvent::EVENT_SELECTION &&
      accessible->Role() == roles::COMBOBOX_OPTION) {
      ::NotifyWinEvent(EVENT_OBJECT_FOCUS, hWnd, OBJID_CLIENT, childID);
    }
  }

  return NS_OK;
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

  // Yes, this means we're only compatibible with 32 bit
  // MSAA is only available for 32 bit windows, so it's okay
  // XXX: bug 606080
  return aAccessible ? - NS_PTR_TO_INT32(aAccessible->UniqueID()) : 0;
}

HWND
AccessibleWrap::GetHWNDFor(Accessible* aAccessible)
{
  if (aAccessible) {
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
  return nullptr;
}

HRESULT
AccessibleWrap::ConvertToIA2Attributes(nsIPersistentProperties *aAttributes,
                                       BSTR *aIA2Attributes)
{
  *aIA2Attributes = nullptr;

  // The format is name:value;name:value; with \ for escaping these
  // characters ":;=,\".

  if (!aAttributes)
    return S_FALSE;

  nsCOMPtr<nsISimpleEnumerator> propEnum;
  aAttributes->Enumerate(getter_AddRefs(propEnum));
  if (!propEnum)
    return E_FAIL;

  nsAutoString strAttrs;

  const char kCharsToEscape[] = ":;=,\\";

  bool hasMore = false;
  while (NS_SUCCEEDED(propEnum->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> propSupports;
    propEnum->GetNext(getter_AddRefs(propSupports));

    nsCOMPtr<nsIPropertyElement> propElem(do_QueryInterface(propSupports));
    if (!propElem)
      return E_FAIL;

    nsAutoCString name;
    if (NS_FAILED(propElem->GetKey(name)))
      return E_FAIL;

    int32_t offset = 0;
    while ((offset = name.FindCharInSet(kCharsToEscape, offset)) != kNotFound) {
      name.Insert('\\', offset);
      offset += 2;
    }

    nsAutoString value;
    if (NS_FAILED(propElem->GetValue(value)))
      return E_FAIL;

    offset = 0;
    while ((offset = value.FindCharInSet(kCharsToEscape, offset)) != kNotFound) {
      value.Insert('\\', offset);
      offset += 2;
    }

    AppendUTF8toUTF16(name, strAttrs);
    strAttrs.Append(':');
    strAttrs.Append(value);
    strAttrs.Append(';');
  }

  if (strAttrs.IsEmpty())
    return S_FALSE;

  *aIA2Attributes = ::SysAllocStringLen(strAttrs.get(), strAttrs.Length());
  return *aIA2Attributes ? S_OK : E_OUTOFMEMORY;
}

IDispatch*
AccessibleWrap::NativeAccessible(nsIAccessible* aAccessible)
{
  if (!aAccessible) {
   NS_WARNING("Not passing in an aAccessible");
   return nullptr;
  }

  IAccessible* msaaAccessible = nullptr;
  aAccessible->GetNativeInterface(reinterpret_cast<void**>(&msaaAccessible));
  return static_cast<IDispatch*>(msaaAccessible);
}

Accessible*
AccessibleWrap::GetXPAccessibleFor(const VARIANT& aVarChild)
{
  if (aVarChild.vt != VT_I4)
    return nullptr;

  // if its us real easy - this seems to always be the case
  if (aVarChild.lVal == CHILDID_SELF)
    return this;

  if (nsAccUtils::MustPrune(this))
    return nullptr;

  // If lVal negative then it is treated as child ID and we should look for
  // accessible through whole accessible subtree including subdocuments.
  // Otherwise we treat lVal as index in parent.

  if (aVarChild.lVal < 0) {
    // Convert child ID to unique ID.
    void* uniqueID = reinterpret_cast<void*>(-aVarChild.lVal);

    // Document.
    if (IsDoc())
      return AsDoc()->GetAccessibleByUniqueIDInSubtree(uniqueID);

    // ARIA document.
    if (ARIARole() == roles::DOCUMENT) {
      DocAccessible* document = Document();
      Accessible* child =
        document->GetAccessibleByUniqueIDInSubtree(uniqueID);

      // Check whether the accessible for the given ID is a child of ARIA
      // document.
      Accessible* parent = child ? child->Parent() : nullptr;
      while (parent && parent != document) {
        if (parent == this)
          return child;

        parent = parent->Parent();
      }
    }

    return nullptr;
  }

  // Gecko child indices are 0-based in contrast to indices used in MSAA.
  return GetChildAt(aVarChild.lVal - 1);
}

void
AccessibleWrap::UpdateSystemCaret()
{
  // Move the system caret so that Windows Tablet Edition and tradional ATs with 
  // off-screen model can follow the caret
  ::DestroyCaret();

  a11y::RootAccessible* rootAccessible = RootAccessible();
  if (!rootAccessible) {
    return;
  }

  nsIWidget* widget = nullptr;
  nsIntRect caretRect = SelectionMgr()->GetCaretRect(&widget);
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
