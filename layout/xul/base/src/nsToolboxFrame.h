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
#include "nsIStyleContext.h"
#include "nsIContent.h"
#include "nsXULAtoms.h"
#include "nsCOMPtr.h"


class nsToolboxFrame : public nsHTMLContainerFrame
{
public:
  friend nsresult NS_NewToolboxFrame(nsIFrame*& aNewFrame);

    // nsIHTMLReflow overrides
  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
  NS_IMETHOD  Paint(nsIPresContext& aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect& aDirtyRect,
                    nsFramePaintLayer aWhichLayer);
  NS_IMETHOD  HandleEvent(nsIPresContext& aPresContext, 
                          nsGUIEvent*     aEvent,
                          nsEventStatus&  aEventStatus);

  NS_IMETHOD Init(nsIPresContext&  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow);
  

  NS_IMETHOD  ReResolveStyleContext ( nsIPresContext* aPresContext, 
                                      nsIStyleContext* aParentContext,
                                      PRInt32 aParentChange,
                                      nsStyleChangeList* aChangeList,
                                      PRInt32* aLocalChange) ;

  NS_IMETHOD GetFrameForPoint(const nsPoint& aPoint, // Overridden to capture events
                              nsIFrame**     aFrame);

protected:
  enum { kGrippyWidthInPixels = 10, kCollapsedGrippyHeightInPixels = 10, kCollapsedGrippyWidthInPixels = 50 } ;
  enum { kNoGrippyHilighted = -1 } ;

  struct TabInfo {
    TabInfo ( ) : mCollapsed(PR_TRUE) { };
  
    void SetProperties ( const nsRect & inBounds, const nsCOMPtr<nsIContent> & inContent, 
                            PRBool inCollapsed )
    {
      mToolbar = inContent;
      mBoundingRect = inBounds;
      mCollapsed = inCollapsed;
    }
    
    nsCOMPtr<nsIContent>  mToolbar;       // content object associated w/ toolbar frame
    nsRect                mBoundingRect;
    PRBool                mCollapsed;
  };
  
  nsToolboxFrame();
  virtual ~nsToolboxFrame();

  PRIntn GetSkipSides() const;

  virtual void UpdateStyles(nsIPresContext* aPresContext);

  void RefreshStyleContext(nsIPresContext* aPresContext,
                            nsIAtom *         aNewContentPseudo,
                            nsCOMPtr<nsIStyleContext>* aCurrentStyle,
                            nsIContent *      aContent,
                            nsIStyleContext*  aParentStyle) ;

  void DrawGrippies ( nsIPresContext& aPresContext, nsIRenderingContext & aContext ) const ;
  void DrawGrippy ( nsIPresContext& aPresContext, nsIRenderingContext & aContext, 
                      const nsRect & aBoundingRect, PRBool aDrawHilighted ) const ;
  void CollapseToolbar ( TabInfo & inTab ) ; 
  void ExpandToolbar ( TabInfo & inTab ) ; 

  void ConvertToLocalPoint ( nsPoint & ioPoint ) ;
  void OnMouseMove ( nsPoint & aMouseLoc ) ;
  void OnMouseExit ( ) ;
  void OnMouseLeftClick ( nsPoint & aMouseLoc ) ;

    // style context for the normal state and rollover state of grippies
  nsCOMPtr<nsIStyleContext>    mGrippyNormalStyle;
  nsCOMPtr<nsIStyleContext>    mGrippyRolloverStyle;
  
  unsigned long mSumOfToolbarHeights;
  TabInfo  mGrippies[10];          //*** make this a list or something!!!!!!
  unsigned short mNumToolbars;
  short mGrippyHilighted;          // used to indicate which grippy the mouse is inside

  const nsCOMPtr<nsIAtom> kCollapsedAtom ;
  const nsCOMPtr<nsIAtom> kHiddenAtom ;
  
    // pass-by-value not allowed for a toolbox because it corresponds 1-to-1
    // with an element in the UI.
  nsToolboxFrame ( const nsToolboxFrame& aFrame ) ;	            // DO NOT IMPLEMENT
  nsToolboxFrame& operator= ( const nsToolboxFrame& aFrame ) ;  // DO NOT IMPLEMENT
  
}; // class nsToolboxFrame
