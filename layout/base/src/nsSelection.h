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
#ifndef nsSelection_h___
#define nsSelection_h___

#include "nsSelectionRange.h"
//#include "nsISupports.h"
#include "nsISelection.h"

class nsSelection : public nsISelection {

  nsSelectionRange * mRange;

  public:
    NS_DECL_ISUPPORTS

    nsSelection();

    /** supports implementation */
    //NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtrResult);

    /**
      * Returns whether there is a valid selection
     */ 
    PRBool IsValidSelection();

    /**
      * Clears the current selection (invalidates the selection)
     */ 
    void ClearSelection();

    /**
      * Copies the data from the param into the internal Range
     */ 
    void SetRange(nsSelectionRange * aRange);

    /**
      * Copies the param's (aRange) contents with the Range's data
     */ 
    void GetRange(nsSelectionRange * aRange);

    /**
      * Copies the param's (aRange) contents with the Range's data
     */ 
    nsSelectionRange * GetRange() { return mRange; }

    char * ToString();
};

#endif /* nsSelection_h___ */
