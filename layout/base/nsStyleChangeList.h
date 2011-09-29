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

/*
 * a list of the recomputation that needs to be done in response to a
 * style change
 */

#ifndef nsStyleChangeList_h___
#define nsStyleChangeList_h___

#include "nsError.h"
#include "nsChangeHint.h"

class nsIFrame;
class nsIContent;

// XXX would all platforms support putting this inside the list?
struct nsStyleChangeData {
  nsIFrame*   mFrame;
  nsIContent* mContent;
  nsChangeHint mHint;
};

static const PRUint32 kStyleChangeBufferSize = 10;

// Note:  nsStyleChangeList owns a reference to
//  nsIContent pointers in its list.
class nsStyleChangeList {
public:
  nsStyleChangeList();
  ~nsStyleChangeList();

  PRInt32 Count(void) const {
    return mCount;
  }

  /**
   * Fills in pointers without reference counting.  
   */
  nsresult ChangeAt(PRInt32 aIndex, nsIFrame*& aFrame, nsIContent*& aContent,
                    nsChangeHint& aHint) const;

  /**
   * Fills in a pointer to the list entry storage (no reference counting
   * involved).
   */
  nsresult ChangeAt(PRInt32 aIndex, const nsStyleChangeData** aChangeData) const;

  nsresult AppendChange(nsIFrame* aFrame, nsIContent* aContent, nsChangeHint aHint);

  void Clear(void);

protected:
  nsStyleChangeList&  operator=(const nsStyleChangeList& aCopy);
  bool                operator==(const nsStyleChangeList& aOther) const;

  nsStyleChangeData*  mArray;
  PRInt32             mArraySize;
  PRInt32             mCount;
  nsStyleChangeData   mBuffer[kStyleChangeBufferSize];

private:
  nsStyleChangeList(const nsStyleChangeList&); // not implemented
};


#endif /* nsStyleChangeList_h___ */
