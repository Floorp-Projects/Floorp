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
#ifndef nsIFormControl_h___
#define nsIFormControl_h___

#include "nsISupports.h"
class nsIDOMHTMLFormElement;
class nsPresState;
class nsIContent;
class nsString;
class nsIFormProcessor;
class nsFormSubmission;

namespace mozilla {
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla

enum FormControlsTypes {
  NS_FORM_FIELDSET = 1,
  NS_FORM_LABEL,
  NS_FORM_OUTPUT,
  NS_FORM_SELECT,
  NS_FORM_TEXTAREA,
  NS_FORM_OBJECT,
  eFormControlsWithoutSubTypesMax,
  // After this, all types will have sub-types which introduce new enum lists.
  // eFormControlsWithoutSubTypesMax let us know if the previous types values
  // are not overlapping with sub-types/masks.

  // Elements with different types, the value is used as a mask.
  // Adding '_ELEMENT' because NS_FORM_INPUT is used for 'oninput' event.
  // When changing the order, adding or removing elements, be sure to update
  // the PR_STATIC_ASSERT checks accordingly.
  NS_FORM_BUTTON_ELEMENT = 0x40, // 0b01000000
  NS_FORM_INPUT_ELEMENT  = 0x80  // 0b10000000
};

enum ButtonElementTypes {
  NS_FORM_BUTTON_BUTTON = NS_FORM_BUTTON_ELEMENT + 1,
  NS_FORM_BUTTON_RESET,
  NS_FORM_BUTTON_SUBMIT,
  eButtonElementTypesMax
};

enum InputElementTypes {
  NS_FORM_INPUT_BUTTON = NS_FORM_INPUT_ELEMENT + 1,
  NS_FORM_INPUT_CHECKBOX,
  NS_FORM_INPUT_EMAIL,
  NS_FORM_INPUT_FILE,
  NS_FORM_INPUT_HIDDEN,
  NS_FORM_INPUT_RESET,
  NS_FORM_INPUT_IMAGE,
  NS_FORM_INPUT_PASSWORD,
  NS_FORM_INPUT_RADIO,
  NS_FORM_INPUT_SEARCH,
  NS_FORM_INPUT_SUBMIT,
  NS_FORM_INPUT_TEL,
  NS_FORM_INPUT_TEXT,
  eInputElementTypesMax
};

PR_STATIC_ASSERT((PRUint32)eFormControlsWithoutSubTypesMax < (PRUint32)NS_FORM_BUTTON_ELEMENT);
PR_STATIC_ASSERT((PRUint32)eButtonElementTypesMax < (PRUint32)NS_FORM_INPUT_ELEMENT);
PR_STATIC_ASSERT((PRUint32)eInputElementTypesMax  < 1<<8);

#define NS_IFORMCONTROL_IID   \
{ 0x0dc5083b, 0xb0a8, 0x48c4, \
 { 0xb2, 0xeb, 0xc2, 0x4f, 0xfb, 0x7e, 0xc2, 0x8e } }

/**
 * Interface which all form controls (e.g. buttons, checkboxes, text,
 * radio buttons, select, etc) implement in addition to their dom specific
 * interface.
 */
class nsIFormControl : public nsISupports
{
public:

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IFORMCONTROL_IID)

  /**
   * Get the form for this form control.
   * @return the form
   */
  virtual mozilla::dom::Element *GetFormElement() = 0;

  /**
   * Set the form for this form control.
   * @param aForm the form.  This must not be null.
   *
   * @note that when setting the form the control is not added to the
   * form.  It adds itself when it gets bound to the tree thereafter,
   * so that it can be properly sorted with the other controls in the
   * form.
   */
  virtual void SetForm(nsIDOMHTMLFormElement* aForm) = 0;

  /**
   * Tell the control to forget about its form.
   *
   * @param aRemoveFromForm set false if you do not want this element removed
   *        from the form.  (Used by nsFormControlList::Clear())
   * @param aNotify If true, send nsIDocumentObserver notifications as needed.
   */
  virtual void ClearForm(PRBool aRemoveFromForm, PRBool aNotify) = 0;

  /**
   * Get the type of this control as an int (see NS_FORM_* above)
   * @return the type of this control
   */
  NS_IMETHOD_(PRUint32) GetType() const = 0 ;

  /**
   * Reset this form control (as it should be when the user clicks the Reset
   * button)
   */
  NS_IMETHOD Reset() = 0;

  /**
   * Tells the form control to submit its names and values to the form
   * submission object
   * @param aFormSubmission the form submission to notify of names/values/files
   *                       to submit
   * @param aSubmitElement the element that was pressed to submit (possibly
   *                       null)
   */
  NS_IMETHOD SubmitNamesValues(nsFormSubmission* aFormSubmission,
                               nsIContent* aSubmitElement) = 0;

  /**
   * Save to presentation state.  The form control will determine whether it
   * has anything to save and if so, create an entry in the layout history for
   * its pres context.
   */
  NS_IMETHOD SaveState() = 0;

  /**
   * Restore from presentation state.  You pass in the presentation state for
   * this form control (generated with GenerateStateKey() + "-C") and the form
   * control will grab its state from there.
   *
   * @param aState the pres state to use to restore the control
   * @return PR_TRUE if the form control was a checkbox and its
   *         checked state was restored, PR_FALSE otherwise.
   */
  virtual PRBool RestoreState(nsPresState* aState) = 0;

  virtual PRBool AllowDrop() = 0;

  /**
   * Returns true if this is a control which submits the form when
   * activated by the user.
   * @return Whether this is a submit control.
   */
  virtual PRBool IsSubmitControl() const = 0;

  /**
   * Returns true if this is a control which has a text field.
   * @param  aExcludePassword  to have NS_FORM_INPUT_PASSWORD returning false.
   * @return Whether this is a text control.
   */
  virtual PRBool IsTextControl(PRBool aExcludePassword) const = 0;

  /**
   * Returns true if this is a control which has a single line text field.
   * @param  aExcludePassword  to have NS_FORM_INPUT_PASSWORD returning false.
   * @return Whether this is a single line text control.
   */
  virtual PRBool IsSingleLineTextControl(PRBool aExcludePassword) const = 0;

  /**
   * Returns true if this is a labelable form control.
   * @return Whether this is a labelable form control.
   */
  virtual PRBool IsLabelableControl() const = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIFormControl, NS_IFORMCONTROL_IID)

#endif /* nsIFormControl_h___ */
