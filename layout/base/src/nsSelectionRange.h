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
#ifndef nsSelectionRange_h___
#define nsSelectionRange_h___

#include "nsSelectionPoint.h"

class nsSelectionRange {

  nsSelectionPoint * mStart;
  nsSelectionPoint * mEnd;

  public:
    nsSelectionRange();

    nsSelectionRange(nsIContent * aStartContent,
                PRInt32      aStartOffset,
                PRBool       aStartIsAnchor,
                nsIContent * aEndContent,
                PRInt32      aEndOffset,
                PRBool       aEndIsAnchor);

    void SetRange(nsIContent * aStartContent,
                  PRInt32      aStartOffset,
                  PRBool       aStartIsAnchor,
                  nsIContent * aEndContent,
                  PRInt32      aEndOffset,
                  PRBool       aEndIsAnchor);

    void SetRange(nsSelectionPoint * aStartPoint,
                  nsSelectionPoint * aEndPoint);

    /**
      * Copies the data from the param into the internal Range
     */ 
    void SetRange(nsSelectionRange * aRange);

    nsIContent       * GetStartContent()   { return mStart->GetContent();}
    nsIContent       * GetEndContent()     { return mEnd->GetContent();  }
    nsSelectionPoint * GetStartPoint()     { return mStart;              }
    nsSelectionPoint * GetEndPoint()       { return mEnd;                }

    /**
      * Copies the data from the param into the Start Point
     */ 
    void SetStartPoint(nsSelectionPoint * aPoint);

    /**
      * Copies the data from the param into the End Point
     */ 
    void SetEndPoint(nsSelectionPoint * aPoint);


    /**
      * Copies the param's (aPoint) contents with the Range's Point data
     */ 
    void GetStartPoint(nsSelectionPoint * aPoint);

    /**
      * Copies the param's (aPoint) contents with the Range's Point data
     */ 
    void GetEndPoint(nsSelectionPoint * aPoint);

    /**
      * Copies the param's (aRange) contents with the Range's data
     */ 
    void GetRange(nsSelectionRange * aRange);

    char * ToString();
};

#endif /* nsSelectionRange_h___ */
