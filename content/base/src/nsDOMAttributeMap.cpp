/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsDOMAttributeMap.h"
#include "nsDOMAttribute.h"
#include "nsGenericElement.h"
#include "nsIContent.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsINameSpaceManager.h"
#include "nsDOMError.h"


//----------------------------------------------------------------------

nsDOMAttributeMap::nsDOMAttributeMap(nsIContent* aContent)
  : mContent(aContent)
{
  NS_INIT_REFCNT();
  mScriptObject = nsnull;
  // We don't add a reference to our content. If it goes away,
  // we'll be told to drop our reference
}

nsDOMAttributeMap::~nsDOMAttributeMap()
{
}

void
nsDOMAttributeMap::DropReference()
{
  mContent = nsnull;
}

NS_INTERFACE_MAP_BEGIN(nsDOMAttributeMap)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMNamedNodeMap)
   NS_INTERFACE_MAP_ENTRY(nsIDOMNamedNodeMap)
   NS_INTERFACE_MAP_ENTRY(nsIScriptObjectOwner)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsDOMAttributeMap)
NS_IMPL_RELEASE(nsDOMAttributeMap)

nsresult
nsDOMAttributeMap::GetScriptObject(nsIScriptContext *aContext,
                                   void** aScriptObject)
{
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    nsIDOMScriptObjectFactory *factory;

    res = nsGenericElement::GetScriptObjectFactory(&factory);
    if (NS_OK != res) {
      return res;
    }

    res = factory->NewScriptNamedNodeMap(aContext,
                                         (nsISupports *)(nsIDOMNamedNodeMap *)this,
                                         (nsISupports *)mContent,
                                         (void**)&mScriptObject);
    NS_RELEASE(factory);
  }
  *aScriptObject = mScriptObject;
  return res;
}

nsresult
nsDOMAttributeMap::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

void
nsDOMAttributeMap::GetNormalizedName(PRInt32 aNameSpaceID,
                                     nsIAtom* aNameAtom,
                                     nsString& aAttrName)
{
  nsCOMPtr<nsIAtom> prefix;
  aAttrName.Truncate();
  mContent->GetNameSpacePrefixFromId(aNameSpaceID, *getter_AddRefs(prefix));

  if (prefix) {
    prefix->ToString(aAttrName);
    aAttrName.AppendWithConversion(":");
  }

  if (aNameAtom) {
    nsAutoString tmp;

    aNameAtom->ToString(tmp);
    aAttrName.Append(tmp);
  }
}

nsresult
nsDOMAttributeMap::GetNamedItem(const nsString &aAttrName,
                                nsIDOMNode** aAttribute)
{
  NS_ENSURE_ARG_POINTER(aAttribute);
  *aAttribute = nsnull;

  nsresult rv = NS_OK;
  if (mContent) {
    nsCOMPtr<nsIAtom> nameAtom;
    PRInt32 nameSpaceID;
    nsAutoString normalizedName;

    mContent->ParseAttributeString(aAttrName, *getter_AddRefs(nameAtom),
                                   nameSpaceID);
    if (kNameSpaceID_Unknown == nameSpaceID) {
      return NS_ERROR_DOM_NOT_FOUND_ERR;
    }

    GetNormalizedName(nameSpaceID, nameAtom, normalizedName);

    nsresult attrResult;
    nsAutoString value;
    attrResult = mContent->GetAttribute(nameSpaceID, nameAtom, value);

    if (NS_CONTENT_ATTR_NOT_THERE != attrResult && NS_SUCCEEDED(attrResult)) {
      nsDOMAttribute* domAttribute;
      domAttribute = new nsDOMAttribute(mContent, normalizedName, value);
      if (!domAttribute) {
        rv = NS_ERROR_OUT_OF_MEMORY;
      }

      rv = domAttribute->QueryInterface(NS_GET_IID(nsIDOMAttr),
                                        (void **)aAttribute);
    }
  }

  return rv;
}

nsresult
nsDOMAttributeMap::SetNamedItem(nsIDOMNode *aNode, nsIDOMNode **aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  if (!aNode) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv = NS_OK;
  *aReturn = nsnull;

  if (mContent) {
    nsCOMPtr<nsIDOMAttr> attribute(do_QueryInterface(aNode));

    if (!attribute) {
      return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
    }

    nsAutoString name, value;
    nsCOMPtr<nsIAtom> nameAtom;
    PRInt32 nameSpaceID;

    // Get normalized attribute name
    attribute->GetName(name);
    mContent->ParseAttributeString(name, *getter_AddRefs(nameAtom),
                                   nameSpaceID);
    if (kNameSpaceID_Unknown == nameSpaceID) {
      nameSpaceID = kNameSpaceID_None;  // ignore unknown prefix XXX is this correct?
    }
    GetNormalizedName(nameSpaceID, nameAtom, name);

    nsresult attrResult = mContent->GetAttribute(nameSpaceID, nameAtom, value);

    if (NS_CONTENT_ATTR_NOT_THERE != attrResult && NS_SUCCEEDED(attrResult)) {
      nsDOMAttribute* domAttribute;
      // We pass a null content here since the attr node we return isn't
      // tied to this content anymore.
      domAttribute = new nsDOMAttribute(nsnull, name, value);
      if (!domAttribute) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      rv = domAttribute->QueryInterface(NS_GET_IID(nsIDOMAttr),
                                        (void **)aReturn);
    }

    attribute->GetValue(value);

    rv = mContent->SetAttribute(nameSpaceID, nameAtom, value, PR_TRUE);
  }

  return rv;
}

NS_IMETHODIMP
nsDOMAttributeMap::RemoveNamedItem(const nsString& aName, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsresult rv = NS_OK;

  if (mContent) {
    nsCOMPtr<nsIDOMNode> attribute;
    nsCOMPtr<nsIAtom> nameAtom;
    PRInt32 nameSpaceID;
    nsAutoString name; name.Assign(aName);

    mContent->ParseAttributeString(aName, *getter_AddRefs(nameAtom),
                                   nameSpaceID);
    if (kNameSpaceID_Unknown == nameSpaceID) {
      return NS_ERROR_DOM_NOT_FOUND_ERR;
    }
    GetNormalizedName(nameSpaceID, nameAtom, name);

    nsresult attrResult;
    nsAutoString value;
    attrResult = mContent->GetAttribute(nameSpaceID, nameAtom, value);

    if (NS_CONTENT_ATTR_NOT_THERE != attrResult && NS_SUCCEEDED(attrResult)) {
      nsDOMAttribute* domAttribute;
      domAttribute = new nsDOMAttribute(nsnull, name, value);
      if (!domAttribute) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      rv = domAttribute->QueryInterface(NS_GET_IID(nsIDOMAttr),
                                        (void **)aReturn);
    } else {
      return NS_ERROR_DOM_NOT_FOUND_ERR;
    }

    rv = mContent->UnsetAttribute(nameSpaceID, nameAtom, PR_TRUE);
  }

  return rv;
}


nsresult
nsDOMAttributeMap::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  PRInt32 nameSpaceID;
  nsCOMPtr<nsIAtom> nameAtom;

  nsresult rv = NS_OK;
  if (mContent &&
      NS_SUCCEEDED(mContent->GetAttributeNameAt(aIndex,
                                                nameSpaceID,
                                                *getter_AddRefs(nameAtom)))) {
    nsAutoString value, name;
    mContent->GetAttribute(nameSpaceID, nameAtom, value);

    GetNormalizedName(nameSpaceID, nameAtom, name);

    nsDOMAttribute* domAttribute;

    domAttribute = new nsDOMAttribute(mContent, name, value);
    if (!domAttribute) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    rv = domAttribute->QueryInterface(NS_GET_IID(nsIDOMAttr),
                                      (void **)aReturn);
  }
  else {
    *aReturn = nsnull;

    rv = NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  return rv;
}

nsresult
nsDOMAttributeMap::GetLength(PRUint32 *aLength)
{
  NS_ENSURE_ARG_POINTER(aLength);

  PRInt32 n;
  nsresult rv = NS_OK;

  if (nsnull != mContent) {
    rv = mContent->GetAttributeCount(n);
    *aLength = PRUint32(n);
  } else {
    *aLength = 0;
  }
  return rv;
}

nsresult
nsDOMAttributeMap::GetNamedItemNS(const nsString& aNamespaceURI,
                                  const nsString& aLocalName,
                                  nsIDOMNode** aReturn)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsDOMAttributeMap::SetNamedItemNS(nsIDOMNode* aArg, nsIDOMNode** aReturn)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsDOMAttributeMap::RemoveNamedItemNS(const nsString& aNamespaceURI,
                                     const nsString&aLocalName,
                                     nsIDOMNode** aReturn)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


#ifdef DEBUG
nsresult
nsDOMAttributeMap::SizeOfNamedNodeMap(nsIDOMNamedNodeMap* aMap,
                                      nsISizeOfHandler* aSizer,
                                      PRUint32* aResult)
{
  if (!aResult) return NS_ERROR_NULL_POINTER;
  nsDOMAttributeMap* map = (nsDOMAttributeMap*) aMap;
  PRUint32 sum = sizeof(nsDOMAttributeMap);
  *aResult = sum;
  return NS_OK;
}
#endif
