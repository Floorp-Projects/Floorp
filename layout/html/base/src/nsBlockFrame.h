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
#ifndef nsBlockFrame_h___
#define nsBlockFrame_h___

#include "nsHTMLContainerFrame.h"
#include "nsIFloaterContainer.h"
#include "nsIHTMLFrameType.h"
#include "nsIRunaround.h"
#include "nsVoidArray.h"
struct BlockBandData;
struct nsMargin;
struct nsStyleFont;

/**
 * Block frames have some state which needs to be made
 * available to child frames for proper reflow. This structure
 * describes that state.
 */
struct nsBlockReflowState {
  // True if this is the first line of text in a block container
  PRPackedBool firstLine;

  // True if leading whitespace is allowed to show
  PRPackedBool allowLeadingWhitespace;

  // This is set when some child frame needs a break after it's placed
  PRPackedBool breakAfterChild;

  // This is set when a child needs a break before it's placed. Note
  // that this value is set by a child AFTER we have called it's
  // ResizeReflow method.
  PRPackedBool breakBeforeChild;

  // This is set when the first child should be treated specially
  // because it's an inside list item bullet.
  // XXX this can go away once we have a way for the bullet's style
  // molecule to *not* be the same as it's parent's
  PRPackedBool firstChildIsInsideBullet;

  // For pre-formatted text, this is our current column
  PRIntn column;

  // The next list ordinal value
  PRInt32 nextListOrdinal;

  //----------------------------------------------------------------------
  // State from here on down is not to be used by block child frames!

  // Space manager to use
  nsISpaceManager* spaceManager;

  // Block's style data
  nsStyleFont* font;
  nsStyleMolecule* mol;

  // Block's available size (computed from the block's parent)
  nsSize availSize;

  // Current band of available space. Used for doing runaround
  BlockBandData* currentBand;

  // Pointer to a max-element-size (nsnull if none required)
  nsSize* maxElementSize;

  // The maximum x-most of our lines and block-level elements. This is used to
  // compute our desired size, and includes our left border/padding. For block-
  // level elements this also includes the block's right margin.
  nscoord kidXMost;

  // Current reflow position
  nscoord y;

  // Current line state
  nscoord x;                            // inline elements only. not include border/padding
  PRBool  isInline;                     // whether the current is inline or block
  nsVoidArray lineLengths;              // line length temporary storage
  PRIntn currentLineNumber;             // index into mLines
  nsIFrame* lineStart;                  // frame starting the line
  PRInt32 lineLength;                   // length of line
  nscoord* ascents;                     // ascent information for each child
  nscoord maxAscent;                    // max ascent for this line
  nscoord maxDescent;                   // max descent for this line
  nscoord lineWidth;                    // current width of line
#if 0
  nscoord maxPosTopMargin;              // maximum positive top margin
  nscoord maxNegTopMargin;              // maximum negative top margin
#else
  nscoord topMargin;                    // current top margin
#endif
  nscoord maxPosBottomMargin;           // maximum positive bottom margin
  nscoord maxNegBottomMargin;           // maximum negative bottom margin
  nsSize lineMaxElementSize;            // max element size for current line
  PRBool lastContentIsComplete;         // reflow status of last child on line
  nsVoidArray floaterToDo;              // list of floaters to place below current line

  PRInt32 maxAscents;                   // size of ascent buffer
  nscoord ascentBuf[20];

  PRPackedBool needRelativePos;         // some kid in line needs relative pos

  // Previous line state that we carry forward to the next line
  nsIFrame* prevLineLastFrame;
  nscoord prevLineHeight;               // height of the previous line
  nscoord prevMaxPosBottomMargin;       // maximum posative bottom margin
  nscoord prevMaxNegBottomMargin;       // maximum negative bottom margin
  PRBool prevLineLastContentIsComplete;

  // Sanitized version of mol->borderPadding; if this block frame is a
  // pseudo-frame then the margin will be zero'd.
  nsMargin borderPadding;

  PRPackedBool justifying;              // we are justifying

  // Status from last PlaceAndReflowChild
  nsIFrame::ReflowStatus reflowStatus;

  // Flags for whether the max size is unconstrained
  PRBool  unconstrainedWidth;
  PRBool  unconstrainedHeight;

  nsBlockReflowState();

  ~nsBlockReflowState();

  void Init(const nsSize& aMaxSize, nsSize* aMaxElementSize,
            nsStyleFont* aFont, nsStyleMolecule* aMol, nsISpaceManager* aSpaceManager);

  void AddAscent(nscoord aAscent);
  void AdvanceToNextLine(nsIFrame* aPrevLineLastFrame, nscoord aPrevLineHeight);

#ifdef NS_DEBUG
  void DumpLine();
  void DumpList();
#endif
};

//----------------------------------------------------------------------

/**
 * <h2>Block Reflow</h2>
 *
 * The block frame reflow machinery performs "2D" layout. Inline
 * elements are flowed into logical lines (left to right or right to
 * left) and the lines are stacked vertically. Block elements are
 * flowed onto their own line after flushing out any preceeding line.<p>
 *
 * After a line is ready to be flushed out, vertical alignment is
 * performed. Vertical alignment may require the line to consume more
 * vertical space than is available thus causing the entire line to
 * be pushed. <p>
 *
 * After vertical alignment is done, horizontal alignment (including
 * justification) is performed. Finally, relative positioning is done
 * on any elements that require it. <p>
 *
 * During reflow, the block frame will make available to child frames
 * it's reflow state using the presentation shell's cached data
 * mechanism. <p>
 *
 * <h2>Reflowing Mapped Content</h2>
 * <h2>Pullup</h2>
 * <h2>Reflowing Unmapped Content</h2>
 * <h2>Content Insertion Handling</h2>
 * <h2>Content Deletion Handling</h2>
 * <h2>Style Change Handling</h2>
 *
 * <h3>Assertions</h3>
 * <b>mLastContentIsComplete</b> always reflects the state of the last
 * child frame on our chlid list.
 */
class nsBlockFrame : public nsHTMLContainerFrame, public nsIHTMLFrameType,
                     public nsIRunaround, public nsIFloaterContainer
{
public:
  /**
   * Create a new block frame that maps the given piece of content.
   */
  static nsresult NewFrame(nsIFrame** aInstancePtrResult,
                           nsIContent* aContent,
                           PRInt32     aIndexInParent,
                           nsIFrame*   aParent);

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  NS_IMETHOD ResizeReflow(nsIPresContext* aPresContext,
                          nsISpaceManager* aSpaceManager,
                          const nsSize& aMaxSize,
                          nsRect& aDesiredRect,
                          nsSize* aMaxElementSize,
                          ReflowStatus& aStatus);

  NS_IMETHOD IncrementalReflow(nsIPresContext* aPresContext,
                               nsISpaceManager* aSpaceManager,
                               const nsSize& aMaxSize,
                               nsRect& aDesiredRect,
                               nsReflowCommand& aReflowCommand,
                               ReflowStatus& aStatus);

  NS_IMETHOD ContentAppended(nsIPresShell* aShell,
                             nsIPresContext* aPresContext,
                             nsIContent* aContainer);

  NS_IMETHOD CreateContinuingFrame(nsIPresContext* aPresContext,
                                   nsIFrame* aParent,
                                   nsIFrame*& aContinuingFrame);

  virtual PRBool AddFloater(nsIPresContext* aCX,
                            nsIFrame* aFloater,
                            PlaceholderFrame* aPlaceholder);
  virtual void PlaceFloater(nsIPresContext* aCX,
                            nsIFrame* aFloater,
                            PlaceholderFrame* aPlaceholder);

  NS_IMETHOD ListTag(FILE* out = stdout) const;

  virtual nsHTMLFrameType GetFrameType() const;

protected:
  nsBlockFrame(nsIContent* aContent,
               PRInt32 aIndexInParent,
               nsIFrame* aParent);

  virtual ~nsBlockFrame();

  virtual PRIntn GetSkipSides() const;

  PRBool MoreToReflow(nsIPresContext* aCX);

  nscoord GetTopMarginFor(nsIPresContext* aCX,
                          nsBlockReflowState& aState,
                          nsStyleMolecule* aKidMol,
                          PRBool aIsInline);

  PRBool AdvanceToNextLine(nsIPresContext* aPresContext,
                           nsBlockReflowState& aState);

  void AddInlineChildToLine(nsIPresContext* aCX,
                            nsBlockReflowState& aState,
                            nsIFrame* aKidFrame,
                            nsReflowMetrics& aKidSize,
                            nsSize* aKidMaxElementSize,
                            nsStyleMolecule* aKidMol);

  void AddBlockChild(nsIPresContext* aCX,
                     nsBlockReflowState& aState,
                     nsIFrame* aKidFrame,
                     nsRect& aKidRect,
                     nsSize* aKidMaxElementSize,
                     nsStyleMolecule* aKidMol);

  void GetAvailSize(nsSize& aResult,
                    nsBlockReflowState& aState,
                    nsStyleMolecule* aKidMol,
                    PRBool aIsInline);

  PRIntn PlaceAndReflowChild(nsIPresContext* aCX,
                             nsBlockReflowState& aState,
                             nsIFrame* kidFrame,
                             nsStyleMolecule* aKidMol);

  void PushKids(nsBlockReflowState& aState);

  void SetupState(nsIPresContext* aCX, nsBlockReflowState& aState,
                  const nsSize& aMaxSize, nsSize* aMaxElementSize,
                  nsISpaceManager* aSpaceManager);

  nsresult DoResizeReflow(nsIPresContext* aPresContext,
                          nsBlockReflowState& aState,
                          nsRect& aDesiredRect,
                          ReflowStatus& aStatus);

  PRBool ReflowMappedChildren(nsIPresContext* aPresContext,
                              nsBlockReflowState& aState);

  PRBool PullUpChildren(nsIPresContext* aCX,
                        nsBlockReflowState& aState);

  ReflowStatus ReflowAppendedChildren(nsIPresContext* aPresContext,
                                      nsBlockReflowState& aState);

  void JustifyLines(nsIPresContext* aPresContext, nsBlockReflowState& aState);

  PRBool IsLeftMostChild(nsIFrame* aFrame);

  void  GetAvailableSpaceBand(nsBlockReflowState& aState, nscoord aY);

  void PlaceBelowCurrentLineFloaters(nsIPresContext* aCX,
                                     nsBlockReflowState& aState,
                                     nscoord aY);

  void ClearFloaters(nsBlockReflowState& aState, PRUint32 aClear);

#ifdef NS_DEBUG
  void DumpFlow() const;
#endif

  /**
   * Array of lines lengths. For each logical line of children, this array
   * contains a count of the number of children on the line.
   */
  PRInt32* mLines;
  PRInt32 mNumLines;
  nsBlockReflowState* mCurrentState;
};

#endif /* nsBlockFrame_h___ */
