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


#include "nsIDOMNamedNodeMap.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIContent.h"
#include "nsGenericDOMDataNode.h"
#include "nsGenericElement.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsLayoutAtoms.h"
#include "nsString.h"
#include "nsIXMLContent.h"

#include "nsCOMPtr.h"
#include "nsISupportsArray.h"


class nsXMLNamedNodeMap : public nsIDOMNamedNodeMap,
                          public nsIScriptObjectOwner
{
public:
  nsXMLNamedNodeMap(nsISupportsArray *aArray);
  virtual ~nsXMLNamedNodeMap();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNamedNodeMap
  NS_IMETHOD    GetLength(PRUint32* aLength);
  NS_IMETHOD    GetNamedItem(const nsAReadableString& aName, nsIDOMNode** aReturn);
  NS_IMETHOD    SetNamedItem(nsIDOMNode* aArg, nsIDOMNode** aReturn);
  NS_IMETHOD    RemoveNamedItem(const nsAReadableString& aName, nsIDOMNode** aReturn);
  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMNode** aReturn);
  NS_IMETHOD    GetNamedItemNS(const nsAReadableString& aNamespaceURI,
                               const nsAReadableString& aLocalName,
                               nsIDOMNode** aReturn);
  NS_IMETHOD    SetNamedItemNS(nsIDOMNode* aArg, nsIDOMNode** aReturn);
  NS_IMETHOD    RemoveNamedItemNS(const nsAReadableString& aNamespaceURI,
                                  const nsAReadableString&aLocalName,
                                  nsIDOMNode** aReturn);

  // nsIScriptObjectOwner interface
  NS_IMETHOD GetScriptObject(nsIScriptContext* aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void *aScriptObject);

protected:
  nsISupportsArray *mArray;

  void* mScriptObject;
};

nsresult
NS_NewXMLNamedNodeMap(nsIDOMNamedNodeMap** aInstancePtrResult,
                      nsISupportsArray *aArray)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIDOMNamedNodeMap* it = new nsXMLNamedNodeMap(aArray);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIDOMNamedNodeMap), (void **) aInstancePtrResult);
}

nsXMLNamedNodeMap::nsXMLNamedNodeMap(nsISupportsArray *aArray)
{
  NS_INIT_REFCNT();
  mScriptObject = nsnull;

  mArray = aArray;

  NS_IF_ADDREF(mArray);
}

nsXMLNamedNodeMap::~nsXMLNamedNodeMap()
{
  NS_IF_RELEASE(mArray);
}

NS_IMPL_ADDREF(nsXMLNamedNodeMap)
NS_IMPL_RELEASE(nsXMLNamedNodeMap)

nsresult 
nsXMLNamedNodeMap::QueryInterface(REFNSIID aIID, void** aInstancePtrResult)
{
  if (NULL == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aIID.Equals(NS_GET_IID(nsISupports))) {                          
    nsIDOMNamedNodeMap* tmp = this;                                
    nsISupports* tmp2 = tmp;                                
    *aInstancePtrResult = (void*) tmp2;                                  
    NS_ADDREF_THIS();                                       
    return NS_OK;                                           
  }                                                         
  if (aIID.Equals(NS_GET_IID(nsIScriptObjectOwner))) {                 
    nsIScriptObjectOwner* tmp = this;                      
    *aInstancePtrResult = (void*) tmp;                                   
    NS_ADDREF_THIS();                                       
    return NS_OK;                                           
  }                                                         
  if (aIID.Equals(NS_GET_IID(nsIDOMNamedNodeMap))) {
    nsIDOMNamedNodeMap* tmp = this;
    *aInstancePtrResult = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP    
nsXMLNamedNodeMap::GetLength(PRUint32* aLength)
{
  if (mArray)
    mArray->Count(aLength);
  else
    *aLength = 0;

  return NS_OK;
}

NS_IMETHODIMP    
nsXMLNamedNodeMap::GetNamedItem(const nsAReadableString& aName, nsIDOMNode** aReturn)
{
  if (!aReturn)
    return NS_ERROR_NULL_POINTER;

  *aReturn = 0;

  if (!mArray)
    return NS_OK;

  PRUint32 i, count;
  mArray->Count(&count);

  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISupports> supports_node(dont_AddRef(mArray->ElementAt(i)));
    nsCOMPtr<nsIDOMNode> node(do_QueryInterface(supports_node));

    if (!node)
      break;

    nsAutoString tmpName;

    node->GetNodeName(tmpName);

    if (aName.Equals(tmpName)) {
      *aReturn = node;

      break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsXMLNamedNodeMap::SetNamedItem(nsIDOMNode* aArg, nsIDOMNode** aReturn)
{
  if (!aReturn || !aArg)
    return NS_ERROR_NULL_POINTER;

  *aReturn = 0;

  nsAutoString argName;
  aArg->GetNodeName(argName);

  if (mArray) {
    PRUint32 i, count;
    mArray->Count(&count);

    for (i = 0; i < count; i++) {
      nsCOMPtr<nsISupports> supports_node(dont_AddRef(mArray->ElementAt(i)));
      nsCOMPtr<nsIDOMNode> node(do_QueryInterface(supports_node));

      if (!node)
        break;

      nsAutoString tmpName;

      node->GetNodeName(tmpName);

      if (argName.Equals(tmpName)) {
        mArray->ReplaceElementAt(aArg, i);

        *aReturn = node;

        break;
      }
    }
  } else {
    nsresult rv = NS_NewISupportsArray(&mArray);

    if (NS_FAILED(rv))
      return rv;
  }

  if (!*aReturn)
    mArray->AppendElement(aArg);

  return NS_OK;
}

NS_IMETHODIMP    
nsXMLNamedNodeMap::RemoveNamedItem(const nsAReadableString& aName, nsIDOMNode** aReturn)
{
  if (!aReturn)
    return NS_ERROR_NULL_POINTER;

  *aReturn = 0;

  if (!mArray)
    return NS_OK;

  PRUint32 i, count;
  mArray->Count(&count);

  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISupports> supports_node(dont_AddRef(mArray->ElementAt(i)));
    nsCOMPtr<nsIDOMNode> node(do_QueryInterface(supports_node));

    if (!node)
      break;

    nsAutoString tmpName;

    node->GetNodeName(tmpName);

    if (aName.Equals(tmpName)) {
      *aReturn = node;

      mArray->RemoveElementAt(i);

      break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsXMLNamedNodeMap::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  PRUint32 count;

  if (!aReturn)
    return NS_ERROR_NULL_POINTER;

  mArray->Count(&count);
  if (mArray && aIndex < count) {
    nsCOMPtr<nsISupports> supports_node(dont_AddRef(mArray->ElementAt(aIndex)));
    nsCOMPtr<nsIDOMNode> domNode(do_QueryInterface(supports_node));

    *aReturn = domNode;
  } else
    *aReturn = 0;

  return NS_OK;
}

nsresult
nsXMLNamedNodeMap::GetNamedItemNS(const nsAReadableString& aNamespaceURI, 
                                  const nsAReadableString& aLocalName,
                                  nsIDOMNode** aReturn)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsXMLNamedNodeMap::SetNamedItemNS(nsIDOMNode* aArg, nsIDOMNode** aReturn)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsXMLNamedNodeMap::RemoveNamedItemNS(const nsAReadableString& aNamespaceURI, 
                                     const nsAReadableString&aLocalName,
                                     nsIDOMNode** aReturn)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsXMLNamedNodeMap::GetScriptObject(nsIScriptContext* aContext, 
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
                                         (nsISupports*)(nsIDOMNamedNodeMap*)this,
                                         nsnull /*mInner.mParent*/, 
                                         (void**)&mScriptObject);

    NS_RELEASE(factory);
  }
  *aScriptObject = mScriptObject;
  return res;
}

NS_IMETHODIMP 
nsXMLNamedNodeMap::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}


