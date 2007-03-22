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
 *   Allan Beaufour <allan@beaufour.dk>
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
 * Implementation of the |attributes| property of DOM Core's nsIDOMNode object.
 */

#include "nsDOMAttributeMap.h"
#include "nsDOMAttribute.h"
#include "nsGenericElement.h"
#include "nsIContent.h"
#include "nsINameSpaceManager.h"
#include "nsDOMError.h"
#include "nsContentUtils.h"
#include "nsNodeInfoManager.h"
#include "nsAttrName.h"

//----------------------------------------------------------------------

nsDOMAttributeMap::nsDOMAttributeMap(nsIContent* aContent)
  : mContent(aContent)
{
  // We don't add a reference to our content. If it goes away,
  // we'll be told to drop our reference
}

PRBool
nsDOMAttributeMap::Init()
{
  return mAttributeCache.Init();
}

/**
 * Clear map pointer for attributes.
 */
PLDHashOperator
RemoveMapRef(nsAttrHashKey::KeyType aKey, nsCOMPtr<nsIDOMNode>& aData, void* aUserArg)
{
  nsCOMPtr<nsIAttribute> attr(do_QueryInterface(aData));
  NS_ASSERTION(attr, "non-nsIAttribute somehow made it into the hashmap?!");
  attr->SetMap(nsnull);

  return PL_DHASH_REMOVE;
}

nsDOMAttributeMap::~nsDOMAttributeMap()
{
  mAttributeCache.Enumerate(RemoveMapRef, nsnull);
}

void
nsDOMAttributeMap::DropReference()
{
  mAttributeCache.Enumerate(RemoveMapRef, nsnull);
  mContent = nsnull;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMAttributeMap)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsDOMAttributeMap)
  tmp->DropReference();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END


PLDHashOperator
TraverseMapEntry(nsAttrHashKey::KeyType aKey, nsCOMPtr<nsIDOMNode>& aData, void* aUserArg)
{
  nsCycleCollectionTraversalCallback *cb = 
    NS_STATIC_CAST(nsCycleCollectionTraversalCallback*, aUserArg);

  cb->NoteXPCOMChild(aData.get());

  return PL_DHASH_NEXT;
}

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsDOMAttributeMap)
  tmp->mAttributeCache.Enumerate(TraverseMapEntry, &cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END


// QueryInterface implementation for nsDOMAttributeMap
NS_INTERFACE_MAP_BEGIN(nsDOMAttributeMap)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNamedNodeMap)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(NamedNodeMap)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsDOMAttributeMap)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMAttributeMap)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMAttributeMap)

PLDHashOperator
SetOwnerDocumentFunc(nsAttrHashKey::KeyType aKey, nsCOMPtr<nsIDOMNode>& aData,
                     void* aUserArg)
{
  nsCOMPtr<nsIAttribute> attr(do_QueryInterface(aData));
  NS_ASSERTION(attr, "non-nsIAttribute somehow made it into the hashmap?!");
  nsresult rv = attr->SetOwnerDocument(NS_STATIC_CAST(nsIDocument*, aUserArg));

  return NS_FAILED(rv) ? PL_DHASH_STOP : PL_DHASH_NEXT;
}

nsresult
nsDOMAttributeMap::SetOwnerDocument(nsIDocument* aDocument)
{
  PRUint32 n = mAttributeCache.Enumerate(SetOwnerDocumentFunc, aDocument);
  NS_ENSURE_TRUE(n == mAttributeCache.Count(), NS_ERROR_FAILURE);

  return NS_OK;
}

void
nsDOMAttributeMap::DropAttribute(PRInt32 aNamespaceID, nsIAtom* aLocalName)
{
  nsAttrKey attr(aNamespaceID, aLocalName);
  nsIDOMNode *node = mAttributeCache.GetWeak(attr);
  if (node) {
    nsCOMPtr<nsIAttribute> iAttr(do_QueryInterface(node));
    NS_ASSERTION(iAttr, "non-nsIAttribute somehow made it into the hashmap?!");

    // Break link to map
    iAttr->SetMap(nsnull);

    // Remove from cache
    mAttributeCache.Remove(attr);
  }
}

nsresult
nsDOMAttributeMap::GetAttribute(nsINodeInfo* aNodeInfo,
                                nsIDOMNode** aReturn,
                                PRBool aRemove)
{
  NS_ASSERTION(aNodeInfo, "GetAttribute() called with aNodeInfo == nsnull!");
  NS_ASSERTION(aReturn, "GetAttribute() called with aReturn == nsnull");

  *aReturn = nsnull;

  nsAttrKey attr(aNodeInfo->NamespaceID(), aNodeInfo->NameAtom());

  if (!mAttributeCache.Get(attr, aReturn)) {
    nsAutoString value;
    if (aRemove) {
      // As we are removing the attribute we need to set the current value in
      // the attribute node.
      mContent->GetAttr(aNodeInfo->NamespaceID(), aNodeInfo->NameAtom(), value);
    }
    nsCOMPtr<nsIDOMNode> newAttr = new nsDOMAttribute(aRemove ? nsnull : this,
                                                      aNodeInfo, value);
    if (!newAttr) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    if (!aRemove && !mAttributeCache.Put(attr, newAttr)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    newAttr.swap(*aReturn);
  }
  else if (aRemove) {
    nsCOMPtr<nsIAttribute> iAttr(do_QueryInterface(*aReturn));
    NS_ASSERTION(iAttr, "non-nsIAttribute somehow made it into the hashmap?!");

    // Break link to map
    iAttr->SetMap(nsnull);

    // Remove from cache
    mAttributeCache.Remove(attr);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttributeMap::GetNamedItem(const nsAString& aAttrName,
                                nsIDOMNode** aAttribute)
{
  NS_ENSURE_ARG_POINTER(aAttribute);
  *aAttribute = nsnull;

  nsresult rv = NS_OK;
  if (mContent) {
    nsCOMPtr<nsINodeInfo> ni =
      mContent->GetExistingAttrNameFromQName(aAttrName);
    if (ni) {
      rv = GetAttribute(ni, aAttribute);
    }
  }

  return rv;
}

NS_IMETHODIMP
nsDOMAttributeMap::SetNamedItem(nsIDOMNode *aNode, nsIDOMNode **aReturn)
{
  return SetNamedItemInternal(aNode, aReturn, PR_FALSE);
}

NS_IMETHODIMP
nsDOMAttributeMap::SetNamedItemNS(nsIDOMNode *aNode, nsIDOMNode **aReturn)
{
  return SetNamedItemInternal(aNode, aReturn, PR_TRUE);
}

nsresult
nsDOMAttributeMap::SetNamedItemInternal(nsIDOMNode *aNode,
                                        nsIDOMNode **aReturn,
                                        PRBool aWithNS)
{
  NS_ENSURE_ARG_POINTER(aNode);
  NS_ENSURE_ARG_POINTER(aReturn);

  nsresult rv = NS_OK;
  *aReturn = nsnull;
  nsCOMPtr<nsIDOMNode> tmpReturn;

  if (mContent) {
    // XXX should check same-origin between mContent and aNode however
    // nsContentUtils::CheckSameOrigin can't deal with attributenodes yet
    
    nsCOMPtr<nsIDOMAttr> attribute(do_QueryInterface(aNode));
    nsCOMPtr<nsIAttribute> iAttribute(do_QueryInterface(aNode));
    if (!attribute || !iAttribute) {
      return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
    }

    // Check that attribute is not owned by somebody else
    nsDOMAttributeMap* owner = iAttribute->GetMap();
    if (owner) {
      if (owner != this) {
        return NS_ERROR_DOM_INUSE_ATTRIBUTE_ERR;
      }

      // setting a preexisting attribute is a no-op, just return the same
      // node.
      NS_ADDREF(*aReturn = aNode);
      
      return NS_OK;
    }

    if (!mContent->HasSameOwnerDoc(iAttribute)) {
      return NS_ERROR_DOM_WRONG_DOCUMENT_ERR;
    }

    // Get nodeinfo and preexisting attribute (if it exists)
    nsAutoString name;
    nsCOMPtr<nsINodeInfo> ni;

    // SetNamedItemNS()
    if (aWithNS) {
      // Return existing attribute, if present
      ni = iAttribute->NodeInfo();

      if (mContent->HasAttr(ni->NamespaceID(), ni->NameAtom())) {
        rv = GetAttribute(ni, getter_AddRefs(tmpReturn), PR_TRUE);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
    else { // SetNamedItem()
      attribute->GetName(name);

      // get node-info of old attribute
      ni = mContent->GetExistingAttrNameFromQName(name);
      if (ni) {
        rv = GetAttribute(ni, getter_AddRefs(tmpReturn), PR_TRUE);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      else {
        rv = mContent->NodeInfo()->NodeInfoManager()->
          GetNodeInfo(name, nsnull, kNameSpaceID_None, getter_AddRefs(ni));
        NS_ENSURE_SUCCESS(rv, rv);
        // value is already empty
      }
    }

    nsAutoString value;
    attribute->GetValue(value);

    // Add the new attribute to the attribute map before updating
    // its value in the element. @see bug 364413.
    nsAttrKey attrkey(ni->NamespaceID(), ni->NameAtom());
    rv = mAttributeCache.Put(attrkey, attribute);
    NS_ENSURE_SUCCESS(rv, rv);
    iAttribute->SetMap(this);

    if (!aWithNS && ni->NamespaceID() == kNameSpaceID_None &&
        mContent->IsNodeOfType(nsINode::eHTML)) {
      // Set via setAttribute(), which may do normalization on the
      // attribute name for HTML
      nsCOMPtr<nsIDOMElement> ourElement(do_QueryInterface(mContent));
      NS_ASSERTION(ourElement, "HTML content that's not an element?");
      rv = ourElement->SetAttribute(name, value);
    }
    else {
      // It's OK to just use SetAttr
      rv = mContent->SetAttr(ni->NamespaceID(), ni->NameAtom(),
                             ni->GetPrefixAtom(), value, PR_TRUE);
    }
    if (NS_FAILED(rv)) {
      DropAttribute(ni->NamespaceID(), ni->NameAtom());
    }
  }

  tmpReturn.swap(*aReturn); // transfers ref.

  return rv;
}

NS_IMETHODIMP
nsDOMAttributeMap::RemoveNamedItem(const nsAString& aName,
                                   nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsresult rv = NS_OK;

  if (mContent) {
    nsCOMPtr<nsINodeInfo> ni = mContent->GetExistingAttrNameFromQName(aName);
    if (!ni) {
      return NS_ERROR_DOM_NOT_FOUND_ERR;
    }

    rv = GetAttribute(ni, aReturn);
    NS_ENSURE_SUCCESS(rv, rv);

    // This removes the attribute node from the attribute map.
    rv = mContent->UnsetAttr(ni->NamespaceID(), ni->NameAtom(), PR_TRUE);
  }

  return rv;
}


NS_IMETHODIMP
nsDOMAttributeMap::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  const nsAttrName* name;
  if (mContent && (name = mContent->GetAttrNameAt(aIndex))) {
    // Don't use the nodeinfo even if one exists since it can
    // have the wrong owner document.
    nsCOMPtr<nsINodeInfo> ni;
    mContent->NodeInfo()->NodeInfoManager()->
      GetNodeInfo(name->LocalName(), name->GetPrefix(), name->NamespaceID(),
                  getter_AddRefs(ni));
    NS_ENSURE_TRUE(ni, NS_ERROR_FAILURE);

    return GetAttribute(ni, aReturn);
  }

  *aReturn = nsnull;

  return NS_OK;
}

nsresult
nsDOMAttributeMap::GetLength(PRUint32 *aLength)
{
  NS_ENSURE_ARG_POINTER(aLength);

  if (mContent) {
    *aLength = mContent->GetAttrCount();
  }
  else {
    *aLength = 0;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttributeMap::GetNamedItemNS(const nsAString& aNamespaceURI,
                                  const nsAString& aLocalName,
                                  nsIDOMNode** aReturn)
{
  return GetNamedItemNSInternal(aNamespaceURI, aLocalName, aReturn);
}

nsresult
nsDOMAttributeMap::GetNamedItemNSInternal(const nsAString& aNamespaceURI,
                                          const nsAString& aLocalName,
                                          nsIDOMNode** aReturn,
                                          PRBool aRemove)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  if (!mContent) {
    return NS_OK;
  }

  NS_ConvertUTF16toUTF8 utf8Name(aLocalName);
  PRInt32 nameSpaceID = kNameSpaceID_None;

  if (!aNamespaceURI.IsEmpty()) {
    nameSpaceID =
      nsContentUtils::NameSpaceManager()->GetNameSpaceID(aNamespaceURI);

    if (nameSpaceID == kNameSpaceID_Unknown) {
      return NS_OK;
    }
  }

  PRUint32 i, count = mContent->GetAttrCount();
  for (i = 0; i < count; ++i) {
    const nsAttrName* name = mContent->GetAttrNameAt(i);
    PRInt32 attrNS = name->NamespaceID();
    nsIAtom* nameAtom = name->LocalName();

    if (nameSpaceID == attrNS &&
        nameAtom->EqualsUTF8(utf8Name)) {
      nsCOMPtr<nsINodeInfo> ni;
      mContent->NodeInfo()->NodeInfoManager()->
        GetNodeInfo(nameAtom, name->GetPrefix(), nameSpaceID,
                    getter_AddRefs(ni));
      NS_ENSURE_TRUE(ni, NS_ERROR_FAILURE);

      return GetAttribute(ni, aReturn, aRemove);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttributeMap::RemoveNamedItemNS(const nsAString& aNamespaceURI,
                                     const nsAString& aLocalName,
                                     nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsresult rv = GetNamedItemNSInternal(aNamespaceURI,
                                       aLocalName,
                                       aReturn,
                                       PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!*aReturn) {
    return NS_OK;
  }

  nsCOMPtr<nsIAttribute> attr = do_QueryInterface(*aReturn);
  NS_ASSERTION(attr, "attribute returned from nsDOMAttributeMap::GetNameItemNS "
               "didn't implement nsIAttribute");
  NS_ENSURE_TRUE(attr, NS_ERROR_UNEXPECTED);

  nsINodeInfo *ni = attr->NodeInfo();
  mContent->UnsetAttr(ni->NamespaceID(), ni->NameAtom(), PR_TRUE);

  return NS_OK;
}

PRUint32
nsDOMAttributeMap::Count() const
{
  return mAttributeCache.Count();
}

PRUint32
nsDOMAttributeMap::Enumerate(AttrCache::EnumReadFunction aFunc,
                             void *aUserArg) const
{
  return mAttributeCache.EnumerateRead(aFunc, aUserArg);
}
