/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsDOMAttributeMap.h"
#include "nsDOMAttribute.h"
#include "nsGenericElement.h"
#include "nsIContent.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsINameSpaceManager.h"

static NS_DEFINE_IID(kIDOMNamedNodeMapIID, NS_IDOMNAMEDNODEMAP_IID);
static NS_DEFINE_IID(kIDOMAttrIID, NS_IDOMATTR_IID);
static NS_DEFINE_IID(kIDOMAttributePrivateIID, NS_IDOMATTRIBUTEPRIVATE_IID);

//----------------------------------------------------------------------

nsDOMAttributeMap::nsDOMAttributeMap(nsIContent* aContent)
  : mContent(aContent)
{
  NS_INIT_REFCNT();
  mScriptObject = nsnull;
  mAttributes = nsnull;
  // We don't add a reference to our content. If it goes away,
  // we'll be told to drop our reference
}

PR_STATIC_CALLBACK (PRIntn)
RemoveAttributes(PLHashEntry* he, PRIntn i, void* arg)
{
  nsIDOMAttr* attr = (nsIDOMAttr*)he->value;
  char* str = (char*)he->key;
  
  if (nsnull != attr) {
    nsIDOMAttributePrivate* attrPrivate;
    attr->QueryInterface(kIDOMAttributePrivateIID, (void**)&attrPrivate);
    attrPrivate->DropReference();
    NS_RELEASE(attrPrivate);
    NS_RELEASE(attr);
  }
  delete [] str;

  return HT_ENUMERATE_REMOVE;
}

PR_STATIC_CALLBACK (PRIntn)
DropReferencesInAttributes(PLHashEntry* he, PRIntn i, void* arg)
{
  nsDOMAttribute* attr = (nsDOMAttribute*)he->value;

  if (nsnull != attr) {
    nsIDOMAttributePrivate* attrPrivate;
    attr->QueryInterface(kIDOMAttributePrivateIID, (void**)&attrPrivate);
    attrPrivate->DropReference();
    NS_RELEASE(attrPrivate);
  }

  return HT_ENUMERATE_NEXT;
}

nsDOMAttributeMap::~nsDOMAttributeMap()
{
  if (nsnull != mAttributes) {
    PL_HashTableEnumerateEntries(mAttributes, RemoveAttributes, nsnull);
  }
}

void 
nsDOMAttributeMap::DropReference()
{
  mContent = nsnull;
  if (nsnull != mAttributes) {
    PL_HashTableEnumerateEntries(mAttributes, DropReferencesInAttributes, nsnull);
  }
}

nsresult
nsDOMAttributeMap::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIDOMNamedNodeMapIID)) {
    nsIDOMNamedNodeMap* tmp = this;
    *aInstancePtr = (void*)tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    nsIScriptObjectOwner* tmp = this;
    *aInstancePtr = (void*)tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    nsIDOMNamedNodeMap* tmp1 = this;
    nsISupports* tmp2 = tmp1;
    *aInstancePtr = (void*)tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

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

PLHashTable*
nsDOMAttributeMap::GetAttributeTable()
{
  if ((nsnull == mAttributes) && (nsnull != mContent)) {
    PRInt32 count;
    mContent->GetAttributeCount(count);
    mAttributes = PL_NewHashTable(count, PL_HashString, PL_CompareStrings,
                                  PL_CompareValues, nsnull, nsnull);
  }

  return mAttributes;
}

nsresult
nsDOMAttributeMap::GetNamedItemCommon(const nsString& aAttrName,
                                      PRInt32 aNameSpaceID,
                                      nsIAtom* aNameAtom,
                                      nsIDOMNode** aAttribute)
{
  nsIDOMAttr* attribute;
  char buf[128];
  nsresult result = NS_OK;
  
  PLHashTable* attrHash = GetAttributeTable();
  aAttrName.ToCString(buf, sizeof(buf));
  if (nsnull != attrHash) {
    attribute = (nsIDOMAttr*)PL_HashTableLookup(attrHash, buf);
    if (nsnull == attribute) {
      nsresult attrResult;
      nsAutoString value;
      attrResult = mContent->GetAttribute(aNameSpaceID, aNameAtom, value);
      if (NS_CONTENT_ATTR_NOT_THERE != attrResult) {
        nsDOMAttribute* domAttribute;
        domAttribute = new nsDOMAttribute(mContent, aAttrName, value);
        if (nsnull == domAttribute) {
          result = NS_ERROR_OUT_OF_MEMORY;
        }
        else {
          result = domAttribute->QueryInterface(kIDOMAttrIID, 
                                                (void **)&attribute);
          char* hashKey = aAttrName.ToNewCString();
          PL_HashTableAdd(attrHash, hashKey, attribute);
        }
      }
    }
  }

  if (nsnull != attribute) {
    result = attribute->QueryInterface(kIDOMNodeIID, (void**)aAttribute);
  }
  else {
    *aAttribute = nsnull;
  }

  return result;
}

void
nsDOMAttributeMap::GetNormalizedName(PRInt32 aNameSpaceID,
                                     nsIAtom* aNameAtom,
                                     nsString& aAttrName)
{
  nsIAtom* prefix;
  aAttrName.Truncate();
  mContent->GetNameSpacePrefixFromId(aNameSpaceID, prefix);

  if (nsnull != prefix) {
    prefix->ToString(aAttrName);
    aAttrName.Append(":");
    NS_RELEASE(prefix);
  }

  if (nsnull != aNameAtom) {
    nsAutoString tmp;
    
    aNameAtom->ToString(tmp);
    aAttrName.Append(tmp);
  } 
}

nsresult
nsDOMAttributeMap::GetNamedItem(const nsString &aAttrName,
                                nsIDOMNode** aAttribute)
{
  nsresult result = NS_OK;
  if (nsnull != mContent) {
    nsIAtom* nameAtom;
    PRInt32 nameSpaceID;
    nsAutoString normalizedName;

    mContent->ParseAttributeString(aAttrName, nameAtom, nameSpaceID);
    GetNormalizedName(nameSpaceID, nameAtom, normalizedName);
    result = GetNamedItemCommon(normalizedName, 
                                nameSpaceID, 
                                nameAtom,
                                aAttribute);
    NS_IF_RELEASE(nameAtom);
  }
  else {
    *aAttribute = nsnull;
  }

  return result;
}

nsresult
nsDOMAttributeMap::SetNamedItem(nsIDOMNode *aNode, nsIDOMNode **aReturn)
{
  nsresult result = NS_OK;
  nsIDOMAttr* attribute;

  if ((nsnull != mContent) && (nsnull != aNode)) {
    result = aNode->QueryInterface(kIDOMAttrIID, (void**)&attribute);
    if (NS_OK == result) {
      PLHashTable* attrHash;

      attrHash = GetAttributeTable();
      if (nsnull != attrHash) {
        nsIDOMNode* oldAttribute;
        nsIDOMAttributePrivate* attrPrivate;
        nsAutoString name, value;
        char buf[128];
        char* key;
        nsIAtom* nameAtom;
        PRInt32 nameSpaceID;

        // Get normalized attribute name
        attribute->GetName(name);
        mContent->ParseAttributeString(name, nameAtom, nameSpaceID);
        GetNormalizedName(nameSpaceID, nameAtom, name);
        name.ToCString(buf, sizeof(buf));
        result = GetNamedItemCommon(name, nameSpaceID, nameAtom, &oldAttribute);

        if (nsnull != oldAttribute) {
          nsIDOMAttributePrivate* oldAttributePrivate;
          PLHashEntry** he;

          // Remove the attribute from the hash table, cleaning
          // the hash table entry as we go about it.
          he = PL_HashTableRawLookup(attrHash, PL_HashString(buf), buf);
          key = (char*)(*he)->key;
          PL_HashTableRemove(attrHash, buf);
          if (nsnull != key) {
            delete [] key;
          }
          
          result = oldAttribute->QueryInterface(kIDOMAttributePrivateIID,
                                                (void **)&oldAttributePrivate);
          if (NS_OK == result) {
            oldAttributePrivate->DropReference();
            NS_RELEASE(oldAttributePrivate);
          }

          *aReturn = oldAttribute;

          // Drop the reference held in the hash table
          NS_RELEASE(oldAttribute);
        }
        else {
          *aReturn = nsnull;
        }

        attribute->GetValue(value);

        // Associate the new attribute with the content
        // XXX Need to fail if it's already associated with other
        // content
        key = name.ToNewCString();
        result = attribute->QueryInterface(kIDOMAttributePrivateIID,
                                           (void **)&attrPrivate);
        if (NS_OK == result) {
          attrPrivate->SetContent(mContent);
          attrPrivate->SetName(name);
          NS_RELEASE(attrPrivate);
        }

        // Add the new attribute node to the hash table (maintaining
        // a reference to it)
        PL_HashTableAdd(attrHash, key, attribute);

        // Set the attribute on the content
        result = mContent->SetAttribute(nameSpaceID, nameAtom, value, PR_TRUE);
        NS_IF_RELEASE(nameAtom);
      }
    }
  }
  else {
    *aReturn = nsnull;
  }
  
  return result;
}

NS_IMETHODIMP
nsDOMAttributeMap::RemoveNamedItem(const nsString& aName, nsIDOMNode** aReturn)
{
  nsresult result = NS_OK;
  if (nsnull != mContent) {
    PLHashTable* attrHash;
    
    attrHash = GetAttributeTable();
    if (nsnull != attrHash) {
      nsIDOMNode* attribute;
      nsIDOMAttributePrivate* attrPrivate;
      char buf[128];
      char* key;
      nsIAtom* nameAtom;
      PRInt32 nameSpaceID;
      nsAutoString name;

      mContent->ParseAttributeString(aName, nameAtom, nameSpaceID);
      GetNormalizedName(nameSpaceID, nameAtom, name);
      name.ToCString(buf, sizeof(buf));
      result = GetNamedItemCommon(name, nameSpaceID, nameAtom, &attribute);
      
      if (nsnull != attribute) {
        PLHashEntry** he;

        // Remove the attribute from the hash table, cleaning
        // the hash table entry as we go about it.
        he = PL_HashTableRawLookup(attrHash, PL_HashString(buf), buf);
        key = (char*)(*he)->key;

        PL_HashTableRemove(attrHash, buf);
        if (nsnull != key) {
          delete [] key;
        }
          
        result = attribute->QueryInterface(kIDOMAttributePrivateIID,
                                           (void **)&attrPrivate);
        if (NS_OK == result) {
          attrPrivate->DropReference();
          NS_RELEASE(attrPrivate);
        }
        
        *aReturn = attribute;

        // Drop the reference held in the hash table
        NS_RELEASE(attribute);
      }
      else {
        *aReturn = nsnull;
      }

      // Unset the attribute in the content
      result = mContent->UnsetAttribute(nameSpaceID, nameAtom, PR_TRUE);
      NS_IF_RELEASE(nameAtom);
    }
  }

  return result;
}


nsresult
nsDOMAttributeMap::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  PRInt32 nameSpaceID;
  nsIAtom* nameAtom = nsnull;
  nsresult result = NS_OK;
  if ((nsnull != mContent) &&
      NS_SUCCEEDED(mContent->GetAttributeNameAt(aIndex, 
                                                nameSpaceID, 
                                                nameAtom))) {
    nsAutoString attrName;

    GetNormalizedName(nameSpaceID, nameAtom, attrName);   
    result = GetNamedItemCommon(attrName, nameSpaceID, nameAtom, aReturn);
    NS_IF_RELEASE(nameAtom);
  }
  else {
    *aReturn = nsnull;
  }
  
  return result;
}

nsresult
nsDOMAttributeMap::GetLength(PRUint32 *aLength)
{
  PRInt32 n;
  nsresult rv = NS_OK;

  if (nsnull != mContent) {
    rv = mContent->GetAttributeCount(n);
    *aLength = PRUint32(n);
  }
  else {
    *aLength = 0;
  }
  return rv;
}
