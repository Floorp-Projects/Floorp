/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "nsIDOMNamedNodeMap.h"
#include "nsGenericDOMDataNode.h"
#include "nsString.h"

#include "nsCOMPtr.h"
#include "nsISupportsArray.h"
#include "nsContentUtils.h"

class nsXMLNamedNodeMap : public nsIDOMNamedNodeMap
{
public:
  nsXMLNamedNodeMap(nsISupportsArray *aArray);
  virtual ~nsXMLNamedNodeMap();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNamedNodeMap
  NS_IMETHOD    GetLength(PRUint32* aLength);
  NS_IMETHOD    GetNamedItem(const nsAReadableString& aName,
                             nsIDOMNode** aReturn);
  NS_IMETHOD    SetNamedItem(nsIDOMNode* aArg, nsIDOMNode** aReturn);
  NS_IMETHOD    RemoveNamedItem(const nsAReadableString& aName,
                                nsIDOMNode** aReturn);
  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMNode** aReturn);
  NS_IMETHOD    GetNamedItemNS(const nsAReadableString& aNamespaceURI,
                               const nsAReadableString& aLocalName,
                               nsIDOMNode** aReturn);
  NS_IMETHOD    SetNamedItemNS(nsIDOMNode* aArg, nsIDOMNode** aReturn);
  NS_IMETHOD    RemoveNamedItemNS(const nsAReadableString& aNamespaceURI,
                                  const nsAReadableString&aLocalName,
                                  nsIDOMNode** aReturn);

protected:
  nsISupportsArray *mArray;
};

nsresult
NS_NewXMLNamedNodeMap(nsIDOMNamedNodeMap** aInstancePtrResult,
                      nsISupportsArray *aArray)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsIDOMNamedNodeMap* it = new nsXMLNamedNodeMap(aArray);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *aInstancePtrResult = it;
  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}

nsXMLNamedNodeMap::nsXMLNamedNodeMap(nsISupportsArray *aArray)
{
  NS_INIT_REFCNT();

  mArray = aArray;

  NS_IF_ADDREF(mArray);
}

nsXMLNamedNodeMap::~nsXMLNamedNodeMap()
{
  NS_IF_RELEASE(mArray);
}

// QueryInterface implementation for nsXMLNamedNodeMap
NS_INTERFACE_MAP_BEGIN(nsXMLNamedNodeMap)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNamedNodeMap)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(NamedNodeMap)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsXMLNamedNodeMap)
NS_IMPL_RELEASE(nsXMLNamedNodeMap)


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
nsXMLNamedNodeMap::GetNamedItem(const nsAReadableString& aName,
                                nsIDOMNode** aReturn)
{
  if (!aReturn)
    return NS_ERROR_NULL_POINTER;

  *aReturn = 0;

  if (!mArray)
    return NS_OK;

  nsCOMPtr<nsIDOMNode> node;
  PRUint32 i, count;

  mArray->Count(&count);

  for (i = 0; i < count; i++) {
    mArray->QueryElementAt(i, NS_GET_IID(nsIDOMNode), getter_AddRefs(node));

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
    nsCOMPtr<nsIDOMNode> node;
    PRUint32 i, count;

    mArray->Count(&count);

    for (i = 0; i < count; i++) {
      mArray->QueryElementAt(i, NS_GET_IID(nsIDOMNode), getter_AddRefs(node));

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
nsXMLNamedNodeMap::RemoveNamedItem(const nsAReadableString& aName,
                                   nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  *aReturn = 0;

  if (!mArray)
    return NS_OK;

  nsCOMPtr<nsIDOMNode> node;
  PRUint32 i, count;
  mArray->Count(&count);

  for (i = 0; i < count; i++) {
    mArray->QueryElementAt(i, NS_GET_IID(nsIDOMNode), getter_AddRefs(node));

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
  NS_ENSURE_ARG_POINTER(aReturn);

  PRUint32 count;

  mArray->Count(&count);
  if (mArray && aIndex < count) {
    nsCOMPtr<nsIDOMNode> node;

    mArray->QueryElementAt(aIndex, NS_GET_IID(nsIDOMNode),
                           getter_AddRefs(node));

    *aReturn = node;
    NS_IF_ADDREF(*aReturn);
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
