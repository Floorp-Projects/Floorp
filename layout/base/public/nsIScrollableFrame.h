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

#ifndef nsIScrollFrame_h___
#define nsIScrollFrame_h___

#include "nsISupports.h"
#include "nsCoord.h"
#include "nsIViewManager.h"
#include "nsIScrollableViewProvider.h"
#include "nsPresContext.h"

class nsIFrame;
class nsIBox;
class nsBoxLayoutState;

// IID for the nsIScrollableFrame interface
#define NS_ISCROLLABLE_FRAME_IID    \
{ 0xc95f1831, 0xc372, 0x11d1, \
{ 0xb7, 0x21, 0x0, 0x64, 0x9, 0x92, 0xd8, 0xc9 } }

class nsIScrollableFrame : public nsIScrollableViewProvider {
public:

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISCROLLABLE_FRAME_IID)

  /**
   * Get the frame that we are scrolling within the scrollable frame.
   * @result child frame
   */
  NS_IMETHOD GetScrolledFrame(nsPresContext* aPresContext,
                              nsIFrame *&aScrolledFrame) const = 0;

  typedef nsPresContext::ScrollbarStyles ScrollbarStyles;

  virtual ScrollbarStyles GetScrollbarStyles() const = 0;

  /**
   * Return the actual sizes of all possible scrollbars. Returns 0 for scrollbar
   * positions that don't have a scrollbar or where the scrollbar is not visible.
   */
  virtual nsMargin GetActualScrollbarSizes() const = 0;

  /**
   * Return the sizes of all scrollbars assuming that any scrollbars that could
   * be visible due to overflowing content, are.
   */
  virtual nsMargin GetDesiredScrollbarSizes(nsBoxLayoutState* aState) = 0;

  /**
   * Get the position of the scrolled view.
   */
  NS_IMETHOD  GetScrollPosition(nsPresContext* aContext, nscoord &aX, nscoord& aY) const=0;

  /**
   * Scroll the view to the given x,y, update's the scrollbar's thumb
   * positions and the view's offset. Clamps the values to be
   * legal. Updates the display based on aUpdateFlags.
   * @param aX left edge to scroll to
   * @param aY top edge to scroll to
   * @param aUpdateFlags passed onto nsIViewManager->UpdateView()
   * @return error status
   */
  NS_IMETHOD ScrollTo(nsPresContext* aContext, nscoord aX, nscoord aY, PRUint32 aFlags = NS_VMREFRESH_NO_SYNC)=0;

  NS_IMETHOD GetScrollableView(nsPresContext* aContext, nsIScrollableView** aResult)=0;


  /**
   * Set information about whether the vertical and horizontal scrollbars
   * are currently visible
   */
  NS_IMETHOD SetScrollbarVisibility(nsPresContext* aPresContext,
                                    PRBool aVerticalVisible,
                                    PRBool aHorizontalVisible) = 0;

  NS_IMETHOD GetScrollbarBox(PRBool aVertical, nsIBox** aResult) = 0;

  NS_IMETHOD CurPosAttributeChanged(nsPresContext* aPresContext,
                                    nsIContent* aChild,
                                    PRInt32 aModType) = 0;

  /**
   * This tells the scroll frame to try scrolling to the scroll
   * position that was restored from the history. This must be called
   * at least once after state has been restored. It is called by the
   * scrolled frame itself during reflow, but sometimes state can be
   * restored after reflows are done...
   */
  virtual void ScrollToRestoredPosition() = 0;
};

#endif
