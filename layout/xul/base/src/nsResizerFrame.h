/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsResizerFrame_h___
#define nsResizerFrame_h___

#include "mozilla/Attributes.h"
#include "nsTitleBarFrame.h"

class nsIBaseWindow;
class nsMenuPopupFrame;

class nsResizerFrame : public nsTitleBarFrame 
{
protected:
  struct Direction {
    int8_t mHorizontal;
    int8_t mVertical;
  };

public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewResizerFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);  

  nsResizerFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  NS_IMETHOD HandleEvent(nsPresContext* aPresContext,
                         mozilla::WidgetGUIEvent* aEvent,
                         nsEventStatus* aEventStatus) MOZ_OVERRIDE;

  virtual void MouseClicked(nsPresContext* aPresContext,
                            mozilla::WidgetGUIEvent* aEvent) MOZ_OVERRIDE;

protected:
  nsIContent* GetContentToResize(nsIPresShell* aPresShell, nsIBaseWindow** aWindow);

  Direction GetDirection();

  /**
   * Adjust the window position and size in a direction according to the mouse
   * movement and the resizer direction. The minimum and maximum size is used
   * to constrain the size.
   *
   * @param aPos left or top position
   * @param aSize width or height
   * @param aMinSize minimum width or height
   * @param aMacSize maximum width or height
   * @param aMovement the amount the mouse was moved
   * @param aResizerDirection resizer direction returned by GetDirection
   */
  static void AdjustDimensions(int32_t* aPos, int32_t* aSize,
                               int32_t aMinSize, int32_t aMaxSize,
                               int32_t aMovement, int8_t aResizerDirection);

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
