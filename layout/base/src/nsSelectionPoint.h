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
#ifndef nsSelectionPoint_h___
#define nsSelectionPoint_h___

#include "nsIContent.h"

class nsSelectionPoint {

  nsIContent * fContent;
  PRInt32      fOffset;
  PRBool       fIsAnchor;
  PRBool       fEntireContent;

  public:

    nsSelectionPoint(nsIContent * aContent,
                     PRInt32      aOffset,
                     PRBool       aIsAnchor);

    virtual ~nsSelectionPoint();

    nsIContent * GetContent() { return fContent; }
    PRInt32      GetOffset()  { return fOffset;  }
    PRBool       IsAnchor()   { return fIsAnchor;}

    PRBool       IsEntireContentSelected()                 { return fEntireContent;   }
    void         setEntireContentSelected(PRBool aState)   { fEntireContent = aState; }

    void         SetContent(nsIContent * aValue)   { fContent  = aValue; }
    void         SetOffset(PRInt32 aValue)         { fOffset   = aValue; }
    void         SetAnchor(PRBool aValue)          { fIsAnchor = aValue; }

    void         SetPoint(nsIContent * aContent,
                          PRInt32      aOffset,
                          PRBool       aIsAnchor);
    char * ToString();

};
#endif /* nsSelectionPoint_h___ */
