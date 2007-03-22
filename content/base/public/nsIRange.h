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

// IID for the nsIRange interface
#define NS_IRANGE_IID \
{ 0x267c8c4e, 0x7c97, 0x4a35, \
  { 0xaa, 0x08, 0x55, 0xa5, 0xbe, 0x3a, 0xc5, 0x74 } }

class nsIRange : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IRANGE_IID)

  nsIRange()
    : mRoot(nsnull),
      mStartOffset(0),
      mEndOffset(0),
      mIsPositioned(PR_FALSE),
      mIsDetached(PR_FALSE)
  {
  }

  nsINode* GetRoot()
  {
    return mRoot;
  }

  nsINode* GetStartParent()
  {
    return mStartParent;
  }

  nsINode* GetEndParent()
  {
    return mEndParent;
  }

  PRInt32 StartOffset()
  {
    return mStartOffset;
  }

  PRInt32 EndOffset()
  {
    return mEndOffset;
  }
  
  PRBool IsPositioned()
  {
    return mIsPositioned;
  }

  PRBool IsDetached()
  {
    return mIsDetached;
  }
  
  PRBool Collapsed()
  {
    return mIsPositioned && mStartParent == mEndParent &&
           mStartOffset == mEndOffset;
  }

  virtual nsINode* GetCommonAncestor() = 0;

  virtual void Reset() = 0;

protected:
  nsINode* mRoot;
  nsCOMPtr<nsINode> mStartParent;
  nsCOMPtr<nsINode> mEndParent;
  PRInt32 mStartOffset;
  PRInt32 mEndOffset;

  PRPackedBool mIsPositioned;
  PRPackedBool mIsDetached;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIRange, NS_IRANGE_IID)

#endif /* nsIRange_h___ */
