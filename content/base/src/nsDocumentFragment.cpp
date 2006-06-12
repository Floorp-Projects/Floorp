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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/*
 * Implementation of DOM Core's nsIDOMDocumentFragment.
 */

#include "nsISupports.h"
#include "nsIContent.h"
#include "nsIDOMDocumentFragment.h"
#include "nsGenericElement.h"
#include "nsINameSpaceManager.h"
#include "nsINodeInfo.h"
#include "nsNodeInfoManager.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMAttr.h"
#include "nsDOMError.h"
#include "nsIDOM3Node.h"
#include "nsLayoutAtoms.h"
#include "nsDOMString.h"
#include "nsIDOMUserDataHandler.h"

class nsDocumentFragment : public nsGenericElement,
                           public nsIDOMDocumentFragment,
                           public nsIDOM3Node
{
public:
  nsDocumentFragment(nsINodeInfo *aNodeInfo);
  virtual ~nsDocumentFragment();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // interface nsIDOMDocumentFragment
  NS_IMETHOD    GetNodeName(nsAString& aNodeName)
  { return nsGenericElement::GetNodeName(aNodeName); }
  NS_IMETHOD    GetNodeValue(nsAString& aNodeValue)
  { return nsGenericElement::GetNodeValue(aNodeValue); }
  NS_IMETHOD    SetNodeValue(const nsAString& aNodeValue)
  { return nsGenericElement::SetNodeValue(aNodeValue); }
  NS_IMETHOD    GetNodeType(PRUint16* aNodeType);
  NS_IMETHOD    GetParentNode(nsIDOMNode** aParentNode)
  { return nsGenericElement::GetParentNode(aParentNode); }
  NS_IMETHOD    GetChildNodes(nsIDOMNodeList** aChildNodes)
  { return nsGenericElement::GetChildNodes(aChildNodes); }
  NS_IMETHOD    GetFirstChild(nsIDOMNode** aFirstChild)
  { return nsGenericElement::GetFirstChild(aFirstChild); }
  NS_IMETHOD    GetLastChild(nsIDOMNode** aLastChild)
  { return nsGenericElement::GetLastChild(aLastChild); }
  NS_IMETHOD    GetPreviousSibling(nsIDOMNode** aPreviousSibling)
  { return nsGenericElement::GetPreviousSibling(aPreviousSibling); }
  NS_IMETHOD    GetNextSibling(nsIDOMNode** aNextSibling)
  { return nsGenericElement::GetNextSibling(aNextSibling); }
  NS_IMETHOD    GetAttributes(nsIDOMNamedNodeMap** aAttributes)
    {
      *aAttributes = nsnull;
      return NS_OK;
    }
  NS_IMETHOD    GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
  { return nsGenericElement::GetOwnerDocument(aOwnerDocument); }
  NS_IMETHOD    InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, 
                             nsIDOMNode** aReturn)
  { return nsGenericElement::InsertBefore(aNewChild, aRefChild, aReturn); }
  NS_IMETHOD    ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, 
                             nsIDOMNode** aReturn)
  { return nsGenericElement::ReplaceChild(aNewChild, aOldChild, aReturn); }
  NS_IMETHOD    RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
  { return nsGenericElement::RemoveChild(aOldChild, aReturn); }
  NS_IMETHOD    AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
  { return nsGenericElement::AppendChild(aNewChild, aReturn); }
  NS_IMETHOD    HasChildNodes(PRBool* aReturn)
  { return nsGenericElement::HasChildNodes(aReturn); }
  NS_IMETHOD    HasAttributes(PRBool* aReturn)
  { return nsGenericElement::HasAttributes(aReturn); }
  NS_IMETHOD    CloneNode(PRBool aDeep, nsIDOMNode** aReturn);
  NS_IMETHOD    GetPrefix(nsAString& aPrefix)
  { return nsGenericElement::GetPrefix(aPrefix); }
  NS_IMETHOD    SetPrefix(const nsAString& aPrefix);
  NS_IMETHOD    GetNamespaceURI(nsAString& aNamespaceURI)
  { return nsGenericElement::GetNamespaceURI(aNamespaceURI); }
  NS_IMETHOD    GetLocalName(nsAString& aLocalName)
  {
    SetDOMStringToNull(aLocalName);
    return NS_OK;
  }
  NS_IMETHOD    Normalize()
  { return nsGenericElement::Normalize(); }
  NS_IMETHOD    IsSupported(const nsAString& aFeature,
                            const nsAString& aVersion,
                            PRBool* aReturn)
  { return nsGenericElement::IsSupported(aFeature, aVersion, aReturn); }

  // nsIDOM3Node
  NS_DECL_NSIDOM3NODE

  // nsIContent
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, PRBool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           PRBool aNotify)
  {
    return NS_OK;
  }
  virtual PRBool GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, 
                         nsAString& aResult) const
  {
    return PR_FALSE;
  }
  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute, 
                             PRBool aNotify)
  {
    return NS_OK;
  }
  virtual const nsAttrName* GetAttrNameAt(PRUint32 aIndex) const
  {
    return nsnull;
  }

  virtual PRBool IsNodeOfType(PRUint32 aFlags) const;

protected:
  nsresult Clone(nsINodeInfo *aNodeInfo, PRBool aDeep,
                 nsIContent **aResult) const;
};

nsresult
NS_NewDocumentFragment(nsIDOMDocumentFragment** aInstancePtrResult,
                       nsNodeInfoManager *aNodeInfoManager)
{
  NS_ENSURE_ARG(aNodeInfoManager);

  nsCOMPtr<nsINodeInfo> nodeInfo;
  nsresult rv =
    aNodeInfoManager->GetNodeInfo(nsLayoutAtoms::documentFragmentNodeName,
                                  nsnull, kNameSpaceID_None,
                                  getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  nsDocumentFragment *it = new nsDocumentFragment(nodeInfo);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aInstancePtrResult = it);

  return NS_OK;
}

nsDocumentFragment::nsDocumentFragment(nsINodeInfo *aNodeInfo)
  : nsGenericElement(aNodeInfo)
{
}

nsDocumentFragment::~nsDocumentFragment()
{
}

PRBool
nsDocumentFragment::IsNodeOfType(PRUint32 aFlags) const
{
  return !(aFlags & ~(eCONTENT | eDOCUMENT_FRAGMENT));
}

// QueryInterface implementation for nsDocumentFragment
NS_INTERFACE_MAP_BEGIN(nsDocumentFragment)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDocumentFragment)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOM3Node)
  NS_INTERFACE_MAP_ENTRY(nsIContent)
  NS_INTERFACE_MAP_ENTRY(nsINode)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContent)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(DocumentFragment)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsDocumentFragment)
NS_IMPL_RELEASE(nsDocumentFragment)

NS_IMETHODIMP    
nsDocumentFragment::GetNodeType(PRUint16* aNodeType)
{
  *aNodeType = (PRUint16)nsIDOMNode::DOCUMENT_FRAGMENT_NODE;
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentFragment::SetPrefix(const nsAString& aPrefix)
{
  return NS_ERROR_DOM_NAMESPACE_ERR;
}

NS_IMPL_DOM_CLONENODE(nsDocumentFragment)

NS_IMETHODIMP
nsDocumentFragment::GetBaseURI(nsAString& aURI)
{
  aURI.Truncate();

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentFragment::LookupPrefix(const nsAString& aNamespaceURI,
                                 nsAString& aPrefix)
{
  aPrefix.Truncate();

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentFragment::LookupNamespaceURI(const nsAString& aNamespacePrefix,
                                       nsAString& aNamespaceURI)
{
  aNamespaceURI.Truncate();

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentFragment::IsDefaultNamespace(const nsAString& aNamespaceURI,
                                       PRBool* aReturn)
{
  *aReturn = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentFragment::CompareDocumentPosition(nsIDOMNode* aOther,
                                            PRUint16* aReturn)
{
  NS_ENSURE_ARG_POINTER(aOther);

  nsCOMPtr<nsINode> other = do_QueryInterface(aOther);
  NS_ENSURE_TRUE(other, NS_ERROR_DOM_NOT_SUPPORTED_ERR);

  *aReturn = nsContentUtils::ComparePosition(other, this);
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentFragment::IsSameNode(nsIDOMNode* aOther,
                               PRBool* aReturn)
{
  PRBool sameNode = PR_FALSE;

  if (NS_STATIC_CAST(nsIDOMNode*, this) == aOther) {
    sameNode = PR_TRUE;
  }

  *aReturn = sameNode;
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentFragment::IsEqualNode(nsIDOMNode* aOther, PRBool* aReturn)
{
  NS_ENSURE_ARG_POINTER(aOther);
  *aReturn = PR_FALSE;

  nsCOMPtr<nsIContent> aOtherFrag = do_QueryInterface(aOther);
  if (!aOtherFrag) {
    return NS_OK;
  }

  *aReturn = nsNode3Tearoff::AreNodesEqual(this, aOtherFrag);
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentFragment::GetFeature(const nsAString& aFeature,
                               const nsAString& aVersion,
                               nsISupports** aReturn)
{
  NS_NOTYETIMPLEMENTED("nsDocumentFragment::GetFeature()");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocumentFragment::SetUserData(const nsAString& aKey,
                                nsIVariant* aData,
                                nsIDOMUserDataHandler* aHandler,
                                nsIVariant** aResult)
{
  nsCOMPtr<nsIAtom> key = do_GetAtom(aKey);
  if (!key) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return nsContentUtils::SetUserData(this, key, aData, aHandler, aResult);
}

NS_IMETHODIMP
nsDocumentFragment::GetUserData(const nsAString& aKey,
                                nsIVariant** aResult)
{
  nsIDocument *document = GetOwnerDoc();
  NS_ENSURE_TRUE(document, NS_ERROR_FAILURE);

  nsCOMPtr<nsIAtom> key = do_GetAtom(aKey);
  if (!key) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *aResult = NS_STATIC_CAST(nsIVariant*, GetProperty(DOM_USER_DATA, key));
  NS_IF_ADDREF(*aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentFragment::GetTextContent(nsAString &aTextContent)
{
  return nsNode3Tearoff::GetTextContent(this, aTextContent);
}

NS_IMETHODIMP
nsDocumentFragment::SetTextContent(const nsAString& aTextContent)
{
  return nsNode3Tearoff::SetTextContent(this, aTextContent);
}

