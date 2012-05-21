/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsResizerFrame_h___
#define nsResizerFrame_h___

#include "nsTitleBarFrame.h"

class nsIBaseWindow;
class nsMenuPopupFrame;

class nsResizerFrame : public nsTitleBarFrame 
{
protected:
  struct Direction {
    PRInt8 mHorizontal;
    PRInt8 mVertical;
  };

public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewResizerFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);  

  nsResizerFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  NS_IMETHOD HandleEvent(nsPresContext* aPresContext, 
                                      nsGUIEvent* aEvent,
                                      nsEventStatus* aEventStatus);

  virtual void MouseClicked(nsPresContext* aPresContext, nsGUIEvent *aEvent);

protected:
  nsIContent* GetContentToResize(nsIPresShell* aPresShell, nsIBaseWindow** aWindow);

  Direction GetDirection();
  static void AdjustDimensions(PRInt32* aPos, PRInt32* aSize,
                        PRInt32 aMovement, PRInt8 aResizerDirection);

  struct SizeInfo {
    nsString width, height;
  };
  static void SizeInfoDtorFunc(void *aObject, nsIAtom *aPropertyName,
                               void *aPropertyValue, void *aData);
  static void ResizeContent(nsIContent* aContent, const Direction& aDirection,
                            const SizeInfo& aSizeInfo, SizeInfo* aOriginalSizeInfo);
  static void MaybePersistOriginalSize(nsIContent* aContent, const SizeInfo& aSizeInfo);
  static void RestoreOriginalSize(nsIContent* aContent);
protected:
	nsIntRect mMouseDownRect;
	nsIntPoint mMouseDownPoint;
}; // class nsResizerFrame

#endif /* nsResizerFrame_h___ */
