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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsIForm_h___
#define nsIForm_h___

#include "nsISupports.h"
#include "nsString.h"
class nsIFormControl;
class nsISizeOfHandler;

#define NS_FORM_METHOD_GET  0
#define NS_FORM_METHOD_POST 1
#define NS_FORM_ENCTYPE_URLENCODED 0
#define NS_FORM_ENCTYPE_MULTIPART  1

// IID for the nsIFormManager interface
#define NS_IFORM_IID    \
{ 0xb7e94510, 0x4c19, 0x11d2,  \
  { 0x80, 0x3f, 0x0, 0x60, 0x8, 0x15, 0xa7, 0x91 } }

/**
 * This interface provides a complete set of methods dealing with
 * elements which belong to a form element. When nsIDOMHTMLCollection
 * allows write operations
 */
class nsIForm : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IFORM_IID)

  /**
    * Add an element to end of this form's list of elements
    * @param aElement the element to add
    * @return NS_OK if the element was successfully added 
    */
  NS_IMETHOD AddElement(nsIFormControl* aElement) = 0;

  /**    
    * Add an element to the lookup table mainted by the form.
    * We can't fold this method into AddElement() because when
    * AddElement() is called, the form control has no
    * attributes.  The name or id attributes of the form control
    * are used as a key into the table.
    */
  NS_IMETHOD AddElementToTable(nsIFormControl* aElement, const nsAReadableString& aName) = 0;

  /**
    * Get the element at a specified index position
    * @param aIndex the index
    * @param aElement the element at that index
    * @return NS_OK if there was an element at that position, -1 otherwise 
    */
  NS_IMETHOD GetElementAt(PRInt32 aIndex, nsIFormControl** aElement) const = 0;

  /**
    * Get the number of elements in this form
    * @param aCount the number of elements
    * @return NS_OK if there was an element at that position, -1 otherwise 
    */
  NS_IMETHOD GetElementCount(PRUint32* aCount) const = 0;

  /**
    * Remove an element from this form's list of elements
    * @param aElement the element to remove
    * @return NS_OK if the element was successfully removed.
    */
  NS_IMETHOD RemoveElement(nsIFormControl* aElement) = 0;

  /**    
    * Remove an element from the lookup table mainted by the form.
    * We can't fold this method into RemoveElement() because when
    * RemoveElement() is called it doesn't know if the element is
    * removed because the id attribute has changed, or bacause the
    * name attribute has changed.
    *
    * @param aElement the element to remove
    * @param aName the name or id of the element to remove
    * @return NS_OK if the element was successfully removed.
    */
  NS_IMETHOD RemoveElementFromTable(nsIFormControl* aElement, const nsAReadableString& aName) = 0;

  NS_IMETHOD  SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const = 0;
};

#endif /* nsIForm_h___ */
