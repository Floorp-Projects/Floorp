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
#ifndef nsISelection_h___
#define nsISelection_h___

#include "nsSelectionRange.h"
#include "nslayout.h"
#include "nsISupports.h"

// IID for the nsISelection interface
#define NS_ISELECTION_IID      \
{ 0xf46e4171, 0xdeaa, 0x11d1, \
  { 0x97, 0xfc, 0x0, 0x60, 0x97, 0x3, 0xc1, 0x4e } }

//----------------------------------------------------------------------

// Selection interface
class nsISelection : public nsISupports {
public:

  /**
    * Returns whether there is a valid selection
   */ 
  virtual PRBool IsValidSelection() = 0;

  /**
    * Clears the current selection (invalidates the selection)
   */ 
  virtual void ClearSelection() = 0;

  /**
    * Copies the data from the param into the internal Range
   */ 
  virtual void SetRange(nsSelectionRange * aRange) = 0;

  /**
    * Copies the param's (aRange) contents with the Range's data
   */ 
  virtual void GetRange(nsSelectionRange * aRange) = 0;

  /**
    * Copies the param's (aRange) contents with the Range's data
   */ 
  virtual nsSelectionRange * GetRange() = 0;

  virtual char * ToString() = 0;

};

// XXX Belongs somewhere else
extern NS_LAYOUT nsresult
   NS_NewSelection(nsISelection** aInstancePtrResult);

#endif /* nsISelection_h___ */
