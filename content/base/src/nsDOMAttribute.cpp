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
 * The Original Code is Mozilla Communicator client code.
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

#include "nsDOMAttribute.h"
#include "nsGenericElement.h"
#include "nsIContent.h"
#include "nsITextContent.h"
#include "nsINameSpaceManager.h"
#include "nsDOMError.h"
#include "nsContentUtils.h"
#include "nsUnicharUtils.h"
#include "nsDOMString.h"

//----------------------------------------------------------------------

nsDOMAttribute::nsDOMAttribute(nsIContent* aContent, nsINodeInfo *aNodeInfo,
                               const nsAString& aValue)
  : nsIAttribute(aContent, aNodeInfo), mValue(aValue), mChild(nsnull),
    mChildList(nsnull)
{
  NS_ABORT_IF_FALSE(mNodeInfo, "We must get a nodeinfo here!");


  // We don't add a reference to our content. It will tell us
  // to drop our reference when it goes away.
}

nsDOMAttribute::~nsDOMAttribute()
{
  NS_IF_RELEASE(mChild);
  NS_IF_RELEASE(mChildList);
}


// QueryInterface implementation for nsDOMAttribute
NS_INTERFACE_MAP_BEGIN(nsDOMAttribute)
  NS_INTERFACE_MAP_ENTRY(nsIDOMAttr)
  NS_INTERFACE_MAP_ENTRY(nsIAttribute)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOM3Node)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMAttr)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(Attr)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsDOMAttribute)
NS_IMPL_RELEASE(nsDOMAttribute)


nsresult
nsDOMAttribute::GetName(nsAString& aName)
{
  mNodeInfo->GetQualifiedName(aName);
  return NS_OK;
}

nsresult
nsDOMAttribute::GetValue(nsAString& aValue)
{
  if (mContent) {
    nsAutoString tmpValue;
    nsresult attrResult = mContent->GetAttr(mNodeInfo->NamespaceID(),
                                            mNodeInfo->NameAtom(),
                                            tmpValue);
    if (NS_CONTENT_ATTR_NOT_THERE != attrResult) {
      mValue = tmpValue;
    }
  }

  aValue = mValue;

  return NS_OK;
}

nsresult
nsDOMAttribute::SetValue(const nsAString& aValue)
{
  nsresult result = NS_OK;
  if (mContent) {
    result = mContent->SetAttr(mNodeInfo->NamespaceID(), mNodeInfo->NameAtom(),
                               mNodeInfo->GetPrefixAtom(), aValue, PR_TRUE);
  }
  mValue=aValue;

  return result;
}

nsresult
nsDOMAttribute::GetSpecified(PRBool* aSpecified)
{
  NS_ENSURE_ARG_POINTER(aSpecified);

  if (!mContent) {
    *aSpecified = PR_FALSE;
    return NS_OK;
  }

  *aSpecified = mContent->HasAttr(mNodeInfo->NamespaceID(),
                                  mNodeInfo->NameAtom());

  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetOwnerElement(nsIDOMElement** aOwnerElement)
{
  NS_ENSURE_ARG_POINTER(aOwnerElement);

  if (mContent) {
    return CallQueryInterface(mContent, aOwnerElement);
  }

  *aOwnerElement = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetNodeName(nsAString& aNodeName)
{
  return GetName(aNodeName);
}

NS_IMETHODIMP
nsDOMAttribute::GetNodeValue(nsAString& aNodeValue)
{
  return GetValue(aNodeValue);
}

NS_IMETHODIMP
nsDOMAttribute::SetNodeValue(const nsAString& aNodeValue)
{
  return SetValue(aNodeValue);
}

NS_IMETHODIMP
nsDOMAttribute::GetNodeType(PRUint16* aNodeType)
{
  NS_ENSURE_ARG_POINTER(aNodeType);

  *aNodeType = (PRUint16)nsIDOMNode::ATTRIBUTE_NODE;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetParentNode(nsIDOMNode** aParentNode)
{
  NS_ENSURE_ARG_POINTER(aParentNode);

  *aParentNode = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
  if (!mChildList) {
    mChildList = new nsAttributeChildList(this);
    NS_ENSURE_TRUE(mChildList, NS_ERROR_OUT_OF_MEMORY);

    NS_ADDREF(mChildList);
  }

  return CallQueryInterface(mChildList, aChildNodes);
}

NS_IMETHODIMP
nsDOMAttribute::HasChildNodes(PRBool* aHasChildNodes)
{
  *aHasChildNodes = PR_FALSE;
  if (mChild) {
    *aHasChildNodes = PR_TRUE;
  }
  else if (mContent) {
    nsAutoString value;

    GetValue(value);
    if (!value.IsEmpty()) {
      *aHasChildNodes = PR_TRUE;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::HasAttributes(PRBool* aHasAttributes)
{
  NS_ENSURE_ARG_POINTER(aHasAttributes);

  *aHasAttributes = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetFirstChild(nsIDOMNode** aFirstChild)
{
  nsAutoString value;
  nsresult result;

  result = GetValue(value);
  if (NS_OK != result) {
    return result;
  }
  if (!value.IsEmpty()) {
    if (!mChild) {
      nsCOMPtr<nsITextContent> content;

      result = NS_NewTextNode(getter_AddRefs(content));
      if (NS_FAILED(result)) {
        return result;
      }
      result = CallQueryInterface(content, &mChild);
    }
    mChild->SetData(value);
    result = CallQueryInterface(mChild, aFirstChild);
  }
  else {
    *aFirstChild = nsnull;
  }
  return result;
}

NS_IMETHODIMP
nsDOMAttribute::GetLastChild(nsIDOMNode** aLastChild)
{
  return GetFirstChild(aLastChild);
}

NS_IMETHODIMP
nsDOMAttribute::GetPreviousSibling(nsIDOMNode** aPreviousSibling)
{
  NS_ENSURE_ARG_POINTER(aPreviousSibling);

  *aPreviousSibling = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetNextSibling(nsIDOMNode** aNextSibling)
{
  NS_ENSURE_ARG_POINTER(aNextSibling);

  *aNextSibling = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{
  NS_ENSURE_ARG_POINTER(aAttributes);

  *aAttributes = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsDOMAttribute::ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsDOMAttribute::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsDOMAttribute::AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsDOMAttribute::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsDOMAttribute* newAttr;

  if (mContent) {
    nsAutoString value;
    mContent->GetAttr(mNodeInfo->NamespaceID(),
                      mNodeInfo->NameAtom(), value);
    newAttr = new nsDOMAttribute(nsnull, mNodeInfo, value);
  }
  else {
    newAttr = new nsDOMAttribute(nsnull, mNodeInfo, mValue);
  }

  if (!newAttr) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return CallQueryInterface(newAttr, aReturn);
}

NS_IMETHODIMP
nsDOMAttribute::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
  nsresult result = NS_OK;
  if (mContent) {
    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(mContent, &result);
    if (NS_SUCCEEDED(result)) {
      result = node->GetOwnerDocument(aOwnerDocument);
    }
  }
  else {
    *aOwnerDocument = nsnull;
  }

  return result;
}

NS_IMETHODIMP
nsDOMAttribute::GetNamespaceURI(nsAString& aNamespaceURI)
{
  return mNodeInfo->GetNamespaceURI(aNamespaceURI);
}

NS_IMETHODIMP
nsDOMAttribute::GetPrefix(nsAString& aPrefix)
{
  mNodeInfo->GetPrefix(aPrefix);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::SetPrefix(const nsAString& aPrefix)
{
  nsCOMPtr<nsINodeInfo> newNodeInfo;
  nsCOMPtr<nsIAtom> prefix;

  if (!aPrefix.IsEmpty()) {
    prefix = do_GetAtom(aPrefix);
  }

  nsresult rv = nsContentUtils::PrefixChanged(mNodeInfo, prefix,
                                              getter_AddRefs(newNodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  if (mContent) {
    nsIAtom *name = mNodeInfo->NameAtom();
    PRInt32 nameSpaceID = mNodeInfo->NamespaceID();

    nsAutoString tmpValue;
    rv = mContent->GetAttr(nameSpaceID, name, tmpValue);
    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
      mContent->UnsetAttr(nameSpaceID, name, PR_TRUE);

      mContent->SetAttr(newNodeInfo->NamespaceID(), newNodeInfo->NameAtom(),
                        newNodeInfo->GetPrefixAtom(), tmpValue, PR_TRUE);
    }
  }

  newNodeInfo.swap(mNodeInfo);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetLocalName(nsAString& aLocalName)
{
  mNodeInfo->GetLocalName(aLocalName);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::Normalize()
{
  // Nothing to do here
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::IsSupported(const nsAString& aFeature,
                            const nsAString& aVersion,
                            PRBool* aReturn)
{
  return nsGenericElement::InternalIsSupported(aFeature, aVersion, aReturn);
}

NS_IMETHODIMP
nsDOMAttribute::GetBaseURI(nsAString &aURI)
{
  aURI.Truncate();
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDOM3Node> node(do_QueryInterface(mContent));
  if (node)
    rv = node->GetBaseURI(aURI);
  return rv;
}

NS_IMETHODIMP
nsDOMAttribute::CompareDocumentPosition(nsIDOMNode* aOther,
                                        PRUint16* aReturn)
{
  NS_ENSURE_ARG_POINTER(aOther);
  NS_PRECONDITION(aReturn, "Must have an out parameter");

  PRUint16 mask = 0;

  nsCOMPtr<nsIDOMElement> el;
  GetOwnerElement(getter_AddRefs(el));
  if (!el) {
    // If we have no owner element then there is no common container node,
    // (of course there isn't if we have no container!) and the order is
    // then based upon order between the root container of each node that
    // is in no container. In this case, the result is disconnected
    // and implementation-dependent.
    mask |= (nsIDOM3Node::DOCUMENT_POSITION_DISCONNECTED |
             nsIDOM3Node::DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC);

    *aReturn = mask;

    return NS_OK;
  }

  // Check to see if the other node is also an attribute
  PRUint16 nodeType = 0;
  aOther->GetNodeType(&nodeType);
  if (nodeType == nsIDOMNode::ATTRIBUTE_NODE) {
    nsCOMPtr<nsIDOMAttr> otherAttr(do_QueryInterface(aOther));
    nsCOMPtr<nsIDOMElement> otherEl;
    otherAttr->GetOwnerElement(getter_AddRefs(otherEl));
    if (el == otherEl) {
      PRBool sameNode = PR_FALSE;
      IsSameNode(aOther, &sameNode);
      if (!sameNode) {
        // If neither of the two determining nodes is a child node and
        // nodeType is the same for both determining nodes, then an
        // implementation-dependent order between the determining nodes
        // is returned.
        mask |= nsIDOM3Node::DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC;
      }

      // If the two nodes being compared are the same node,
      // then no flags are set on the return.
    }

    *aReturn = mask;

    return NS_OK;
  }

  PRBool sameNode = PR_FALSE;

  if (nodeType == nsIDOMNode::TEXT_NODE ||
      nodeType == nsIDOMNode::CDATA_SECTION_NODE ||
      nodeType == nsIDOMNode::ENTITY_REFERENCE_NODE) {
    // XXXcaa we really should check the other node's parentNode
    // against ourselves.  But since we can't, we should walk our
    // child node list to see if it's a descendant.  But wait!
    // We suck so bad that we cannot even do that, since we store
    // only one text node per attribute, even if there are multiple.
    // So technically, we could walk the child nodes list, but it
    // would not make sense really to walk it for only one thing.
    // How lame.  So.... it seems the only option that we DO have
    // is to get our one and only child and compare it against the
    // other node.  As such, that is exactly what we'll do.
    // *Sigh*  These silly hacks are quite disgusting, really....

    nsCOMPtr<nsIDOMNode> ourOnlyChild;
    GetFirstChild(getter_AddRefs(ourOnlyChild));

    nsCOMPtr<nsIDOM3Node> longLostRelative(do_QueryInterface(aOther));
    NS_ASSERTION(longLostRelative, "All our data nodes support DOM3Node");

    longLostRelative->IsSameNode(ourOnlyChild, &sameNode);
    if (sameNode) {
      // Woohoo!  We found our long lost relative and it's our child!
      // Throw a party!  Celebrate by returning that it is contained
      // and following this node.

      mask |= (nsIDOM3Node::DOCUMENT_POSITION_CONTAINED_BY |
               nsIDOM3Node::DOCUMENT_POSITION_FOLLOWING);

      *aReturn = mask;
      return NS_OK;
    }

    // Sigh.  The other node isn't our child, but it still may be
    // related to us.  Fall through so we can keep looking.
  }


  // The other node isn't an attribute, or a child.
  // Compare position relative to this attribute's owner element.

  nsCOMPtr<nsIDOM3Node> parent(do_QueryInterface(el));
  parent->IsSameNode(aOther, &sameNode);
  if (sameNode) {
    // If the other node contains us, then it precedes us.
    mask |= (nsIDOM3Node::DOCUMENT_POSITION_CONTAINS |
             nsIDOM3Node::DOCUMENT_POSITION_PRECEDING);

    *aReturn = mask;
    return NS_OK;
  }

  PRUint16 parentMask = 0;
  parent->CompareDocumentPosition(aOther, &parentMask);

  // We already established earlier that the node is not contained
  // by this attribute.  So if it is contained by our owner element,
  // unset the flag.
  mask |= parentMask & ~nsIDOM3Node::DOCUMENT_POSITION_CONTAINED_BY;

  *aReturn = mask;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::IsSameNode(nsIDOMNode* aOther,
                           PRBool* aReturn)
{
  PRBool sameNode = PR_FALSE;

  // XXXcaa Comparing pointers on two attributes is not yet reliable.
  // When bug 93614 is fixed, this should be changed to simple pointer
  // comparisons. But for now, check owner elements and node names.
  PRUint16 otherType = 0;
  aOther->GetNodeType(&otherType);
  if (nsIDOMNode::ATTRIBUTE_NODE == otherType) {
    nsCOMPtr<nsIDOMElement> nodeOwner;
    GetOwnerElement(getter_AddRefs(nodeOwner));
    nsCOMPtr<nsIDOMAttr> other(do_QueryInterface(aOther));
    nsCOMPtr<nsIDOMElement> otherOwner;
    other->GetOwnerElement(getter_AddRefs(otherOwner));

    // Do these attributes belong to the same element?
    if (nodeOwner == otherOwner) {
      PRBool ci = PR_FALSE;
      nsCOMPtr<nsIContent> content(do_QueryInterface(nodeOwner));
      // Check to see if we're in HTML.
      if (content->IsContentOfType(nsIContent::eHTML)) {
        nsINodeInfo *ni = content->GetNodeInfo();
        if (ni) {
          // If there is no namespace, we're in HTML (as opposed to XHTML)
          // and we'll need to compare node names case insensitively.
          ci = ni->NamespaceEquals(kNameSpaceID_None);
        }
      }

      nsAutoString nodeName;
      nsAutoString otherName;
      GetNodeName(nodeName);
      aOther->GetNodeName(otherName);
      // Compare node names
      sameNode = ci ? nodeName.Equals(otherName,
                                      nsCaseInsensitiveStringComparator())
                    : nodeName.Equals(otherName);
    }
  }

  *aReturn = sameNode;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::IsEqualNode(nsIDOMNode* aOther,
                            PRBool* aReturn)
{
  NS_NOTYETIMPLEMENTED("nsDocument::IsEqualNode()");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDOMAttribute::IsDefaultNamespace(const nsAString& aNamespaceURI,
                                   PRBool* aReturn)
{
  NS_NOTYETIMPLEMENTED("nsDOMAttribute::IsDefaultNamespace()");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDOMAttribute::GetTextContent(nsAString &aTextContent)
{
  return GetNodeValue(aTextContent);
}

NS_IMETHODIMP
nsDOMAttribute::SetTextContent(const nsAString& aTextContent)
{
  return SetNodeValue(aTextContent);
}


NS_IMETHODIMP
nsDOMAttribute::GetFeature(const nsAString& aFeature,
                           const nsAString& aVersion,
                           nsISupports** aReturn)
{
  NS_NOTYETIMPLEMENTED("nsDocument::GetFeature()");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDOMAttribute::SetUserData(const nsAString& aKey,
                            nsIVariant* aData,
                            nsIDOMUserDataHandler* aHandler,
                            nsIVariant** aReturn)
{
  NS_NOTYETIMPLEMENTED("nsDocument::SetUserData()");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDOMAttribute::GetUserData(const nsAString& aKey,
                            nsIVariant** aReturn)
{
  NS_NOTYETIMPLEMENTED("nsDocument::GetUserData()");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDOMAttribute::LookupPrefix(const nsAString& aNamespaceURI,
                             nsAString& aPrefix)
{
  aPrefix.Truncate();

  nsCOMPtr<nsIDOM3Node> node(do_QueryInterface(mContent));
  if (node) {
    return node->LookupPrefix(aNamespaceURI, aPrefix);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::LookupNamespaceURI(const nsAString& aNamespacePrefix,
                                   nsAString& aNamespaceURI)
{
  aNamespaceURI.Truncate();
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDOM3Node> node(do_QueryInterface(mContent));
  if (node)
    rv = node->LookupNamespaceURI(aNamespacePrefix, aNamespaceURI);
  return rv;
}

//----------------------------------------------------------------------

nsAttributeChildList::nsAttributeChildList(nsDOMAttribute* aAttribute)
{
  // Don't increment the reference count. The attribute will tell
  // us when it's going away
  mAttribute = aAttribute;
}

nsAttributeChildList::~nsAttributeChildList()
{
}

NS_IMETHODIMP
nsAttributeChildList::GetLength(PRUint32* aLength)
{
  *aLength = 0;
  if (mAttribute) {
    nsAutoString value;
    mAttribute->GetValue(value);
    if (!value.IsEmpty()) {
      *aLength = 1;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAttributeChildList::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  *aReturn = nsnull;
  if (mAttribute && 0 == aIndex) {
    mAttribute->GetFirstChild(aReturn);
  }

  return NS_OK;
}

void
nsAttributeChildList::DropReference()
{
  mAttribute = nsnull;
}
