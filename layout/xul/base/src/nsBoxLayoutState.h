/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

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

class nsReflowState;
class nsCalculatedBoxInfo;
struct nsHTMLReflowMetrics;
class nsString;
class nsIBox;
class nsIReflowCommand;

class nsBoxLayoutState
{
public:
  enum eBoxLayoutReason {
    Dirty,
    Resize,
    Initial
  };

  nsBoxLayoutState(nsIPresContext* aPresContext, const nsHTMLReflowState& aReflowState, nsHTMLReflowMetrics& aDesiredSize);
  nsBoxLayoutState(nsIPresContext* aPresContext);
  nsBoxLayoutState(nsIPresShell* aShell);
  nsBoxLayoutState(const nsBoxLayoutState& aState);
  virtual ~nsBoxLayoutState() {}

  virtual void HandleReflow(nsIBox* aRootBox);

  virtual nsIPresContext* GetPresContext() { return mPresContext.get(); }
  virtual nsresult GetPresShell(nsIPresShell** aShell);
  virtual void GetMaxElementSize(nsSize** aMaxElementSize);

  virtual void GetOverFlowSize(nsSize& aSize);
  virtual void SetOverFlowSize(const nsSize& aSize);
  virtual void GetIncludeOverFlow(PRBool& aOverFlow);
  virtual void SetIncludeOverFlow(const PRBool& aOverFlow);
  virtual void GetLayoutFlags(PRUint32& aFlags);
  virtual void SetLayoutFlags(const PRUint32& aFlags);

  // if true no one under us will paint during reflow.
  virtual void SetDisablePainting(PRBool aDisable) { mDisablePainting = aDisable; }
  virtual PRBool GetDisablePainting() { return mDisablePainting; }

  virtual eBoxLayoutReason GetLayoutReason() { return mType; }
  virtual void SetLayoutReason(eBoxLayoutReason aReason) { mType = aReason; }
  virtual const nsHTMLReflowState* GetReflowState() { return mReflowState; }

  static void* Allocate(size_t sz, nsIPresShell* aPresShell);
  static void Free(void* aPtr, size_t sz);
  static void RecycleFreedMemory(nsIPresShell* aPresShell, void* mem);

  nsresult PushStackMemory();
  nsresult PopStackMemory();
  nsresult AllocateStackMemory(size_t aSize, void** aResult);

private:
  //void DirtyAllChildren(nsBoxLayoutState& aState, nsIBox* aBox);
  void UnWind(nsIReflowCommand* aCommand, nsIBox* aRootBox);
  nsIBox* GetTargetBox(nsIReflowCommand* mCommand, PRBool& aIsAdaptor);
  nsIBox* GetBoxForFrame(nsIFrame* aFrame, PRBool& aIsAdaptor);

  nsCOMPtr<nsIPresContext> mPresContext;
  const nsHTMLReflowState* mReflowState;
  eBoxLayoutReason mType;
  nsSize* mMaxElementSize;
  nsSize mOverFlowSize;
  PRBool mIncludeOverFlow;
  PRUint32 mLayoutFlags;
  PRBool mDisablePainting;
};

#endif

