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
 * The Original Code is Mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla.com.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *         Boris Zbarsky <bzbarsky@mit.edu> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsIRange_h___
#define nsIRange_h___

#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsINode.h"
#include "nsIDOMRange.h"

// IID for the nsIRange interface
#define NS_IRANGE_IID \
{ 0x09dec26b, 0x1ab7, 0x4ff0, \
 { 0xa1, 0x67, 0x7f, 0x22, 0x9c, 0xaa, 0xc3, 0x04 } }

class nsIRange : public nsIDOMRange {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IRANGE_IID)

  nsIRange()
    : mRoot(nsnull),
      mStartOffset(0),
      mEndOffset(0),
      mIsPositioned(PR_FALSE),
      mIsDetached(PR_FALSE),
      mMaySpanAnonymousSubtrees(PR_FALSE)
  {
  }

  nsINode* GetRoot() const
  {
    return mRoot;
  }

  nsINode* GetStartParent() const
  {
    return mStartParent;
  }

  nsINode* GetEndParent() const
  {
    return mEndParent;
  }

  PRInt32 StartOffset() const
  {
    return mStartOffset;
  }

  PRInt32 EndOffset() const
  {
    return mEndOffset;
  }
  
  PRBool IsPositioned() const
  {
    return mIsPositioned;
  }

  PRBool IsDetached() const
  {
    return mIsDetached;
  }
  
  PRBool Collapsed() const
  {
    return mIsPositioned && mStartParent == mEndParent &&
           mStartOffset == mEndOffset;
  }

  void SetMaySpanAnonymousSubtrees(PRBool aMaySpanAnonymousSubtrees)
  {
    mMaySpanAnonymousSubtrees = aMaySpanAnonymousSubtrees;
  }

  virtual nsINode* GetCommonAncestor() const = 0;

  virtual void Reset() = 0;

  // XXXbz we could make these non-virtual if a bunch of nsRange stuff
  // became nsIRange stuff... and if no one outside layout needs them.
  virtual nsresult SetStart(nsINode* aParent, PRInt32 aOffset) = 0;
  virtual nsresult SetEnd(nsINode* aParent, PRInt32 aOffset) = 0;
  virtual nsresult CloneRange(nsIRange** aNewRange) const = 0;

  // Work around hiding warnings
  NS_IMETHOD SetStart(nsIDOMNode* aParent, PRInt32 aOffset) = 0;
  NS_IMETHOD SetEnd(nsIDOMNode* aParent, PRInt32 aOffset) = 0;
  NS_IMETHOD CloneRange(nsIDOMRange** aNewRange) = 0;

protected:
  nsCOMPtr<nsINode> mRoot;
  nsCOMPtr<nsINode> mStartParent;
  nsCOMPtr<nsINode> mEndParent;
  PRInt32 mStartOffset;
  PRInt32 mEndOffset;

  PRPackedBool mIsPositioned;
  PRPackedBool mIsDetached;
  PRPackedBool mMaySpanAnonymousSubtrees;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIRange, NS_IRANGE_IID)

#endif /* nsIRange_h___ */
