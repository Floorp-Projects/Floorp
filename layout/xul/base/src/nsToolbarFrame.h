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

//
// Mike Pinkerton
// Netscape Communications
//
// nsToolbarFrame is a layout object that contains items (specified as
// children in the DOM). The layout for toolbars is a little complicated, but
// it basically just lays out its children in as a box. Toolbars themselves
// don't know anything about grippies (as in 4.x) but are associated with them
// through toolboxes. This allows a developer to create a standalone toolbar
// (for inclusion in a webpage), which obviously doesn't need to have a grippy.
// 
// As mentioned above, the Toolbar expects its toolbars to be its children in
// the DOM. The exact structure of the children is documented on:
//   http://www.mozilla.org/xpfe/DMWSpecNew.html
//
// This implementation of toolbars now uses evaughan's box code for layout
// of its children and to determine its size.
//

#ifndef nsToolbarFrame_h__
#define nsToolbarFrame_h__

#define TOOLBARITEM_MIME "moz/toolbaritem"
#define TOOLBAR_MIME     "moz/toolbar"


#include "nsCOMPtr.h"
#include "nsBoxFrame.h"

class nsIStyleContext;
class nsIContent;
class nsIPresContext;
class nsIFrame;
class nsToolbarDragListener;


class nsToolbarFrame : public nsBoxFrame
{
public:
  friend nsresult NS_NewToolbarFrame(nsIFrame** aNewFrame);

  NS_IMETHOD  Init(nsIPresContext&  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIStyleContext* aContext,
                   nsIFrame*        asPrevInFlow);

    // nsIHTMLReflow overrides
  NS_IMETHOD  Paint(nsIPresContext& aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect& aDirtyRect,
                    nsFramePaintLayer aWhichLayer);

    // nsFrame overrides
  NS_IMETHOD GetFrameForPoint(const nsPoint& aPoint, // Overridden to capture events
                              nsIFrame**     aFrame);
  NS_IMETHOD  HandleEvent(nsIPresContext& aPresContext, 
                          nsGUIEvent*     aEvent,
                          nsEventStatus&  aEventStatus);

#if WTF_IS_THIS
    //еее not sure at all where this comes from. I asked rods, no reply yet.
  virtual void ReResolveStyles(nsIPresContext& aPresContext,
                               PRInt32 aParentChange,
                               nsStyleChangeList* aChangeList,
                               PRInt32* aLocalChange);
#endif

  void SetDropfeedbackLocation(nscoord aX)  { mXDropLoc = aX; }

protected:

  nsToolbarFrame();
  virtual ~nsToolbarFrame();

    // pass-by-value not allowed for a coordinator because it corresponds 1-to-1
    // with an element in the UI.
  nsToolbarFrame ( const nsToolbarFrame& aFrame ) ;	            // DO NOT IMPLEMENT
  nsToolbarFrame& operator= ( const nsToolbarFrame& aFrame ) ;  // DO NOT IMPLEMENT
  
    // our event handler registered with the content model. See the discussion
    // in Init() for why this is a weak ref.
  nsToolbarDragListener* mDragListener;

    // only used during drag and drop for drop feedback. These are not
    // guaranteed to be meaningful when no drop is underway.
  PRInt32 mXDropLoc;
  nsCOMPtr<nsIStyleContext> mMarkerStyle;

}; // class nsToolbarFrame

#endif
