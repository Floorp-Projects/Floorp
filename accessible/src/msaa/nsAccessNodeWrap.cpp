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

#include "nsAccessNodeWrap.h"
#include "ISimpleDOMNode_i.c"
#include "nsAccessibilityAtoms.h"
#include "nsIAccessibilityService.h"
#include "nsIAccessible.h"
#include "nsAttrName.h"
#include "nsIDocument.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMNSHTMLElement.h"
#include "nsIDOMViewCSS.h"
#include "nsIFrame.h"
#include "nsINameSpaceManager.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPresShell.h"
#include "nsPIDOMWindow.h"
#include "nsRootAccessible.h"
#include "nsIServiceManager.h"
#include "AccessibleApplication.h"
#include "nsApplicationAccessibleWrap.h"

/// the accessible library and cached methods
HINSTANCE nsAccessNodeWrap::gmAccLib = nsnull;
HINSTANCE nsAccessNodeWrap::gmUserLib = nsnull;
LPFNACCESSIBLEOBJECTFROMWINDOW nsAccessNodeWrap::gmAccessibleObjectFromWindow = nsnull;
LPFNNOTIFYWINEVENT nsAccessNodeWrap::gmNotifyWinEvent = nsnull;
LPFNGETGUITHREADINFO nsAccessNodeWrap::gmGetGUIThreadInfo = nsnull;

PRBool nsAccessNodeWrap::gIsEnumVariantSupportDisabled = 0;

nsIAccessibleTextChangeEvent *nsAccessNodeWrap::gTextEvent = nsnull;


/* For documentation of the accessibility architecture, 
 * see http://lxr.mozilla.org/seamonkey/source/accessible/accessible-docs.html
 */

/*
 * Class nsAccessNodeWrap
 */

//-----------------------------------------------------
// construction 
//-----------------------------------------------------

nsAccessNodeWrap::nsAccessNodeWrap(nsIDOMNode *aNode, nsIWeakReference* aShell): 
  nsAccessNode(aNode, aShell)
{
}

//-----------------------------------------------------
// destruction
//-----------------------------------------------------
nsAccessNodeWrap::~nsAccessNodeWrap()
{
}

//-----------------------------------------------------
// nsISupports methods
//-----------------------------------------------------

NS_IMPL_ISUPPORTS_INHERITED1(nsAccessNodeWrap, nsAccessNode, nsIWinAccessNode);

//-----------------------------------------------------
// nsIWinAccessNode methods
//-----------------------------------------------------

NS_IMETHODIMP
nsAccessNodeWrap::QueryNativeInterface(REFIID aIID, void** aInstancePtr)
{
  return QueryInterface(aIID, aInstancePtr);
}

//-----------------------------------------------------
// IUnknown interface methods - see iunknown.h for documentation
//-----------------------------------------------------

STDMETHODIMP nsAccessNodeWrap::QueryInterface(REFIID iid, void** ppv)
{
  *ppv = nsnull;

  if (IID_IUnknown == iid || IID_ISimpleDOMNode == iid)
    *ppv = static_cast<ISimpleDOMNode*>(this);

  if (nsnull == *ppv)
    return E_NOINTERFACE;      //iid not supported.
   
  (reinterpret_cast<IUnknown*>(*ppv))->AddRef(); 
  return S_OK;
}

STDMETHODIMP
nsAccessNodeWrap::QueryService(REFGUID guidService, REFIID iid, void** ppv)
{
  // Can get to IAccessibleApplication from any node via QS
  if (iid == IID_IAccessibleApplication) {
    nsRefPtr<nsApplicationAccessibleWrap> app =
      GetApplicationAccessible();
    nsresult rv = app->QueryNativeInterface(iid, ppv);
    return NS_SUCCEEDED(rv) ? S_OK : E_NOINTERFACE;
  }

  /**
   * To get an ISimpleDOMNode, ISimpleDOMDocument, ISimpleDOMText
   * or any IAccessible2 interface on should use IServiceProvider like this:
   * -----------------------------------------------------------------------
   * ISimpleDOMDocument *pAccDoc = NULL;
   * IServiceProvider *pServProv = NULL;
   * pAcc->QueryInterface(IID_IServiceProvider, (void**)&pServProv);
   * if (pServProv) {
   *   const GUID unused;
   *   pServProv->QueryService(unused, IID_ISimpleDOMDocument, (void**)&pAccDoc);
   *   pServProv->Release();
   * }
   */

  return QueryInterface(iid, ppv);
}

//-----------------------------------------------------
// ISimpleDOMNode methods
//-----------------------------------------------------

STDMETHODIMP nsAccessNodeWrap::get_nodeInfo( 
    /* [out] */ BSTR __RPC_FAR *aNodeName,
    /* [out] */ short __RPC_FAR *aNameSpaceID,
    /* [out] */ BSTR __RPC_FAR *aNodeValue,
    /* [out] */ unsigned int __RPC_FAR *aNumChildren,
    /* [out] */ unsigned int __RPC_FAR *aUniqueID,
    /* [out] */ unsigned short __RPC_FAR *aNodeType)
{
  *aNodeName = nsnull;
  *aNodeValue = nsnull;

  if (!mDOMNode)
    return E_FAIL;
 
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));

  PRUint16 nodeType = 0;
  mDOMNode->GetNodeType(&nodeType);
  *aNodeType=static_cast<unsigned short>(nodeType);

  if (*aNodeType !=  NODETYPE_TEXT) {
    nsAutoString nodeName;
    mDOMNode->GetNodeName(nodeName);
    *aNodeName =   ::SysAllocString(nodeName.get());
  }

  nsAutoString nodeValue;

  mDOMNode->GetNodeValue(nodeValue);
  *aNodeValue = ::SysAllocString(nodeValue.get());
  *aNameSpaceID = content ? static_cast<short>(content->GetNameSpaceID()) : 0;

  // This is a unique ID for every content node.  The 3rd party
  // accessibility application can compare this to the childID we
  // return for events such as focus events, to correlate back to
  // data nodes in their internal object model.
  void *uniqueID;
  GetUniqueID(&uniqueID);
  *aUniqueID = - NS_PTR_TO_INT32(uniqueID);

  *aNumChildren = 0;
  PRUint32 numChildren = 0;
  nsCOMPtr<nsIDOMNodeList> nodeList;
  mDOMNode->GetChildNodes(getter_AddRefs(nodeList));
  if (nodeList && NS_OK == nodeList->GetLength(&numChildren))
    *aNumChildren = static_cast<unsigned int>(numChildren);

  return S_OK;
}


       
STDMETHODIMP nsAccessNodeWrap::get_attributes( 
    /* [in] */ unsigned short aMaxAttribs,
    /* [length_is][size_is][out] */ BSTR __RPC_FAR *aAttribNames,
    /* [length_is][size_is][out] */ short __RPC_FAR *aNameSpaceIDs,
    /* [length_is][size_is][out] */ BSTR __RPC_FAR *aAttribValues,
    /* [out] */ unsigned short __RPC_FAR *aNumAttribs)
{
  *aNumAttribs = 0;

  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  if (!content) 
    return E_FAIL;

  PRUint32 numAttribs = content->GetAttrCount();

  if (numAttribs > aMaxAttribs)
    numAttribs = aMaxAttribs;
  *aNumAttribs = static_cast<unsigned short>(numAttribs);

  for (PRUint32 index = 0; index < numAttribs; index++) {
    aNameSpaceIDs[index] = 0; aAttribValues[index] = aAttribNames[index] = nsnull;
    nsAutoString attributeValue;
    const char *pszAttributeName; 

    const nsAttrName* name = content->GetAttrNameAt(index);
    aNameSpaceIDs[index] = static_cast<short>(name->NamespaceID());
    name->LocalName()->GetUTF8String(&pszAttributeName);
    aAttribNames[index] = ::SysAllocString(NS_ConvertUTF8toUTF16(pszAttributeName).get());
    content->GetAttr(name->NamespaceID(), name->LocalName(), attributeValue);
    aAttribValues[index] = ::SysAllocString(attributeValue.get());
  }

  return S_OK; 
}
        

STDMETHODIMP nsAccessNodeWrap::get_attributesForNames( 
    /* [in] */ unsigned short aNumAttribs,
    /* [length_is][size_is][in] */ BSTR __RPC_FAR *aAttribNames,
    /* [length_is][size_is][in] */ short __RPC_FAR *aNameSpaceID,
    /* [length_is][size_is][retval] */ BSTR __RPC_FAR *aAttribValues)
{
  nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(mDOMNode));
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));

  if (!domElement || !content) 
    return E_FAIL;

  if (!content->GetDocument())
    return E_FAIL;

  nsCOMPtr<nsINameSpaceManager> nameSpaceManager =
    do_GetService(NS_NAMESPACEMANAGER_CONTRACTID);

  PRInt32 index;

  for (index = 0; index < aNumAttribs; index++) {
    aAttribValues[index] = nsnull;
    if (aAttribNames[index]) {
      nsAutoString attributeValue, nameSpaceURI;
      nsAutoString attributeName(nsDependentString(static_cast<PRUnichar*>(aAttribNames[index])));
      nsresult rv;

      if (aNameSpaceID[index]>0 && 
        NS_SUCCEEDED(nameSpaceManager->GetNameSpaceURI(aNameSpaceID[index], nameSpaceURI)))
          rv = domElement->GetAttributeNS(nameSpaceURI, attributeName, attributeValue);
      else 
        rv = domElement->GetAttribute(attributeName, attributeValue);

      if (NS_SUCCEEDED(rv))
        aAttribValues[index] = ::SysAllocString(attributeValue.get());
    }
  }

  return S_OK; 
}

/* To do: use media type if not null */
STDMETHODIMP nsAccessNodeWrap::get_computedStyle( 
    /* [in] */ unsigned short aMaxStyleProperties,
    /* [in] */ boolean aUseAlternateView,
    /* [length_is][size_is][out] */ BSTR __RPC_FAR *aStyleProperties,
    /* [length_is][size_is][out] */ BSTR __RPC_FAR *aStyleValues,
    /* [out] */ unsigned short __RPC_FAR *aNumStyleProperties)
{
  nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(mDOMNode));
  if (!domElement)
    return E_FAIL;
  
  *aNumStyleProperties = 0;
  nsCOMPtr<nsIDOMCSSStyleDeclaration> cssDecl;
  GetComputedStyleDeclaration(EmptyString(), domElement, getter_AddRefs(cssDecl));
  NS_ENSURE_TRUE(cssDecl, E_FAIL);

  PRUint32 length;
  cssDecl->GetLength(&length);

  PRUint32 index, realIndex;
  for (index = realIndex = 0; index < length && realIndex < aMaxStyleProperties; index ++) {
    nsAutoString property, value;
    if (NS_SUCCEEDED(cssDecl->Item(index, property)) && property.CharAt(0) != '-')  // Ignore -moz-* properties
      cssDecl->GetPropertyValue(property, value);  // Get property value
    if (!value.IsEmpty()) {
      aStyleProperties[realIndex] =   ::SysAllocString(property.get());
      aStyleValues[realIndex]     =   ::SysAllocString(value.get());
      ++realIndex;
    }
  }
  *aNumStyleProperties = static_cast<unsigned short>(realIndex);

  return S_OK;
}


STDMETHODIMP nsAccessNodeWrap::get_computedStyleForProperties( 
    /* [in] */ unsigned short aNumStyleProperties,
    /* [in] */ boolean aUseAlternateView,
    /* [length_is][size_is][in] */ BSTR __RPC_FAR *aStyleProperties,
    /* [length_is][size_is][out] */ BSTR __RPC_FAR *aStyleValues)
{
  nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(mDOMNode));
  if (!domElement)
    return E_FAIL;
 
  nsCOMPtr<nsIDOMCSSStyleDeclaration> cssDecl;
  GetComputedStyleDeclaration(EmptyString(), domElement, getter_AddRefs(cssDecl));
  NS_ENSURE_TRUE(cssDecl, E_FAIL);

  PRUint32 index;
  for (index = 0; index < aNumStyleProperties; index ++) {
    nsAutoString value;
    if (aStyleProperties[index])
      cssDecl->GetPropertyValue(nsDependentString(static_cast<PRUnichar*>(aStyleProperties[index])), value);  // Get property value
    aStyleValues[index] = ::SysAllocString(value.get());
  }

  return S_OK;
}

STDMETHODIMP nsAccessNodeWrap::scrollTo(/* [in] */ boolean aScrollTopLeft)
{
  PRUint32 scrollType =
    aScrollTopLeft ? nsIAccessibleScrollType::SCROLL_TYPE_TOP_LEFT :
                     nsIAccessibleScrollType::SCROLL_TYPE_BOTTOM_RIGHT;

  nsresult rv = ScrollTo(scrollType);
  if (NS_SUCCEEDED(rv))
    return S_OK;

  return E_FAIL;
}

ISimpleDOMNode* nsAccessNodeWrap::MakeAccessNode(nsIDOMNode *node)
{
  if (!node) 
    return NULL;

  nsAccessNodeWrap *newNode = NULL;
  
  nsCOMPtr<nsIContent> content(do_QueryInterface(node));
  nsCOMPtr<nsIDocument> doc;

  if (content) 
    doc = content->GetDocument();
  else {
    // Get the document via QueryInterface, since there is no content node
    doc = do_QueryInterface(node);
    content = do_QueryInterface(node);
  }

  if (!doc)
    return NULL;

  nsCOMPtr<nsIAccessibilityService> accService(do_GetService("@mozilla.org/accessibilityService;1"));
  if (!accService)
    return NULL;

  ISimpleDOMNode *iNode = NULL;
  nsCOMPtr<nsIAccessible> nsAcc;
  accService->GetAccessibleInWeakShell(node, mWeakShell, getter_AddRefs(nsAcc));
  if (nsAcc) {
    nsCOMPtr<nsIAccessNode> accessNode(do_QueryInterface(nsAcc));
    NS_ASSERTION(accessNode, "nsIAccessible impl does not inherit from nsIAccessNode");
    IAccessible *msaaAccessible;
    nsAcc->GetNativeInterface((void**)&msaaAccessible); // addrefs
    msaaAccessible->QueryInterface(IID_ISimpleDOMNode, (void**)&iNode); // addrefs
    msaaAccessible->Release(); // Release IAccessible
  }
  else {
    newNode = new nsAccessNodeWrap(node, mWeakShell);
    if (!newNode)
      return NULL;

    newNode->Init();
    iNode = static_cast<ISimpleDOMNode*>(newNode);
    iNode->AddRef();
  }

  return iNode;
}


STDMETHODIMP nsAccessNodeWrap::get_parentNode(ISimpleDOMNode __RPC_FAR *__RPC_FAR *aNode)
{
  if (!mDOMNode)
    return E_FAIL;
 
  nsCOMPtr<nsIDOMNode> node;
  mDOMNode->GetParentNode(getter_AddRefs(node));
  *aNode = MakeAccessNode(node);

  return S_OK;
}

STDMETHODIMP nsAccessNodeWrap::get_firstChild(ISimpleDOMNode __RPC_FAR *__RPC_FAR *aNode)
{
  if (!mDOMNode)
    return E_FAIL;
 
  nsCOMPtr<nsIDOMNode> node;
  mDOMNode->GetFirstChild(getter_AddRefs(node));
  *aNode = MakeAccessNode(node);

  return S_OK;
}

STDMETHODIMP nsAccessNodeWrap::get_lastChild(ISimpleDOMNode __RPC_FAR *__RPC_FAR *aNode)
{
  if (!mDOMNode)
    return E_FAIL;

  nsCOMPtr<nsIDOMNode> node;
  mDOMNode->GetLastChild(getter_AddRefs(node));
  *aNode = MakeAccessNode(node);

  return S_OK;
}

STDMETHODIMP nsAccessNodeWrap::get_previousSibling(ISimpleDOMNode __RPC_FAR *__RPC_FAR *aNode)
{
  if (!mDOMNode)
    return E_FAIL;

  nsCOMPtr<nsIDOMNode> node;
  mDOMNode->GetPreviousSibling(getter_AddRefs(node));
  *aNode = MakeAccessNode(node);

  return S_OK;
}

STDMETHODIMP nsAccessNodeWrap::get_nextSibling(ISimpleDOMNode __RPC_FAR *__RPC_FAR *aNode)
{
  if (!mDOMNode)
    return E_FAIL;

  nsCOMPtr<nsIDOMNode> node;
  mDOMNode->GetNextSibling(getter_AddRefs(node));
  *aNode = MakeAccessNode(node);

  return S_OK;
}

STDMETHODIMP 
nsAccessNodeWrap::get_childAt(unsigned aChildIndex,
                              ISimpleDOMNode __RPC_FAR *__RPC_FAR *aNode)
{
  *aNode = nsnull;

  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  if (!content)
    return E_FAIL;  // Node already shut down

  nsCOMPtr<nsIDOMNode> node =
    do_QueryInterface(content->GetChildAt(aChildIndex));

  if (!node)
    return E_FAIL; // No such child

  *aNode = MakeAccessNode(node);

  return S_OK;
}

STDMETHODIMP 
nsAccessNodeWrap::get_innerHTML(BSTR __RPC_FAR *aInnerHTML)
{
  *aInnerHTML = nsnull;

  nsCOMPtr<nsIDOMNSHTMLElement> domNSElement(do_QueryInterface(mDOMNode));
  if (!domNSElement)
    return E_FAIL; // Node already shut down

  nsAutoString innerHTML;
  domNSElement->GetInnerHTML(innerHTML);
  *aInnerHTML = ::SysAllocString(innerHTML.get());

  return S_OK;
}

STDMETHODIMP 
nsAccessNodeWrap::get_language(BSTR __RPC_FAR *aLanguage)
{
  *aLanguage = nsnull;

  nsAutoString language;
  if (NS_FAILED(GetLanguage(language))) {
    return E_FAIL;
  }
  *aLanguage = ::SysAllocString(language.get());
  return S_OK;
}

STDMETHODIMP 
nsAccessNodeWrap::get_localInterface( 
    /* [out] */ void __RPC_FAR *__RPC_FAR *localInterface)
{
  *localInterface = static_cast<nsIAccessNode*>(this);
  NS_ADDREF_THIS();
  return S_OK;
}
 
void nsAccessNodeWrap::InitAccessibility()
{
  if (gIsAccessibilityActive) {
    return;
  }

  nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (prefBranch) {
    prefBranch->GetBoolPref("accessibility.disableenumvariant", &gIsEnumVariantSupportDisabled);
  }

  if (!gmUserLib) {
    gmUserLib =::LoadLibrary("USER32.DLL");
  }

  if (gmUserLib) {
    if (!gmNotifyWinEvent)
      gmNotifyWinEvent = (LPFNNOTIFYWINEVENT)GetProcAddress(gmUserLib,"NotifyWinEvent");
    if (!gmGetGUIThreadInfo)
      gmGetGUIThreadInfo = (LPFNGETGUITHREADINFO)GetProcAddress(gmUserLib,"GetGUIThreadInfo");
  }

  nsAccessNode::InitXPAccessibility();
}

void nsAccessNodeWrap::ShutdownAccessibility()
{
  NS_IF_RELEASE(gTextEvent);
  ::DestroyCaret();

  if (!gIsAccessibilityActive) {
    return;
  }

  nsAccessNode::ShutdownXPAccessibility();
}
