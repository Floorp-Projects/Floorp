/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Vidur Apparao <vidur@netscape.com> (original author)
 */

#ifndef _nsDOMUtils_h__
#define _nsDOMUtils_h__

// content includes
#include "nsIContent.h"
#include "nsINodeInfo.h"
#include "nsIDOMElement.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"

// string includes
#include "nsString.h"
#include "nsReadableUtils.h"

// collection includes
#include "nsVoidArray.h"

////////////////////////////////////////////////////////////
//
// nsChildElementIterator 
//
////////////////////////////////////////////////////////////

class nsChildElementIterator {
private:
  nsCOMPtr<nsIDOMNodeList> mNodeList;
  PRUint32 mIndex;
  PRUint32 mLength;
  nsString mNamespace;
  const char** mNamespaceArray;
  PRUint32 mNumNamespaces;

public:
  nsChildElementIterator(nsIDOMElement* aParent) :
    mIndex(0), mLength(0), mNumNamespaces(0)
  {
    SetElement(aParent);
  }    
  
  nsChildElementIterator(nsIDOMElement* aParent,
                         const nsAString& aNamespace) :
    mIndex(0), mLength(0), mNamespace(aNamespace), mNumNamespaces(0)
  {
    SetElement(aParent);
  }

  nsChildElementIterator(nsIDOMElement* aParent,
                         const char** aNamespaceArray,
                         PRUint32 aNumNamespaces) :
    mIndex(0), mLength(0), mNamespaceArray(aNamespaceArray), 
    mNumNamespaces(aNumNamespaces)
  {
    SetElement(aParent);    
  }

  ~nsChildElementIterator() {}

  void SetElement(nsIDOMElement* aParent) 
  {
    aParent->GetChildNodes(getter_AddRefs(mNodeList));
    if (mNodeList) {
      mNodeList->GetLength(&mLength);
    }    
  }

  PRUint32 GetCurrentIndex() { return mIndex; }

  void Reset(PRUint32 aIndex=0) { mIndex = aIndex; }

  PRBool HasChildNodes() { return mLength; }

  nsresult GetNextChild(nsIDOMElement** aChildElement,
                        nsIAtom** aElementName)
  {
    *aChildElement = nsnull;
    
    if (!mNodeList) {
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIDOMNode> child; 
    while (mIndex < mLength) {
      mNodeList->Item(mIndex++, getter_AddRefs(child));
      nsCOMPtr<nsIDOMElement> childElement(do_QueryInterface(child));
      if (!childElement) {
        continue;
      }
      
      // Confirm that the element is an element of the specified namespace
      nsAutoString namespaceURI;           
      childElement->GetNamespaceURI(namespaceURI);  
      if (!mNamespace.IsEmpty()) {
        if (!namespaceURI.Equals(mNamespace)) {
          continue;
        }
      }
      else if (mNumNamespaces) {
        PRUint32 i;
        for (i = 0; i < mNumNamespaces; i++) {
          if (!namespaceURI.Equals(NS_ConvertASCIItoUCS2(mNamespaceArray[i]))) {
            continue;
          }
        }
      }
      
      nsCOMPtr<nsIContent> content(do_QueryInterface(childElement));
      NS_ASSERTION(content, "Element is not content");
      if (!content) {
        return NS_ERROR_FAILURE;
      }
      
      nsINodeInfo *nodeInfo = content->GetNodeInfo();
      if (!nodeInfo) {
        return NS_ERROR_FAILURE;
      }

      NS_ADDREF(*aElementName = nodeInfo->NameAtom());

      *aChildElement = childElement;
      NS_ADDREF(*aChildElement);
      break;
    }
    
    return NS_OK;
  }
};

#endif // _nsDOMUtils_h__
