/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsIFormManager_h___
#define nsIFormManager_h___

#include "nsISupports.h"
class nsIFormControl;
class nsIPresContext;
class nsIFrame;

// IID for the nsIFormManager interface
#define NS_IFORMMANAGER_IID    \
{ 0x592daa01, 0xcb23, 0x11d1,  \
  { 0x80, 0x2d, 0x0, 0x60, 0x8, 0x15, 0xa7, 0x91 } }

/** 
  * Abstract form manager interface. Form managers are responsible for
  * the management of form controls. This includes gathering of data
  * for form submission, resetting form controls to their initial state,
  * and other programmatic control during form processing.
 **/
class nsIFormManager : public nsISupports {
public:
  // Event methods

  /**
    * Respond to a radio button being checked. This form manager is responsible
    * for radio group coordination.
    * @param aRadio the radio button that was checked
    */
  virtual void OnRadioChecked(nsIFormControl& aRadio) = 0;

  /**
    * Reset the values of all of this manager's controls back to their
    * initial values. This is in response to a reset button being pushed.
    */
  virtual void OnReset() = 0;

  /** 
    * If there is a single text form control, this is identical to OnReset,
    * otherwise do nothing. This in response to a carriage return being
    * entered within a text control.
    */
  virtual void OnReturn() = 0;

  /**
    * Submit the values of this manager's controls depending on its action,
    * method attributes. This in response to a submit button being clicked.
    * @param aPresContext the presentation context
    * @param aFrame the frame of the submit button 
    */
  virtual void OnSubmit(nsIPresContext* aPresContext, nsIFrame* aFrame) = 0;

  /**
    * This is tbd and is in repsonse to a tab key being entered in one
    * of the controls.
    */
  virtual void OnTab() = 0;

  // methods accessing controls

  /**
    * Add a control to end of this manager's list of controls
    * @param aFormControl the control to add
    * @return PR_TRUE if the control was successfully added 
    */
  virtual PRBool AddFormControl(nsIFormControl* aFormControl) = 0;

  /**
    * Get the number of controls that this manager has in its list
    * @return the number of controls
    */
  virtual PRInt32 GetFormControlCount() const = 0;

  /**
    * Get the control at a specific position in this manager's list
    * @param aIndex the position at which to get
    * @return the control at that position
    */
  virtual nsIFormControl* GetFormControlAt(PRInt32 aIndex) const = 0;

  /**
    * Remove a control from this manager's list
    * @param aFormControl thc control to remove
    * @param aChildIsRef if PR_TRUE, the controls ref count will be decremented
    * otherwise not. This is to facilitate circular references.
    * @return PR_TRUE if the control was successfully removed.
    */
  virtual PRBool RemoveFormControl(nsIFormControl* aFormControl, 
                                   PRBool aChildIsRef = PR_TRUE) = 0;

  // methods accessing attributes

  /**
    * Get the named attribute of this manager
    * @param aName the name of the attribute
    * @param aResult the value of the attribute 
    * @return PR_TRUE if there is an attribute with name aName
    */
  virtual PRBool GetAttribute(const nsString& aName,
                              nsString& aResult) const = 0;
  /**
    * Set the named attribute of this manager
    * @param aName the name of the attribute
    * @param aValue the value of the attribute
    */
  virtual void SetAttribute(const nsString& aName, const nsString& aValue) = 0;

  // misc methods

  /**
    * Initialize this form manager after all of the controls have been added
    * This makes it possible to determine what the radio groups are.
    * @param aReinit if PR_TRUE initialize even if Init has been called before.
    */
  virtual void Init(PRBool aReinit) = 0;

  /**
    * Get the number of references to this form manager by other objects.
    * @return the ref count
    */
  virtual nsrefcnt GetRefCount() const = 0;
};

#endif /* nsIFormManager_h___ */
