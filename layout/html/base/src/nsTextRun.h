/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#ifndef nsTextRun_h___
#define nsTextRun_h___

#include "nslayout.h"
#include "nsIFrame.h"
#include "nsVoidArray.h"

class nsIContent;
class nsTextFragment;

// XXX it might be cheaper to have the text-run only contain the
// content pointer instead of the frame pointer

// A run of text. In mText are the nsIFrame's that are considered text
// frames.
class nsTextRun {
public:
  nsTextRun();
  ~nsTextRun();

  nsIFrame* FindNextText(nsIFrame* aFrame);

  nsTextRun* GetNext() const {
    return mNext;
  }

  void SetNext(nsTextRun* aNext) {
    mNext = aNext;
  }

  PRInt32 Count() const {
    return mArray.Count();
  }

  nsIFrame* operator[](PRInt32 aIndex) const {
    return (nsIFrame*) mArray[aIndex];
  }

  void List(FILE* out, PRInt32 aIndent);

  static void DeleteTextRuns(nsTextRun* aRun) {
    while (nsnull != aRun) {
      nsTextRun* next = aRun->GetNext();
      delete aRun;
      aRun = next;
    }
  }

protected:
  nsVoidArray mArray;
  nsTextRun* mNext;

  // XXX get rid of this
  friend class nsLineLayout;
};

#endif /* nsTextRun_h___ */
