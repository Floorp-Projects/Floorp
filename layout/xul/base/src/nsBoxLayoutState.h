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
 * The Original Code is Mozilla Communicator client code.
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

/**
 
  Author:
  Eric D Vaughan

**/

#ifndef nsBoxLayoutState_h___
#define nsBoxLayoutState_h___

#include "nsIFrame.h"
#include "nsCOMPtr.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsHTMLReflowState.h"

class nsReflowState;
class nsCalculatedBoxInfo;
struct nsHTMLReflowMetrics;
class nsString;
class nsIBox;
class nsHTMLReflowCommand;

class nsBoxLayoutState
{
public:
  enum eBoxLayoutReason {
    Dirty,
    Resize,
    Initial
  };

  nsBoxLayoutState(nsIPresContext* aPresContext,
                   const nsHTMLReflowState& aReflowState,
                   nsHTMLReflowMetrics& aDesiredSize) NS_HIDDEN;
  nsBoxLayoutState(nsIPresContext* aPresContext) NS_HIDDEN;
  nsBoxLayoutState(nsIPresShell* aShell) NS_HIDDEN;
  nsBoxLayoutState(const nsBoxLayoutState& aState) NS_HIDDEN;

  NS_HIDDEN_(void) HandleReflow(nsIBox* aRootBox);

  nsIPresContext* PresContext() { return mPresContext; }
  nsIPresShell*   PresShell() { return mPresContext->PresShell(); }
  nscoord* GetMaxElementWidth() { return mReflowState ? mMaxElementWidth : nsnull; }

  nsSize ScrolledBlockSizeConstraint() const
  { return mScrolledBlockSizeConstraint; }
  void SetScrolledBlockSizeConstraint(const nsSize& aSize)
  { mScrolledBlockSizeConstraint = aSize; }
  PRUint32 LayoutFlags() const { return mLayoutFlags; }
  void SetLayoutFlags(PRUint32 aFlags) { mLayoutFlags = aFlags; }

  // if true no one under us will paint during reflow.
  void SetPaintingDisabled(PRBool aDisable) { mPaintingDisabled = aDisable; }
  PRBool PaintingDisabled() const { return mPaintingDisabled; }

  eBoxLayoutReason LayoutReason() { return mType; }
  void SetLayoutReason(eBoxLayoutReason aReason) { mType = aReason; }
  const nsHTMLReflowState* GetReflowState() { return mReflowState; }

  static NS_HIDDEN_(void*) Allocate(size_t sz, nsIPresShell* aPresShell);
  static NS_HIDDEN_(void) Free(void* aPtr, size_t sz);
  static NS_HIDDEN_(void) RecycleFreedMemory(nsIPresShell* aPresShell,
                                             void* mem);

  nsresult PushStackMemory() { return PresShell()->PushStackMemory(); }
  nsresult PopStackMemory()  { return PresShell()->PopStackMemory(); }
  nsresult AllocateStackMemory(size_t aSize, void** aResult)
  { return PresShell()->AllocateStackMemory(aSize, aResult); }

private:
  //void DirtyAllChildren(nsBoxLayoutState& aState, nsIBox* aBox);
  NS_HIDDEN_(void) Unwind(nsReflowPath* aReflowPath, nsIBox* aRootBox);
  NS_HIDDEN_(nsIBox*) GetBoxForFrame(nsIFrame* aFrame, PRBool& aIsAdaptor);

  nsCOMPtr<nsIPresContext> mPresContext;
  const nsHTMLReflowState* mReflowState;
  eBoxLayoutReason mType;
  nscoord* mMaxElementWidth;
  nsSize mScrolledBlockSizeConstraint;
  PRUint32 mLayoutFlags;
  PRBool mPaintingDisabled;
};

#endif

