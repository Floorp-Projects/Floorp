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
// nsToolboxFrame is a layout object that contains one or more toolbar frames
// (specified as children in the DOM). These toolbars are laid out one on top
// of the other, and can be of varying heights but are all of the same width
// (the full width of the toolbox). Each toolbar is associated with a "grippy"
// which can be used to either collapse a particular toolbar or as a handle to
// pick up and move a toolbar to a new position within the toolbox. When toolbars
// are collapsed, it's grippy is moved to the bottom of the box and laid on
// its side. Clicking again on the grippy will reinstate the toolbar to its previous
// position in the toolbox.
//
// As mentioned above, the toolbox expects its toolbars to be its children in
// the DOM. The exact structure of the children is documented on:
//   http://www.mozilla.org/xpfe/DMWSpecNew.html
//

#include "nsHTMLContainerFrame.h"


class nsToolboxFrame : public nsHTMLContainerFrame
{
public:
  // friend nsresult NS_NewToolboxFrame(nsIFrame*& aNewFrame, PRUint32 aFlags);

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
  nsToolboxFrame();
  virtual ~nsToolboxFrame();

  virtual PRIntn GetSkipSides() const;

    // pass-by-value not allowed for a coordinator because it corresponds 1-to-1
    // with an element in the UI.
  nsToolboxFrame ( const nsToolboxFrame& aFrame ) ;	            // DO NOT IMPLEMENT
  nsToolboxFrame& operator= ( const nsToolboxFrame& aFrame ) ;  // DO NOT IMPLEMENT
  
}; // class nsToolboxFrame
