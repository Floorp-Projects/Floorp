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
#include "nsBlockFrame.h"
#include "nsSize.h"
#include "nsIAnchoredItems.h"
#include "nsIContent.h"
#include "nsIContentDelegate.h"
#include "nsISpaceManager.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsMargin.h"
#include "nsHTMLIIDs.h"
#include "nsCSSLayout.h"
#include "nsCRT.h"
#include "nsIPresShell.h"
#include "nsReflowCommand.h"
#include "nsPlaceholderFrame.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLValue.h"
#include "nsIHTMLContent.h"
#include "nsAbsoluteFrame.h"
#include "nsIPtr.h"

#ifdef NS_DEBUG
#undef NOISY
#undef NOISY_FLOW
#else
#undef NOISY
#undef NOISY_FLOW
#endif

static NS_DEFINE_IID(kIRunaroundIID, NS_IRUNAROUND_IID);
static NS_DEFINE_IID(kIFloaterContainerIID, NS_IFLOATERCONTAINER_IID);
static NS_DEFINE_IID(kIAnchoredItemsIID, NS_IANCHOREDITEMS_IID);

static NS_DEFINE_IID(kStyleDisplaySID, NS_STYLEDISPLAY_SID);
static NS_DEFINE_IID(kStyleFontSID, NS_STYLEFONT_SID);
static NS_DEFINE_IID(kStylePositionSID, NS_STYLEPOSITION_SID);
static NS_DEFINE_IID(kStyleTextSID, NS_STYLETEXT_SID);
static NS_DEFINE_IID(kStyleSpacingSID, NS_STYLESPACING_SID);

NS_DEF_PTR(nsIStyleContext);
NS_DEF_PTR(nsIContent);

struct BlockBandData : public nsBandData {
  nsBandTrapezoid data[12];

  // Bounding rect of available space between any left and right floaters
  nsRect          availSpace;

  // Constructor
  BlockBandData() {size = 12; trapezoids = data;}

  // Computes the bounding rect of the available space, i.e. space between
  // any left and right floaters
  //
  // Uses the current trapezoid data, see nsISpaceManager::GetBandData().
  // Updates member data "availSpace"
  void ComputeAvailSpaceRect();
};

void BlockBandData::ComputeAvailSpaceRect()
{
  nsBandTrapezoid*  trapezoid = data;

  if (count > 1) {
    // If there's more than one trapezoid that means there are floaters
    PRInt32 i;

    // Stop when we get to space occupied by a right floater
    for (i = 0; i < count; i++) {
      nsBandTrapezoid*  trapezoid = &data[i];

      if (trapezoid->state != nsBandTrapezoid::smAvailable) {
        nsStyleDisplay* display;
      
        // XXX Handle the case of multiple frames
        trapezoid->frame->GetStyleData(kStyleDisplaySID, (nsStyleStruct*&)display);
        if (NS_STYLE_FLOAT_RIGHT == display->mFloats) {
          break;
        }
      }
    }

    if (i > 0) {
      trapezoid = &data[i - 1];
    }
  }

  if (nsBandTrapezoid::smAvailable == trapezoid->state) {
    // The trapezoid is available
    trapezoid->GetRect(availSpace);
  } else {
    nsStyleDisplay* display;

    // The trapezoid is occupied. That means there's no available space
    trapezoid->GetRect(availSpace);

    // XXX Handle the case of multiple frames
    trapezoid->frame->GetStyleData(kStyleDisplaySID, (nsStyleStruct*&)display);
    if (NS_STYLE_FLOAT_LEFT == display->mFloats) {
      availSpace.x = availSpace.XMost();
    }
    availSpace.width = 0;
  }
}

// XXX Bugs
// 1. right to left reflow can generate negative x coordinates.

// XXX Speedup idea (all containers)

// If I reflow a child and it gives back not-complete status then
// there is no sense in trying to pullup children. For blocks, it's a
// little more complicated unless the child is a block - if the child
// is a block, then we must be out of room hence we should stop. If
// the child is not a block then our line should be flushed (see #2
// below) if our line is already empty then we must be out of room.

// For inline frames and column frames, if we reflow a child and get
// back not-complete status then we should bail immediately because we
// are out of room.

// XXX Speedup ideas:
// 1. change pullup code to use line information from next in flow
// 2. we can advance to next line immediately after reflowing something
//    and noticing that it's not complete.
// 3. pass down last child information in aState so that pullup, etc.,
//    don't need to recompute it

// XXX TODO:
// 0. Move justification into line flushing code

// 1. To get ebina margins I need "auto" information from the style
//    system margin's. A bottom/top margin of auto will then be computed like
//    ebina computes it [however the heck that is].

// 2. kicking out floaters and talking with floater container to adjust
//    left and right margins

nsBlockReflowState::nsBlockReflowState()
{
}

void nsBlockReflowState::Init(const nsSize& aMaxSize,
                              nsSize* aMaxElementSize,
                              nsIStyleContext* aBlockSC,
                              nsISpaceManager* aSpaceManager)
{
  firstLine = PR_TRUE;
  allowLeadingWhitespace = PR_FALSE;
  breakAfterChild = PR_FALSE;
  breakBeforeChild = PR_FALSE;
  firstChildIsInsideBullet = PR_FALSE;
  nextListOrdinal = -1;
  column = 0;

  spaceManager = aSpaceManager;
  currentBand = new BlockBandData;

  styleContext = aBlockSC;
  styleText = (nsStyleText*) aBlockSC->GetData(kStyleTextSID);
  styleFont = (nsStyleFont*) aBlockSC->GetData(kStyleFontSID);
  styleDisplay = (nsStyleDisplay*) aBlockSC->GetData(kStyleDisplaySID);

  justifying = (NS_STYLE_TEXT_ALIGN_JUSTIFY == styleText->mTextAlign) &&
    (NS_STYLE_WHITESPACE_PRE != styleText->mWhiteSpace);

  availSize.width = aMaxSize.width;
  availSize.height = aMaxSize.height;
  maxElementSize = aMaxElementSize;
  if (nsnull != aMaxElementSize) {
    aMaxElementSize->width = 0;
    aMaxElementSize->height = 0;
  }

  kidXMost = 0;
  x = 0;
  y = 0;

  isInline = PR_FALSE;
  currentLineNumber = 0;
  lineStart = nsnull;
  lineLength = 0;
  ascents = ascentBuf;
  maxAscent = 0;
  maxDescent = 0;
  lineWidth = 0;
  maxPosBottomMargin = 0;
  maxNegBottomMargin = 0;
  lineMaxElementSize.width = 0;
  lineMaxElementSize.height = 0;
  lastContentIsComplete = PR_TRUE;

  maxAscents = sizeof(ascentBuf) / sizeof(ascentBuf[0]);
  needRelativePos = PR_FALSE;

  prevLineLastFrame = nsnull;
  prevLineHeight = 0;
  topMargin = 0;
  prevMaxPosBottomMargin = 0;
  prevMaxNegBottomMargin = 0;
  prevLineLastContentIsComplete = PR_TRUE;

  unconstrainedWidth = PRBool(aMaxSize.width == NS_UNCONSTRAINEDSIZE);
  unconstrainedHeight = PRBool(aMaxSize.height == NS_UNCONSTRAINEDSIZE);

  reflowStatus = nsIFrame::frNotComplete;
}

nsBlockReflowState::~nsBlockReflowState()
{
  if (ascents != ascentBuf) {
    delete ascents;
  }
  delete currentBand;
}

void nsBlockReflowState::AddAscent(nscoord aAscent)
{
  NS_PRECONDITION(lineLength <= maxAscents, "bad line length");
  if (lineLength == maxAscents) {
    maxAscents *= 2;
    nscoord* newAscents = new nscoord[maxAscents];
    if (nsnull != newAscents) {
      nsCRT::memcpy(newAscents, ascents, sizeof(nscoord) * lineLength);
      if (ascents != ascentBuf) {
        delete ascents;
      }
      ascents = newAscents;
    } else {
      // Yikes! Out of memory!
      return;
    }
  }
  ascents[lineLength] = aAscent;
}

void nsBlockReflowState::AdvanceToNextLine(nsIFrame* aPrevLineLastFrame,
                                           nscoord   aPrevLineHeight)
{
  firstLine = PR_FALSE;
  allowLeadingWhitespace = PR_FALSE;
  column = 0;
  breakAfterChild = PR_FALSE;
  breakBeforeChild = PR_FALSE;
  lineStart = nsnull;
  lineLength = 0;
  currentLineNumber++;
  maxAscent = 0;
  maxDescent = 0;
  lineWidth = 0;
  needRelativePos = PR_FALSE;

  prevLineLastFrame = aPrevLineLastFrame;
  prevLineHeight = aPrevLineHeight;
  prevMaxPosBottomMargin = maxPosBottomMargin;
  prevMaxNegBottomMargin = maxNegBottomMargin;

  // Remember previous line's lastContentIsComplete
  prevLineLastContentIsComplete = lastContentIsComplete;
  lastContentIsComplete = PR_TRUE;

  topMargin = 0;
  maxPosBottomMargin = 0;
  maxNegBottomMargin = 0;
}

#ifdef NS_DEBUG
void nsBlockReflowState::DumpLine()
{
  nsIFrame* f = lineStart;
  PRInt32 ll = lineLength;
  while (--ll >= 0) {
    printf("  ");
    ((nsFrame*)f)->ListTag(stdout);/* XXX */
    printf("\n");
    f->GetNextSibling(f);
  }
}

void nsBlockReflowState::DumpList()
{
  nsIFrame* f = lineStart;
  while (nsnull != f) {
    printf("  ");
    ((nsFrame*)f)->ListTag(stdout);/* XXX */
    printf("\n");
    f->GetNextSibling(f);
  }
}
#endif

//----------------------------------------------------------------------

nsresult nsBlockFrame::NewFrame(nsIFrame** aInstancePtrResult,
                                nsIContent* aContent,
                                PRInt32     aIndexInParent,
                                nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsBlockFrame(aContent, aIndexInParent, aParent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}

nsBlockFrame::nsBlockFrame(nsIContent* aContent,
                           PRInt32     aIndexInParent,
                           nsIFrame*   aParent)
  : nsHTMLContainerFrame(aContent, aIndexInParent, aParent)
{
}

nsBlockFrame::~nsBlockFrame()
{
  if (nsnull != mLines) {
    delete mLines;
  }
}

nsresult
nsBlockFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIHTMLFrameTypeIID)) {
    *aInstancePtr = (void*) ((nsIHTMLFrameType*) this);
    return NS_OK;
  } else if (aIID.Equals(kIRunaroundIID)) {
    *aInstancePtr = (void*) ((nsIRunaround*) this);
    return NS_OK;
  } else if (aIID.Equals(kIFloaterContainerIID)) {
    *aInstancePtr = (void*) ((nsIFloaterContainer*) this);
    return NS_OK;
  }
  return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);
}

// Computes the top margin to use for this child frames based on its display
// type and the display type of the previous child frame.
//
// Adjacent vertical margins between block-level elements are collapsed.
nscoord nsBlockFrame::GetTopMarginFor(nsIPresContext*     aCX,
                                      nsBlockReflowState& aState,
                                      nsIFrame*           aKidFrame,
                                      nsIStyleContext*    aKidSC,
                                      PRBool              aIsInline)
{
  if (aIsInline) {
    // Just use whatever the previous bottom margin was
    return aState.prevMaxPosBottomMargin - aState.prevMaxNegBottomMargin;
  } else {
    // Does the frame have a prev-in-flow?
    nsIFrame* kidPrevInFlow;

    aKidFrame->GetPrevInFlow(kidPrevInFlow);

    if (nsnull == kidPrevInFlow) {
      nscoord maxNegTopMargin = 0;
      nscoord maxPosTopMargin = 0;
      nsStyleSpacing* ss = (nsStyleSpacing*) aKidSC->GetData(kStyleSpacingSID);
      if (ss->mMargin.top < 0) {
        maxNegTopMargin = -ss->mMargin.top;
      } else {
        maxPosTopMargin = ss->mMargin.top;
      }
    
      nscoord maxPos = PR_MAX(aState.prevMaxPosBottomMargin, maxPosTopMargin);
      nscoord maxNeg = PR_MAX(aState.prevMaxNegBottomMargin, maxNegTopMargin);
      return maxPos - maxNeg;
    } else {
      return 0;
    }
  }
}

void nsBlockFrame::PlaceBelowCurrentLineFloaters(nsIPresContext* aCX,
                                                 nsBlockReflowState& aState,
                                                 nscoord aY)
{
  NS_PRECONDITION(aState.floaterToDo.Count() > 0, "no floaters");

  // XXX Factor this code with PlaceFloater()...
  PRInt32   numFloaters = aState.floaterToDo.Count();
  
  for (PRInt32 i = 0; i < numFloaters; i++) {
    nsIFrame* floater = (nsIFrame*)aState.floaterToDo[i];
    nsRect    region;

    // Get the band of available space
    // XXX This is inefficient to do this inside the loop...
    GetAvailableSpaceBand(aState, aY);

    // Get the type of floater
    nsIStyleContextPtr  styleContext;
     
    floater->GetStyleContext(aCX, styleContext.AssignRef());
    nsStyleDisplay* sd = (nsStyleDisplay*)
      styleContext->GetData(kStyleDisplaySID);
  
    floater->GetRect(region);
    region.y = mCurrentState->currentBand->availSpace.y;

    if (NS_STYLE_FLOAT_LEFT == sd->mFloats) {
      region.x = mCurrentState->currentBand->availSpace.x;
    } else {
      NS_ASSERTION(NS_STYLE_FLOAT_RIGHT == sd->mFloats, "bad float type");
      region.x = mCurrentState->currentBand->availSpace.XMost() - region.width;
    }

    // XXX Don't forget the floater's margins...
    mCurrentState->spaceManager->Translate(mCurrentState->borderPadding.left,
                                           0);
    mCurrentState->spaceManager->AddRectRegion(region, floater);

    // Set the origin of the floater in world coordinates
    nscoord worldX, worldY;

    mCurrentState->spaceManager->GetTranslation(worldX, worldY);
    floater->MoveTo(region.x + worldX, region.y + worldY);
    mCurrentState->spaceManager->Translate(-mCurrentState->borderPadding.left, 0);
  }
  aState.floaterToDo.Clear();
}

/**
 * Flush a line out. Return true if the line fits in our available
 * height. If the line does not fit then return false. When the line
 * fits we advance the y coordinate, reset the x coordinate and
 * prepare the nsBlockReflowState for the next line.
 */
PRBool nsBlockFrame::AdvanceToNextLine(nsIPresContext* aCX,
                                       nsBlockReflowState& aState)
{
  NS_PRECONDITION(aState.lineLength > 0, "bad line");
  NS_PRECONDITION(nsnull != aState.lineStart, "bad line");

  nscoord y = aState.y + aState.topMargin;
  nscoord lineHeight;

  if (aState.isInline) {
    // Vertically align the children on this line, returning the height of
    // the line upon completion.
    lineHeight = nsCSSLayout::VerticallyAlignChildren(aCX, this,
                                                      aState.styleFont,
                                                      y,
                                                      aState.lineStart,
                                                      aState.lineLength,
                                                      aState.ascents,
                                                      aState.maxAscent);

    // Any below current line floaters to place?
    if (aState.floaterToDo.Count() > 0) {
      PlaceBelowCurrentLineFloaters(aCX, aState, y + lineHeight);
      // XXX Factor in the height of the floaters as well when considering
      // whether the line fits.
      // The default policy is that if there isn't room for the floaters then
      // both the line and the floaters are pushed to the next-in-flow...
    }
  } else {
    nsSize  size;

    aState.lineStart->GetSize(size);
    lineHeight = size.height;
  }

  // The first line always fits
  if (aState.currentLineNumber > 0) {
    nscoord yb = aState.borderPadding.top + aState.availSize.height;
    if (y + lineHeight > yb) {
      // After vertical alignment of the children and factoring in the
      // proper margin, the line doesn't fit.
      return PR_FALSE;
    }
  }

  if (aState.isInline) {
    // Check if the right-edge of the line exceeds our running x-most
    nscoord xMost = aState.borderPadding.left + aState.lineWidth;
    if (xMost > aState.kidXMost) {
      aState.kidXMost = xMost;
    }
  }

  // Advance the y coordinate to the new position where the next
  // line or block element will go.
  aState.y = y + lineHeight;
  aState.x = 0;

  // Now that the vertical alignment is done we can perform horizontal
  // alignment and relative positioning. Skip all of these if we are
  // doing an unconstrained (in x) reflow. There's no point in doing
  // the work if we *know* we are going to reflowed again.
  if (!aState.unconstrainedWidth) {
    nsCSSLayout::HorizontallyPlaceChildren(aCX, this, aState.styleText,
                                           aState.lineStart, aState.lineLength,
                                           aState.lineWidth,
                                           aState.availSize.width);

    // Finally, now that the in-flow positions of the line's frames are
    // known we can apply relative positioning if any of them need it.
    if (!aState.justifying) { 
      nsCSSLayout::RelativePositionChildren(aCX, this,
                                            aState.lineStart,
                                            aState.lineLength);
    }
  }

  // Record line length
  aState.lineLengths.AppendElement((void*)aState.lineLength);

  // Find the last frame in the line
  // XXX keep this as running state in the nsBlockReflowState
  nsIFrame* lastFrame = aState.lineStart;
  PRInt32 lineLen = aState.lineLength - 1;
  while (--lineLen >= 0) {
    lastFrame->GetNextSibling(lastFrame);
  }

  // Update maxElementSize
  if (nsnull != aState.maxElementSize) {
    nsSize& lineMax = aState.lineMaxElementSize;
    nsSize* maxMax = aState.maxElementSize;
    if (lineMax.width > maxMax->width) {
      maxMax->width = lineMax.width;
    }
    if (lineMax.height > maxMax->height) {
      maxMax->height = lineMax.height;
    }
    aState.lineMaxElementSize.width = 0;
    aState.lineMaxElementSize.height = 0;
  }

  // Advance to the next line
  aState.AdvanceToNextLine(lastFrame, lineHeight);

  return PR_TRUE;
}

/**
 * Add an inline  child to the current line. Advance various running
 * values after placement.
 */
void nsBlockFrame::AddInlineChildToLine(nsIPresContext* aCX,
                                        nsBlockReflowState& aState,
                                        nsIFrame* aKidFrame,
                                        nsReflowMetrics& aKidSize,
                                        nsSize* aKidMaxElementSize,
                                        nsIStyleContext* aKidSC)
{
  NS_PRECONDITION(nsnull != aState.lineStart, "bad line");

  nsStyleDisplay* ds = (nsStyleDisplay*) aKidSC->GetData(kStyleDisplaySID);
  nsStyleSpacing* ss = (nsStyleSpacing*) aKidSC->GetData(kStyleSpacingSID);
  nsStylePosition* sp = (nsStylePosition*) aKidSC->GetData(kStylePositionSID);

  if (NS_STYLE_POSITION_RELATIVE == sp->mPosition) {
    aState.needRelativePos = PR_TRUE;
  }

  // Place and size the child
  // XXX add in left margin from kid
  nsRect r;
  r.y = aState.y;
  r.width = aKidSize.width;
  r.height = aKidSize.height;
  if (NS_STYLE_DIRECTION_LTR == aState.styleDisplay->mDirection) {
    // Left to right positioning.
    r.x = aState.borderPadding.left + aState.x + ss->mMargin.left;
    aState.x += aKidSize.width + ss->mMargin.left + ss->mMargin.right;
  } else {
    // Right to left positioning
    // XXX what should we do when aState.x goes negative???
    r.x = aState.x - aState.borderPadding.right - ss->mMargin.right -
      aKidSize.width;
    aState.x -= aKidSize.width + ss->mMargin.right + ss->mMargin.left;
  }
  aKidFrame->SetRect(r);
  aState.AddAscent(aKidSize.ascent);
  aState.lineWidth += aKidSize.width;
  aState.lineLength++;

  // Update maximums for the line
  if (aKidSize.ascent > aState.maxAscent) {
    aState.maxAscent = aKidSize.ascent;
  }
  if (aKidSize.descent > aState.maxDescent) {
    aState.maxDescent = aKidSize.descent;
  }
  // Update running margin maximums
  if (aState.firstChildIsInsideBullet && (aKidFrame == mFirstChild)) {
    // XXX temporary code. Since the molecule for the bullet frame
    // is the same as the LI frame, we get bad style information.
    // ignore it.
  } else {
    nscoord margin;
#if 0
    // XXX CSS2 spec says that top/bottom margin don't affect line height
    // calculation. We're waiting for clarification on this issue...
    if ((margin = ss->mMargin.top) < 0) {
      margin = -margin;
      if (margin > aState.maxNegTopMargin) {
        aState.maxNegTopMargin = margin;
      }
    } else {
      if (margin > aState.maxPosTopMargin) {
        aState.maxPosTopMargin = margin;
      }
    }
#endif
    if ((margin = ss->mMargin.bottom) < 0) {
      margin = -margin;
      if (margin > aState.maxNegBottomMargin) {
        aState.maxNegBottomMargin = margin;
      }
    } else {
      if (margin > aState.maxPosBottomMargin) {
        aState.maxPosBottomMargin = margin;
      }
    }
  }

  // Update line max element size
  nsSize& mes = aState.lineMaxElementSize;
  if (nsnull != aKidMaxElementSize) {
    if (aKidMaxElementSize->width > mes.width) {
      mes.width = aKidMaxElementSize->width;
    }
    if (aKidMaxElementSize->height > mes.height) {
      mes.height = aKidMaxElementSize->height;
    }
  }
}

// Places and sizes the block-level element, and advances the line.
// The rect is in the local coordinate space of the kid frame.
void nsBlockFrame::AddBlockChild(nsIPresContext* aCX,
                                 nsBlockReflowState& aState,
                                 nsIFrame* aKidFrame,
                                 nsRect& aKidRect,
                                 nsSize* aKidMaxElementSize,
                                 nsIStyleContext* aKidSC)
{
  NS_PRECONDITION(nsnull != aState.lineStart, "bad line");

  nsStyleDisplay* ds = (nsStyleDisplay*) aKidSC->GetData(kStyleDisplaySID);
  nsStyleSpacing* ss = (nsStyleSpacing*) aKidSC->GetData(kStyleSpacingSID);
  nsStylePosition* sp = (nsStylePosition*) aKidSC->GetData(kStylePositionSID);

  if (NS_STYLE_POSITION_RELATIVE == sp->mPosition) {
    aState.needRelativePos = PR_TRUE;
  }

  // Translate from the kid's coordinate space to our coordinate space
  aKidRect.x += aState.borderPadding.left + ss->mMargin.left;
  aKidRect.y += aState.y + aState.topMargin;

  // Place and size the child
  aKidFrame->SetRect(aKidRect);

  aState.AddAscent(aKidRect.height);
  aState.lineLength++;

  // Is this the widest child frame?
  nscoord xMost = aKidRect.XMost() + ss->mMargin.right;
  if (xMost > aState.kidXMost) {
    aState.kidXMost = xMost;
  }

  // Update the max element size
  if (nsnull != aKidMaxElementSize) {
    if (aKidMaxElementSize->width > aState.maxElementSize->width) {
      aState.maxElementSize->width = aKidMaxElementSize->width;
    }
    if (aKidMaxElementSize->height > aState.maxElementSize->height) {
      aState.maxElementSize->height = aKidMaxElementSize->height;
    }
  }

  // and the bottom line margin information which we'll use when placing
  // the next child
  if (ss->mMargin.bottom < 0) {
    aState.maxNegBottomMargin = -ss->mMargin.bottom;
  } else {
    aState.maxPosBottomMargin = ss->mMargin.bottom;
  }

  // Update the running y-offset
  aState.y += aKidRect.height + aState.topMargin;

  // Apply relative positioning if necessary
  nsCSSLayout::RelativePositionChildren(aCX, this, aKidFrame, 1);

  // Advance to the next line
  aState.AdvanceToNextLine(aKidFrame, aKidRect.height);
}

/**
 * Compute the available size for reflowing the given child at the
 * current x,y position in the state. Note that this may return
 * negative or zero width/height's if we are out of room.
 */
void nsBlockFrame::GetAvailSize(nsSize& aResult,
                                nsBlockReflowState& aState,
                                nsIStyleContext* aKidSC,
                                PRBool aIsInline)
{
  // Determine the maximum available reflow height for the child
  nscoord yb = aState.borderPadding.top + aState.availSize.height;
  aResult.height = aState.unconstrainedHeight ? NS_UNCONSTRAINEDSIZE :
    yb - aState.y - aState.topMargin;

  // Determine the maximum available reflow width for the child
  if (aState.unconstrainedWidth) {
    aResult.width = NS_UNCONSTRAINEDSIZE;
  } else if (aIsInline) {
    if (NS_STYLE_DIRECTION_LTR == aState.styleDisplay->mDirection) {
      aResult.width = aState.currentBand->availSpace.XMost() - aState.x;
    } else {
      aResult.width = aState.x - aState.currentBand->availSpace.x;
    }
  } else {
    // It's a block. Don't adjust for the left/right margin here. That happens
    // later on once we know the current left/right edge
    aResult.width = aState.availSize.width;
  }
}

/**
 * Push all of the kids that we have not reflowed, starting at
 * aState.lineStart. aPrevKid is the kid previous to aState.lineStart
 * and is also our last child. Note that line length is <B>NOT</B> a
 * reflection of the number of children we are actually pushing
 * (because we don't break the sibling list as we add children to the
 * line).
 */
void nsBlockFrame::PushKids(nsBlockReflowState& aState)
{
  nsIFrame* prevFrame = aState.prevLineLastFrame;
  NS_PRECONDITION(nsnull != prevFrame, "pushing all kids");
#ifdef NS_DEBUG
  nsIFrame* nextSibling;

  prevFrame->GetNextSibling(nextSibling);
  NS_PRECONDITION(nextSibling == aState.lineStart, "bad prev line");
#endif

#ifdef NS_DEBUG
  PRInt32 numKids = LengthOf(mFirstChild);
  NS_ASSERTION(numKids == mChildCount, "bad child count");
#endif

#ifdef NOISY
  ListTag(stdout);
  printf(": push kids (childCount=%d)\n", mChildCount);
  DumpFlow();
#endif

  PushChildren(aState.lineStart, prevFrame, mLastContentIsComplete);
  SetLastContentOffset(prevFrame);

  // Set mLastContentIsComplete to the previous lines last content is
  // complete now that the previous line's last child is our last
  // child.
  mLastContentIsComplete = aState.prevLineLastContentIsComplete;

  // Fix up child count
  // XXX is there a better way? aState.lineLength doesn't work because
  // we might be pushing more than just the pending line.
  nsIFrame* kid = mFirstChild;
  PRInt32 kids = 0;
  while (nsnull != kid) {
    kids++;
    kid->GetNextSibling(kid);
  }
  mChildCount = kids;

  // Make sure we have no lingering line data
  aState.lineLength = 0;
  aState.lineStart = nsnull;

#ifdef NOISY
  ListTag(stdout);
  printf(": push kids done (childCount=%d) [%c]\n", mChildCount,
         (mLastContentIsComplete ? 'T' : 'F'));
  DumpFlow();
#endif
}

/**
 * Gets a band of available space starting at the specified y-offset. Assumes
 * the local coordinate space is currently set to the upper-left origin of the
 * bounding rect
 *
 * Updates "currentBand" and "x" member data of the block reflow state
 */
void nsBlockFrame::GetAvailableSpaceBand(nsBlockReflowState& aState, nscoord aY)
{
  // Gets a band of available space.
  aState.spaceManager->Translate(aState.borderPadding.left, 0);
  aState.spaceManager->GetBandData(aY, aState.availSize, *aState.currentBand);

  // Compute the bounding rect of the available space, i.e. space between any
  // left and right floaters
  aState.currentBand->ComputeAvailSpaceRect();
  aState.spaceManager->Translate(-aState.borderPadding.left, 0);
  aState.x = aState.currentBand->availSpace.x;
}

void nsBlockFrame::ClearFloaters(nsBlockReflowState& aState, PRUint32 aClear)
{
  // Translate the coordinate space
  aState.spaceManager->Translate(aState.borderPadding.left, 0);

getBand:
  nscoord y = aState.y + aState.topMargin;
  PRBool  isLeftFloater = PR_FALSE;
  PRBool  isRightFloater = PR_FALSE;

  // Get a band of available space
  aState.spaceManager->GetBandData(y, aState.availSize, *aState.currentBand);

  // Scan the trapezoids looking for left and right floaters
  nsBandTrapezoid*  trapezoid = aState.currentBand->trapezoids;
  for (PRInt32 i = 0; i < aState.currentBand->count; i++) {
    // XXX Handle multiply occupied
    if (nsBandTrapezoid::smOccupied == trapezoid->state) {
      nsStyleDisplay* display;

      trapezoid->frame->GetStyleData(kStylePositionSID, (nsStyleStruct*&)display);
      if (NS_STYLE_FLOAT_LEFT == display->mFloats) {
        isLeftFloater = PR_TRUE;
      } else if (NS_STYLE_FLOAT_RIGHT == display->mFloats) {
        isRightFloater = PR_TRUE;
      }
    }

    trapezoid++;
  }

  if (isLeftFloater) {
    if ((aClear == NS_STYLE_CLEAR_LEFT) ||
        (aClear == NS_STYLE_CLEAR_LEFT_AND_RIGHT)) {
      aState.y += aState.currentBand->trapezoids[0].GetHeight();
      goto getBand;
    }
  }
  if (isRightFloater) {
    if ((aClear == NS_STYLE_CLEAR_RIGHT) ||
        (aClear == NS_STYLE_CLEAR_LEFT_AND_RIGHT)) {
      aState.y += aState.currentBand->trapezoids[0].GetHeight();
      goto getBand;
    }
  }

  aState.spaceManager->Translate(-aState.borderPadding.left, 0);
}

// Bit's for PlaceAndReflowChild return value
#define PLACE_FIT    0x1
#define PLACE_FLOWED 0x2

PRIntn
nsBlockFrame::PlaceAndReflowChild(nsIPresContext* aCX,
                                  nsBlockReflowState& aState,
                                  nsIFrame* aKidFrame,
                                  nsIStyleContext* aKidSC)
{
  nsSize kidMaxElementSize;
  nsSize* pKidMaxElementSize =
    (nsnull != aState.maxElementSize) ? &kidMaxElementSize : nsnull;

  // Get line start setup if we are at the start of a new line
  if (nsnull == aState.lineStart) {
    NS_ASSERTION(0 == aState.lineLength, "bad line length");
    aState.lineStart = aKidFrame;
  }

  // Get kid and its style
  nsStyleDisplay* styleDisplay = (nsStyleDisplay*)
    aKidSC->GetData(kStyleDisplaySID);

  // Figure out if kid is a block element or not
  PRBool isInline = PR_TRUE;
  PRIntn display = styleDisplay->mDisplay;
  if (aState.firstChildIsInsideBullet && (mFirstChild == aKidFrame)) {
    // XXX Special hack for properly reflowing bullets that have the
    // inside value for list-style-position.
    display = NS_STYLE_DISPLAY_INLINE;
  }
  if ((NS_STYLE_DISPLAY_BLOCK == display) ||
      (NS_STYLE_DISPLAY_LIST_ITEM == display)) {
    // Block elements always end up on the next line (unless they are
    // already at the start of the line).
    isInline = PR_FALSE;
    if (aState.lineLength > 0) {
      aState.breakAfterChild = PR_TRUE;
    }
  }

  // Handle forced break first
  if (aState.breakAfterChild) {
    NS_ASSERTION(aState.lineStart != aKidFrame, "bad line");

    // Get the last child in the current line
    nsIFrame* lastFrame = aState.lineStart;
    PRInt32   lineLen = aState.lineLength - 1;
    while (--lineLen >= 0) {
      lastFrame->GetNextSibling(lastFrame);
    }

    if (!AdvanceToNextLine(aCX, aState)) {
      // The previous line didn't fit.
      return 0;
    }
    aState.lineStart = aKidFrame;

    // Get the style for the last child, and see if it wanted to clear
    // floaters.  This handles the BR tag, which is the only inline
    // element for which clear applies
    nsIStyleContextPtr lastChildSC;
     
    lastFrame->GetStyleContext(aCX, lastChildSC.AssignRef());
    nsStyleDisplay* lastChildDisplay = (nsStyleDisplay*)
      lastChildSC->GetData(kStyleDisplaySID); 
    switch (lastChildDisplay->mBreakType) {
    case NS_STYLE_CLEAR_LEFT:
    case NS_STYLE_CLEAR_RIGHT:
    case NS_STYLE_CLEAR_LEFT_AND_RIGHT:
      ClearFloaters(aState, lastChildDisplay->mBreakType);
      break;
    }
  }

  // Now that we've handled force breaks (and maybe called AdvanceToNextLine()
  // which checks), remember whether it's an inline frame
  aState.isInline = isInline;

  // If we're at the beginning of a line then compute the top margin that we
  // should use
  if (aState.lineStart == aKidFrame) {
    // Compute the top margin to use for this line
    aState.topMargin = GetTopMarginFor(aCX, aState, aKidFrame, aKidSC,
                                       aState.isInline);

    // If it's an inline element then get a band of available space
    //
    // XXX If we have a current band and there's unused space in that band
    // then avoid this call to get a band...
    if (aState.isInline) {
      GetAvailableSpaceBand(aState, aState.y + aState.topMargin);
    }
  }

  // Compute the available space to reflow the child into and then
  // reflow it into that space.
  nsSize kidAvailSize;
  GetAvailSize(kidAvailSize, aState, aKidSC, aState.isInline);
  if ((aState.currentLineNumber > 0) && (kidAvailSize.height <= 0)) {
    // No more room
    return 0;
  }

  ReflowStatus    status;

  if (aState.isInline) {
    nsReflowMetrics kidSize;

    // Inline elements are never passed the space manager
    status = ReflowChild(aKidFrame, aCX, kidSize, kidAvailSize,
                         pKidMaxElementSize);

    // For first children, we skip all the fit checks because we must
    // fit at least one child for a parent to figure what to do with us.
    if ((aState.currentLineNumber > 0) || (aState.lineLength > 0)) {
      NS_ASSERTION(nsnull != aState.lineStart, "bad line start");

      if (aKidFrame == aState.lineStart) {
        // Width always fits when we are at the logical left margin.
        // Just check the height.
        //
        // XXX This height check isn't correct now that we have bands of
        // available space...
        if (kidSize.height > kidAvailSize.height) {
          // It's too tall
          return PLACE_FLOWED;
        }
      } else {
        // Examine state and if the breakBeforeChild is set and we
        // aren't already on the new line, do the forcing now.
        // XXX Why aren't we doing this check BEFORE we resize reflow the child?
        if (aState.breakBeforeChild) {
          aState.breakBeforeChild = PR_FALSE;
          if (aKidFrame != aState.lineStart) {
            if (!AdvanceToNextLine(aCX, aState)) {
              // Flushing out the line failed.
              return PLACE_FLOWED;
            }
            aState.lineStart = aKidFrame;

            // Get a band of available space
            GetAvailableSpaceBand(aState, aState.y + aState.topMargin);

            // Reflow child now that it has the line to itself
            GetAvailSize(kidAvailSize, aState, aKidSC, PR_TRUE);
            status = ReflowChild(aKidFrame, aCX, kidSize, kidAvailSize,
                                 pKidMaxElementSize);
          }
        }

        // When we are not at the logical left margin then we need
        // to check the width first. If we are too wide then advance
        // to the next line and try reflowing again.
        if (kidSize.width > kidAvailSize.width) {
          // Too wide. Try next line
          if (!AdvanceToNextLine(aCX, aState)) {
            // Flushing out the line failed.
            return PLACE_FLOWED;
          }
          aState.lineStart = aKidFrame;

          // Get a band of available space
          GetAvailableSpaceBand(aState, aState.y + aState.topMargin);

          // Reflow splittable children
          SplittableType  isSplittable;

          aKidFrame->IsSplittable(isSplittable);
          if (isSplittable != frNotSplittable) {
            // Update size info now that we are on the next line. Then
            // reflow the child into the new available space.
            GetAvailSize(kidAvailSize, aState, aKidSC, PR_TRUE);
            status = ReflowChild(aKidFrame, aCX, kidSize, kidAvailSize,
                                 pKidMaxElementSize);

            // If we just reflowed our last child then update the
            // mLastContentIsComplete state.
            nsIFrame* nextSibling;

            aKidFrame->GetNextSibling(nextSibling);
            if (nsnull == nextSibling) {
              // Use state from the reflow we just did
              mLastContentIsComplete = PRBool(status == frComplete);
            }
          }

          // XXX This height check isn't correct now that we have bands of
          // available space...
          if (kidSize.height > kidAvailSize.height) {
            // It's too tall on the next line
            return PLACE_FLOWED;
          }
          // It's ok if it's too wide on the next line.
        }
      }
    }

    // Add child to the line
    AddInlineChildToLine(aCX, aState, aKidFrame, kidSize,
                         pKidMaxElementSize, aKidSC);
  } else {
    nsRect  kidRect;

    // Does the block-level element want to clear any floaters that impact
    // it? Note that the clear property only applies to block-level elements
    // and the BR tag
    nsStyleDisplay* styleDisplay = (nsStyleDisplay*)
      aKidSC->GetData(kStyleDisplaySID); 
    switch (styleDisplay->mBreakType) {
    case NS_STYLE_CLEAR_LEFT:
    case NS_STYLE_CLEAR_RIGHT:
    case NS_STYLE_CLEAR_LEFT_AND_RIGHT:
      ClearFloaters(aState, styleDisplay->mBreakType);
      GetAvailSize(kidAvailSize, aState, aKidSC, PR_FALSE);
      if ((aState.currentLineNumber > 0) && (kidAvailSize.height <= 0)) {
        // No more room
        return 0;
      }
      break;
    }

    // Give the block its own local coordinate space.. Note: ReflowChild()
    // will adjust for the child's left/right margin after determining the
    // current left/right edge
    aState.spaceManager->Translate(aState.borderPadding.left, 0);
    // Give the block-level element the opportunity to use the space manager
    status = ReflowChild(aKidFrame, aCX, aState.spaceManager,
                         kidAvailSize, kidRect, pKidMaxElementSize);
    aState.spaceManager->Translate(-aState.borderPadding.left, 0);

    // For first children, we skip all the fit checks because we must
    // fit at least one child for a parent to figure what to do with us.
    if ((aState.currentLineNumber > 0) || (aState.lineLength > 0)) {
      // Block elements always fit horizontally (because they are
      // always placed at the logical left margin). Check to see if
      // the block fits vertically
      if (kidRect.YMost() > kidAvailSize.height) {
        // Nope
        return PLACE_FLOWED;
      }
    }

    // Add block child
    // XXX We need to set lastContentIsComplete here, because AddBlockChild()
    // calls AdvaneceToNextLine(). We need to restructure the flow of control,
    // and use a state machine...
    aState.lastContentIsComplete = PRBool(status == frComplete);
    AddBlockChild(aCX, aState, aKidFrame, kidRect, pKidMaxElementSize, aKidSC);
  }

  // If we just reflowed our last child then update the
  // mLastContentIsComplete state.
  nsIFrame* nextSibling;

  aKidFrame->GetNextSibling(nextSibling);
  if (nsnull == nextSibling) {
    // Use state from the reflow we just did
    mLastContentIsComplete = PRBool(status == frComplete);
  }

  aState.lastContentIsComplete = PRBool(status == frComplete);
  if (aState.isInline && (frNotComplete == status)) {
    // Since the inline child didn't complete its reflow we *know*
    // that a continuation of it can't possibly fit on the current
    // line. Therefore, set a flag in the state that will cause the
    // a line break before the next frame is placed.
    aState.breakAfterChild = PR_TRUE;
  }

  aState.reflowStatus = status;
  return PLACE_FLOWED | PLACE_FIT;
}

/**
 * Reflow the existing frames.
 *
 * @param   aCX presentation context to use
 * @param   aState <b>in out</b> parameter which tracks the state of
 *            reflow for the block frame.
 * @return  true if we successfully reflowed all the mapped children and false
 *            otherwise, e.g. we pushed children to the next in flow
 */
PRBool
nsBlockFrame::ReflowMappedChildren(nsIPresContext* aCX,
                                   nsBlockReflowState& aState)
{
#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif
#ifdef NOISY
  ListTag(stdout);
  printf(": reflow mapped (childCount=%d) [%d,%d,%c]\n",
         mChildCount,
         mFirstContentOffset, mLastContentOffset,
         (mLastContentIsComplete ? 'T' : 'F'));
  DumpFlow();
#endif

  PRBool result = PR_TRUE;
  nsIFrame* kidFrame;

  for (kidFrame = mFirstChild; nsnull != kidFrame; ) {

    /* we get the kid's style from kidFrame's cached style context */
    nsIStyleContextPtr kidSC; 
    kidFrame->GetStyleContext(aCX, kidSC.AssignRef()); 

    // Attempt to place and reflow the child

    // XXX if child is not splittable and it fits just place it where
    // it is, otherwise advance to the next line and place it there if
    // possible

    PRIntn placeStatus = PlaceAndReflowChild(aCX, aState, kidFrame, kidSC);
    ReflowStatus status = aState.reflowStatus;
    if (0 == (placeStatus & PLACE_FIT)) {
      // The child doesn't fit. Push it and any remaining children.
      PushKids(aState);
      result = PR_FALSE;
      goto push_done;
    }

    // Is the child complete?
    nsIFrame* kidNextInFlow;

    kidFrame->GetNextInFlow(kidNextInFlow);
    if (frComplete == status) {
      // Yes, the child is complete
      NS_ASSERTION(nsnull == kidNextInFlow, "bad child flow list");
    } else {
      // No the child isn't complete
      if (nsnull == kidNextInFlow) {
        // The child doesn't have a next-in-flow so create a continuing
        // frame. This hooks the child into the flow
        nsIFrame* continuingFrame;
         
        kidFrame->CreateContinuingFrame(aCX, this, continuingFrame);
        NS_ASSERTION(nsnull != continuingFrame, "frame creation failed");

        // Add the continuing frame to the sibling list
        nsIFrame* nextSib;
         
        kidFrame->GetNextSibling(nextSib);
        continuingFrame->SetNextSibling(nextSib);
        kidFrame->SetNextSibling(continuingFrame);
        mChildCount++;
      }

      // Unlike the inline frame code we can't assume that we used
      // up all of our space because the child's reflow status is
      // frNotComplete. Instead, the child is probably split and
      // we need to reflow the continuations as well.
    }

    // Get the next child frame
    kidFrame->GetNextSibling(kidFrame);
  }

 push_done:
#ifdef NS_DEBUG
  nsIFrame* lastChild;
  PRInt32   lastIndexInParent;

  LastChild(lastChild);
  lastChild->GetIndexInParent(lastIndexInParent);
  NS_POSTCONDITION(lastIndexInParent == mLastContentOffset, "bad last content offset");

  PRInt32 len = LengthOf(mFirstChild);
  NS_POSTCONDITION(len == mChildCount, "bad child count");
  VerifyLastIsComplete();
#endif

#ifdef NOISY
  ListTag(stdout);
  printf(": reflow mapped %sok (childCount=%d) [%d,%d,%c]\n",
         (result ? "" : "NOT "),
         mChildCount,
         mFirstContentOffset, mLastContentOffset,
         (mLastContentIsComplete ? 'T' : 'F'));
  DumpFlow();
#endif
  return result;
}

/* XXX:
 * In this method, we seem to be getting the style for the next child,
 * and checking that child's style for the child's display type. 
 * The problem is that it's very inefficient to ResolveStyleFor(kid) here 
 * and then just throw it away, when we end up doing a ResolveStyleFor(kid)
 * again when we actually create a frame for the content.
 * It would be nice to cache the resolved style, maybe in the reflow state?
 */
PRBool nsBlockFrame::MoreToReflow(nsIPresContext* aCX)
{
  PRBool rv = PR_FALSE;
  if (IsPseudoFrame()) {
    // Get the next content object that we would like to reflow
    PRInt32 kidIndex = NextChildOffset();
    nsIContentPtr kid = mContent->ChildAt(kidIndex);
    if (kid.IsNotNull()) {
      // Resolve style for the kid
      nsIStyleContextPtr kidSC = aCX->ResolveStyleContextFor(kid, this);
      nsStyleDisplay* kidStyleDisplay = (nsStyleDisplay*)
        kidSC->GetData(kStyleDisplaySID);
      switch (kidStyleDisplay->mDisplay) {
      case NS_STYLE_DISPLAY_BLOCK:
      case NS_STYLE_DISPLAY_LIST_ITEM:
        // Block pseudo-frames do not contain other block elements
        break;

      default:
        rv = PR_TRUE;
        break;
      }
    }
  } else {
    if (NextChildOffset() < mContent->ChildCount()) {
      rv = PR_TRUE;
    }
  }
  return rv;
}

/**
 * Create new frames for content we haven't yet mapped
 *
 * @param   aCX presentation context to use
 * @return  frComplete if all content has been mapped and frNotComplete
 *            if we should be continued
 */
nsIFrame::ReflowStatus
nsBlockFrame::ReflowAppendedChildren(nsIPresContext* aCX,
                                     nsBlockReflowState& aState)
{
#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif
  nsIFrame*    kidPrevInFlow = nsnull;
  ReflowStatus result = frNotComplete;

  // If we have no children and we have a prev-in-flow then we need to pick
  // up where it left off. If we have children, e.g. we're being resized, then
  // our content offset should already be set correctly...
  if ((nsnull == mFirstChild) && (nsnull != mPrevInFlow)) {
    nsBlockFrame* prev = (nsBlockFrame*) mPrevInFlow;
    NS_ASSERTION(prev->mLastContentOffset >= prev->mFirstContentOffset, "bad prevInFlow");
    mFirstContentOffset = prev->NextChildOffset();
    if (PR_FALSE == prev->mLastContentIsComplete) {
      // Our prev-in-flow's last child is not complete
      prev->LastChild(kidPrevInFlow);
    }
  }

  // Place our children, one at a time until we are out of children
  PRInt32 kidIndex = NextChildOffset();
  nsIFrame* kidFrame = nsnull;
  nsIFrame* prevKidFrame;
   
  LastChild(prevKidFrame);
  for (;;) {
    // Get the next content object
    nsIContentPtr kid = mContent->ChildAt(kidIndex);
    if (kid.IsNull()) {
      result = frComplete;
      break;
    }

    // Resolve style for the kid
    nsIStyleContextPtr kidSC = aCX->ResolveStyleContextFor(kid, this);
    nsStylePosition* kidPosition = (nsStylePosition*)
      kidSC->GetData(kStylePositionSID);
    nsStyleDisplay* kidDisplay = (nsStyleDisplay*)
      kidSC->GetData(kStyleDisplaySID);

    // Check whether it wants to floated or absolutely positioned
    if (NS_STYLE_POSITION_ABSOLUTE == kidPosition->mPosition) {
      AbsoluteFrame::NewFrame(&kidFrame, kid, kidIndex, this);
      kidFrame->SetStyleContext(kidSC);
    } else if (kidDisplay->mFloats != NS_STYLE_FLOAT_NONE) {
      PlaceholderFrame::NewFrame(&kidFrame, kid, kidIndex, this);
      kidFrame->SetStyleContext(kidSC);
    } else if (nsnull == kidPrevInFlow) {
      // Create initial frame for the child
      nsIContentDelegate* kidDel;
      switch (kidDisplay->mDisplay) {
      case NS_STYLE_DISPLAY_BLOCK:
      case NS_STYLE_DISPLAY_LIST_ITEM:
        // Pseudo block frames do not contain other block elements
        // unless the block element would be the first child.
        if (IsPseudoFrame()) {
          // If we're being used as a pseudo frame, i.e. we map the same
          // content as our parent then we want to indicate we're complete;
          // otherwise we'll be continued and go on mapping children...

          // It better be true that we are not being asked to flow a
          // block element as our first child. That means the body
          // decided it needed a pseudo-frame when it shouldn't have.
          NS_ASSERTION(nsnull != mFirstChild, "bad body");

          result = frComplete;
          goto done;
        }
        // FALL THROUGH (and create frame)

      case NS_STYLE_DISPLAY_INLINE:
        kidDel = kid->GetDelegate(aCX);
        kidFrame = kidDel->CreateFrame(aCX, kid, kidIndex, this);
        NS_RELEASE(kidDel);
        break;

      default:
        NS_ASSERTION(nsnull == kidPrevInFlow, "bad prev in flow");
        nsFrame::NewFrame(&kidFrame, kid, kidIndex, this);
        break;
      }
      kidFrame->SetStyleContext(kidSC);
    } else {
      // Since kid has a prev-in-flow, use that to create the next
      // frame.
      kidPrevInFlow->CreateContinuingFrame(aCX, this, kidFrame);
    }

    // Link child frame into the list of children. If the frame ends
    // up not fitting and getting pushed, the PushKids code will fixup
    // the child count for us.
    if (nsnull != prevKidFrame) {
#ifdef NS_DEBUG
      nsIFrame* nextSibling;

      prevKidFrame->GetNextSibling(nextSibling);
      NS_ASSERTION(nsnull == nextSibling, "bad append");
#endif
      prevKidFrame->SetNextSibling(kidFrame);
    } else {
      NS_ASSERTION(nsnull == mFirstChild, "bad create");
      mFirstChild = kidFrame;
      SetFirstContentOffset(kidFrame);
    }
    prevKidFrame = kidFrame;
    mChildCount++;

    // Reflow child frame as many times as necessary until it is
    // complete.
    ReflowStatus status;
    do {
      PRIntn placeStatus = PlaceAndReflowChild(aCX, aState, kidFrame, kidSC);
      status = aState.reflowStatus;
      if (0 == (placeStatus & PLACE_FIT)) {
        // We ran out of room.
        nsIFrame* kidNextInFlow;

        kidFrame->GetNextInFlow(kidNextInFlow);
        mLastContentIsComplete = PRBool(nsnull == kidNextInFlow);
        PushKids(aState);

        goto push_done;
      }

      // Did the child complete?
      prevKidFrame = kidFrame;
      if (frNotComplete == status) {
        // Child didn't complete so create a continuing frame
        kidPrevInFlow = kidFrame;
        nsIFrame* continuingFrame;
         
        kidFrame->CreateContinuingFrame(aCX, this, continuingFrame);

        // Add the continuing frame to the sibling list
        nsIFrame* kidNextSibling;

        kidFrame->GetNextSibling(kidNextSibling);
        continuingFrame->SetNextSibling(kidNextSibling);
        kidFrame->SetNextSibling(continuingFrame);
        kidFrame = continuingFrame;
        mChildCount++;

        // Switch to new kid style
        kidFrame->GetStyleContext(aCX, kidSC.AssignRef());
      }
#ifdef NS_DEBUG
      nsIFrame* kidNextInFlow;

      kidFrame->GetNextInFlow(kidNextInFlow);
      NS_ASSERTION(nsnull == kidNextInFlow, "huh?");
#endif
    } while (frNotComplete == status);

    // The child that we just reflowed is complete
#ifdef NS_DEBUG
    nsIFrame* kidNextInFlow;

    kidFrame->GetNextInFlow(kidNextInFlow);
    NS_ASSERTION(nsnull == kidNextInFlow, "bad child flow list");
#endif
    kidIndex++;
    kidPrevInFlow = nsnull;
  }

 done:
  // To get here we either completely reflowed all our appended
  // children OR we are a pseudo-frame and we ran into a block
  // element. In either case our last content MUST be complete.
  NS_ASSERTION(PR_TRUE == aState.lastContentIsComplete, "bad state");
  NS_ASSERTION(IsLastChild(prevKidFrame), "bad last child");
  SetLastContentOffset(prevKidFrame);

 push_done:
#ifdef NS_DEBUG
  PRInt32 len = LengthOf(mFirstChild);
  NS_ASSERTION(len == mChildCount, "bad child count");
  VerifyLastIsComplete();
#endif
  return result;
}

/**
 * Pullup frames from our next in flow and try to place them. Before
 * this is called our previously mapped children, if any have been
 * reflowed which means that the block reflow state's x and y
 * coordinates and other data are ready to go.
 *
 * Return true if we pulled everything up.
 */
PRBool
nsBlockFrame::PullUpChildren(nsIPresContext* aCX,
                             nsBlockReflowState& aState)
{
#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif
#ifdef NOISY
  ListTag(stdout);
  printf(": pullup (childCount=%d) [%d,%d,%c]\n",
         mChildCount,
         mFirstContentOffset, mLastContentOffset,
         (mLastContentIsComplete ? 'T' : 'F'));
  DumpFlow();
#endif

  PRBool result = PR_TRUE;
  nsBlockFrame* nextInFlow = (nsBlockFrame*) mNextInFlow;
  nsIFrame* prevKidFrame;
   
  LastChild(prevKidFrame);
  while (nsnull != nextInFlow) {
    // Get first available frame from the next-in-flow
    nsIFrame* kidFrame = PullUpOneChild(nextInFlow, prevKidFrame);
    if (nsnull == kidFrame) {
      // We've pulled up all the children from that next-in-flow, so
      // move to the next next-in-flow.
      nextInFlow = (nsBlockFrame*) nextInFlow->mNextInFlow;
      continue;
    }

    // Get style information for the pulled up kid
    nsIContentPtr kid;
     
    kidFrame->GetContent(kid.AssignRef());
    nsIStyleContextPtr kidSC = aCX->ResolveStyleContextFor(kid, this);

    ReflowStatus status;
    do {
      PRIntn placeStatus = PlaceAndReflowChild(aCX, aState, kidFrame, kidSC);
      status = aState.reflowStatus;
      if (0 == (placeStatus & PLACE_FIT)) {
        // Push the kids that didn't fit back down to the next-in-flow
        nsIFrame* kidNextInFlow;

        kidFrame->GetNextInFlow(kidNextInFlow);
        mLastContentIsComplete = PRBool(nsnull == kidNextInFlow);
        PushKids(aState);

        result = PR_FALSE;
        goto push_done;
      }

      if (frNotComplete == status) {
        // Child is not complete
        nsIFrame* kidNextInFlow;
         
        kidFrame->GetNextInFlow(kidNextInFlow);
        if (nsnull == kidNextInFlow) {
          // Create a continuing frame for the incomplete child
          nsIFrame* continuingFrame;
           
          kidFrame->CreateContinuingFrame(aCX, this, continuingFrame);

          // Add the continuing frame to our sibling list.
          nsIFrame* nextSibling;

          kidFrame->GetNextSibling(nextSibling);
          continuingFrame->SetNextSibling(nextSibling);
          kidFrame->SetNextSibling(continuingFrame);
          prevKidFrame = kidFrame;
          kidFrame = continuingFrame;
          mChildCount++;

          // Switch to new kid style
          kidFrame->GetStyleContext(aCX, kidSC.AssignRef());
        } else {
          // The child has a next-in-flow, but it's not one of ours.
          // It *must* be in one of our next-in-flows. Collect it
          // then.
          NS_ASSERTION(!IsChild(kidNextInFlow), "busted kid next-in-flow");
          break;
        }
      }
    } while (frNotComplete == status);

    prevKidFrame = kidFrame;
  }

  if (nsnull != prevKidFrame) {
    // The only way we can get here is by pulling up every last child
    // in our next-in-flows (and reflowing any continunations they
    // have). Therefore we KNOW that our last child is complete.
    NS_ASSERTION(PR_TRUE == aState.lastContentIsComplete, "bad state");
    NS_ASSERTION(IsLastChild(prevKidFrame), "bad last child");
    SetLastContentOffset(prevKidFrame);
  }

 push_done:;

  if (result == PR_FALSE) {
    // If our next-in-flow is empty OR our next next-in-flow is empty
    // then adjust the offsets of all of the empty next-in-flows.
    nextInFlow = (nsBlockFrame*) mNextInFlow;
    if ((0 == nextInFlow->mChildCount) ||
        ((nsnull != nextInFlow->mNextInFlow) &&
         (0 == ((nsBlockFrame*)(nextInFlow->mNextInFlow))->mChildCount))) {
      // We didn't pullup everything and we need to fixup one of our
      // next-in-flows content offsets.
      AdjustOffsetOfEmptyNextInFlows();
    }
  }


#ifdef NS_DEBUG
  PRInt32 len = LengthOf(mFirstChild);
  NS_ASSERTION(len == mChildCount, "bad child count");
  VerifyLastIsComplete();
#endif
#ifdef NOISY
  ListTag(stdout);
  printf(": pullup %sok (childCount=%d) [%d,%d,%c]\n",
         (result ? "" : "NOT "),
         mChildCount,
         mFirstContentOffset, mLastContentOffset,
         (mLastContentIsComplete ? 'T' : 'F'));
  DumpFlow();
#endif
  return result;
}

void nsBlockFrame::SetupState(nsIPresContext* aCX,
                              nsBlockReflowState& aState,
                              const nsSize& aMaxSize,
                              nsSize* aMaxElementSize,
                              nsISpaceManager* aSpaceManager)
{
  // Setup reflow state
  aState.Init(aMaxSize, aMaxElementSize, mStyleContext, aSpaceManager);

  nsStyleSpacing* mySpacing = (nsStyleSpacing*)
    mStyleContext->GetData(kStyleSpacingSID);

  // Apply border and padding adjustments for regular frames only
  if (PR_FALSE == IsPseudoFrame()) {
    aState.borderPadding = mySpacing->mBorderPadding;
    aState.y = mySpacing->mBorderPadding.top;
    aState.availSize.width -=
      (mySpacing->mBorderPadding.left + mySpacing->mBorderPadding.right);
    aState.availSize.height -=
      (mySpacing->mBorderPadding.top + mySpacing->mBorderPadding.bottom);
  } else {
    aState.borderPadding.SizeTo(0, 0, 0, 0);
  }

  // Setup initial list ordinal value
  nsIAtom* tag = mContent->GetTag();
  if ((tag == nsHTMLAtoms::ul) || (tag == nsHTMLAtoms::ol) ||
      (tag == nsHTMLAtoms::menu) || (tag == nsHTMLAtoms::dir)) {
    nsHTMLValue value;
    if (eContentAttr_HasValue ==
        ((nsIHTMLContent*)mContent)->GetAttribute(nsHTMLAtoms::start, value)) {
      if (eHTMLUnit_Absolute == value.GetUnit()) {
        aState.nextListOrdinal = value.GetIntValue();
      }
    }
  }
  NS_RELEASE(tag);

  mCurrentState = &aState;
}

#include "nsUnitConversion.h"/* XXX */
NS_METHOD nsBlockFrame::ResizeReflow(nsIPresContext* aCX,
                                     nsISpaceManager* aSpaceManager,
                                     const nsSize& aMaxSize,
                                     nsRect& aDesiredRect,
                                     nsSize* aMaxElementSize,
                                     ReflowStatus& aStatus)
{
  nsBlockReflowState state;
  SetupState(aCX, state, aMaxSize, aMaxElementSize, aSpaceManager);
  return DoResizeReflow(aCX, state, aDesiredRect, aStatus);
}

nsresult nsBlockFrame::DoResizeReflow(nsIPresContext* aCX,
                                      nsBlockReflowState& aState,
                                      nsRect& aDesiredRect,
                                      ReflowStatus& aStatus)
{
#ifdef NS_DEBUG
  PreReflowCheck();
#endif
#ifdef NOISY
  ListTag(stdout);
  printf(": resize reflow %g,%g\n",
         NS_TWIPS_TO_POINTS_FLOAT(aState.availSize.width),
         NS_TWIPS_TO_POINTS_FLOAT(aState.availSize.height));
  DumpFlow();
#endif

  // Zap old line data
  if (nsnull != mLines) {
    delete mLines;
    mLines = nsnull;
  }
  mNumLines = 0;

  // Check for an overflow list
  MoveOverflowToChildList();

  // Before we start reflowing, cache a pointer to our state structure
  // so that inline frames can find it.
  nsIPresShell* shell = aCX->GetShell();
  shell->PutCachedData(this, &aState);

  // First reflow any existing frames
  PRBool reflowMappedOK = PR_TRUE;
  aStatus = frComplete;
  if (nsnull != mFirstChild) {
    reflowMappedOK = ReflowMappedChildren(aCX, aState);
    if (!reflowMappedOK) {
      aStatus = frNotComplete;
    }
  }

  if (reflowMappedOK) {
    // Any space left?
    nscoord yb = aState.borderPadding.top + aState.availSize.height;
    if ((nsnull != mFirstChild) && (aState.y >= yb)) {
      // No space left. Don't try to pull-up children or reflow
      // unmapped.  We need to return the correct completion status,
      // so see if there is more to reflow.
      if (MoreToReflow(aCX)) {
        aStatus = frNotComplete;
      }
    } else if (MoreToReflow(aCX)) {
      // Try and pull-up some children from a next-in-flow
      if ((nsnull == mNextInFlow) || PullUpChildren(aCX, aState)) {
        // If we still have unmapped children then create some new frames
        if (MoreToReflow(aCX)) {
          aStatus = ReflowAppendedChildren(aCX, aState);
        }
      } else {
        // We were unable to pull-up all the existing frames from the
        // next in flow
        aStatus = frNotComplete;
      }
    }
  }

  // Deal with last line - make sure it gets vertically and
  // horizontally aligned. This also updates the state's y coordinate
  // which is good because that's how we size ourselves.
  if (0 != aState.lineLength) {
    if (!AdvanceToNextLine(aCX, aState)) {
      // The last line of output doesn't fit. Push all of the kids to
      // the next in flow and change our reflow status to not complete
      // so that we are continued.
#ifdef NOISY
      ListTag(stdout);
      printf(": pushing kids since last line doesn't fit\n");
#endif

      PushKids(aState);
      aStatus = frNotComplete;
    }
  }

  if (frComplete == aStatus) {
    // Don't forget to add in the bottom margin from our last child.
    // Only add it in if there is room for it.
    nscoord margin = aState.prevMaxPosBottomMargin -
      aState.prevMaxNegBottomMargin;
    nscoord y = aState.y + margin;
    if (y <= aState.borderPadding.top + aState.availSize.height) {
      aState.y = y;
    }
  }

  // Now that reflow has finished, remove the cached pointer
  shell->RemoveCachedData(this);
  NS_RELEASE(shell);

  // Translate state.lineLengths into an integer array
  mNumLines = aState.lineLengths.Count();
  if (mNumLines > 0) {
    mLines = new PRInt32[mNumLines];
    for (PRInt32 i = 0; i < mNumLines; i++) {
      PRInt32 ll = (PRInt32) aState.lineLengths.ElementAt(i);
      mLines[i] = ll;
    }
  }

  if (!aState.unconstrainedWidth && aState.justifying) {
    // Perform justification now that we know how many lines we have.
    JustifyLines(aCX, aState);
  }

  // Return our desired rect and our status
  aDesiredRect.x = 0;
  aDesiredRect.y = 0;
  aDesiredRect.width = aState.kidXMost + aState.borderPadding.right;
  if (!aState.unconstrainedWidth) {
    // Make sure we're at least as wide as the max size we were given
    nscoord maxWidth = aState.availSize.width + aState.borderPadding.left +
                       aState.borderPadding.right;

    if (aDesiredRect.width < maxWidth) {
      aDesiredRect.width = maxWidth;
    }
  }
  aState.y += aState.borderPadding.bottom;
  aDesiredRect.height = aState.y;

#ifdef NS_DEBUG
  PostReflowCheck(aStatus);
#endif
#ifdef NOISY
  ListTag(stdout);
  printf(": resize reflow %g,%g %scomplete [%d,%d,%c]\n",
         NS_TWIPS_TO_POINTS_FLOAT(aState.availSize.width),
         NS_TWIPS_TO_POINTS_FLOAT(aState.availSize.height),
         ((status == frNotComplete) ? "NOT " : ""),
         mFirstContentOffset, mLastContentOffset,
         (mLastContentIsComplete ? 'T' : 'F')
    );
  DumpFlow();
#endif
  mCurrentState = nsnull;
  return NS_OK;
}

void nsBlockFrame::JustifyLines(nsIPresContext* aCX,
                                nsBlockReflowState& aState)
{
  // XXX we don't justify the last line; what if we are continued,
  // should we do it then?
  nsIFrame* kid = mFirstChild;
  for (PRInt32 i = 0; i < mNumLines; i++) {
    nsIFrame* lineStart = kid;
    PRInt32 lineLength = mLines[i];
    if (i < mNumLines - 1) {
      if (1 == lineLength) {
        // For lines with one element on them we can take a shortcut and
        // let them do the justification. Note that we still call
        // JustifyReflow even if the available space is zero in case the
        // child is a hunk of text that ends in whitespace. The whitespace
        // will be distributed elsewhere causing a proper flush right look
        // for the last word.
        nsRect r;
        kid->GetRect(r);
        nscoord maxWidth = aState.availSize.width;
        nscoord availableSpace = maxWidth - r.width;
        nscoord maxAvailSpace = nscoord(maxWidth * 0.1f);
        if ((availableSpace >= 0) && (availableSpace < maxAvailSpace)) {
          kid->JustifyReflow(aCX, availableSpace);
          kid->SizeTo(r.width + availableSpace, r.height);
        }
        kid->GetNextSibling(kid);
      } else {
        // XXX Get justification of multiple elements working
        while (--lineLength >= 0) {
          kid->GetNextSibling(kid);
        }
      }
    }
    
    // Finally, now that the in-flow positions of the line's frames are
    // known we can apply relative positioning if any of them need it.
    nsCSSLayout::RelativePositionChildren(aCX, this, lineStart, mLines[i]);
  }
}

NS_METHOD nsBlockFrame::IsSplittable(SplittableType& aIsSplittable) const
{
  aIsSplittable = frSplittableNonRectangular;
  return NS_OK;
}

NS_METHOD nsBlockFrame::CreateContinuingFrame(nsIPresContext* aCX,
                                              nsIFrame* aParent,
                                              nsIFrame*& aContinuingFrame)
{
  nsBlockFrame* cf = new nsBlockFrame(mContent, mIndexInParent, aParent);
  PrepareContinuingFrame(aCX, aParent, cf);
  aContinuingFrame = cf;
  return NS_OK;
}

NS_METHOD nsBlockFrame::IncrementalReflow(nsIPresContext* aCX,
                                          nsISpaceManager* aSpaceManager,
                                          const nsSize& aMaxSize,
                                          nsRect& aDesiredRect,
                                          nsReflowCommand& aReflowCommand,
                                          ReflowStatus& aStatus)
{
  aStatus = frComplete;

  if (aReflowCommand.GetTarget() == this) {
    // XXX for now, just do a complete reflow mapped (it'll kinda
    // work, but it's slow)

    nsBlockReflowState state;
    SetupState(aCX, state, aMaxSize, nsnull, aSpaceManager);
    PRBool reflowMappedOK = ReflowMappedChildren(aCX, state);
    if (!reflowMappedOK) {
      aStatus = frNotComplete;
    }
  } else {
    // XXX not yet implemented
    NS_ABORT();
    // XXX work to compute initial state goes *HERE*
    aDesiredRect.x = 0;
    aDesiredRect.y = 0;
    aDesiredRect.width = 0;
    aDesiredRect.height = 0;
  }

  mCurrentState = nsnull;
  return NS_OK;
}

PRBool nsBlockFrame::IsLeftMostChild(nsIFrame* aFrame)
{
  do {
    nsIFrame* parent;
     
    aFrame->GetGeometricParent(parent);
  
    // See if there are any non-zero sized child frames that precede aFrame
    // in the child list
    nsIFrame* child;
     
    parent->FirstChild(child);
    while ((nsnull != child) && (aFrame != child)) {
      nsSize  size;

      // Is the child zero-sized?
      child->GetSize(size);
      if ((size.width > 0) || (size.height > 0)) {
        // We found a non-zero sized child frame that precedes aFrame
        return PR_FALSE;
      }
  
      child->GetNextSibling(child);
    }
  
    // aFrame is the left-most non-zero sized frame in its geometric parent.
    // Walk up one level and check that its parent is left-most as well
    aFrame = parent;
  } while (aFrame != this);

  return PR_TRUE;
}

PRBool nsBlockFrame::AddFloater(nsIPresContext* aCX,
                                nsIFrame* aFloater,
                                PlaceholderFrame* aPlaceholder)
{
  // Get the frame associated with the space manager, and get its nsIAnchoredItems
  // interface
  nsIFrame*          frame = mCurrentState->spaceManager->GetFrame();
  nsIAnchoredItems*  anchoredItems = nsnull;

  frame->QueryInterface(kIAnchoredItemsIID, (void**)&anchoredItems);
  NS_ASSERTION(nsnull != anchoredItems, "no anchored items interface");

  if (nsnull != anchoredItems) {
    anchoredItems->AddAnchoredItem(aFloater, nsIAnchoredItems::anHTMLFloater, this);
    PlaceFloater(aCX, aFloater, aPlaceholder);
    return PR_TRUE;
  }

  return PR_FALSE;
}

// XXX The size of the floater needs to be taken into consideration if we're
// computing a maximum element size
void nsBlockFrame::PlaceFloater(nsIPresContext* aCX,
                                nsIFrame* aFloater,
                                PlaceholderFrame* aPlaceholder)
{
  // If the floater is the left-most non-zero size child frame then insert
  // it before the current line; otherwise add it to the below-current-line
  // todo list and we'll handle it when we flush out the line
  if (IsLeftMostChild(aPlaceholder)) {
    // Get the type of floater
    nsIStyleContextPtr  styleContext;

    aFloater->GetStyleContext(aCX, styleContext.AssignRef());
    nsStyleDisplay* floaterDisplay = (nsStyleDisplay*)
      styleContext->GetData(kStyleDisplaySID);

    if (!mCurrentState->isInline) {
      // Get the current band for this line
      GetAvailableSpaceBand(*mCurrentState, mCurrentState->y + mCurrentState->topMargin);
    }
  
    // Commit some space in the space manager
    nsRect  region;

    aFloater->GetRect(region);
    region.y = mCurrentState->currentBand->availSpace.y;

    if (NS_STYLE_FLOAT_LEFT == floaterDisplay->mFloats) {
      region.x = mCurrentState->currentBand->availSpace.x;
    } else {
      NS_ASSERTION(NS_STYLE_FLOAT_RIGHT == floaterDisplay->mFloats, "bad float type");
      region.x = mCurrentState->currentBand->availSpace.XMost() - region.width;
    }

    // XXX Don't forget the floater's margins...
    mCurrentState->spaceManager->Translate(mCurrentState->borderPadding.left, 0);
    mCurrentState->spaceManager->AddRectRegion(region, aFloater);

    // Set the origin of the floater in world coordinates
    nscoord worldX, worldY;

    mCurrentState->spaceManager->GetTranslation(worldX, worldY);
    aFloater->MoveTo(region.x + worldX, region.y + worldY);
    mCurrentState->spaceManager->Translate(-mCurrentState->borderPadding.left, 0);

    // Update the band of available space to reflect space taken up by the floater
    GetAvailableSpaceBand(*mCurrentState, mCurrentState->y + mCurrentState->topMargin);
  } else {
    // Add the floater to our to-do list
    mCurrentState->floaterToDo.AppendElement(aFloater);
  }
}

NS_METHOD nsBlockFrame::ContentAppended(nsIPresShell* aShell,
                                        nsIPresContext* aPresContext,
                                        nsIContent* aContainer)
{
  // Get the last-in-flow
  nsBlockFrame* flow = (nsBlockFrame*)GetLastInFlow();
  PRInt32 kidIndex = flow->NextChildOffset();
  PRInt32 startIndex = kidIndex;

#if 0
  nsIFrame* kidFrame = nsnull;
  nsIFrame* prevKidFrame;
   
  flow->LastChild(prevKidFrame);
  for (;;) {
    // Get the next content object
    nsIContentPtr kid = mContent->ChildAt(kidIndex);
    if (nsnull == kid) {
      break;
    }

    // Resolve style for the kid
    nsIStyleContextPtr  kidSC = aPresContext->ResolveStyleContextFor(kid, this);
    nsStyleDisplay* kidDisplay = (nsStyleDisplay*)
      kidSC->GetData(kStyleDisplaySID);

    // Is it a floater?
    if (kidDisplay->mFloats != NS_STYLE_FLOAT_NONE) {
      PlaceholderFrame::NewFrame(&kidFrame, kid, kidIndex, this);
    } else {
      // Create initial frame for the child
      nsIContentDelegate* kidDel;
      nsresult fr;
      switch (kidDisplay->mDisplay) {
      case NS_STYLE_DISPLAY_BLOCK:
      case NS_STYLE_DISPLAY_LIST_ITEM:
        // Pseudo block frames do not contain other block elements
        // unless the block element would be the first child.
        if (IsPseudoFrame()) {
          // Update last content offset now that we are done drawing
          // children from our parent.
          SetLastContentOffset(prevKidFrame);
  
          // It better be true that we are not being asked to flow a
          // block element as our first child. That means the body
          // decided it needed a pseudo-frame when it shouldn't have.
          NS_ASSERTION(nsnull != mFirstChild, "bad body");
  
          return NS_OK;
        }
        // FALL THROUGH (and create frame)
  
      case NS_STYLE_DISPLAY_INLINE:
        kidDel = kid->GetDelegate(aPresContext);
        kidFrame = kidDel->CreateFrame(aPresContext, kid, kidIndex, this);
        NS_RELEASE(kidDel);
        break;
  
      default:
        fr = nsFrame::NewFrame(&kidFrame, kid, kidIndex, this);
        break;
      }
    }
    kidFrame->SetStyleContext(kidSC);

    // Link child frame into the list of children
    if (nsnull != prevKidFrame) {
#ifdef NS_DEBUG
      nsIFrame* nextSibling;

      prevKidFrame->GetNextSibling(nextSibling);
      NS_ASSERTION(nsnull == nextSibling, "bad append");
#endif
      prevKidFrame->SetNextSibling(kidFrame);
    } else {
      NS_ASSERTION(nsnull == mFirstChild, "bad create");
      mFirstChild = kidFrame;
      SetFirstContentOffset(kidFrame);
    }
    prevKidFrame = kidFrame;
    kidIndex++;
    mChildCount++;
  }
  SetLastContentOffset(prevKidFrame);
#endif

  // If this is a pseudo-frame then our parent will generate the
  // reflow command. Otherwise, if the container is us then we should
  // generate the reflow command because we were directly called.
  if (!IsPseudoFrame() && (aContainer == mContent)) {
    nsReflowCommand* rc =
      new nsReflowCommand(aPresContext, flow, nsReflowCommand::FrameAppended,
                          startIndex);
    aShell->AppendReflowCommand(rc);
  }
  return NS_OK;
}

PRIntn nsBlockFrame::GetSkipSides() const
{
  PRIntn skip = 0;
  if (nsnull != mPrevInFlow) {
    skip |= 1 << NS_SIDE_TOP;
  }
  if (nsnull != mNextInFlow) {
    skip |= 1 << NS_SIDE_BOTTOM;
  }
  return skip;
}

nsHTMLFrameType nsBlockFrame::GetFrameType() const
{
  return eHTMLFrame_Block;
}

NS_METHOD nsBlockFrame::ListTag(FILE* out) const
{
  if ((nsnull != mGeometricParent) && IsPseudoFrame()) {
    fprintf(out, "*block<");
    nsIAtom* atom = mContent->GetTag();
    if (nsnull != atom) {
      nsAutoString tmp;
      atom->ToString(tmp);
      fputs(tmp, out);
    }
    fprintf(out, ">(%d)@%p", mIndexInParent, this);
  } else {
    nsHTMLContainerFrame::ListTag(out);
  }
  return NS_OK;
}

#ifdef NS_DEBUG
void nsBlockFrame::DumpFlow() const
{
#ifdef NOISY_FLOW
  nsBlockFrame* flow = (nsBlockFrame*) mNextInFlow;
  while (flow != 0) {
    printf("  %p: [%d,%d,%c]\n",
           flow, flow->mFirstContentOffset, flow->mLastContentOffset,
           (flow->mLastContentIsComplete ? 'T' : 'F'));
    flow = (nsBlockFrame*) flow->mNextInFlow;
  }
#endif
}
#endif
