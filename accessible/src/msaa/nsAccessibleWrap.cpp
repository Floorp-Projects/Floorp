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

#include "nsIAccessibleDocument.h"
#include "nsIAccessibleSelectable.h"
#include "nsIAccessibleEvent.h"
#include "nsIAccessibleWin32Object.h"

#include "Accessible2_i.c"
#include "AccessibleStates.h"

#include "nsIMutableArray.h"
#include "nsIDOMDocument.h"
#include "nsIFrame.h"
#include "nsIScrollableFrame.h"
#include "nsINameSpaceManager.h"
#include "nsINodeInfo.h"
#include "nsIPrefService.h"
#include "nsRootAccessible.h"
#include "nsIServiceManager.h"
#include "nsTextFormatter.h"
#include "nsIView.h"
#include "nsRoleMap.h"
#include "nsEventMap.h"
#include "nsArrayUtils.h"

/* For documentation of the accessibility architecture,
 * see http://lxr.mozilla.org/seamonkey/source/accessible/accessible-docs.html
 */

//#define DEBUG_LEAKS

#ifdef DEBUG_LEAKS
static gAccessibles = 0;
#endif

EXTERN_C GUID CDECL CLSID_Accessible =
{ 0x61044601, 0xa811, 0x4e2b, { 0xbb, 0xba, 0x17, 0xbf, 0xab, 0xd3, 0x29, 0xd7 } };

/*
 * Class nsAccessibleWrap
 */

//-----------------------------------------------------
// construction
//-----------------------------------------------------
nsAccessibleWrap::nsAccessibleWrap(nsIDOMNode* aNode, nsIWeakReference *aShell):
  nsAccessible(aNode, aShell), mEnumVARIANTPosition(0)
{
}

//-----------------------------------------------------
// destruction
//-----------------------------------------------------
nsAccessibleWrap::~nsAccessibleWrap()
{
}

NS_IMPL_ISUPPORTS_INHERITED0(nsAccessibleWrap, nsAccessible);

//-----------------------------------------------------
// IUnknown interface methods - see iunknown.h for documentation
//-----------------------------------------------------

// Microsoft COM QueryInterface
STDMETHODIMP nsAccessibleWrap::QueryInterface(REFIID iid, void** ppv)
{
  *ppv = NULL;

  if (IID_IUnknown == iid || IID_IDispatch == iid || IID_IAccessible == iid)
    *ppv = static_cast<IAccessible*>(this);
  else if (IID_IEnumVARIANT == iid && !gIsEnumVariantSupportDisabled) {
    long numChildren;
    get_accChildCount(&numChildren);
    if (numChildren > 0)  // Don't support this interface for leaf elements
      *ppv = static_cast<IEnumVARIANT*>(this);
  } else if (IID_IServiceProvider == iid)
    *ppv = static_cast<IServiceProvider*>(this);
  else if (IID_IAccessible2 == iid)
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
    gmAccLib =::LoadLibrary("OLEACC.DLL");

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
  *ppdispParent = NULL;
  if (!mWeakShell)
    return E_FAIL;  // We've been shut down

  nsIFrame *frame = GetFrame();
  HWND hwnd = 0;
  if (frame) {
    nsIView *view = frame->GetViewExternal();
    if (view) {
      // This code is essentially our implementation of WindowFromAccessibleObject,
      // because MSAA iterates get_accParent() until it sees an object of ROLE_WINDOW
      // to know where the window for a given accessible is. We must expose the native
      // window accessible that MSAA creates for us. This must be done for the document
      // object as well as any layout that creates its own window (e.g. via overflow: scroll)
      nsIWidget *widget = view->GetWidget();
      if (widget) {
        hwnd = (HWND)widget->GetNativeData(NS_NATIVE_WINDOW);
        NS_ASSERTION(hwnd, "No window handle for window");
        nsIView *rootView;
        view->GetViewManager()->GetRootView(rootView);
        if (rootView == view) {
          // If the current object has a widget but was created by an
          // outer object with its own outer window, then
          // we want the native accessible for that outer window
          hwnd = ::GetParent(hwnd);
          NS_ASSERTION(hwnd, "No window handle for window");
        }
      }
      else {
        // If a frame is a scrollable frame, then it has one window for the client area,
        // not an extra parent window for just the scrollbars
        nsIScrollableFrame *scrollFrame = nsnull;
        CallQueryInterface(frame, &scrollFrame);
        if (scrollFrame) {
          hwnd = (HWND)scrollFrame->GetScrolledFrame()->GetWindow()->GetNativeData(NS_NATIVE_WINDOW);
          NS_ASSERTION(hwnd, "No window handle for window");
        }
      }
    }

    if (hwnd && SUCCEEDED(AccessibleObjectFromWindow(hwnd, OBJID_WINDOW, IID_IAccessible,
                                              (void**)ppdispParent))) {
      return S_OK;
    }
  }

  nsCOMPtr<nsIAccessible> xpParentAccessible(GetParent());
  NS_ASSERTION(xpParentAccessible, "No parent accessible where we're not direct child of window");
  if (!xpParentAccessible) {
    return E_UNEXPECTED;
  }
  *ppdispParent = NativeAccessible(xpParentAccessible);

  return S_OK;
}

STDMETHODIMP nsAccessibleWrap::get_accChildCount( long __RPC_FAR *pcountChildren)
{
  *pcountChildren = 0;
  if (MustPrune(this)) {
    return NS_OK;
  }

  PRInt32 numChildren;
  GetChildCount(&numChildren);
  *pcountChildren = numChildren;

  return S_OK;
}

STDMETHODIMP nsAccessibleWrap::get_accChild(
      /* [in] */ VARIANT varChild,
      /* [retval][out] */ IDispatch __RPC_FAR *__RPC_FAR *ppdispChild)
{
  *ppdispChild = NULL;

  if (!mWeakShell || varChild.vt != VT_I4)
    return E_FAIL;

  if (varChild.lVal == CHILDID_SELF) {
    *ppdispChild = static_cast<IDispatch*>(this);
    AddRef();
    return S_OK;
  }

  nsCOMPtr<nsIAccessible> childAccessible;
  if (!MustPrune(this)) {
    GetChildAt(varChild.lVal - 1, getter_AddRefs(childAccessible));
    if (childAccessible) {
      *ppdispChild = NativeAccessible(childAccessible);
    }
  }

  return (*ppdispChild)? S_OK: E_FAIL;
}

STDMETHODIMP nsAccessibleWrap::get_accName(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszName)
{
  *pszName = NULL;
  nsCOMPtr<nsIAccessible> xpAccessible;
  GetXPAccessibleFor(varChild, getter_AddRefs(xpAccessible));
  if (xpAccessible) {
    nsAutoString name;
    if (NS_FAILED(xpAccessible->GetName(name)))
      return S_FALSE;
    if (!name.IsVoid()) {
      *pszName = ::SysAllocString(name.get());
    }
#ifdef DEBUG_A11Y
    NS_ASSERTION(mIsInitialized, "Access node was not initialized");
#endif
  }

  return S_OK;
}


STDMETHODIMP nsAccessibleWrap::get_accValue(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszValue)
{
  *pszValue = NULL;
  nsCOMPtr<nsIAccessible> xpAccessible;
  GetXPAccessibleFor(varChild, getter_AddRefs(xpAccessible));
  if (xpAccessible) {
    nsAutoString value;
    if (NS_FAILED(xpAccessible->GetValue(value)))
      return S_FALSE;

    *pszValue = ::SysAllocString(value.get());
  }

  return S_OK;
}

STDMETHODIMP
nsAccessibleWrap::get_accDescription(VARIANT varChild,
                                     BSTR __RPC_FAR *pszDescription)
{
  *pszDescription = NULL;
  nsCOMPtr<nsIAccessible> xpAccessible;
  GetXPAccessibleFor(varChild, getter_AddRefs(xpAccessible));
  if (!xpAccessible)
    return E_FAIL;

  // For items that are a choice in a list of choices, use MSAA description
  // field to shoehorn positional info, it's becoming a defacto standard use for
  // the field.

  nsAutoString description;

  // Try nsIAccessible::groupPosition to make a positional description string.
  PRInt32 groupLevel;
  PRInt32 similarItemsInGroup;
  PRInt32 positionInGroup;

  nsresult rv = xpAccessible->GroupPosition(&groupLevel, &similarItemsInGroup,
                                            &positionInGroup);
  if (NS_SUCCEEDED(rv)) {
    if (positionInGroup > 0) {
      if (groupLevel > 0) {
        // XXX: How do we calculate the number of children? Now we append
        // " with [numChildren]c" for tree item. In the future we may need to
        // use the ARIA owns property to calculate that if it's present.
        PRInt32 numChildren = 0;

        PRUint32 currentRole = 0;
        rv = xpAccessible->GetFinalRole(&currentRole);
        if (NS_SUCCEEDED(rv) &&
            currentRole == nsIAccessibleRole::ROLE_OUTLINEITEM) {
          nsCOMPtr<nsIAccessible> child;
          xpAccessible->GetFirstChild(getter_AddRefs(child));
          while (child) {
            child->GetFinalRole(&currentRole);
            if (currentRole == nsIAccessibleRole::ROLE_GROUPING) {
              nsCOMPtr<nsIAccessible> groupChild;
              child->GetFirstChild(getter_AddRefs(groupChild));
              while (groupChild) {
                groupChild->GetFinalRole(&currentRole);
                numChildren +=
                  (currentRole == nsIAccessibleRole::ROLE_OUTLINEITEM);
                nsCOMPtr<nsIAccessible> nextGroupChild;
                groupChild->GetNextSibling(getter_AddRefs(nextGroupChild));
                groupChild.swap(nextGroupChild);
              }
              break;
            }
            nsCOMPtr<nsIAccessible> nextChild;
            child->GetNextSibling(getter_AddRefs(nextChild));
            child.swap(nextChild);
          }
        }

        if (numChildren) {
          nsTextFormatter::ssprintf(description,
                                    NS_LITERAL_STRING("L%d, %d of %d with %d").get(),
                                    groupLevel, positionInGroup,
                                    similarItemsInGroup + 1, numChildren);
        } else {
          nsTextFormatter::ssprintf(description,
                                    NS_LITERAL_STRING("L%d, %d of %d").get(),
                                    groupLevel, positionInGroup,
                                    similarItemsInGroup + 1);
        }
      } else { // Position has no level
        nsTextFormatter::ssprintf(description,
                                  NS_LITERAL_STRING("%d of %d").get(),
                                  positionInGroup, similarItemsInGroup + 1);
      }

      *pszDescription = ::SysAllocString(description.get());
      return S_OK;
    }
  }

  xpAccessible->GetDescription(description);
  if (!description.IsEmpty()) {
    // Signal to screen readers that this description is speakable
    // and is not a formatted positional information description
    // Don't localize the "Description: " part of this string, it will be
    // parsed out by assistive technologies.
    description = NS_LITERAL_STRING("Description: ") + description;
  }

  *pszDescription = ::SysAllocString(description.get());
  return S_OK;
}

STDMETHODIMP nsAccessibleWrap::get_accRole(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ VARIANT __RPC_FAR *pvarRole)
{
  VariantInit(pvarRole);

  nsCOMPtr<nsIAccessible> xpAccessible;
  GetXPAccessibleFor(varChild, getter_AddRefs(xpAccessible));

  if (!xpAccessible)
    return E_FAIL;

#ifdef DEBUG_A11Y
  NS_ASSERTION(nsAccessible::IsTextInterfaceSupportCorrect(xpAccessible), "Does not support nsIAccessibleText when it should");
#endif

  PRUint32 xpRole = 0, msaaRole = 0;
  if (NS_FAILED(xpAccessible->GetFinalRole(&xpRole)))
    return E_FAIL;

  msaaRole = gWindowsRoleMap[xpRole].msaaRole;
  NS_ASSERTION(gWindowsRoleMap[nsIAccessibleRole::ROLE_LAST_ENTRY].msaaRole == ROLE_WINDOWS_LAST_ENTRY,
               "MSAA role map skewed");

  // Special case, if there is a ROLE_ROW inside of a ROLE_TREE_TABLE, then call the MSAA role
  // a ROLE_OUTLINEITEM for consistency and compatibility.
  // We need this because ARIA has a role of "row" for both grid and treegrid
  if (xpRole == nsIAccessibleRole::ROLE_ROW) {
    nsCOMPtr<nsIAccessible> parent = GetParent();
    if (parent && Role(parent) == nsIAccessibleRole::ROLE_TREE_TABLE) {
      msaaRole = ROLE_SYSTEM_OUTLINEITEM;
    }
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
  nsCOMPtr<nsIDOMNode> domNode;
  nsCOMPtr<nsIAccessNode> accessNode(do_QueryInterface(xpAccessible));
  if (!accessNode)
    return E_FAIL;

  accessNode->GetDOMNode(getter_AddRefs(domNode));
  nsIContent *content = GetRoleContent(domNode);
  if (!content)
    return E_FAIL;

  if (content->IsNodeOfType(nsINode::eELEMENT)) {
    nsAutoString roleString;
    if (msaaRole != ROLE_SYSTEM_CLIENT && !GetARIARole(content, roleString)) {
      nsINodeInfo *nodeInfo = content->NodeInfo();
      nodeInfo->GetName(roleString);
      nsAutoString nameSpaceURI;
      nodeInfo->GetNamespaceURI(nameSpaceURI);
      if (!nameSpaceURI.IsEmpty()) {
        // Only append name space if different from that of current document
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

STDMETHODIMP nsAccessibleWrap::get_accState(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ VARIANT __RPC_FAR *pvarState)
{
  VariantInit(pvarState);
  pvarState->vt = VT_I4;
  pvarState->lVal = 0;

  nsCOMPtr<nsIAccessible> xpAccessible;
  GetXPAccessibleFor(varChild, getter_AddRefs(xpAccessible));
  if (!xpAccessible)
    return E_FAIL;

  PRUint32 state = 0;
  if (NS_FAILED(xpAccessible->GetFinalState(&state, nsnull)))
    return E_FAIL;

  pvarState->lVal = state;

  return S_OK;
}


STDMETHODIMP nsAccessibleWrap::get_accHelp(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszHelp)
{
  *pszHelp = NULL;
  return S_FALSE;
}

STDMETHODIMP nsAccessibleWrap::get_accHelpTopic(
      /* [out] */ BSTR __RPC_FAR *pszHelpFile,
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ long __RPC_FAR *pidTopic)
{
  *pszHelpFile = NULL;
  *pidTopic = 0;
  return E_NOTIMPL;
}

STDMETHODIMP nsAccessibleWrap::get_accKeyboardShortcut(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszKeyboardShortcut)
{
  *pszKeyboardShortcut = NULL;
  nsCOMPtr<nsIAccessible> xpAccessible;
  GetXPAccessibleFor(varChild, getter_AddRefs(xpAccessible));
  if (xpAccessible) {
    nsAutoString shortcut;
    nsresult rv = xpAccessible->GetKeyboardShortcut(shortcut);
    if (NS_FAILED(rv))
      return S_FALSE;

    *pszKeyboardShortcut = ::SysAllocString(shortcut.get());
    return S_OK;
  }
  return S_FALSE;
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

  if (!mDOMNode) {
    return E_FAIL; // This node is shut down
  }

  VariantInit(pvarChild);

  // Return the current IAccessible child that has focus
  nsCOMPtr<nsIAccessible> focusedAccessible;
  GetFocusedChild(getter_AddRefs(focusedAccessible));
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
  PRUint32 length = 0;
  mArray->GetLength(&length);
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
  *  nsIAccessible object which implement nsIAccessibleSelectable.
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
  VariantInit(pvarChildren);
  pvarChildren->vt = VT_EMPTY;

  nsCOMPtr<nsIAccessibleSelectable> 
    select(do_QueryInterface(static_cast<nsIAccessible*>(this)));

  if (select) {  // do we have an nsIAccessibleSelectable?
    // we have an accessible that can have children selected
    nsCOMPtr<nsIArray> selectedOptions;
    // gets the selected options as nsIAccessibles.
    select->GetSelectedChildren(getter_AddRefs(selectedOptions));
    if (selectedOptions) { // false if the select has no children or none are selected
      // 1) Create and initialize the enumeration
      nsRefPtr<AccessibleEnumerator> pEnum = new AccessibleEnumerator(selectedOptions);

      // 2) Put the enumerator in the VARIANT
      if (!pEnum)
        return E_OUTOFMEMORY;
      pvarChildren->vt = VT_UNKNOWN;    // this must be VT_UNKNOWN for an IEnumVARIANT
      NS_ADDREF(pvarChildren->punkVal = pEnum);
    }
  }
  return S_OK;
}

STDMETHODIMP nsAccessibleWrap::get_accDefaultAction(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszDefaultAction)
{
  *pszDefaultAction = NULL;
  nsCOMPtr<nsIAccessible> xpAccessible;
  GetXPAccessibleFor(varChild, getter_AddRefs(xpAccessible));
  if (xpAccessible) {
    nsAutoString defaultAction;
    if (NS_FAILED(xpAccessible->GetActionName(0, defaultAction)))
      return S_FALSE;

    *pszDefaultAction = ::SysAllocString(defaultAction.get());
  }

  return S_OK;
}

STDMETHODIMP nsAccessibleWrap::accSelect(
      /* [in] */ long flagsSelect,
      /* [optional][in] */ VARIANT varChild)
{
  // currently only handle focus and selection
  nsCOMPtr<nsIAccessible> xpAccessible;
  GetXPAccessibleFor(varChild, getter_AddRefs(xpAccessible));
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

  return E_FAIL;
}

STDMETHODIMP nsAccessibleWrap::accLocation(
      /* [out] */ long __RPC_FAR *pxLeft,
      /* [out] */ long __RPC_FAR *pyTop,
      /* [out] */ long __RPC_FAR *pcxWidth,
      /* [out] */ long __RPC_FAR *pcyHeight,
      /* [optional][in] */ VARIANT varChild)
{
  nsCOMPtr<nsIAccessible> xpAccessible;
  GetXPAccessibleFor(varChild, getter_AddRefs(xpAccessible));

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

  return E_FAIL;
}

STDMETHODIMP nsAccessibleWrap::accNavigate(
      /* [in] */ long navDir,
      /* [optional][in] */ VARIANT varStart,
      /* [retval][out] */ VARIANT __RPC_FAR *pvarEndUpAt)
{
  nsCOMPtr<nsIAccessible> xpAccessibleStart, xpAccessibleResult;
  GetXPAccessibleFor(varStart, getter_AddRefs(xpAccessibleStart));
  if (!xpAccessibleStart)
    return E_FAIL;

  VariantInit(pvarEndUpAt);
  PRUint32 xpRelation = 0;

  switch(navDir) {
    case NAVDIR_DOWN:
      xpAccessibleStart->GetAccessibleBelow(getter_AddRefs(xpAccessibleResult));
      break;
    case NAVDIR_FIRSTCHILD:
      if (!MustPrune(xpAccessibleStart)) {
        xpAccessibleStart->GetFirstChild(getter_AddRefs(xpAccessibleResult));
      }
      break;
    case NAVDIR_LASTCHILD:
      if (!MustPrune(xpAccessibleStart)) {
        xpAccessibleStart->GetLastChild(getter_AddRefs(xpAccessibleResult));
      }
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
    nsresult rv = GetAccessibleRelated(xpRelation,
                                       getter_AddRefs(xpAccessibleResult));
    if (rv == NS_ERROR_NOT_IMPLEMENTED) {
      return E_NOTIMPL;
    }
  }

  if (xpAccessibleResult) {
    pvarEndUpAt->pdispVal = NativeAccessible(xpAccessibleResult);
    pvarEndUpAt->vt = VT_DISPATCH;
    return NS_OK;
  }
  return E_FAIL;
}

STDMETHODIMP nsAccessibleWrap::accHitTest(
      /* [in] */ long xLeft,
      /* [in] */ long yTop,
      /* [retval][out] */ VARIANT __RPC_FAR *pvarChild)
{
  VariantInit(pvarChild);

  // convert to window coords
  nsCOMPtr<nsIAccessible> xpAccessible;

  xLeft = xLeft;
  yTop = yTop;

  if (MustPrune(this)) {
    xpAccessible = this;
  }
  else {
    GetChildAtPoint(xLeft, yTop, getter_AddRefs(xpAccessible));
  }

  // if we got a child
  if (xpAccessible) {
    // if the child is us
    if (xpAccessible == static_cast<nsIAccessible*>(this)) {
      pvarChild->vt = VT_I4;
      pvarChild->lVal = CHILDID_SELF;
    } else { // its not create an Accessible for it.
      pvarChild->vt = VT_DISPATCH;
      pvarChild->pdispVal = NativeAccessible(xpAccessible);
      nsCOMPtr<nsIAccessNode> accessNode(do_QueryInterface(xpAccessible));
      NS_ASSERTION(accessNode, "Unable to QI to nsIAccessNode");
      nsCOMPtr<nsIDOMNode> domNode;
      accessNode->GetDOMNode(getter_AddRefs(domNode));
      if (!domNode) {
        // Has already been shut down
        pvarChild->vt = VT_EMPTY;
        return E_FAIL;
      }
    }
  } else {
    // no child at that point
    pvarChild->vt = VT_EMPTY;
    return E_FAIL;
  }

  return S_OK;
}

STDMETHODIMP nsAccessibleWrap::accDoDefaultAction(
      /* [optional][in] */ VARIANT varChild)
{
  nsCOMPtr<nsIAccessible> xpAccessible;
  GetXPAccessibleFor(varChild, getter_AddRefs(xpAccessible));

  if (!xpAccessible || FAILED(xpAccessible->DoAction(0))) {
    return E_FAIL;
  }
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

STDMETHODIMP
nsAccessibleWrap::Next(ULONG aNumElementsRequested, VARIANT FAR* pvar, ULONG FAR* aNumElementsFetched)
{
  // If there are two clients using this at the same time, and they are
  // each using a different mEnumVariant position it would be bad, because
  // we have only 1 object and can only keep of mEnumVARIANT position once

  // Children already cached via QI to IEnumVARIANT
  *aNumElementsFetched = 0;

  PRInt32 numChildren;
  GetChildCount(&numChildren);

  if (aNumElementsRequested <= 0 || !pvar ||
      mEnumVARIANTPosition >= numChildren) {
    return E_FAIL;
  }

  VARIANT varStart;
  VariantInit(&varStart);
  varStart.lVal = CHILDID_SELF;
  varStart.vt = VT_I4;

  accNavigate(NAVDIR_FIRSTCHILD, varStart, &pvar[0]);

  for (long childIndex = 0; pvar[*aNumElementsFetched].vt == VT_DISPATCH; ++childIndex) {
    PRBool wasAccessibleFetched = PR_FALSE;
    nsAccessibleWrap *msaaAccessible =
      static_cast<nsAccessibleWrap*>(pvar[*aNumElementsFetched].pdispVal);
    if (!msaaAccessible)
      break;
    if (childIndex >= mEnumVARIANTPosition) {
      if (++*aNumElementsFetched >= aNumElementsRequested)
        break;
      wasAccessibleFetched = PR_TRUE;
    }
    msaaAccessible->accNavigate(NAVDIR_NEXT, varStart, &pvar[*aNumElementsFetched] );
    if (!wasAccessibleFetched)
      msaaAccessible->nsAccessNode::Release(); // this accessible will not be received by the caller
  }

  mEnumVARIANTPosition += static_cast<PRUint16>(*aNumElementsFetched);
  return NOERROR;
}

STDMETHODIMP
nsAccessibleWrap::Skip(ULONG aNumElements)
{
  mEnumVARIANTPosition += static_cast<PRUint16>(aNumElements);

  PRInt32 numChildren;
  GetChildCount(&numChildren);

  if (mEnumVARIANTPosition > numChildren)
  {
    mEnumVARIANTPosition = numChildren;
    return S_FALSE;
  }
  return NOERROR;
}

STDMETHODIMP
nsAccessibleWrap::Reset(void)
{
  mEnumVARIANTPosition = 0;
  return NOERROR;
}


// IAccessible2

STDMETHODIMP
nsAccessibleWrap::get_nRelations(long *aNRelations)
{
  PRUint32 count = 0;
  nsresult rv = GetRelationsCount(&count);
  *aNRelations = count;

  return NS_FAILED(rv) ? E_FAIL : S_OK;
}

STDMETHODIMP
nsAccessibleWrap::get_relation(long aRelationIndex,
                               IAccessibleRelation **aRelation)
{
  nsCOMPtr<nsIAccessibleRelation> relation;
  nsresult rv = GetRelation(aRelationIndex, getter_AddRefs(relation));
  if (NS_FAILED(rv))
    return E_FAIL;

  nsCOMPtr<nsIWinAccessNode> winAccessNode(do_QueryInterface(relation));
  if (!winAccessNode)
    return E_FAIL;

  void *instancePtr = NULL;
  rv =  winAccessNode->QueryNativeInterface(IID_IAccessibleRelation,
                                            &instancePtr);
  if (NS_FAILED(rv))
    return E_FAIL;

  *aRelation = static_cast<IAccessibleRelation*>(instancePtr);
  return S_OK;
}

STDMETHODIMP
nsAccessibleWrap::get_relations(long aMaxRelations,
                                IAccessibleRelation **aRelation,
                                long *aNRelations)
{
  *aNRelations = 0;

  nsCOMPtr<nsIArray> relations;
  nsresult rv = GetRelations(getter_AddRefs(relations));
  if (NS_FAILED(rv))
    return E_FAIL;

  PRUint32 length = 0;
  rv = relations->GetLength(&length);
  if (NS_FAILED(rv))
    return E_FAIL;

  PRUint32 count = length < (PRUint32)aMaxRelations ? length : aMaxRelations;

  PRUint32 index = 0;
  for (; index < count; index++) {
    nsCOMPtr<nsIWinAccessNode> winAccessNode(do_QueryElementAt(relations, index, &rv));
    if (NS_FAILED(rv) || !winAccessNode)
      break;

    void *instancePtr = NULL;
    nsresult rv =  winAccessNode->QueryNativeInterface(IID_IAccessibleRelation,
                                                       &instancePtr);
    if (NS_FAILED(rv))
      break;

    aRelation[index] = static_cast<IAccessibleRelation*>(instancePtr);
  }

  if (NS_FAILED(rv)) {
    for (PRUint32 index2 = 0; index2 < index; index2++) {
      aRelation[index2]->Release();
      aRelation[index2] = NULL;
    }
    return E_FAIL;
  }

  *aNRelations = count;
  return S_OK;
}

STDMETHODIMP
nsAccessibleWrap::role(long *role)
{
  PRUint32 xpRole = 0;
  if (NS_FAILED(GetFinalRole(&xpRole)))
    return E_FAIL;

  NS_ASSERTION(gWindowsRoleMap[nsIAccessibleRole::ROLE_LAST_ENTRY].ia2Role == ROLE_WINDOWS_LAST_ENTRY,
               "MSAA role map skewed");

  *role = gWindowsRoleMap[xpRole].ia2Role;

  return S_OK;
}

STDMETHODIMP
nsAccessibleWrap::scrollTo(enum IA2ScrollType aScrollType)
{
  if (NS_SUCCEEDED(ScrollTo(aScrollType)))
    return S_OK;
  return E_FAIL;
}

STDMETHODIMP
nsAccessibleWrap::scrollToPoint(enum IA2CoordinateType coordinateType,
                                long x, long y)
{
  return E_NOTIMPL;
}

STDMETHODIMP
nsAccessibleWrap::get_groupPosition(long *aGroupLevel,
                                    long *aSimilarItemsInGroup,
                                    long *aPositionInGroup)
{
  PRInt32 groupLevel = 0;
  PRInt32 similarItemsInGroup = 0;
  PRInt32 positionInGroup = 0;
  nsresult rv = GroupPosition(&groupLevel, &similarItemsInGroup,
                              &positionInGroup);

  if (NS_SUCCEEDED(rv)) {
   *aGroupLevel = groupLevel;
   *aSimilarItemsInGroup = similarItemsInGroup;
   *aPositionInGroup = positionInGroup;
    return S_OK;
  }

  return E_FAIL;
}

STDMETHODIMP
nsAccessibleWrap::get_states(AccessibleStates *aStates)
{
  *aStates = 0;

  // XXX: bug 344674 should come with better approach that we have here.

  PRUint32 states = 0, extraStates = 0;
  nsresult rv = GetFinalState(&states, &extraStates);
  if (NS_FAILED(rv))
    return E_FAIL;

  if (states & nsIAccessibleStates::STATE_INVALID)
    *aStates |= IA2_STATE_INVALID_ENTRY;
  if (states & nsIAccessibleStates::STATE_REQUIRED)
    *aStates |= IA2_STATE_REQUIRED;

  // The following IA2 states are not supported by Gecko
  // IA2_STATE_ARMED
  // IA2_STATE_MANAGES_DESCENDANTS
  // IA2_STATE_ICONIFIED
  // IA2_STATE_INVALID // This is not a state, it is the absence of a state

  if (extraStates & nsIAccessibleStates::EXT_STATE_ACTIVE)
    *aStates |= IA2_STATE_ACTIVE;
  if (extraStates & nsIAccessibleStates::EXT_STATE_DEFUNCT)
    *aStates |= IA2_STATE_DEFUNCT;
  if (extraStates & nsIAccessibleStates::EXT_STATE_EDITABLE)
    *aStates |= IA2_STATE_EDITABLE;
  if (extraStates & nsIAccessibleStates::EXT_STATE_HORIZONTAL)
    *aStates |= IA2_STATE_HORIZONTAL;
  if (extraStates & nsIAccessibleStates::EXT_STATE_MODAL)
    *aStates |= IA2_STATE_MODAL;
  if (extraStates & nsIAccessibleStates::EXT_STATE_MULTI_LINE)
    *aStates |= IA2_STATE_MULTI_LINE;
  if (extraStates & nsIAccessibleStates::EXT_STATE_OPAQUE)
    *aStates |= IA2_STATE_OPAQUE;
  if (extraStates & nsIAccessibleStates::EXT_STATE_SELECTABLE_TEXT)
    *aStates |= IA2_STATE_SELECTABLE_TEXT;
  if (extraStates & nsIAccessibleStates::EXT_STATE_SINGLE_LINE)
    *aStates |= IA2_STATE_SINGLE_LINE;
  if (extraStates & nsIAccessibleStates::EXT_STATE_STALE)
    *aStates |= IA2_STATE_STALE;
  if (extraStates & nsIAccessibleStates::EXT_STATE_SUPPORTS_AUTOCOMPLETION)
    *aStates |= IA2_STATE_SUPPORTS_AUTOCOMPLETION;
  if (extraStates & nsIAccessibleStates::EXT_STATE_TRANSIENT)
    *aStates |= IA2_STATE_TRANSIENT;
  if (extraStates & nsIAccessibleStates::EXT_STATE_VERTICAL)
    *aStates |= IA2_STATE_VERTICAL;

  return S_OK;
}

STDMETHODIMP
nsAccessibleWrap::get_extendedRole(BSTR *extendedRole)
{
  return E_NOTIMPL;
}

STDMETHODIMP
nsAccessibleWrap::get_localizedExtendedRole(BSTR *localizedExtendedRole)
{
  return E_NOTIMPL;
}

STDMETHODIMP
nsAccessibleWrap::get_nExtendedStates(long *nExtendedStates)
{
  *nExtendedStates = 0;
  return E_NOTIMPL;
}

STDMETHODIMP
nsAccessibleWrap::get_extendedStates(long maxExtendedStates,
                                     BSTR **extendedStates,
                                     long *nExtendedStates)
{
  *nExtendedStates = 0;
  return E_NOTIMPL;
}

STDMETHODIMP
nsAccessibleWrap::get_localizedExtendedStates(long maxLocalizedExtendedStates,
                                              BSTR **localizedExtendedStates,
                                              long *nLocalizedExtendedStates)
{
  *nLocalizedExtendedStates = 0;
  return E_NOTIMPL;
}

STDMETHODIMP
nsAccessibleWrap::get_uniqueID(long *uniqueID)
{
  void *id;
  if (NS_SUCCEEDED(GetUniqueID(&id))) {
    *uniqueID = reinterpret_cast<long>(id);
    return S_OK;
  }
  return E_FAIL;
}

STDMETHODIMP
nsAccessibleWrap::get_windowHandle(HWND *windowHandle)
{
  void **handle = nsnull;
  if (NS_SUCCEEDED(GetOwnerWindow(handle))) {
    *windowHandle = reinterpret_cast<HWND>(*handle);
    return S_OK;
  }
  return E_FAIL;
}

STDMETHODIMP
nsAccessibleWrap::get_indexInParent(long *indexInParent)
{
  PRInt32 index;
  if (NS_SUCCEEDED(GetIndexInParent(&index))) {
    *indexInParent = index;
    return S_OK;
  }
  return E_FAIL;
}

STDMETHODIMP
nsAccessibleWrap::get_locale(IA2Locale *locale)
{
  return E_NOTIMPL;
}

STDMETHODIMP
nsAccessibleWrap::get_attributes(BSTR *aAttributes)
{
  // The format is name:value;name:value; with \ for escaping these
  // characters ":;=,\".

  *aAttributes = NULL;

  nsCOMPtr<nsIPersistentProperties> attributes;
  if (NS_FAILED(GetAttributes(getter_AddRefs(attributes))))
    return E_FAIL;

  if (!attributes)
    return S_OK;

  nsCOMPtr<nsISimpleEnumerator> propEnum;
  attributes->Enumerate(getter_AddRefs(propEnum));
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

  *aAttributes = ::SysAllocString(strAttrs.get());
  return S_OK;
}

STDMETHODIMP
nsAccessibleWrap::Clone(IEnumVARIANT FAR* FAR* ppenum)
{
  // Clone could be bad, the cloned items aren't tracked for shutdown
  // Then again, as long as the client releases the items in time, we're okay
  *ppenum = nsnull;

  nsAccessibleWrap *accessibleWrap = new nsAccessibleWrap(mDOMNode, mWeakShell);
  if (!accessibleWrap)
    return E_FAIL;

  IAccessible *msaaAccessible = static_cast<IAccessible*>(accessibleWrap);
  msaaAccessible->AddRef();
  QueryInterface(IID_IEnumVARIANT, (void**)ppenum);
  if (*ppenum)
    (*ppenum)->Skip(mEnumVARIANTPosition); // QI addrefed
  msaaAccessible->Release();

  return NOERROR;
}


// For IDispatch support
STDMETHODIMP
nsAccessibleWrap::GetTypeInfoCount(UINT *p)
{
  *p = 0;
  return E_NOTIMPL;
}

// For IDispatch support
STDMETHODIMP nsAccessibleWrap::GetTypeInfo(UINT i, LCID lcid, ITypeInfo **ppti)
{
  *ppti = 0;
  return E_NOTIMPL;
}

// For IDispatch support
STDMETHODIMP
nsAccessibleWrap::GetIDsOfNames(REFIID riid, LPOLESTR *rgszNames,
                           UINT cNames, LCID lcid, DISPID *rgDispId)
{
  return E_NOTIMPL;
}

// For IDispatch support
STDMETHODIMP nsAccessibleWrap::Invoke(DISPID dispIdMember, REFIID riid,
    LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
    VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
  return E_NOTIMPL;
}


NS_IMETHODIMP nsAccessibleWrap::GetNativeInterface(void **aOutAccessible)
{
  *aOutAccessible = static_cast<IAccessible*>(this);
  NS_ADDREF_THIS();
  return NS_OK;
}

// nsPIAccessible

NS_IMETHODIMP
nsAccessibleWrap::FireAccessibleEvent(nsIAccessibleEvent *aEvent)
{
  NS_ENSURE_ARG(aEvent);

  nsresult rv = nsAccessible::FireAccessibleEvent(aEvent);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 eventType = 0;
  aEvent->GetEventType(&eventType);

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

  nsCOMPtr<nsIAccessible> accessible;
  aEvent->GetAccessible(getter_AddRefs(accessible));
  if (!accessible)
    return NS_OK;

  PRUint32 role = ROLE_SYSTEM_TEXT; // Default value

  nsCOMPtr<nsIAccessNode> accessNode(do_QueryInterface(accessible));
  NS_ENSURE_STATE(accessNode);

  if (eventType == nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED ||
      eventType == nsIAccessibleEvent::EVENT_FOCUS) {
    UpdateSystemCaret();
  }
 
  PRInt32 childID = GetChildIDFor(accessible); // get the id for the accessible
  if (!childID)
    return NS_OK; // Can't fire an event without a child ID

  // See if we're in a scrollable area with its own window
  nsCOMPtr<nsIAccessible> newAccessible;
  if (eventType == nsIAccessibleEvent::EVENT_ASYNCH_HIDE ||
      eventType == nsIAccessibleEvent::EVENT_DOM_DESTROY) {
    // Don't use frame from current accessible when we're hiding that
    // accessible.
    accessible->GetParent(getter_AddRefs(newAccessible));
  } else {
    newAccessible = accessible;
  }

  HWND hWnd = GetHWNDFor(accessible);
  NS_ENSURE_TRUE(hWnd, NS_ERROR_FAILURE);

  // Gecko uses two windows for every scrollable area. One window contains
  // scrollbars and the child window contains only the client area.
  // Details of the 2 window system:
  // * Scrollbar window: caret drawing window & return value for WindowFromAccessibleObject()
  // * Client area window: text drawing window & MSAA event window

  // Fire MSAA event for client area window.
  NotifyWinEvent(winEvent, hWnd, OBJID_CLIENT, childID);

  return NS_OK;
}

//------- Helper methods ---------

PRInt32 nsAccessibleWrap::GetChildIDFor(nsIAccessible* aAccessible)
{
  // A child ID of the window is required, when we use NotifyWinEvent,
  // so that the 3rd party application can call back and get the IAccessible
  // the event occured on.

  void *uniqueID;
  nsCOMPtr<nsIAccessNode> accessNode(do_QueryInterface(aAccessible));
  if (!accessNode) {
    return 0;
  }
  accessNode->GetUniqueID(&uniqueID);

  // Yes, this means we're only compatibible with 32 bit
  // MSAA is only available for 32 bit windows, so it's okay
  return - NS_PTR_TO_INT32(uniqueID);
}

HWND
nsAccessibleWrap::GetHWNDFor(nsIAccessible *aAccessible)
{
  nsCOMPtr<nsIAccessNode> accessNode(do_QueryInterface(aAccessible));
  nsCOMPtr<nsPIAccessNode> privateAccessNode(do_QueryInterface(accessNode));
  if (!privateAccessNode)
    return 0;

  HWND hWnd = 0;

  nsIFrame *frame = privateAccessNode->GetFrame();
  if (frame) {
    nsIWidget *window = frame->GetWindow();
    PRBool isVisible;
    window->IsVisible(isVisible);
    if (isVisible) {
      // Short explanation:
      // If HWND for frame is inside a hidden window, fire the event on the
      // containing document's visible window.
      //
      // Long explanation:
      // This is really just to fix combo boxes with JAWS. Window-Eyes already
      // worked with combo boxes because they use the value change event in
      // the closed combo box case. JAWS will only pay attention to the focus
      // events on the list items. The JAWS developers haven't fixed that, so
      // we'll use the focus events to make JAWS work. However, JAWS is
      // ignoring events on a hidden window. So, in order to fix the bug where
      // JAWS doesn't echo the current option as it changes in a closed
      // combo box, we need to use an ensure that we never fire an event with
      // an HWND for a hidden window.
      hWnd = (HWND)frame->GetWindow()->GetNativeData(NS_NATIVE_WINDOW);
    }
  }

  if (!hWnd) {
    void* handle = nsnull;
    nsCOMPtr<nsIAccessibleDocument> accessibleDoc;
    accessNode->GetAccessibleDocument(getter_AddRefs(accessibleDoc));
    if (!accessibleDoc)
      return 0;

    accessibleDoc->GetWindowHandle(&handle);
    hWnd = (HWND)handle;
  }

  return hWnd;
}

IDispatch *nsAccessibleWrap::NativeAccessible(nsIAccessible *aXPAccessible)
{
  if (!aXPAccessible) {
   NS_WARNING("Not passing in an aXPAccessible");
   return NULL;
  }

  nsCOMPtr<nsIAccessibleWin32Object> accObject(do_QueryInterface(aXPAccessible));
  if (accObject) {
    void* hwnd;
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


void nsAccessibleWrap::GetXPAccessibleFor(const VARIANT& aVarChild, nsIAccessible **aXPAccessible)
{
  *aXPAccessible = nsnull;
  if (!mWeakShell)
    return; // Fail, we don't want to do anything after we've shut down

  // if its us real easy - this seems to always be the case
  if (aVarChild.lVal == CHILDID_SELF) {
    *aXPAccessible = static_cast<nsIAccessible*>(this);
  }
  else if (MustPrune(this)) {
    return;
  }
  else {
    // XXX It is rare to use a VARIANT with a child num
    // so optimizing this is not a priority
    // We can come back it do it later, if there are perf problems
    // with a specific assistive technology
    nsCOMPtr<nsIAccessible> xpAccessible, nextAccessible;
    GetFirstChild(getter_AddRefs(xpAccessible));
    for (PRInt32 index = 0; xpAccessible; index ++) {
      if (!xpAccessible)
        break; // Failed
      if (aVarChild.lVal == index) {
        *aXPAccessible = xpAccessible;
        break;
      }
      nextAccessible = xpAccessible;
      nextAccessible->GetNextSibling(getter_AddRefs(xpAccessible));
    }
  }
  NS_IF_ADDREF(*aXPAccessible);
}

void nsAccessibleWrap::UpdateSystemCaret()
{
  // Move the system caret so that Windows Tablet Edition and tradional ATs with 
  // off-screen model can follow the caret
  ::DestroyCaret();

  nsRefPtr<nsRootAccessible> rootAccessible = GetRootAccessible();
  if (!rootAccessible) {
    return;
  }

  nsRefPtr<nsCaretAccessible> caretAccessible = rootAccessible->GetCaretAccessible();
  if (!caretAccessible) {
    return;
  }

  nsIWidget *widget;
  nsRect caretRect = caretAccessible->GetCaretRect(&widget);        
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
