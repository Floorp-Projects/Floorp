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
#ifndef nsIForm_h___
#define nsIForm_h___

#include "nsISupports.h"
#include "nsAString.h"

class nsIFormControl;
class nsIDOMHTMLInputElement;
class nsIRadioVisitor;
class nsISimpleEnumerator;
class nsIURI;

#define NS_FORM_METHOD_GET  0
#define NS_FORM_METHOD_POST 1
#define NS_FORM_ENCTYPE_URLENCODED 0
#define NS_FORM_ENCTYPE_MULTIPART  1
#define NS_FORM_ENCTYPE_TEXTPLAIN  2


// IID for the nsIFormManager interface
#define NS_IFORM_IID    \
{ 0xb7e94510, 0x4c19, 0x11d2,  \
  { 0x80, 0x3f, 0x0, 0x60, 0x8, 0x15, 0xa7, 0x91 } }


/**
 * This interface provides a complete set of methods dealing with
 * elements which belong to a form element. When nsIDOMHTMLCollection
 * allows write operations
 */

class nsIForm : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IFORM_IID)

  /**
   * Add an element to end of this form's list of elements
   *
   * @param aElement the element to add
   * @return NS_OK if the element was successfully added
   */
  NS_IMETHOD AddElement(nsIFormControl* aElement) = 0;

  /**    
   * Add an element to the lookup table mainted by the form.
   *
   * We can't fold this method into AddElement() because when
   * AddElement() is called, the form control has no
   * attributes.  The name or id attributes of the form control
   * are used as a key into the table.
   */
  NS_IMETHOD AddElementToTable(nsIFormControl* aElement,
                               const nsAString& aName) = 0;

  /**
   * Get the element at a specified index position in form.elements
   *
   * @param aIndex the index
   * @param aElement the element at that index
   * @return NS_OK if there was an element at that position, -1 otherwise
   */
  NS_IMETHOD GetElementAt(PRInt32 aIndex, nsIFormControl** aElement) const = 0;

  /**
   * Get the number of elements in form.elements
   *
   * @param aCount the number of elements
   * @return NS_OK if there was an element at that position, -1 otherwise
   */
  NS_IMETHOD GetElementCount(PRUint32* aCount) const = 0;

  /**
   * Remove an element from this form's list of elements
   *
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
  NS_IMETHOD RemoveElementFromTable(nsIFormControl* aElement,
                                    const nsAString& aName) = 0;

  /**
   * Resolve a name in the scope of the form object, this means find
   * form controls in this form with the correct value in the name
   * attribute.
   *
   * @param aElement the element to remove
   * @param aName the name or id of the element to remove
   * @return NS_OK if the element was successfully removed.
   */
  NS_IMETHOD ResolveName(const nsAString& aName,
                         nsISupports **aResult) = 0;

  /**
   * Get the index of the given control within form.elements.
   * @param aControl the control to find the index of
   * @param aIndex the index [OUT]
   */
  NS_IMETHOD IndexOfControl(nsIFormControl* aControl, PRInt32* aIndex) = 0;

  /**
   * Get an enumeration that goes through all controls, including images and
   * that ilk
   * @param aEnum the enumeration [OUT]
   */
  NS_IMETHOD GetControlEnumerator(nsISimpleEnumerator** aEnum) = 0;

  /**
   * Flag the form to know that a button or image triggered scripted form
   * submission. In that case the form will defer the submission until the
   * script handler returns and the return value is known.
   */ 
  NS_IMETHOD OnSubmitClickBegin() = 0;
  NS_IMETHOD OnSubmitClickEnd() = 0;

  /**
   * Flush a possible pending submission. If there was a scripted submission
   * triggered by a button or image, the submission was defered. This method
   * forces the pending submission to be submitted. (happens when the handler
   * returns false or there is an action/target change in the script)
   */
  NS_IMETHOD FlushPendingSubmission() = 0;
  /**
   * Forget a possible pending submission. Same as above but this time we
   * get rid of the pending submission cause the handler returned true
   * so we will rebuild the submission with the name/value of the triggering
   * element
   */
  NS_IMETHOD ForgetPendingSubmission() = 0;

  /**
   * Get the full URL to submit to.  Do not submit if the returned URL is null.
   *
   * @param aActionURL the full, unadulterated URL you'll be submitting to [OUT]
   */
  NS_IMETHOD GetActionURL(nsIURI** aActionURL) = 0;

};

#endif /* nsIForm_h___ */
