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
#include "nsGkAtoms.h"
#include "nsDOMString.h"
#include "nsIDOMUserDataHandler.h"

class nsDocumentFragment : public nsGenericElement,
                           public nsIDOMDocumentFragment
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
  NS_IMETHOD    CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
  { return nsGenericElement::CloneNode(aDeep, aReturn); }
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
  nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
};

nsresult
NS_NewDocumentFragment(nsIDOMDocumentFragment** aInstancePtrResult,
                       nsNodeInfoManager *aNodeInfoManager)
{
  NS_ENSURE_ARG(aNodeInfoManager);

  nsCOMPtr<nsINodeInfo> nodeInfo;
  nsresult rv =
    aNodeInfoManager->GetNodeInfo(nsGkAtoms::documentFragmentNodeName,
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
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOM3Node, new nsNode3Tearoff(this))
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(DocumentFragment)
NS_INTERFACE_MAP_END_INHERITING(nsGenericElement)


NS_IMPL_ADDREF_INHERITED(nsDocumentFragment, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsDocumentFragment, nsGenericElement)

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

NS_IMPL_ELEMENT_CLONE(nsDocumentFragment)
