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
#ifndef nsListItemFrame_h___
#define nsListItemFrame_h___

#include "nsBlockFrame.h"

class nsListItemFrame : public nsBlockFrame
{
public:
  static nsresult NewFrame(nsIFrame** aInstancePtrResult,
                           nsIContent* aContent,
                           nsIFrame*   aParent);

  // nsIFrame
  NS_IMETHOD ResizeReflow(nsIPresContext* aCX,
                          nsISpaceManager* aSpaceManager,
                          const nsSize& aMaxSize,
                          nsRect& aDesiredRect,
                          nsSize* aMaxElementSize,
                          ReflowStatus& aStatus);
  NS_IMETHOD CreateContinuingFrame(nsIPresContext* aCX,
                                   nsIFrame* aParent,
                                   nsIFrame*& aContinuingFrame);

  /**
   * Return the reflow state for the list container that contains this
   * list item frame. There may be no list container (a dangling LI)
   * therefore this may return nsnull.
   */
  nsBlockReflowState* GetListContainerReflowState(nsIPresContext* aCX);

protected:
  nsListItemFrame(nsIContent* aContent,
                  nsIFrame*   aParent);

  virtual ~nsListItemFrame();

  nsIFrame* CreateBullet(nsIPresContext* aCX);

  void InsertBullet(nsIFrame* aBullet);

  virtual void PaintChildren(nsIPresContext& aCX,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect& aDirtyRect);

  void PlaceOutsideBullet(nsIFrame* aBullet,
                          nsIPresContext* aCX);
};

#endif /* nsListItemFrame_h___ */
