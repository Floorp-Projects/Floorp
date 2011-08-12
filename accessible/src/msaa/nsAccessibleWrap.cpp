/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: Aaron Leventhal (aaronl@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsAccessibleWrap.h"

#include "nsAccessibilityAtoms.h"
#include "nsAccUtils.h"
#include "nsCoreUtils.h"
#include "nsWinUtils.h"
#include "Relation.h"
#include "States.h"

#include "ia2AccessibleRelation.h"

#include "nsIAccessibleDocument.h"
#include "nsIAccessibleEvent.h"
#include "nsIAccessibleRelation.h"
#include "nsIAccessibleWin32Object.h"

#include "Accessible2_i.c"
#include "AccessibleStates.h"

#include "nsIMutableArray.h"
#include "nsIDOMDocument.h"
#include "nsIFrame.h"
#include "nsIScrollableFrame.h"
#include "nsINameSpaceManager.h"
#include "nsINodeInfo.h"
#include "nsRootAccessible.h"
#include "nsIServiceManager.h"
#include "nsTextFormatter.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsRoleMap.h"
#include "nsEventMap.h"
#include "nsArrayUtils.h"

using namespace mozilla::a11y;

/* For documentation of the accessibility architecture,
 * see http://lxr.mozilla.org/seamonkey/source/accessible/accessible-docs.html
 */

//#define DEBUG_LEAKS

#ifdef DEBUG_LEAKS
static gAccessibles = 0;
#endif

EXTERN_C GUID CDECL CLSID_Accessible =
{ 0x61044601, 0xa811, 0x4e2b, { 0xbb, 0xba, 0x17, 0xbf, 0xab, 0xd3, 0x29, 0xd7 } };

static const PRInt32 kIEnumVariantDisconnected = -1;

////////////////////////////////////////////////////////////////////////////////
// nsAccessibleWrap
////////////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------
// construction
//-----------------------------------------------------
nsAccessibleWrap::
  nsAccessibleWrap(nsIContent *aContent, nsIWeakReference *aShell) :
  nsAccessible(aContent, aShell), mEnumVARIANTPosition(0), mTypeInfo(NULL)
{
}

//-----------------------------------------------------
// destruction
//-----------------------------------------------------
nsAccessibleWrap::~nsAccessibleWrap()
{
  if (mTypeInfo)
    mTypeInfo->Release();
}

NS_IMPL_ISUPPORTS_INHERITED0(nsAccessibleWrap, nsAccessible);

//-----------------------------------------------------
// IUnknown interface methods - see iunknown.h for documentation
//-----------------------------------------------------

// Microsoft COM QueryInterface
STDMETHODIMP nsAccessibleWrap::QueryInterface(REFIID iid, void** ppv)
{
__try {
  *ppv = NULL;

  if (IID_IUnknown == iid || IID_IDispatch == iid || IID_IAccessible == iid)
    *ppv = static_cast<IAccessible*>(this);
  else if (IID_IEnumVARIANT == iid) {
    long numChildren;
    get_accChildCount(&numChildren);
    if (numChildren > 0)  // Don't support this interface for leaf elements
      *ppv = static_cast<IEnumVARIANT*>(this);
  } else if (IID_IServiceProvider == iid)
    *ppv = static_cast<IServiceProvider*>(this);
  else if (IID_IAccessible2 == iid && !gIsIA2Disabled)
    *ppv = static_cast<IAccessible2*>(this);

  if (NULL == *ppv) {
    HRESULT hr = CAccessibleComponent::QueryInterface(iid, ppv);
    if (SUCCEEDED(hr))
      return hr;
  }

  if (NULL == *ppv) {
    HRESULT hr = CAccessibleHyperlink::QueryInterface(iid, ppv);
    if (SUCCEEDED(hr))
      return hr;
  }

  if (NULL == *ppv) {
    HRESULT hr = CAccessibleValue::QueryInterface(iid, ppv);
    if (SUCCEEDED(hr))
      return hr;
  }

  if (NULL == *ppv)
    return nsAccessNodeWrap::QueryInterface(iid, ppv);

  (reinterpret_cast<IUnknown*>(*ppv))->AddRef();
} __except(FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return S_OK;
}

//-----------------------------------------------------
// IAccessible methods
//-----------------------------------------------------


STDMETHODIMP nsAccessibleWrap::AccessibleObjectFromWindow(HWND hwnd,
                                                          DWORD dwObjectID,
                                                          REFIID riid,
                                                          void **ppvObject)
{
  // open the dll dynamically
  if (!gmAccLib)
    gmAccLib =::LoadLibraryW(L"OLEACC.DLL");

  if (gmAccLib) {
    if (!gmAccessibleObjectFromWindow)
      gmAccessibleObjectFromWindow = (LPFNACCESSIBLEOBJECTFROMWINDOW)GetProcAddress(gmAccLib,"AccessibleObjectFromWindow");

    if (gmAccessibleObjectFromWindow)
      return gmAccessibleObjectFromWindow(hwnd, dwObjectID, riid, ppvObject);
  }

  return E_FAIL;
}

STDMETHODIMP nsAccessibleWrap::NotifyWinEvent(DWORD event,
                                              HWND hwnd,
                                              LONG idObjectType,
                                              LONG idObject)
{
  if (gmNotifyWinEvent)
    return gmNotifyWinEvent(event, hwnd, idObjectType, idObject);

  return E_FAIL;
}

STDMETHODIMP nsAccessibleWrap::get_accParent( IDispatch __RPC_FAR *__RPC_FAR *ppdispParent)
{
__try {
  *ppdispParent = NULL;

  if (IsDefunct())
    return E_FAIL;

  nsRefPtr<nsDocAccessible> doc(do_QueryObject(this));
  if (doc) {
    // Return window system accessible object for root document and tab document
    // accessibles.
    if (!doc->ParentDocument() ||
        nsWinUtils::IsWindowEmulationStarted() &&
        nsCoreUtils::IsTabDocument(doc->GetDocumentNode())) {
      HWND hwnd = static_cast<HWND>(doc->GetNativeWindow());
      if (hwnd && SUCCEEDED(AccessibleObjectFromWindow(hwnd, OBJID_WINDOW,
                                                       IID_IAccessible,
                                                       (void**)ppdispParent))) {
        return S_OK;
      }
    }
  }

  nsAccessible* xpParentAcc = Parent();
  if (!xpParentAcc) {
    if (IsApplication())
      return S_OK;

    NS_ERROR("No parent accessible. Should we really assert here?");
    return E_UNEXPECTED;
  }

  *ppdispParent = NativeAccessible(xpParentAcc);

} __except(FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return S_OK;
}

STDMETHODIMP nsAccessibleWrap::get_accChildCount( long __RPC_FAR *pcountChildren)
{
__try {
  if (!pcountChildren)
    return E_INVALIDARG;

  *pcountChildren = 0;

  if (IsDefunct())
    return E_FAIL;

  if (nsAccUtils::MustPrune(this))
    return S_OK;

  *pcountChildren = GetChildCount();
} __except(FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }

  return S_OK;
}

STDMETHODIMP nsAccessibleWrap::get_accChild(
      /* [in] */ VARIANT varChild,
      /* [retval][out] */ IDispatch __RPC_FAR *__RPC_FAR *ppdispChild)
{
__try {
  *ppdispChild = NULL;
  if (IsDefunct())
    return E_FAIL;

  // IAccessible::accChild is used to return this accessible or child accessible
  // at the given index or to get an accessible by child ID in the case of
  // document accessible (it's handled by overriden GetXPAccessibleFor method
  // on the document accessible). The getting an accessible by child ID is used
  // by AccessibleObjectFromEvent() called by AT when AT handles our MSAA event.
  nsAccessible* child = GetXPAccessibleFor(varChild);
  if (child)
    *ppdispChild = NativeAccessible(child);

} __except(FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }

  return (*ppdispChild)? S_OK: E_INVALIDARG;
}

STDMETHODIMP nsAccessibleWrap::get_accName(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszName)
{
__try {
  *pszName = NULL;
  nsAccessible *xpAccessible = GetXPAccessibleFor(varChild);
  if (!xpAccessible)
    return E_FAIL;
  nsAutoString name;
  nsresult rv = xpAccessible->GetName(name);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);
    
  if (name.IsVoid()) {
    // Valid return value for the name:
    // The name was not provided, e.g. no alt attribute for an image.
    // A screen reader may choose to invent its own accessible name, e.g. from
    // an image src attribute.
    // See nsHTMLImageAccessible::GetName()
    return S_OK;
  }

  *pszName = ::SysAllocStringLen(name.get(), name.Length());
  if (!*pszName)
    return E_OUTOFMEMORY;

} __except(FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }

  return S_OK;
}


STDMETHODIMP nsAccessibleWrap::get_accValue(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszValue)
{
__try {
  *pszValue = NULL;
  nsAccessible *xpAccessible = GetXPAccessibleFor(varChild);
  if (xpAccessible) {
    nsAutoString value;
    if (NS_FAILED(xpAccessible->GetValue(value)))
      return E_FAIL;

    // see bug 438784: Need to expose URL on doc's value attribute.
    // For this, reverting part of fix for bug 425693 to make this MSAA method 
    // behave IAccessible2-style.
    if (value.IsEmpty())
      return S_FALSE;

    *pszValue = ::SysAllocStringLen(value.get(), value.Length());
    if (!*pszValue)
      return E_OUTOFMEMORY;
  }
} __except(FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return S_OK;
}

STDMETHODIMP
nsAccessibleWrap::get_accDescription(VARIANT varChild,
                                     BSTR __RPC_FAR *pszDescription)
{
__try {
  *pszDescription = NULL;

  nsAccessible *xpAccessible = GetXPAccessibleFor(varChild);
  if (!xpAccessible || xpAccessible->IsDefunct())
    return E_FAIL;

  nsAutoString description;
  xpAccessible->Description(description);

  *pszDescription = ::SysAllocStringLen(description.get(),
                                        description.Length());
  return *pszDescription ? S_OK : E_OUTOFMEMORY;

} __except(FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP nsAccessibleWrap::get_accRole(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ VARIANT __RPC_FAR *pvarRole)
{
__try {
  VariantInit(pvarRole);

  if (IsDefunct())
    return E_FAIL;

  nsAccessible *xpAccessible = GetXPAccessibleFor(varChild);
  if (!xpAccessible)
    return E_FAIL;

#ifdef DEBUG_A11Y
  NS_ASSERTION(nsAccUtils::IsTextInterfaceSupportCorrect(xpAccessible),
               "Does not support nsIAccessibleText when it should");
#endif

  PRUint32 xpRole = xpAccessible->Role();
  PRUint32 msaaRole = gWindowsRoleMap[xpRole].msaaRole;
  NS_ASSERTION(gWindowsRoleMap[nsIAccessibleRole::ROLE_LAST_ENTRY].msaaRole == ROLE_WINDOWS_LAST_ENTRY,
               "MSAA role map skewed");

  // Special case, if there is a ROLE_ROW inside of a ROLE_TREE_TABLE, then call the MSAA role
  // a ROLE_OUTLINEITEM for consistency and compatibility.
  // We need this because ARIA has a role of "row" for both grid and treegrid
  if (xpRole == nsIAccessibleRole::ROLE_ROW) {
    nsAccessible* xpParent = Parent();
    if (xpParent && xpParent->Role() == nsIAccessibleRole::ROLE_TREE_TABLE)
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
        !content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::role, roleString)) {
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
} __except(FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP nsAccessibleWrap::get_accState(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ VARIANT __RPC_FAR *pvarState)
{
__try {
  VariantInit(pvarState);
  pvarState->vt = VT_I4;
  pvarState->lVal = 0;

  nsAccessible *xpAccessible = GetXPAccessibleFor(varChild);
  if (!xpAccessible)
    return E_FAIL;

  // MSAA only has 31 states and the lowest 31 bits of our state bit mask
  // are the same states as MSAA.
  // Note: we map the following Gecko states to different MSAA states:
  //   REQUIRED -> ALERT_LOW
  //   ALERT -> ALERT_MEDIUM
  //   INVALID -> ALERT_HIGH
  //   CHECKABLE -> MARQUEED

  PRUint32 msaaState = 0;
  nsAccUtils::To32States(xpAccessible->State(), &msaaState, nsnull);
  pvarState->lVal = msaaState;
} __except(FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return S_OK;
}


STDMETHODIMP nsAccessibleWrap::get_accHelp(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszHelp)
{
__try {
  *pszHelp = NULL;
  return S_FALSE;

} __except(FilterA11yExceptions(::GetExceptionCode(),
                                GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP nsAccessibleWrap::get_accHelpTopic(
      /* [out] */ BSTR __RPC_FAR *pszHelpFile,
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ long __RPC_FAR *pidTopic)
{
__try {
  *pszHelpFile = NULL;
  *pidTopic = 0;
  return S_FALSE;

} __except(FilterA11yExceptions(::GetExceptionCode(),
                                GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP nsAccessibleWrap::get_accKeyboardShortcut(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszKeyboardShortcut)
{
__try {
  if (!pszKeyboardShortcut)
    return E_INVALIDARG;
  *pszKeyboardShortcut = NULL;

  nsAccessible* acc = GetXPAccessibleFor(varChild);
  if (!acc || acc->IsDefunct())
    return E_FAIL;

  KeyBinding keyBinding = acc->AccessKey();
  if (keyBinding.IsEmpty())
    keyBinding = acc->KeyboardShortcut();

  nsAutoString shortcut;
  keyBinding.ToString(shortcut);

  *pszKeyboardShortcut = ::SysAllocStringLen(shortcut.get(),
                                             shortcut.Length());
  return *pszKeyboardShortcut ? S_OK : E_OUTOFMEMORY;
} __except(FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP nsAccessibleWrap::get_accFocus(
      /* [retval][out] */ VARIANT __RPC_FAR *pvarChild)
{
  // VT_EMPTY:    None. This object does not have the keyboard focus itself
  //              and does not contain a child that has the keyboard focus.
  // VT_I4:       lVal is CHILDID_SELF. The object itself has the keyboard focus.
  // VT_I4:       lVal contains the child ID of the child element with the keyboard focus.
  // VT_DISPATCH: pdispVal member is the address of the IDispatch interface
  //              for the child object with the keyboard focus.
__try {
  if (IsDefunct())
    return E_FAIL;

  VariantInit(pvarChild);

  // Return the current IAccessible child that has focus
  nsAccessible* focusedAccessible = FocusedChild();
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

} __except(FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return S_OK;
}

// This helper class implements IEnumVARIANT for a nsIArray containing nsIAccessible objects.

class AccessibleEnumerator : public IEnumVARIANT
{
public:
  AccessibleEnumerator(nsIArray* aArray) : mArray(aArray), mCurIndex(0) { }
  AccessibleEnumerator(const AccessibleEnumerator& toCopy) :
    mArray(toCopy.mArray), mCurIndex(toCopy.mCurIndex) { }
  ~AccessibleEnumerator() { }

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject);
  STDMETHODIMP_(ULONG) AddRef(void);
  STDMETHODIMP_(ULONG) Release(void);

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
  PRUint32 mCurIndex;
  nsAutoRefCnt mRefCnt;
};

HRESULT
AccessibleEnumerator::QueryInterface(REFIID iid, void ** ppvObject)
{
__try {
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

  *ppvObject = NULL;
} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG)
AccessibleEnumerator::AddRef(void)
{
  return ++mRefCnt;
}

STDMETHODIMP_(ULONG)
AccessibleEnumerator::Release(void)
{
  ULONG r = --mRefCnt;
  if (r == 0)
    delete this;
  return r;
}

STDMETHODIMP
AccessibleEnumerator::Next(unsigned long celt, VARIANT FAR* rgvar, unsigned long FAR* pceltFetched)
{
__try {
  PRUint32 length = 0;
  mArray->GetLength(&length);

  HRESULT hr = S_OK;

  // Can't get more elements than there are...
  if (celt > length - mCurIndex) {
    hr = S_FALSE;
    celt = length - mCurIndex;
  }

  for (PRUint32 i = 0; i < celt; ++i, ++mCurIndex) {
    // Copy the elements of the array into rgvar
    nsCOMPtr<nsIAccessible> accel(do_QueryElementAt(mArray, mCurIndex));
    NS_ASSERTION(accel, "Invalid pointer in mArray");

    if (accel) {
      rgvar[i].vt = VT_DISPATCH;
      rgvar[i].pdispVal = nsAccessibleWrap::NativeAccessible(accel);
    }
  }

  if (pceltFetched)
    *pceltFetched = celt;

  return hr;
} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }

  return S_OK;
}

STDMETHODIMP
AccessibleEnumerator::Clone(IEnumVARIANT FAR* FAR* ppenum)
{
__try {
  *ppenum = new AccessibleEnumerator(*this);
  if (!*ppenum)
    return E_OUTOFMEMORY;
  NS_ADDREF(*ppenum);
} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return S_OK;
}

STDMETHODIMP
AccessibleEnumerator::Skip(unsigned long celt)
{
__try {
  PRUint32 length = 0;
  mArray->GetLength(&length);
  // Check if we can skip the requested number of elements
  if (celt > length - mCurIndex) {
    mCurIndex = length;
    return S_FALSE;
  }
  mCurIndex += celt;
} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return S_OK;
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
STDMETHODIMP nsAccessibleWrap::get_accSelection(VARIANT __RPC_FAR *pvarChildren)
{
__try {
  VariantInit(pvarChildren);
  pvarChildren->vt = VT_EMPTY;

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
} __except(FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return S_OK;
}

STDMETHODIMP nsAccessibleWrap::get_accDefaultAction(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszDefaultAction)
{
__try {
  *pszDefaultAction = NULL;
  nsAccessible *xpAccessible = GetXPAccessibleFor(varChild);
  if (xpAccessible) {
    nsAutoString defaultAction;
    if (NS_FAILED(xpAccessible->GetActionName(0, defaultAction)))
      return E_FAIL;

    *pszDefaultAction = ::SysAllocStringLen(defaultAction.get(),
                                            defaultAction.Length());
    return *pszDefaultAction ? S_OK : E_OUTOFMEMORY;
  }

} __except(FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP nsAccessibleWrap::accSelect(
      /* [in] */ long flagsSelect,
      /* [optional][in] */ VARIANT varChild)
{
__try {
  // currently only handle focus and selection
  nsAccessible *xpAccessible = GetXPAccessibleFor(varChild);
  NS_ENSURE_TRUE(xpAccessible, E_FAIL);

  if (flagsSelect & (SELFLAG_TAKEFOCUS|SELFLAG_TAKESELECTION|SELFLAG_REMOVESELECTION))
  {
    if (flagsSelect & SELFLAG_TAKEFOCUS)
      xpAccessible->TakeFocus();

    if (flagsSelect & SELFLAG_TAKESELECTION)
      xpAccessible->TakeSelection();

    if (flagsSelect & SELFLAG_ADDSELECTION)
      xpAccessible->SetSelected(PR_TRUE);

    if (flagsSelect & SELFLAG_REMOVESELECTION)
      xpAccessible->SetSelected(PR_FALSE);

    if (flagsSelect & SELFLAG_EXTENDSELECTION)
      xpAccessible->ExtendSelection();

    return S_OK;
  }

} __except(FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP nsAccessibleWrap::accLocation(
      /* [out] */ long __RPC_FAR *pxLeft,
      /* [out] */ long __RPC_FAR *pyTop,
      /* [out] */ long __RPC_FAR *pcxWidth,
      /* [out] */ long __RPC_FAR *pcyHeight,
      /* [optional][in] */ VARIANT varChild)
{
__try {
  nsAccessible *xpAccessible = GetXPAccessibleFor(varChild);

  if (xpAccessible) {
    PRInt32 x, y, width, height;
    if (NS_FAILED(xpAccessible->GetBounds(&x, &y, &width, &height)))
      return E_FAIL;

    *pxLeft = x;
    *pyTop = y;
    *pcxWidth = width;
    *pcyHeight = height;
    return S_OK;
  }
} __except(FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }

  return E_FAIL;
}

STDMETHODIMP nsAccessibleWrap::accNavigate(
      /* [in] */ long navDir,
      /* [optional][in] */ VARIANT varStart,
      /* [retval][out] */ VARIANT __RPC_FAR *pvarEndUpAt)
{
__try {
  nsAccessible *xpAccessibleStart = GetXPAccessibleFor(varStart);
  if (!xpAccessibleStart)
    return E_FAIL;

  VariantInit(pvarEndUpAt);

  nsCOMPtr<nsIAccessible> xpAccessibleResult;
  PRUint32 xpRelation = 0;

  switch(navDir) {
    case NAVDIR_DOWN:
      xpAccessibleStart->GetAccessibleBelow(getter_AddRefs(xpAccessibleResult));
      break;
    case NAVDIR_FIRSTCHILD:
      if (!nsAccUtils::MustPrune(xpAccessibleStart))
        xpAccessibleStart->GetFirstChild(getter_AddRefs(xpAccessibleResult));
      break;
    case NAVDIR_LASTCHILD:
      if (!nsAccUtils::MustPrune(xpAccessibleStart))
        xpAccessibleStart->GetLastChild(getter_AddRefs(xpAccessibleResult));
      break;
    case NAVDIR_LEFT:
      xpAccessibleStart->GetAccessibleToLeft(getter_AddRefs(xpAccessibleResult));
      break;
    case NAVDIR_NEXT:
      xpAccessibleStart->GetNextSibling(getter_AddRefs(xpAccessibleResult));
      break;
    case NAVDIR_PREVIOUS:
      xpAccessibleStart->GetPreviousSibling(getter_AddRefs(xpAccessibleResult));
      break;
    case NAVDIR_RIGHT:
      xpAccessibleStart->GetAccessibleToRight(getter_AddRefs(xpAccessibleResult));
      break;
    case NAVDIR_UP:
      xpAccessibleStart->GetAccessibleAbove(getter_AddRefs(xpAccessibleResult));
      break;

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
  }

  pvarEndUpAt->vt = VT_EMPTY;

  if (xpRelation) {
    Relation rel = RelationByType(xpRelation);
    xpAccessibleResult = rel.Next();
  }

  if (xpAccessibleResult) {
    pvarEndUpAt->pdispVal = NativeAccessible(xpAccessibleResult);
    pvarEndUpAt->vt = VT_DISPATCH;
    return S_OK;
  }
} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP nsAccessibleWrap::accHitTest(
      /* [in] */ long xLeft,
      /* [in] */ long yTop,
      /* [retval][out] */ VARIANT __RPC_FAR *pvarChild)
{
__try {
  VariantInit(pvarChild);
  if (IsDefunct())
    return E_FAIL;

  nsAccessible* accessible = ChildAtPoint(xLeft, yTop, eDirectChild);

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
} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }

  return S_OK;
}

STDMETHODIMP nsAccessibleWrap::accDoDefaultAction(
      /* [optional][in] */ VARIANT varChild)
{
__try {
  nsAccessible *xpAccessible = GetXPAccessibleFor(varChild);
  if (!xpAccessible || FAILED(xpAccessible->DoAction(0))) {
    return E_FAIL;
  }
} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return S_OK;
}

STDMETHODIMP nsAccessibleWrap::put_accName(
      /* [optional][in] */ VARIANT varChild,
      /* [in] */ BSTR szName)
{
  return E_NOTIMPL;
}

STDMETHODIMP nsAccessibleWrap::put_accValue(
      /* [optional][in] */ VARIANT varChild,
      /* [in] */ BSTR szValue)
{
  return E_NOTIMPL;
}

#include "mshtml.h"

////////////////////////////////////////////////////////////////////////////////
// nsAccessibleWrap. IEnumVariant

STDMETHODIMP
nsAccessibleWrap::Next(ULONG aNumElementsRequested, VARIANT FAR* aPVar,
                       ULONG FAR* aNumElementsFetched)
{
  // Children already cached via QI to IEnumVARIANT
__try {
  *aNumElementsFetched = 0;

  if (aNumElementsRequested <= 0 || !aPVar)
    return E_INVALIDARG;

  if (mEnumVARIANTPosition == kIEnumVariantDisconnected)
    return CO_E_OBJNOTCONNECTED;

  PRUint32 numElementsFetched = 0;
  for (; numElementsFetched < aNumElementsRequested;
       numElementsFetched++, mEnumVARIANTPosition++) {

    nsAccessible* accessible = GetChildAt(mEnumVARIANTPosition);
    if (!accessible)
      break;

    VariantInit(&aPVar[numElementsFetched]);

    aPVar[numElementsFetched].pdispVal = NativeAccessible(accessible);
    aPVar[numElementsFetched].vt = VT_DISPATCH;
  }

  (*aNumElementsFetched) = numElementsFetched;

  return numElementsFetched < aNumElementsRequested ? S_FALSE : S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
nsAccessibleWrap::Skip(ULONG aNumElements)
{
__try {
  if (mEnumVARIANTPosition == kIEnumVariantDisconnected)
    return CO_E_OBJNOTCONNECTED;

  mEnumVARIANTPosition += aNumElements;

  PRInt32 numChildren = GetChildCount();
  if (mEnumVARIANTPosition > numChildren)
  {
    mEnumVARIANTPosition = numChildren;
    return S_FALSE;
  }
} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return NOERROR;
}

STDMETHODIMP
nsAccessibleWrap::Reset(void)
{
  mEnumVARIANTPosition = 0;
  return NOERROR;
}

STDMETHODIMP
nsAccessibleWrap::Clone(IEnumVARIANT FAR* FAR* ppenum)
{
__try {
  *ppenum = nsnull;
  
  nsCOMPtr<nsIArray> childArray;
  nsresult rv = GetChildren(getter_AddRefs(childArray));

  *ppenum = new AccessibleEnumerator(childArray);
  if (!*ppenum)
    return E_OUTOFMEMORY;
  NS_ADDREF(*ppenum);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return NOERROR;
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessibleWrap. IAccessible2

STDMETHODIMP
nsAccessibleWrap::get_nRelations(long *aNRelations)
{
__try {
  if (!aNRelations)
    return E_INVALIDARG;

  *aNRelations = 0;

  if (IsDefunct())
    return E_FAIL;

  for (PRUint32 relType = nsIAccessibleRelation::RELATION_FIRST;
       relType <= nsIAccessibleRelation::RELATION_LAST; relType++) {
    Relation rel = RelationByType(relType);
    if (rel.Next())
      (*aNRelations)++;
  }
  return S_OK;
} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
nsAccessibleWrap::get_relation(long aRelationIndex,
                               IAccessibleRelation **aRelation)
{
__try {
  if (!aRelation)
    return E_INVALIDARG;

  *aRelation = NULL;

  if (IsDefunct())
    return E_FAIL;

  PRUint32 relIdx = 0;
  for (PRUint32 relType = nsIAccessibleRelation::RELATION_FIRST;
       relType <= nsIAccessibleRelation::RELATION_LAST; relType++) {
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
} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
nsAccessibleWrap::get_relations(long aMaxRelations,
                                IAccessibleRelation **aRelation,
                                long *aNRelations)
{
__try {
  if (!aRelation || !aNRelations)
    return E_INVALIDARG;

  *aNRelations = 0;

  if (IsDefunct())
    return E_FAIL;

  for (PRUint32 relType = nsIAccessibleRelation::RELATION_FIRST;
       relType <= nsIAccessibleRelation::RELATION_LAST &&
       *aNRelations < aMaxRelations; relType++) {
    Relation rel = RelationByType(relType);
    nsRefPtr<ia2AccessibleRelation> ia2Rel =
      new ia2AccessibleRelation(relType, &rel);
    if (ia2Rel->HasTargets()) {
      ia2Rel.forget(aRelation + (*aNRelations));
      (*aNRelations)++;
    }
  }
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
nsAccessibleWrap::role(long *aRole)
{
__try {
  *aRole = 0;

  if (IsDefunct())
    return E_FAIL;

  NS_ASSERTION(gWindowsRoleMap[nsIAccessibleRole::ROLE_LAST_ENTRY].ia2Role == ROLE_WINDOWS_LAST_ENTRY,
               "MSAA role map skewed");

  PRUint32 xpRole = Role();
  *aRole = gWindowsRoleMap[xpRole].ia2Role;

  // Special case, if there is a ROLE_ROW inside of a ROLE_TREE_TABLE, then call
  // the IA2 role a ROLE_OUTLINEITEM.
  if (xpRole == nsIAccessibleRole::ROLE_ROW) {
    nsAccessible* xpParent = Parent();
    if (xpParent && xpParent->Role() == nsIAccessibleRole::ROLE_TREE_TABLE)
      *aRole = ROLE_SYSTEM_OUTLINEITEM;
  }

  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
nsAccessibleWrap::scrollTo(enum IA2ScrollType aScrollType)
{
__try {
  nsresult rv = ScrollTo(aScrollType);
  return GetHRESULT(rv);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
nsAccessibleWrap::scrollToPoint(enum IA2CoordinateType aCoordType,
                                long aX, long aY)
{
__try {
  PRUint32 geckoCoordType = (aCoordType == IA2_COORDTYPE_SCREEN_RELATIVE) ?
    nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE :
    nsIAccessibleCoordinateType::COORDTYPE_PARENT_RELATIVE;

  nsresult rv = ScrollToPoint(geckoCoordType, aX, aY);
  return GetHRESULT(rv);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
nsAccessibleWrap::get_groupPosition(long *aGroupLevel,
                                    long *aSimilarItemsInGroup,
                                    long *aPositionInGroup)
{
__try {
  PRInt32 groupLevel = 0;
  PRInt32 similarItemsInGroup = 0;
  PRInt32 positionInGroup = 0;

  nsresult rv = GroupPosition(&groupLevel, &similarItemsInGroup,
                              &positionInGroup);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  // Group information for accessibles having level only (like html headings
  // elements) isn't exposed by this method. AT should look for 'level' object
  // attribute.
  if (!similarItemsInGroup && !positionInGroup)
    return S_FALSE;

  *aGroupLevel = groupLevel;
  *aSimilarItemsInGroup = similarItemsInGroup;
  *aPositionInGroup = positionInGroup;

  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
nsAccessibleWrap::get_states(AccessibleStates *aStates)
{
__try {
  *aStates = 0;

  // XXX: bug 344674 should come with better approach that we have here.

  PRUint64 state = State();

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

  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
nsAccessibleWrap::get_extendedRole(BSTR *aExtendedRole)
{
__try {
  *aExtendedRole = NULL;
} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }

  return E_NOTIMPL;
}

STDMETHODIMP
nsAccessibleWrap::get_localizedExtendedRole(BSTR *aLocalizedExtendedRole)
{
__try {
  *aLocalizedExtendedRole = NULL;
} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }

  return E_NOTIMPL;
}

STDMETHODIMP
nsAccessibleWrap::get_nExtendedStates(long *aNExtendedStates)
{
__try {
  *aNExtendedStates = 0;
} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }

  return E_NOTIMPL;
}

STDMETHODIMP
nsAccessibleWrap::get_extendedStates(long aMaxExtendedStates,
                                     BSTR **aExtendedStates,
                                     long *aNExtendedStates)
{
__try {
  *aExtendedStates = NULL;
  *aNExtendedStates = 0;
} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }

  return E_NOTIMPL;
}

STDMETHODIMP
nsAccessibleWrap::get_localizedExtendedStates(long aMaxLocalizedExtendedStates,
                                              BSTR **aLocalizedExtendedStates,
                                              long *aNLocalizedExtendedStates)
{
__try {
  *aLocalizedExtendedStates = NULL;
  *aNLocalizedExtendedStates = 0;
} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }

  return E_NOTIMPL;
}

STDMETHODIMP
nsAccessibleWrap::get_uniqueID(long *uniqueID)
{
__try {
  *uniqueID = - reinterpret_cast<long>(UniqueID());
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
nsAccessibleWrap::get_windowHandle(HWND *aWindowHandle)
{
__try {
  *aWindowHandle = 0;

  if (IsDefunct())
    return E_FAIL;

  *aWindowHandle = GetHWNDFor(this);
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
nsAccessibleWrap::get_indexInParent(long *aIndexInParent)
{
__try {
  if (!aIndexInParent)
    return E_INVALIDARG;

  *aIndexInParent = -1;
  if (IsDefunct())
    return E_FAIL;

  *aIndexInParent = IndexInParent();
  if (*aIndexInParent == -1)
    return S_FALSE;

  return S_OK;
} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
nsAccessibleWrap::get_locale(IA2Locale *aLocale)
{
__try {
  // Language codes consist of a primary code and a possibly empty series of
  // subcodes: language-code = primary-code ( "-" subcode )*
  // Two-letter primary codes are reserved for [ISO639] language abbreviations.
  // Any two-letter subcode is understood to be a [ISO3166] country code.

  nsAutoString lang;
  nsresult rv = GetLanguage(lang);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  // If primary code consists from two letters then expose it as language.
  PRInt32 offset = lang.FindChar('-', 0);
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

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
nsAccessibleWrap::get_attributes(BSTR *aAttributes)
{
  // The format is name:value;name:value; with \ for escaping these
  // characters ":;=,\".
__try {
  *aAttributes = NULL;

  nsCOMPtr<nsIPersistentProperties> attributes;
  nsresult rv = GetAttributes(getter_AddRefs(attributes));
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  return ConvertToIA2Attributes(attributes, aAttributes);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

////////////////////////////////////////////////////////////////////////////////
// IDispatch

STDMETHODIMP
nsAccessibleWrap::GetTypeInfoCount(UINT *pctinfo)
{
  *pctinfo = 1;
  return S_OK;
}

STDMETHODIMP
nsAccessibleWrap::GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo **ppTInfo)
{
  *ppTInfo = NULL;

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
nsAccessibleWrap::GetIDsOfNames(REFIID riid, LPOLESTR *rgszNames,
                                UINT cNames, LCID lcid, DISPID *rgDispId)
{
  ITypeInfo *typeInfo = GetTI(lcid);
  if (!typeInfo)
    return E_FAIL;

  HRESULT hr = DispGetIDsOfNames(typeInfo, rgszNames, cNames, rgDispId);
  return hr;
}

STDMETHODIMP
nsAccessibleWrap::Invoke(DISPID dispIdMember, REFIID riid,
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
NS_IMETHODIMP nsAccessibleWrap::GetNativeInterface(void **aOutAccessible)
{
  *aOutAccessible = static_cast<IAccessible*>(this);
  NS_ADDREF_THIS();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessible

nsresult
nsAccessibleWrap::HandleAccEvent(AccEvent* aEvent)
{
  nsresult rv = nsAccessible::HandleAccEvent(aEvent);
  NS_ENSURE_SUCCESS(rv, rv);

  return FirePlatformEvent(aEvent);
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessibleWrap

nsresult
nsAccessibleWrap::FirePlatformEvent(AccEvent* aEvent)
{
  PRUint32 eventType = aEvent->GetEventType();

  NS_ENSURE_TRUE(eventType > 0 &&
                 eventType < nsIAccessibleEvent::EVENT_LAST_ENTRY,
                 NS_ERROR_FAILURE);

  PRUint32 winLastEntry = gWinEventMap[nsIAccessibleEvent::EVENT_LAST_ENTRY];
  NS_ASSERTION(winLastEntry == kEVENT_LAST_ENTRY,
               "MSAA event map skewed");

  PRUint32 winEvent = gWinEventMap[eventType];
  if (!winEvent)
    return NS_OK;

  // Means we're not active.
  NS_ENSURE_TRUE(mWeakShell, NS_ERROR_FAILURE);

  nsAccessible *accessible = aEvent->GetAccessible();
  if (!accessible)
    return NS_OK;

  if (eventType == nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED ||
      eventType == nsIAccessibleEvent::EVENT_FOCUS) {
    UpdateSystemCaret();

  } else if (eventType == nsIAccessibleEvent::EVENT_REORDER) {
    // If the accessible children are changed then drop the IEnumVariant current
    // position of the accessible.
    UnattachIEnumVariant();
  }

  PRInt32 childID = GetChildIDFor(accessible); // get the id for the accessible
  if (!childID)
    return NS_OK; // Can't fire an event without a child ID

  HWND hWnd = GetHWNDFor(accessible);
  NS_ENSURE_TRUE(hWnd, NS_ERROR_FAILURE);

  nsAutoString tag;
  nsCAutoString id;
  nsIContent* cnt = accessible->GetContent();
  if (cnt) {
    cnt->Tag()->ToString(tag);
    nsIAtom* aid = cnt->GetID();
    if (aid)
      aid->ToUTF8String(id);
  }

#ifdef DEBUG_A11Y
  printf("\n\nMSAA event: event: %d, target: %s@id='%s', childid: %d, hwnd: %d\n\n",
         eventType, NS_ConvertUTF16toUTF8(tag).get(), id.get(),
         childID, hWnd);
#endif

  // Fire MSAA event for client area window.
  NotifyWinEvent(winEvent, hWnd, OBJID_CLIENT, childID);
  return NS_OK;
}

//------- Helper methods ---------

PRInt32 nsAccessibleWrap::GetChildIDFor(nsAccessible* aAccessible)
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
nsAccessibleWrap::GetHWNDFor(nsAccessible *aAccessible)
{
  if (aAccessible) {
    // Popup lives in own windows, use its HWND until the popup window is
    // hidden to make old JAWS versions work with collapsed comboboxes (see
    // discussion in bug 379678).
    nsIFrame* frame = aAccessible->GetFrame();
    if (frame) {
      nsIWidget* widget = frame->GetNearestWidget();
      PRBool isVisible = PR_FALSE;
      widget->IsVisible(isVisible);
      if (isVisible) {
        nsCOMPtr<nsIPresShell> shell(aAccessible->GetPresShell());
        nsIViewManager* vm = shell->GetViewManager();
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

    nsDocAccessible* document = aAccessible->GetDocAccessible();
    if (document)
      return static_cast<HWND>(document->GetNativeWindow());
  }
  return nsnull;
}

HRESULT
nsAccessibleWrap::ConvertToIA2Attributes(nsIPersistentProperties *aAttributes,
                                         BSTR *aIA2Attributes)
{
  *aIA2Attributes = NULL;

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

  PRBool hasMore = PR_FALSE;
  while (NS_SUCCEEDED(propEnum->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> propSupports;
    propEnum->GetNext(getter_AddRefs(propSupports));

    nsCOMPtr<nsIPropertyElement> propElem(do_QueryInterface(propSupports));
    if (!propElem)
      return E_FAIL;

    nsCAutoString name;
    if (NS_FAILED(propElem->GetKey(name)))
      return E_FAIL;

    PRUint32 offset = 0;
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

IDispatch *nsAccessibleWrap::NativeAccessible(nsIAccessible *aXPAccessible)
{
  if (!aXPAccessible) {
   NS_WARNING("Not passing in an aXPAccessible");
   return NULL;
  }

  nsCOMPtr<nsIAccessibleWin32Object> accObject(do_QueryInterface(aXPAccessible));
  if (accObject) {
    void* hwnd = nsnull;
    accObject->GetHwnd(&hwnd);
    if (hwnd) {
      IDispatch *retval = nsnull;
      AccessibleObjectFromWindow(reinterpret_cast<HWND>(hwnd),
        OBJID_WINDOW, IID_IAccessible, (void **) &retval);
      return retval;
    }
  }

  IAccessible *msaaAccessible;
  aXPAccessible->GetNativeInterface((void**)&msaaAccessible);

  return static_cast<IDispatch*>(msaaAccessible);
}

void
nsAccessibleWrap::UnattachIEnumVariant()
{
  if (mEnumVARIANTPosition > 0)
    mEnumVARIANTPosition = kIEnumVariantDisconnected;
}

nsAccessible*
nsAccessibleWrap::GetXPAccessibleFor(const VARIANT& aVarChild)
{
  if (aVarChild.vt != VT_I4)
    return nsnull;

  // if its us real easy - this seems to always be the case
  if (aVarChild.lVal == CHILDID_SELF)
    return this;

  if (nsAccUtils::MustPrune(this))
    return nsnull;

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
    if (ARIARole() == nsIAccessibleRole::ROLE_DOCUMENT) {
      nsDocAccessible* document = GetDocAccessible();
      nsAccessible* child =
        document->GetAccessibleByUniqueIDInSubtree(uniqueID);

      // Check whether the accessible for the given ID is a child of ARIA
      // document.
      nsAccessible* parent = child ? child->Parent() : nsnull;
      while (parent && parent != document) {
        if (parent == this)
          return child;

        parent = parent->Parent();
      }
    }

    return nsnull;
  }

  // Gecko child indices are 0-based in contrast to indices used in MSAA.
  return GetChildAt(aVarChild.lVal - 1);
}

void nsAccessibleWrap::UpdateSystemCaret()
{
  // Move the system caret so that Windows Tablet Edition and tradional ATs with 
  // off-screen model can follow the caret
  ::DestroyCaret();

  nsRootAccessible* rootAccessible = RootAccessible();
  if (!rootAccessible) {
    return;
  }

  nsRefPtr<nsCaretAccessible> caretAccessible = rootAccessible->GetCaretAccessible();
  if (!caretAccessible) {
    return;
  }

  nsIWidget *widget;
  nsIntRect caretRect = caretAccessible->GetCaretRect(&widget);
  HWND caretWnd; 
  if (caretRect.IsEmpty() || !(caretWnd = (HWND)widget->GetNativeData(NS_NATIVE_WINDOW))) {
    return;
  }

  // Create invisible bitmap for caret, otherwise its appearance interferes
  // with Gecko caret
  HBITMAP caretBitMap = CreateBitmap(1, caretRect.height, 1, 1, NULL);
  if (::CreateCaret(caretWnd, caretBitMap, 1, caretRect.height)) {  // Also destroys the last caret
    ::ShowCaret(caretWnd);
    RECT windowRect;
    ::GetWindowRect(caretWnd, &windowRect);
    ::SetCaretPos(caretRect.x - windowRect.left, caretRect.y - windowRect.top);
    ::DeleteObject(caretBitMap);
  }
}

ITypeInfo*
nsAccessibleWrap::GetTI(LCID lcid)
{
  if (mTypeInfo)
    return mTypeInfo;

  ITypeLib *typeLib = NULL;
  HRESULT hr = LoadRegTypeLib(LIBID_Accessibility, 1, 0, lcid, &typeLib);
  if (FAILED(hr))
    return NULL;

  hr = typeLib->GetTypeInfoOfGuid(IID_IAccessible, &mTypeInfo);
  typeLib->Release();

  if (FAILED(hr))
    return NULL;

  return mTypeInfo;
}
