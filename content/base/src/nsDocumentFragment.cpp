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

#include "nsISupports.h"
#include "nsIContent.h"
#include "nsIDOMDocumentFragment.h"
#include "nsGenericElement.h"
#include "nsINameSpaceManager.h"
#include "nsINodeInfo.h"
#include "nsNodeInfoManager.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsDOMError.h"
#include "nsIDOM3Node.h"
#include "nsLayoutAtoms.h"
#include "nsDOMString.h"

class nsDocumentFragment : public nsGenericElement,
                           public nsIDocumentFragment,
                           public nsIDOM3Node
{
public:
  nsDocumentFragment(nsINodeInfo *aNodeInfo, nsIDocument* aOwnerDocument);
  virtual ~nsDocumentFragment();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // interface nsIDocumentFragment
  NS_IMETHOD DisconnectChildren();
  NS_IMETHOD ReconnectChildren();
  NS_IMETHOD DropChildReferences();

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
  NS_IMETHOD    GetOwnerDocument(nsIDOMDocument** aOwnerDocument);
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
  { return nsGenericElement::GetLocalName(aLocalName); }
  NS_IMETHOD    Normalize()
  { return nsGenericElement::Normalize(); }
  NS_IMETHOD    IsSupported(const nsAString& aFeature,
                            const nsAString& aVersion,
                            PRBool* aReturn)
  { return nsGenericElement::IsSupported(aFeature, aVersion, aReturn); }

  // nsIDOM3Node
  NS_DECL_NSIDOM3NODE

  // nsIContent
  virtual void SetParent(nsIContent* aParent) { }
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
  virtual nsresult GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, 
                           nsAString& aResult) const
  {
    return NS_CONTENT_ATTR_NOT_THERE;
  }
  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute, 
                             PRBool aNotify)
  {
    return NS_OK;
  }
  virtual nsresult GetAttrNameAt(PRUint32 aIndex, PRInt32* aNameSpaceID,
                                 nsIAtom** aName, nsIAtom** aPrefix) const
  {
    *aNameSpaceID = kNameSpaceID_None;
    *aName = nsnull;
    *aPrefix = nsnull;
    return NS_ERROR_ILLEGAL_VALUE;
  }
  virtual nsresult HandleDOMEvent(nsIPresContext* aPresContext,
                                  nsEvent* aEvent, nsIDOMEvent** aDOMEvent,
                                  PRUint32 aFlags, nsEventStatus* aEventStatus)
  {
    NS_ENSURE_ARG_POINTER(aEventStatus);
    *aEventStatus = nsEventStatus_eIgnore;
    return NS_OK;
  }

protected:
  nsCOMPtr<nsIDocument> mOwnerDocument;
};

nsresult
NS_NewDocumentFragment(nsIDOMDocumentFragment** aInstancePtrResult,
                       nsIDocument* aOwnerDocument)
{
  NS_ENSURE_ARG(aOwnerDocument);

  nsINodeInfoManager *nimgr = aOwnerDocument->GetNodeInfoManager();

  nsCOMPtr<nsINodeInfo> nodeInfo;
  nsresult rv = nimgr->GetNodeInfo(nsLayoutAtoms::documentFragmentNodeName,
                                   nsnull, kNameSpaceID_None,
                                   getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  nsDocumentFragment* it = new nsDocumentFragment(nodeInfo, aOwnerDocument);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *aInstancePtrResult = NS_STATIC_CAST(nsIDOMDocumentFragment *, it);

  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}

nsDocumentFragment::nsDocumentFragment(nsINodeInfo *aNodeInfo,
                                       nsIDocument* aOwnerDocument)
  : nsGenericElement(aNodeInfo)
{
  mOwnerDocument = aOwnerDocument;
}

nsDocumentFragment::~nsDocumentFragment()
{
}


// QueryInterface implementation for nsDocumentFragment
NS_INTERFACE_MAP_BEGIN(nsDocumentFragment)
  NS_INTERFACE_MAP_ENTRY(nsIDocumentFragment)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDocumentFragment)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOM3Node)
  NS_INTERFACE_MAP_ENTRY(nsIContent)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContent)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(DocumentFragment)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsDocumentFragment)
NS_IMPL_RELEASE(nsDocumentFragment)

NS_IMETHODIMP
nsDocumentFragment::DisconnectChildren()
{
  PRUint32 i, count = GetChildCount();

  for (i = 0; i < count; i++) {
    GetChildAt(i)->SetParent(nsnull);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentFragment::ReconnectChildren()
{
  PRUint32 i, count = GetChildCount();

  for (i = 0; i < count; i++) {
    nsIContent *child = GetChildAt(i);
    nsIContent *parent = child->GetParent();

    if (parent) {
      // This is potentially a O(n**2) operation, but it should only
      // happen in error cases (such as out of memory or something
      // similar) so we don't care for now.

      PRInt32 indx = parent->IndexOf(child);

      if (indx >= 0) {
        parent->RemoveChildAt(indx, PR_TRUE);
      }
    }

    child->SetParent(this);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentFragment::DropChildReferences()
{
  PRUint32 count = mAttrsAndChildren.ChildCount();
  while (count > 0) {
    mAttrsAndChildren.RemoveChildAt(--count);
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsDocumentFragment::GetNodeType(PRUint16* aNodeType)
{
  *aNodeType = (PRUint16)nsIDOMNode::DOCUMENT_FRAGMENT_NODE;
  return NS_OK;
}

NS_IMETHODIMP    
nsDocumentFragment::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
  NS_ENSURE_ARG_POINTER(aOwnerDocument);

  if (!mOwnerDocument) {
    *aOwnerDocument = nsnull;
    return NS_OK;
  }

  return CallQueryInterface(mOwnerDocument, aOwnerDocument);
}

NS_IMETHODIMP
nsDocumentFragment::SetPrefix(const nsAString& aPrefix)
{
  return NS_ERROR_DOM_NAMESPACE_ERR;
}

NS_IMETHODIMP    
nsDocumentFragment::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsresult rv = NS_OK;
  nsCOMPtr<nsIDOMDocumentFragment> newFragment;

  rv = NS_NewDocumentFragment(getter_AddRefs(newFragment), mOwnerDocument);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDeep) {
    nsCOMPtr<nsIDOMNodeList> childNodes;

    GetChildNodes(getter_AddRefs(childNodes));
    if (childNodes) {
      PRUint32 index, count;
      childNodes->GetLength(&count);

      for (index = 0; index < count; ++index) {
        nsCOMPtr<nsIDOMNode> child;
        childNodes->Item(index, getter_AddRefs(child));
        if (child) {
          nsCOMPtr<nsIDOMNode> newChild;
          rv = child->CloneNode(PR_TRUE, getter_AddRefs(newChild));
          NS_ENSURE_SUCCESS(rv, rv);

          nsCOMPtr<nsIDOMNode> dummyNode;
          rv = newFragment->AppendChild(newChild,
                                        getter_AddRefs(dummyNode));
          NS_ENSURE_SUCCESS(rv, rv);
        }
      } // End of for loop
    } // if (childNodes)
  } // if (aDeep)

  return CallQueryInterface(newFragment, aReturn);
}

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
  NS_NOTYETIMPLEMENTED("nsDocumentFragment::IsDefaultNamespace()");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocumentFragment::CompareDocumentPosition(nsIDOMNode* aOther,
                                            PRUint16* aReturn)
{
  NS_ENSURE_ARG_POINTER(aOther);
  NS_PRECONDITION(aReturn, "Must have an out parameter");

  if (this == aOther) {
    // If the two nodes being compared are the same node,
    // then no flags are set on the return.
    *aReturn = 0;
    return NS_OK;
  }

  PRUint16 mask = 0;

  nsCOMPtr<nsIDOMNode> other(aOther);
  do {
    nsCOMPtr<nsIDOMNode> tmp(other);
    tmp->GetParentNode(getter_AddRefs(other));
    if (!other) {
      // No parent.  Check to see if we're at an attribute node.
      PRUint16 nodeType = 0;
      tmp->GetNodeType(&nodeType);
      if (nodeType != nsIDOMNode::ATTRIBUTE_NODE) {
        // If there is no common container node, then the order
        // is based upon order between the root container of each
        // node that is in no container. In this case, the result
        // is disconnected and implementation-dependent.
        mask |= (nsIDOM3Node::DOCUMENT_POSITION_DISCONNECTED |
                 nsIDOM3Node::DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC);

        break;
      }

      // If we are, let's get the owner element and continue up the tree
      nsCOMPtr<nsIDOMAttr> attr(do_QueryInterface(tmp));
      nsCOMPtr<nsIDOMElement> owner;
      attr->GetOwnerElement(getter_AddRefs(owner));
      other = do_QueryInterface(owner);
    }

    if (NS_STATIC_CAST(nsIDOMNode*, this) == other) {
      // If the node being compared is contained by our node,
      // then it follows it.
      mask |= (nsIDOM3Node::DOCUMENT_POSITION_CONTAINED_BY |
               nsIDOM3Node::DOCUMENT_POSITION_FOLLOWING);
      break;
    }
  } while (other);

  *aReturn = mask;
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
  NS_NOTYETIMPLEMENTED("nsDocumentFragment::IsEqualNode()");

  return NS_ERROR_NOT_IMPLEMENTED;
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
                                nsIVariant** aReturn)
{
  NS_NOTYETIMPLEMENTED("nsDocumentFragment::SetUserData()");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocumentFragment::GetUserData(const nsAString& aKey,
                                nsIVariant** aReturn)
{
  NS_NOTYETIMPLEMENTED("nsDocumentFragment::GetUserData()");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocumentFragment::GetTextContent(nsAString &aTextContent)
{
  if (mOwnerDocument) {
    return nsNode3Tearoff::GetTextContent(mOwnerDocument,
                                          this,
                                          aTextContent);
  }

  SetDOMStringToNull(aTextContent);

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentFragment::SetTextContent(const nsAString& aTextContent)
{
  return nsNode3Tearoff::SetTextContent(this, aTextContent);
}

