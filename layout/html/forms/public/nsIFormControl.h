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
#ifndef nsIFormControl_h___
#define nsIFormControl_h___

#include "nsISupports.h"
class nsIFormManager;

#define NS_IFORMCONTROL_IID   \
{ 0x282ff440, 0xcd7e, 0x11d1, \
  {0x89, 0xad, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81} }

/**
  * Interface which all form controls (e.g. buttons, checkboxes, text,
  * radio buttons, select, etc) implement. 
 **/
class nsIFormControl : public nsISupports {
public:
  /**
    * Get the form manager which manages this control.
    * @return the form manager
    */
  virtual nsIFormManager* GetFormManager() const = 0;

  /**
    * Get the max number of values that this control can have; the actual
    * number of values may be less.
    * @return the max number of values
    */
  virtual PRInt32 GetMaxNumValues() = 0;

  /**
    * Get the name of this control. Controls without names will not have any
    * data submitted.
    * @param aResult the nsString which will be set to the name (out parm)
    * @return PR_TRUE if there was a name, PR_FALSE otherwise 
    */
  virtual PRBool GetName(nsString& aResult) = 0;

  /**
    * Get the number of references to this control by other objects.
    * @return the ref count
    */
  virtual nsrefcnt GetRefCount() const = 0;

  /**
    * Get the vaules which this control could submit.
    * @param aMaxNumValues the maximum number of values to set (in parm)
    * @param aNumValues the actual number of values set (out parm)
    * @param aValues an array of nsString which contains the values (out parm
    * that is allocated by the caller)
    * @return PR_TRUE if any values were set, PR_FALSE otherwise
    */
  virtual PRBool GetValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                           nsString* aValues) = 0;

  /**
    * Set this control back to its initial value
    */
  virtual void Reset() = 0;

  /**
    * Set the form manager for this control
    * @param aFormMan the new form manager
    * @param aDecrementRef if PR_TRUE, decrement the ref count to the existing
    * form manager. This facilitates handling circular references.
    */
  virtual void SetFormManager(nsIFormManager* aFormMan, PRBool aDecrementRef = PR_TRUE) = 0;

};

#endif /* nsIFormControl_h___ */
