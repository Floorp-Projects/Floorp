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
#include "nsIDocument.h"
#include "nsIDOMDocument.h"

//----------------------------------------------------------------------
PRBool nsDOMAttribute::sInitialized;

nsDOMAttribute::nsDOMAttribute(nsDOMAttributeMap *aAttrMap,
                               nsINodeInfo       *aNodeInfo,
                               const nsAString   &aValue)
  : nsIAttribute(aAttrMap, aNodeInfo), mValue(aValue), mChild(nsnull),
    mChildList(nsnull)
{
  NS_ABORT_IF_FALSE(mNodeInfo, "We must get a nodeinfo here!");


  // We don't add a reference to our content. It will tell us
  // to drop our reference when it goes away.
}

nsDOMAttribute::~nsDOMAttribute()
{
  nsIDocument *doc = GetOwnerDoc();
  if (doc)
    doc->PropertyTable()->DeleteAllPropertiesFor(this);

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

void
nsDOMAttribute::SetMap(nsDOMAttributeMap *aMap)
{
  if (mAttrMap && !aMap && sInitialized) {
    // We're breaking a relationship with content and not getting a new one,
    // need to locally cache value. GetValue() does that.
    nsAutoString tmp;
    GetValue(tmp);
  }
  
  mAttrMap = aMap;
}

nsIContent*
nsDOMAttribute::GetContent() const
{
  return GetContentInternal();
}

nsresult
nsDOMAttribute::GetName(nsAString& aName)
{
  mNodeInfo->GetQualifiedName(aName);
  return NS_OK;
}

nsresult
nsDOMAttribute::GetValue(nsAString& aValue)
{
  nsIContent* content = GetContentInternal();
  if (content) {
    nsAutoString tmpValue;
    nsresult attrResult = content->GetAttr(mNodeInfo->NamespaceID(),
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
  nsresult rv = NS_OK;
  nsIContent* content = GetContentInternal();
  if (content) {
    rv = content->SetAttr(mNodeInfo->NamespaceID(),
                          mNodeInfo->NameAtom(),
                          mNodeInfo->GetPrefixAtom(),
                          aValue,
                          PR_TRUE);
  }
  mValue = aValue;

  return rv;
}

nsresult
nsDOMAttribute::GetSpecified(PRBool* aSpecified)
{
  NS_ENSURE_ARG_POINTER(aSpecified);

  nsIContent* content = GetContentInternal();
  *aSpecified = content && content->HasAttr(mNodeInfo->NamespaceID(),
                                            mNodeInfo->NameAtom());

  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetOwnerElement(nsIDOMElement** aOwnerElement)
{
  NS_ENSURE_ARG_POINTER(aOwnerElement);

  nsIContent* content = GetContentInternal();
  PRBool hasAttr = content && content->HasAttr(mNodeInfo->NamespaceID(),
                                               mNodeInfo->NameAtom());
  if (hasAttr) {
    return CallQueryInterface(content, aOwnerElement);
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
  else {
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
      // XXX We should be setting |this| as the parent of the textnode!
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

  nsAutoString value;
  GetValue(value);
  newAttr = new nsDOMAttribute(nsnull, mNodeInfo, value);

  if (!newAttr) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return CallQueryInterface(newAttr, aReturn);
}

NS_IMETHODIMP
nsDOMAttribute::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
  *aOwnerDocument = nsnull;

  nsIDocument *document = GetOwnerDoc();

  return document ? CallQueryInterface(document, aOwnerDocument) : NS_OK;
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

  nsIContent* content = GetContentInternal();
  if (content) {
    nsIAtom *name = mNodeInfo->NameAtom();
    PRInt32 nameSpaceID = mNodeInfo->NamespaceID();

    nsAutoString tmpValue;
    rv = content->GetAttr(nameSpaceID, name, tmpValue);
    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
      content->UnsetAttr(nameSpaceID, name, PR_TRUE);

      content->SetAttr(newNodeInfo->NamespaceID(), newNodeInfo->NameAtom(),
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
  return nsGenericElement::InternalIsSupported(NS_STATIC_CAST(nsIDOMAttr*, this), 
                                               aFeature, aVersion, aReturn);
}

NS_IMETHODIMP
nsDOMAttribute::GetBaseURI(nsAString &aURI)
{
  aURI.Truncate();
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDOM3Node> node(do_QueryInterface(GetContentInternal()));
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
  NS_ASSERTION(aReturn, "IsSameNode() called with aReturn == nsnull!");
  
  *aReturn = SameCOMIdentity(NS_STATIC_CAST(nsIDOMNode*, this), aOther);

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
  *aReturn = PR_FALSE;
  nsCOMPtr<nsIDOM3Node> node(do_QueryInterface(GetContentInternal()));
  if (node) {
    return node->IsDefaultNamespace(aNamespaceURI, aReturn);
  }
  return NS_OK;
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
  return nsGenericElement::InternalGetFeature(NS_STATIC_CAST(nsIDOMAttr*, this), 
                                              aFeature, aVersion, aReturn);
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

  nsCOMPtr<nsIDOM3Node> node(do_QueryInterface(GetContentInternal()));
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
  nsCOMPtr<nsIDOM3Node> node(do_QueryInterface(GetContentInternal()));
  if (node)
    rv = node->LookupNamespaceURI(aNamespacePrefix, aNamespaceURI);
  return rv;
}

void*
nsDOMAttribute::GetProperty(nsIAtom* aPropertyName, nsresult* aStatus)
{
  nsIDocument *doc = GetOwnerDoc();
  if (!doc)
    return nsnull;

  return doc->PropertyTable()->GetProperty(this, aPropertyName, aStatus);
}

nsresult
nsDOMAttribute::SetProperty(nsIAtom            *aPropertyName,
                            void               *aValue,
                            NSPropertyDtorFunc  aDtor)
{
  nsIDocument *doc = GetOwnerDoc();
  if (!doc)
    return NS_ERROR_FAILURE;

  return doc->PropertyTable()->SetProperty(this, aPropertyName, aValue, aDtor,
                                           nsnull);
}

nsresult
nsDOMAttribute::DeleteProperty(nsIAtom* aPropertyName)
{
  nsIDocument *doc = GetOwnerDoc();
  if (!doc)
    return nsnull;

  return doc->PropertyTable()->DeleteProperty(this, aPropertyName);
}

void*
nsDOMAttribute::UnsetProperty(nsIAtom* aPropertyName, nsresult* aStatus)
{
  nsIDocument *doc = GetOwnerDoc();
  if (!doc)
    return nsnull;

  return doc->PropertyTable()->UnsetProperty(this, aPropertyName, aStatus);
}

void
nsDOMAttribute::Initialize()
{
  sInitialized = PR_TRUE;
}

void
nsDOMAttribute::Shutdown()
{
  sInitialized = PR_FALSE;
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
