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
// it basically just lays out its children in order. Toolbars themselves
// don't know anything about grippies (as in 4.x) but are associated with them
// through toolboxes. This allows a developer to create a standalone toolbar
// (for inclusion in a webpage), which obviously doesn't need to have a grippy.
// 
// As mentioned above, the Toolbar expects its toolbars to be its children in
// the DOM. The exact structure of the children is documented on:
//   http://www.mozilla.org/xpfe/DMWSpecNew.html
//
// <<describe toolbar layout in detail>>
//


#include "nsBoxFrame.h"

class nsToolbarFrame : public nsBoxFrame
{
public:
  friend nsresult NS_NewToolbarFrame(nsIFrame*& aNewFrame);

    // nsIHTMLReflow overrides
  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
  NS_IMETHOD  Paint(nsIPresContext& aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect& aDirtyRect,
                    nsFramePaintLayer aWhichLayer);

protected:
  nsToolbarFrame();
  virtual ~nsToolbarFrame();

    // pass-by-value not allowed for a coordinator because it corresponds 1-to-1
    // with an element in the UI.
  nsToolbarFrame ( const nsToolbarFrame& aFrame ) ;	            // DO NOT IMPLEMENT
  nsToolbarFrame& operator= ( const nsToolbarFrame& aFrame ) ;  // DO NOT IMPLEMENT
  
}; // class nsToolbarFrame
