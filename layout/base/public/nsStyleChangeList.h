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
#ifndef nsStyleChangeList_h___
#define nsStyleChangeList_h___

#include "nslayout.h"
#include "nsError.h"
class nsIFrame;

// XXX would all platforms support putting this inside the list?
struct nsStyleChangeData {
  nsIFrame* mFrame;
  PRInt32   mHint;
};

static const PRUint32 kStyleChangeBufferSize = 10;

class NS_LAYOUT nsStyleChangeList {
public:
  nsStyleChangeList(void);
  ~nsStyleChangeList(void);

  PRInt32 Count(void) const {
    return mCount;
  }

  nsresult ChangeAt(PRInt32 aIndex, nsIFrame*& aFrame, PRInt32& aHint) const;

  nsresult AppendChange(nsIFrame* aFrame, PRInt32 aHint);

  void Clear();

protected:
  nsStyleChangeList&  operator=(const nsStyleChangeList& aCopy);
  PRBool              operator==(const nsStyleChangeList& aOther) const;

  nsStyleChangeData*  mArray;
  PRInt32             mArraySize;
  PRInt32             mCount;
  nsStyleChangeData   mBuffer[kStyleChangeBufferSize];
};


#endif /* nsStyleChangeList_h___ */
