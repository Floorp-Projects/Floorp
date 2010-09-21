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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Uri Bernstein <uriber@gmail.com>
 *   Haamed Gheibi <gheibi@metanetworking.com>
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com>
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

#ifdef IBMBIDI

#include "nsBidiPresUtils.h"
#include "nsTextFragment.h"
#include "nsGkAtoms.h"
#include "nsPresContext.h"
#include "nsIRenderingContext.h"
#include "nsIServiceManager.h"
#include "nsFrameManager.h"
#include "nsBidiFrames.h"
#include "nsBidiUtils.h"
#include "nsCSSFrameConstructor.h"
#include "nsHTMLContainerFrame.h"
#include "nsInlineFrame.h"
#include "nsPlaceholderFrame.h"
#include "nsContainerFrame.h"
#include "nsFirstLetterFrame.h"

using namespace mozilla;

static const PRUnichar kSpace            = 0x0020;
static const PRUnichar kLineSeparator    = 0x2028;
static const PRUnichar kObjectSubstitute = 0xFFFC;
static const PRUnichar kLRE              = 0x202A;
static const PRUnichar kRLE              = 0x202B;
static const PRUnichar kLRO              = 0x202D;
static const PRUnichar kRLO              = 0x202E;
static const PRUnichar kPDF              = 0x202C;
static const PRUnichar ALEF              = 0x05D0;

#define CHAR_IS_HEBREW(c) ((0x0590 <= (c)) && ((c)<= 0x05FF))
// Note: The above code are moved from gfx/src/windows/nsRenderingContextWin.cpp

nsIFrame*
NS_NewDirectionalFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, PRUnichar aChar);

nsBidiPresUtils::nsBidiPresUtils() : mArraySize(8),
                                     mIndexMap(nsnull),
                                     mLevels(nsnull),
                                     mSuccess(NS_ERROR_FAILURE),
                                     mBidiEngine(nsnull)
{
  mBidiEngine = new nsBidi();
  if (mBidiEngine && mContentToFrameIndex.Init()) {
    mSuccess = NS_OK;
  }
}

nsBidiPresUtils::~nsBidiPresUtils()
{
  if (mLevels) {
    delete[] mLevels;
  }
  if (mIndexMap) {
    delete[] mIndexMap;
  }
  delete mBidiEngine;
}

PRBool
nsBidiPresUtils::IsSuccessful() const
{ 
  return NS_SUCCEEDED(mSuccess); 
}

/* Some helper methods for Resolve() */

// Should this frame be split between text runs?
PRBool
IsBidiSplittable(nsIFrame* aFrame) {
  nsIAtom* frameType = aFrame->GetType();
  // Bidi inline containers should be split, unless they're line frames.
  return aFrame->IsFrameOfType(nsIFrame::eBidiInlineContainer)
    && frameType != nsGkAtoms::lineFrame;
}

static nsresult
SplitInlineAncestors(nsIFrame*     aFrame)
{
  nsPresContext *presContext = aFrame->PresContext();
  nsIPresShell *presShell = presContext->PresShell();
  nsIFrame* frame = aFrame;
  nsIFrame* parent = aFrame->GetParent();
  nsIFrame* newParent;

  while (IsBidiSplittable(parent)) {
    nsIFrame* grandparent = parent->GetParent();
    NS_ASSERTION(grandparent, "Couldn't get parent's parent in nsBidiPresUtils::SplitInlineAncestors");
    
    nsresult rv = presShell->FrameConstructor()->
      CreateContinuingFrame(presContext, parent, grandparent, &newParent, PR_FALSE);
    if (NS_FAILED(rv)) {
      return rv;
    }
    
    // Split the child list after |frame|.
    nsContainerFrame* container = do_QueryFrame(parent);
    nsFrameList tail = container->StealFramesAfter(frame);

    // Reparent views as necessary
    rv = nsHTMLContainerFrame::ReparentFrameViewList(presContext, tail, parent, newParent);
    if (NS_FAILED(rv)) {
      return rv;
    }
    
    // The parent's continuation adopts the siblings after the split.
    rv = newParent->InsertFrames(nsGkAtoms::nextBidi, nsnull, tail);
    if (NS_FAILED(rv)) {
      return rv;
    }
    // The list name nsGkAtoms::nextBidi would indicate we don't want reflow
    nsFrameList temp(newParent, newParent);
    rv = grandparent->InsertFrames(nsGkAtoms::nextBidi, parent, temp);
    if (NS_FAILED(rv)) {
      return rv;
    }
    
    frame = parent;
    parent = grandparent;
  }
  
  return NS_OK;
}

// Convert bidi continuations to fluid continuations for a frame and all of its
// inline ancestors.
static void
JoinInlineAncestors(nsIFrame* aFrame)
{
  nsIFrame* frame = aFrame;
  while (frame && IsBidiSplittable(frame)) {
    nsIFrame* next = frame->GetNextContinuation();
    if (next) {
      NS_ASSERTION (!frame->GetNextInFlow() || frame->GetNextInFlow() == next, 
                    "next-in-flow is not next continuation!");
      frame->SetNextInFlow(next);

      NS_ASSERTION (!next->GetPrevInFlow() || next->GetPrevInFlow() == frame,
                    "prev-in-flow is not prev continuation!");
      next->SetPrevInFlow(frame);
    }
    // Join the parent only as long as we're its last child.
    if (frame->GetNextSibling())
      break;
    frame = frame->GetParent();
  }
}

static nsresult
CreateBidiContinuation(nsIFrame*       aFrame,
                       nsIFrame**      aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  NS_PRECONDITION(aFrame, "null ptr");

  *aNewFrame = nsnull;

  nsPresContext *presContext = aFrame->PresContext();
  nsIPresShell *presShell = presContext->PresShell();
  NS_ASSERTION(presShell, "PresShell must be set on PresContext before calling nsBidiPresUtils::CreateBidiContinuation");

  nsIFrame* parent = aFrame->GetParent();
  NS_ASSERTION(parent, "Couldn't get frame parent in nsBidiPresUtils::CreateBidiContinuation");

  nsresult rv = NS_OK;
  
  // Have to special case floating first letter frames because the continuation
  // doesn't go in the first letter frame. The continuation goes with the rest
  // of the text that the first letter frame was made out of.
  if (parent->GetType() == nsGkAtoms::letterFrame &&
      parent->GetStyleDisplay()->IsFloating()) {
    nsFirstLetterFrame* letterFrame = do_QueryFrame(parent);
    rv = letterFrame->CreateContinuationForFloatingParent(presContext, aFrame,
                                                          aNewFrame, PR_FALSE);
    return rv;
  }

  rv = presShell->FrameConstructor()->
    CreateContinuingFrame(presContext, aFrame, parent, aNewFrame, PR_FALSE);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // The list name nsGkAtoms::nextBidi would indicate we don't want reflow
  // XXXbz this needs higher-level framelist love
  nsFrameList temp(*aNewFrame, *aNewFrame);
  rv = parent->InsertFrames(nsGkAtoms::nextBidi, aFrame, temp);
  if (NS_FAILED(rv)) {
    return rv;
  }
  
  // Split inline ancestor frames
  rv = SplitInlineAncestors(aFrame);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

static PRBool
IsFrameInCurrentLine(nsBlockInFlowLineIterator* aLineIter,
                     nsIFrame* aPrevFrame, nsIFrame* aFrame)
{
  nsIFrame* endFrame = aLineIter->IsLastLineInList() ? nsnull :
    aLineIter->GetLine().next()->mFirstChild;
  nsIFrame* startFrame = aPrevFrame ? aPrevFrame : aLineIter->GetLine()->mFirstChild;
  for (nsIFrame* frame = startFrame; frame && frame != endFrame;
       frame = frame->GetNextSibling()) {
    if (frame == aFrame)
      return PR_TRUE;
  }
  return PR_FALSE;
}

static void
AdvanceLineIteratorToFrame(nsIFrame* aFrame,
                           nsBlockInFlowLineIterator* aLineIter,
                           nsIFrame*& aPrevFrame)
{
  // Advance aLine to the line containing aFrame
  nsIFrame* child = aFrame;
  nsFrameManager* frameManager = aFrame->PresContext()->FrameManager();
  nsIFrame* parent = nsLayoutUtils::GetParentOrPlaceholderFor(frameManager, child);
  while (parent && !nsLayoutUtils::GetAsBlock(parent)) {
    child = parent;
    parent = nsLayoutUtils::GetParentOrPlaceholderFor(frameManager, child);
  }
  NS_ASSERTION (parent, "aFrame is not a descendent of aBlockFrame");
  while (!IsFrameInCurrentLine(aLineIter, aPrevFrame, child)) {
#ifdef DEBUG
    PRBool hasNext =
#endif
      aLineIter->Next();
    NS_ASSERTION(hasNext, "Can't find frame in lines!");
    aPrevFrame = nsnull;
  }
  aPrevFrame = child;
}

/*
 * Overview of the implementation of Resolve():
 *
 *  Walk through the descendants of aBlockFrame and build:
 *   * mLogicalFrames: an nsTArray of nsIFrame* pointers in logical order
 *   * mBuffer: an nsAutoString containing a representation of
 *     the content of the frames.
 *     In the case of text frames, this is the actual text context of the
 *     frames, but some other elements are represented in a symbolic form which
 *     will make the Unicode Bidi Algorithm give the correct results.
 *     Bidi embeddings and overrides set by CSS or <bdo> elements are
 *     represented by the corresponding Unicode control characters.
 *     <br> elements are represented by U+2028 LINE SEPARATOR
 *     Other inline elements are represented by U+FFFC OBJECT REPLACEMENT
 *     CHARACTER
 *
 *  Then pass mBuffer to the Bidi engine for resolving of embedding levels
 *  by nsBidi::SetPara() and division into directional runs by
 *  nsBidi::CountRuns().
 *
 *  Finally, walk these runs in logical order using nsBidi::GetLogicalRun() and
 *  correlate them with the frames indexed in mLogicalFrames, setting the
 *  baseLevel and embeddingLevel properties according to the results returned
 *  by the Bidi engine.
 *
 *  The rendering layer requires each text frame to contain text in only one
 *  direction, so we may need to call EnsureBidiContinuation() to split frames.
 *  We may also need to call RemoveBidiContinuation() to convert frames created
 *  by EnsureBidiContinuation() in previous reflows into fluid continuations.
 */
nsresult
nsBidiPresUtils::Resolve(nsBlockFrame* aBlockFrame)
{
  mLogicalFrames.Clear();
  mContentToFrameIndex.Clear();
  
  nsPresContext *presContext = aBlockFrame->PresContext();
  nsIPresShell* shell = presContext->PresShell();
  nsStyleContext* styleContext = aBlockFrame->GetStyleContext();

  // handle bidi-override being set on the block itself before calling
  // InitLogicalArray.
  const nsStyleVisibility* vis = aBlockFrame->GetStyleVisibility();
  const nsStyleTextReset* text = aBlockFrame->GetStyleTextReset();

  if (text->mUnicodeBidi == NS_STYLE_UNICODE_BIDI_OVERRIDE) {
    nsIFrame *directionalFrame = nsnull;

    if (NS_STYLE_DIRECTION_RTL == vis->mDirection) {
      directionalFrame = NS_NewDirectionalFrame(shell, styleContext, kRLO);
    }
    else if (NS_STYLE_DIRECTION_LTR == vis->mDirection) {
      directionalFrame = NS_NewDirectionalFrame(shell, styleContext, kLRO);
    }

    if (directionalFrame) {
      mLogicalFrames.AppendElement(directionalFrame);
    }
  }
  for (nsBlockFrame* block = aBlockFrame; block;
       block = static_cast<nsBlockFrame*>(block->GetNextContinuation())) {
    block->RemoveStateBits(NS_BLOCK_NEEDS_BIDI_RESOLUTION);
    InitLogicalArray(block->GetFirstChild(nsnull));
  }

  if (text->mUnicodeBidi == NS_STYLE_UNICODE_BIDI_OVERRIDE) {
    nsIFrame* directionalFrame = NS_NewDirectionalFrame(shell, styleContext, kPDF);
    if (directionalFrame) {
      mLogicalFrames.AppendElement(directionalFrame);
    }
  }

  CreateBlockBuffer();

  PRInt32 bufferLength = mBuffer.Length();

  if (bufferLength < 1) {
    mSuccess = NS_OK;
    return mSuccess;
  }
  PRInt32 runCount;
  PRUint8 embeddingLevel;

  nsBidiLevel paraLevel = embeddingLevel =
    (NS_STYLE_DIRECTION_RTL == vis->mDirection)
    ? NSBIDI_RTL : NSBIDI_LTR;

  mSuccess = mBidiEngine->SetPara(mBuffer.get(), bufferLength, paraLevel, nsnull);
  if (NS_FAILED(mSuccess) ) {
      return mSuccess;
  }

  mSuccess = mBidiEngine->CountRuns(&runCount);
  if (NS_FAILED(mSuccess) ) {
    return mSuccess;
  }
  PRInt32     runLength      = 0;   // the length of the current run of text
  PRInt32     lineOffset     = 0;   // the start of the current run
  PRInt32     logicalLimit   = 0;   // the end of the current run + 1
  PRInt32     numRun         = -1;
  PRInt32     fragmentLength = 0;   // the length of the current text frame
  PRInt32     frameIndex     = -1;  // index to the frames in mLogicalFrames
  PRInt32     frameCount     = mLogicalFrames.Length();
  PRInt32     contentOffset  = 0;   // offset of current frame in its content node
  PRBool      isTextFrame    = PR_FALSE;
  nsIFrame*   frame = nsnull;
  nsIContent* content = nsnull;
  PRInt32     contentTextLength;
  nsIAtom*    frameType = nsnull;

  FramePropertyTable *propTable = presContext->PropertyTable();

  nsBlockInFlowLineIterator lineIter(aBlockFrame, aBlockFrame->begin_lines(), PR_FALSE);
  if (lineIter.GetLine() == aBlockFrame->end_lines()) {
    // Advance to first valid line (might be in a next-continuation)
    lineIter.Next();
  }
  nsIFrame* prevFrame = nsnull;
  PRBool lineNeedsUpdate = PR_FALSE;

  PRBool isVisual = presContext->IsVisualMode();
  if (isVisual) {
    /**
     * Drill up in content to detect whether this is an element that needs to be
     * rendered with logical order even on visual pages.
     *
     * We always use logical order on form controls, firstly so that text entry
     * will be in logical order, but also because visual pages were written with
     * the assumption that even if the browser had no support for right-to-left
     * text rendering, it would use native widgets with bidi support to display
     * form controls.
     *
     * We also use logical order in XUL elements, since we expect that if a XUL
     * element appears in a visual page, it will be generated by an XBL binding
     * and contain localized text which will be in logical order.
     */
    for (content = aBlockFrame->GetContent() ; content; content = content->GetParent()) {
      if (content->IsNodeOfType(nsINode::eHTML_FORM_CONTROL) || content->IsXUL()) {
        isVisual = PR_FALSE;
        break;
      }
    }
  }
  
  for (; ;) {
    if (fragmentLength <= 0) {
      // Get the next frame from mLogicalFrames
      if (++frameIndex >= frameCount) {
        break;
      }
      frame = mLogicalFrames[frameIndex];
      frameType = frame->GetType();
      lineNeedsUpdate = PR_TRUE;
      if (nsGkAtoms::textFrame == frameType) {
        content = frame->GetContent();
        if (!content) {
          mSuccess = NS_OK;
          break;
        }
        contentTextLength = content->TextLength();
        if (contentTextLength == 0) {
          frame->AdjustOffsetsForBidi(0, 0);
          // Set the base level and embedding level of the current run even
          // on an empty frame. Otherwise frame reordering will not be correct.
          propTable->Set(frame, nsIFrame::EmbeddingLevelProperty(),
                         NS_INT32_TO_PTR(embeddingLevel));
          propTable->Set(frame, nsIFrame::BaseLevelProperty(),
                         NS_INT32_TO_PTR(paraLevel));
          continue;
        }
        PRInt32 start, end;
        frame->GetOffsets(start, end);
        NS_ASSERTION(!(contentTextLength < end - start),
                     "Frame offsets don't fit in content");
        fragmentLength = NS_MIN(contentTextLength, end - start);
        contentOffset = start;
        isTextFrame = PR_TRUE;
      }
      else {
        /*
         * Any non-text frame corresponds to a single character in the text buffer
         * (a bidi control character, LINE SEPARATOR, or OBJECT SUBSTITUTE)
         */
        isTextFrame = PR_FALSE;
        fragmentLength = 1;
      }
    } // if (fragmentLength <= 0)

    if (runLength <= 0) {
      // Get the next run of text from the Bidi engine
      if (++numRun >= runCount) {
        break;
      }
      lineOffset = logicalLimit;
      if (NS_FAILED(mBidiEngine->GetLogicalRun(
              lineOffset, &logicalLimit, &embeddingLevel) ) ) {
        break;
      }
      runLength = logicalLimit - lineOffset;
      if (isVisual) {
        embeddingLevel = paraLevel;
      }
    } // if (runLength <= 0)

    if (nsGkAtoms::directionalFrame == frameType) {
      frame->Destroy();
      frame = nsnull;
      ++lineOffset;
    }
    else {
      propTable->Set(frame, nsIFrame::EmbeddingLevelProperty(),
                     NS_INT32_TO_PTR(embeddingLevel));
      propTable->Set(frame, nsIFrame::BaseLevelProperty(),
                     NS_INT32_TO_PTR(paraLevel));
      if (isTextFrame) {
        if ( (runLength > 0) && (runLength < fragmentLength) ) {
          /*
           * The text in this frame continues beyond the end of this directional run.
           * Create a non-fluid continuation frame for the next directional run.
           */
          if (lineNeedsUpdate) {
            AdvanceLineIteratorToFrame(frame, &lineIter, prevFrame);
            lineNeedsUpdate = PR_FALSE;
          }
          lineIter.GetLine()->MarkDirty();
          nsIFrame* nextBidi;
          PRInt32 runEnd = contentOffset + runLength;
          EnsureBidiContinuation(frame, &nextBidi, frameIndex,
                                 contentOffset,
                                 runEnd);
          if (NS_FAILED(mSuccess)) {
            break;
          }
          nextBidi->AdjustOffsetsForBidi(runEnd,
                                         contentOffset + fragmentLength);
          frame = nextBidi;
          contentOffset = runEnd;
        } // if (runLength < fragmentLength)
        else {
          if (contentOffset + fragmentLength == contentTextLength) {
            /* 
             * We have finished all the text in this content node. Convert any
             * further non-fluid continuations to fluid continuations and advance
             * frameIndex to the last frame in the content node
             */
            PRInt32 newIndex = 0;
            mContentToFrameIndex.Get(content, &newIndex);
            if (newIndex > frameIndex) {
              RemoveBidiContinuation(frame, frameIndex, newIndex, lineOffset);
              frameIndex = newIndex;
            }
          } else if (fragmentLength > 0 && runLength > fragmentLength) {
            /*
             * There is more text that belongs to this directional run in the next
             * text frame: make sure it is a fluid continuation of the current frame.
             * Do not advance frameIndex, because the next frame may contain
             * multi-directional text and need to be split
             */
            PRInt32 newIndex = frameIndex;
            do {
            } while (mLogicalFrames[++newIndex]->GetType() == nsGkAtoms::directionalFrame);
            RemoveBidiContinuation(frame, frameIndex, newIndex, lineOffset);
          } else if (runLength == fragmentLength) {
            /*
             * The directional run ends at the end of the frame. Make sure that
             * the next frame is a non-fluid continuation
             */
            nsIFrame* next = frame->GetNextInFlow();
            if (next) {
              frame->SetNextContinuation(next);
              next->SetPrevContinuation(frame);
            }
          }
          frame->AdjustOffsetsForBidi(contentOffset, contentOffset + fragmentLength);
          if (lineNeedsUpdate) {
            AdvanceLineIteratorToFrame(frame, &lineIter, prevFrame);
            lineNeedsUpdate = PR_FALSE;
          }
          lineIter.GetLine()->MarkDirty();
        }
      } // isTextFrame
      else {
        ++lineOffset;
      }
    } // not directionalFrame
    PRInt32 temp = runLength;
    runLength -= fragmentLength;
    fragmentLength -= temp;

    if (frame && fragmentLength <= 0) {
      // If the frame is at the end of a run, split all ancestor inlines that
      // need splitting.
      // To determine whether we're at the end of the run, we check that we've
      // finished processing the current run, and that the current frame
      // doesn't have a fluid continuation (it could have a fluid continuation
      // of zero length, so testing runLength alone is not sufficient).
      if (runLength <= 0 && !frame->GetNextInFlow()) {
        nsIFrame* child = frame;
        nsIFrame* parent = frame->GetParent();
        // As long as we're on the last sibling, the parent doesn't have to be split.
        // However, if the parent has a fluid continuation, we do have to make
        // it non-fluid. This can happen e.g. when we have a first-letter frame
        // and the end of the first-letter coincides with the end of a
        // directional run.
        while (parent &&
               IsBidiSplittable(parent) &&
               !child->GetNextSibling()) {
          nsIFrame* next = parent->GetNextInFlow();
          if (next) {
            parent->SetNextContinuation(next);
            next->SetPrevContinuation(parent);
          }
          child = parent;
          parent = child->GetParent();
        }
        if (parent && IsBidiSplittable(parent))
          SplitInlineAncestors(child);
      }
      else if (!frame->GetNextSibling()) {
        // We're not at an end of a run, and |frame| is the last child of its parent.
        // If its ancestors happen to have bidi continuations, convert them into
        // fluid continuations.
        nsIFrame* parent = frame->GetParent();
        JoinInlineAncestors(parent);
      }
    }
  } // for
  return mSuccess;
}

// Should this frame be treated as a leaf (e.g. when building mLogicalFrames)?
PRBool IsBidiLeaf(nsIFrame* aFrame) {
  nsIFrame* kid = aFrame->GetFirstChild(nsnull);
  return !kid
    || !aFrame->IsFrameOfType(nsIFrame::eBidiInlineContainer);
}

void
nsBidiPresUtils::InitLogicalArray(nsIFrame*       aCurrentFrame)
{
  if (!aCurrentFrame)
    return;

  nsIPresShell* shell = aCurrentFrame->PresContext()->PresShell();
  nsStyleContext* styleContext;

  for (nsIFrame* childFrame = aCurrentFrame; childFrame;
       childFrame = childFrame->GetNextSibling()) {

    // If the real frame for a placeholder is a first letter frame, we need to
    // drill down into it and include its contents in Bidi resolution.
    // If not, we just use the placeholder.
    nsIFrame* frame = childFrame;
    if (nsGkAtoms::placeholderFrame == childFrame->GetType()) {
      nsIFrame* realFrame =
        nsPlaceholderFrame::GetRealFrameForPlaceholder(childFrame);
      if (realFrame->GetType() == nsGkAtoms::letterFrame) {
        frame = realFrame;
      }
    }

    PRUnichar ch = 0;
    if (frame->IsFrameOfType(nsIFrame::eBidiInlineContainer)) {
      const nsStyleVisibility* vis = frame->GetStyleVisibility();
      const nsStyleTextReset* text = frame->GetStyleTextReset();
      switch (text->mUnicodeBidi) {
        case NS_STYLE_UNICODE_BIDI_NORMAL:
          break;
        case NS_STYLE_UNICODE_BIDI_EMBED:
          styleContext = frame->GetStyleContext();

          if (NS_STYLE_DIRECTION_RTL == vis->mDirection) {
            ch = kRLE;
          }
          else if (NS_STYLE_DIRECTION_LTR == vis->mDirection) {
            ch = kLRE;
          }
          break;
        case NS_STYLE_UNICODE_BIDI_OVERRIDE:
          styleContext = frame->GetStyleContext();

          if (NS_STYLE_DIRECTION_RTL == vis->mDirection) {
            ch = kRLO;
          }
          else if (NS_STYLE_DIRECTION_LTR == vis->mDirection) {
            ch = kLRO;
          }
          break;
      }

      // Create a directional frame before the first frame of an
      // element specifying embedding or override
      if (ch != 0 && !frame->GetPrevContinuation()) {
        nsIFrame* dirFrame = NS_NewDirectionalFrame(shell, styleContext, ch);
        if (dirFrame) {
          mLogicalFrames.AppendElement(dirFrame);
        }
      }
    }

    if (IsBidiLeaf(frame)) {
      /* Bidi leaf frame: add the frame to the mLogicalFrames array,
       * and add its index to the mContentToFrameIndex hashtable. This
       * will be used in RemoveBidiContinuation() to identify the last
       * frame in the array with a given content.
       */
      nsIContent* content = frame->GetContent();
      if (content) {
        mContentToFrameIndex.Put(content, mLogicalFrames.Length());
      }
      mLogicalFrames.AppendElement(frame);
    }
    else {
      nsIFrame* kid = frame->GetFirstChild(nsnull);
      InitLogicalArray(kid);
    }

    // If the element is attributed by dir, indicate direction pop (add PDF frame)
    if (ch != 0 && !frame->GetNextContinuation()) {
      // Create a directional frame after the last frame of an
      // element specifying embedding or override
      nsIFrame* dirFrame = NS_NewDirectionalFrame(shell, styleContext, kPDF);
      if (dirFrame) {
        mLogicalFrames.AppendElement(dirFrame);
      }
    }
  } // for
}

void
nsBidiPresUtils::CreateBlockBuffer()
{
  mBuffer.SetLength(0);

  nsIFrame*                 frame;
  nsIContent*               prevContent = nsnull;
  PRUint32                  i;
  PRUint32                  count = mLogicalFrames.Length();

  for (i = 0; i < count; i++) {
    frame = mLogicalFrames[i];
    nsIAtom* frameType = frame->GetType();

    if (nsGkAtoms::textFrame == frameType) {
      nsIContent* content = frame->GetContent();
      if (!content) {
        mSuccess = NS_OK;
        break;
      }
      if (content == prevContent) {
        continue;
      }
      prevContent = content;
      content->AppendTextTo(mBuffer);
    }
    else if (nsGkAtoms::brFrame == frameType) { // break frame
      // Append line separator
      mBuffer.Append(kLineSeparator);
    }
    else if (nsGkAtoms::directionalFrame == frameType) {
      nsDirectionalFrame* dirFrame = static_cast<nsDirectionalFrame*>(frame);
      mBuffer.Append(dirFrame->GetChar());
    }
    else { // not text frame
      // See the Unicode Bidi Algorithm:
      // "...inline objects (such as graphics) are treated as if they are ... U+FFFC"
      mBuffer.Append(kObjectSubstitute);
    }
  }
  // XXX: TODO: Handle preformatted text ('\n')
  mBuffer.ReplaceChar("\t\r\n", kSpace);
}

void
nsBidiPresUtils::ReorderFrames(nsIFrame*            aFirstFrameOnLine,
                               PRInt32              aNumFramesOnLine)
{
  // If this line consists of a line frame, reorder the line frame's children.
  if (aFirstFrameOnLine->GetType() == nsGkAtoms::lineFrame) {
    aFirstFrameOnLine = aFirstFrameOnLine->GetFirstChild(nsnull);
    if (!aFirstFrameOnLine)
      return;
    // All children of the line frame are on the first line. Setting aNumFramesOnLine
    // to -1 makes InitLogicalArrayFromLine look at all of them.
    aNumFramesOnLine = -1;
  }

  InitLogicalArrayFromLine(aFirstFrameOnLine, aNumFramesOnLine);

  PRBool isReordered;
  PRBool hasRTLFrames;
  Reorder(isReordered, hasRTLFrames);
  RepositionInlineFrames(aFirstFrameOnLine);
}

nsresult
nsBidiPresUtils::Reorder(PRBool& aReordered, PRBool& aHasRTLFrames)
{
  aReordered = PR_FALSE;
  aHasRTLFrames = PR_FALSE;
  PRInt32 count = mLogicalFrames.Length();

  if (mArraySize < count) {
    mArraySize = count << 1;
    if (mLevels) {
      delete[] mLevels;
      mLevels = nsnull;
    }
    if (mIndexMap) {
      delete[] mIndexMap;
      mIndexMap = nsnull;
    }
  }
  if (!mLevels) {
    mLevels = new PRUint8[mArraySize];
    if (!mLevels) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  memset(mLevels, 0, sizeof(PRUint8) * mArraySize);

  nsIFrame* frame;
  PRInt32   i;

  for (i = 0; i < count; i++) {
    frame = mLogicalFrames[i];
    mLevels[i] = GetFrameEmbeddingLevel(frame);
    if (mLevels[i] & 1) {
      aHasRTLFrames = PR_TRUE;
    }      
  }
  if (!mIndexMap) {
    mIndexMap = new PRInt32[mArraySize];
  }
  if (!mIndexMap) {
    mSuccess = NS_ERROR_OUT_OF_MEMORY;
  }
  else {
    memset(mIndexMap, 0, sizeof(PRUint32) * mArraySize);

    mSuccess = mBidiEngine->ReorderVisual(mLevels, count, mIndexMap);

    if (NS_SUCCEEDED(mSuccess) ) {
      mVisualFrames.Clear();

      for (i = 0; i < count; i++) {
        mVisualFrames.AppendElement(mLogicalFrames[mIndexMap[i]]);
        if (i != mIndexMap[i]) {
          aReordered = PR_TRUE;
        }
      }
    } // NS_SUCCEEDED(mSuccess)
  } // indexMap

  if (NS_FAILED(mSuccess) ) {
    aReordered = PR_FALSE;
  }
  return mSuccess;
}

nsBidiLevel
nsBidiPresUtils::GetFrameEmbeddingLevel(nsIFrame* aFrame)
{
  nsIFrame* firstLeaf = aFrame;
  while (!IsBidiLeaf(firstLeaf)) {
    nsIFrame* firstChild = firstLeaf->GetFirstChild(nsnull);
    nsIFrame* realFrame = nsPlaceholderFrame::GetRealFrameFor(firstChild);
    firstLeaf = (realFrame->GetType() == nsGkAtoms::letterFrame) ?
                 realFrame : firstChild;
  }
  return NS_GET_EMBEDDING_LEVEL(firstLeaf);
}

nsBidiLevel
nsBidiPresUtils::GetFrameBaseLevel(nsIFrame* aFrame)
{
  nsIFrame* firstLeaf = aFrame;
  while (!IsBidiLeaf(firstLeaf)) {
    firstLeaf = firstLeaf->GetFirstChild(nsnull);
  }
  return NS_GET_BASE_LEVEL(firstLeaf);
}

void
nsBidiPresUtils::IsLeftOrRightMost(nsIFrame*              aFrame,
                                   nsContinuationStates*  aContinuationStates,
                                   PRBool&                aIsLeftMost /* out */,
                                   PRBool&                aIsRightMost /* out */) const
{
  const nsStyleVisibility* vis = aFrame->GetStyleVisibility();
  PRBool isLTR = (NS_STYLE_DIRECTION_LTR == vis->mDirection);

  /*
   * Since we lay out frames from left to right (in both LTR and RTL), visiting a
   * frame with 'mFirstVisualFrame == nsnull', means it's the first appearance of
   * one of its continuation chain frames on the line.
   * To determine if it's the last visual frame of its continuation chain on the line
   * or not, we count the number of frames of the chain on the line, and then reduce
   * it when we lay out a frame of the chain. If this value becomes 1 it means
   * that it's the last visual frame of its continuation chain on this line.
   */

  nsFrameContinuationState* frameState = aContinuationStates->GetEntry(aFrame);
  nsFrameContinuationState* firstFrameState;

  if (!frameState->mFirstVisualFrame) {
    // aFrame is the first visual frame of its continuation chain
    nsFrameContinuationState* contState;
    nsIFrame* frame;

    frameState->mFrameCount = 1;
    frameState->mFirstVisualFrame = aFrame;

    /**
     * Traverse continuation chain of aFrame in both backward and forward
     * directions while the frames are on this line. Count the frames and
     * set their mFirstVisualFrame to aFrame.
     */
    // Traverse continuation chain backward
    for (frame = aFrame->GetPrevContinuation();
         frame && (contState = aContinuationStates->GetEntry(frame));
         frame = frame->GetPrevContinuation()) {
      frameState->mFrameCount++;
      contState->mFirstVisualFrame = aFrame;
    }
    frameState->mHasContOnPrevLines = (frame != nsnull);

    // Traverse continuation chain forward
    for (frame = aFrame->GetNextContinuation();
         frame && (contState = aContinuationStates->GetEntry(frame));
         frame = frame->GetNextContinuation()) {
      frameState->mFrameCount++;
      contState->mFirstVisualFrame = aFrame;
    }
    frameState->mHasContOnNextLines = (frame != nsnull);

    aIsLeftMost = isLTR ? !frameState->mHasContOnPrevLines
                        : !frameState->mHasContOnNextLines;
    firstFrameState = frameState;
  } else {
    // aFrame is not the first visual frame of its continuation chain
    aIsLeftMost = PR_FALSE;
    firstFrameState = aContinuationStates->GetEntry(frameState->mFirstVisualFrame);
  }

  aIsRightMost = (firstFrameState->mFrameCount == 1) &&
                 (isLTR ? !firstFrameState->mHasContOnNextLines
                        : !firstFrameState->mHasContOnPrevLines);

  if ((aIsLeftMost || aIsRightMost) &&
      (aFrame->GetStateBits() & NS_FRAME_IS_SPECIAL)) {
    // For ib splits, don't treat anything except the last part as
    // endmost or anything except the first part as startmost.
    // As an optimization, only get the first continuation once.
    nsIFrame* firstContinuation = aFrame->GetFirstContinuation();
    if (nsLayoutUtils::FrameIsNonLastInIBSplit(firstContinuation)) {
      // We are not endmost
      if (isLTR) {
        aIsRightMost = PR_FALSE;
      } else {
        aIsLeftMost = PR_FALSE;
      }
    }
    if (nsLayoutUtils::FrameIsNonFirstInIBSplit(firstContinuation)) {
      // We are not startmost
      if (isLTR) {
        aIsLeftMost = PR_FALSE;
      } else {
        aIsRightMost = PR_FALSE;
      }
    }
  }

  // Reduce number of remaining frames of the continuation chain on the line.
  firstFrameState->mFrameCount--;
}

void
nsBidiPresUtils::RepositionFrame(nsIFrame*              aFrame,
                                 PRBool                 aIsOddLevel,
                                 nscoord&               aLeft,
                                 nsContinuationStates*  aContinuationStates) const
{
  if (!aFrame)
    return;

  PRBool isLeftMost, isRightMost;
  IsLeftOrRightMost(aFrame,
                    aContinuationStates,
                    isLeftMost /* out */,
                    isRightMost /* out */);

  nsInlineFrame* testFrame = do_QueryFrame(aFrame);
  if (testFrame) {
    aFrame->AddStateBits(NS_INLINE_FRAME_BIDI_VISUAL_STATE_IS_SET);

    if (isLeftMost)
      aFrame->AddStateBits(NS_INLINE_FRAME_BIDI_VISUAL_IS_LEFT_MOST);
    else
      aFrame->RemoveStateBits(NS_INLINE_FRAME_BIDI_VISUAL_IS_LEFT_MOST);

    if (isRightMost)
      aFrame->AddStateBits(NS_INLINE_FRAME_BIDI_VISUAL_IS_RIGHT_MOST);
    else
      aFrame->RemoveStateBits(NS_INLINE_FRAME_BIDI_VISUAL_IS_RIGHT_MOST);
  }
  // This method is called from nsBlockFrame::PlaceLine via the call to
  // bidiUtils->ReorderFrames, so this is guaranteed to be after the inlines
  // have been reflowed, which is required for GetUsedMargin/Border/Padding
  nsMargin margin = aFrame->GetUsedMargin();
  if (isLeftMost)
    aLeft += margin.left;

  nscoord start = aLeft;

  if (!IsBidiLeaf(aFrame))
  {
    nscoord x = 0;
    nsMargin borderPadding = aFrame->GetUsedBorderAndPadding();
    if (isLeftMost) {
      x += borderPadding.left;
    }

    // If aIsOddLevel is true, so we need to traverse the child list
    // in reverse order, to make it O(n) we store the list locally and
    // iterate the list reversely
    nsTArray<nsIFrame*> childList;
    nsIFrame *frame = aFrame->GetFirstChild(nsnull);
    if (frame && aIsOddLevel) {
      childList.AppendElement((nsIFrame*)nsnull);
      while (frame) {
        childList.AppendElement(frame);
        frame = frame->GetNextSibling();
      }
      frame = childList[childList.Length() - 1];
    }

    // Reposition the child frames
    PRInt32 index = 0;
    while (frame) {
      RepositionFrame(frame,
                      aIsOddLevel,
                      x,
                      aContinuationStates);
      index++;
      frame = aIsOddLevel ?
                childList[childList.Length() - index - 1] :
                frame->GetNextSibling();
    }

    if (isRightMost) {
      x += borderPadding.right;
    }
    aLeft += x;
  } else {
    aLeft += aFrame->GetSize().width;
  }
  nsRect rect = aFrame->GetRect();
  aFrame->SetRect(nsRect(start, rect.y, aLeft - start, rect.height));

  if (isRightMost)
    aLeft += margin.right;
}

void
nsBidiPresUtils::InitContinuationStates(nsIFrame*              aFrame,
                                        nsContinuationStates*  aContinuationStates) const
{
  nsFrameContinuationState* state = aContinuationStates->PutEntry(aFrame);
  state->mFirstVisualFrame = nsnull;
  state->mFrameCount = 0;

  if (!IsBidiLeaf(aFrame)) {
    // Continue for child frames
    nsIFrame* frame;
    for (frame = aFrame->GetFirstChild(nsnull);
         frame;
         frame = frame->GetNextSibling()) {
      InitContinuationStates(frame,
                             aContinuationStates);
    }
  }
}

void
nsBidiPresUtils::RepositionInlineFrames(nsIFrame* aFirstChild) const
{
  const nsStyleVisibility* vis = aFirstChild->GetStyleVisibility();
  PRBool isLTR = (NS_STYLE_DIRECTION_LTR == vis->mDirection);
  nscoord leftSpace = 0;

  // This method is called from nsBlockFrame::PlaceLine via the call to
  // bidiUtils->ReorderFrames, so this is guaranteed to be after the inlines
  // have been reflowed, which is required for GetUsedMargin/Border/Padding
  nsMargin margin = aFirstChild->GetUsedMargin();
  if (!aFirstChild->GetPrevContinuation() &&
      !nsLayoutUtils::FrameIsNonFirstInIBSplit(aFirstChild))
    leftSpace = isLTR ? margin.left : margin.right;

  nscoord left = aFirstChild->GetPosition().x - leftSpace;
  nsIFrame* frame;
  PRInt32 count = mVisualFrames.Length();
  PRInt32 index;
  nsContinuationStates continuationStates;

  continuationStates.Init();

  // Initialize continuation states to (nsnull, 0) for
  // each frame on the line.
  for (index = 0; index < count; index++) {
    InitContinuationStates(mVisualFrames[index], &continuationStates);
  }

  // Reposition frames in visual order
  for (index = 0; index < count; index++) {
    frame = mVisualFrames[index];
    RepositionFrame(frame,
                    (mLevels[mIndexMap[index]] & 1),
                    left,
                    &continuationStates);
  } // for
}

void 
nsBidiPresUtils::InitLogicalArrayFromLine(nsIFrame* aFirstFrameOnLine,
                                          PRInt32   aNumFramesOnLine) {
  mLogicalFrames.Clear();
  for (nsIFrame* frame = aFirstFrameOnLine;
       frame && aNumFramesOnLine--;
       frame = frame->GetNextSibling()) {
    mLogicalFrames.AppendElement(frame);
  }
}

PRBool
nsBidiPresUtils::CheckLineOrder(nsIFrame*  aFirstFrameOnLine,
                                PRInt32    aNumFramesOnLine,
                                nsIFrame** aFirstVisual,
                                nsIFrame** aLastVisual)
{
  InitLogicalArrayFromLine(aFirstFrameOnLine, aNumFramesOnLine);
  
  PRBool isReordered;
  PRBool hasRTLFrames;
  Reorder(isReordered, hasRTLFrames);
  PRInt32 count = mLogicalFrames.Length();
  
  if (aFirstVisual) {
    *aFirstVisual = mVisualFrames[0];
  }
  if (aLastVisual) {
    *aLastVisual = mVisualFrames[count-1];
  }
  
  // If there's an RTL frame, assume the line is reordered
  return isReordered || hasRTLFrames;
}

nsIFrame*
nsBidiPresUtils::GetFrameToRightOf(const nsIFrame*  aFrame,
                                   nsIFrame*        aFirstFrameOnLine,
                                   PRInt32          aNumFramesOnLine)
{
  InitLogicalArrayFromLine(aFirstFrameOnLine, aNumFramesOnLine);
  
  PRBool isReordered;
  PRBool hasRTLFrames;
  Reorder(isReordered, hasRTLFrames);
  PRInt32 count = mVisualFrames.Length();

  if (aFrame == nsnull)
    return mVisualFrames[0];
  
  for (PRInt32 i = 0; i < count - 1; i++) {
    if (mVisualFrames[i] == aFrame) {
      return mVisualFrames[i+1];
    }
  }
  
  return nsnull;
}

nsIFrame*
nsBidiPresUtils::GetFrameToLeftOf(const nsIFrame*  aFrame,
                                  nsIFrame*        aFirstFrameOnLine,
                                  PRInt32          aNumFramesOnLine)
{
  InitLogicalArrayFromLine(aFirstFrameOnLine, aNumFramesOnLine);
  
  PRBool isReordered;
  PRBool hasRTLFrames;
  Reorder(isReordered, hasRTLFrames);
  PRInt32 count = mVisualFrames.Length();
  
  if (aFrame == nsnull)
    return mVisualFrames[count-1];
  
  for (PRInt32 i = 1; i < count; i++) {
    if (mVisualFrames[i] == aFrame) {
      return mVisualFrames[i-1];
    }
  }
  
  return nsnull;
}

inline void
nsBidiPresUtils::EnsureBidiContinuation(nsIFrame*       aFrame,
                                        nsIFrame**      aNewFrame,
                                        PRInt32&        aFrameIndex,
                                        PRInt32         aStart,
                                        PRInt32         aEnd)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  NS_PRECONDITION(aFrame, "aFrame is null");

  aFrame->AdjustOffsetsForBidi(aStart, aEnd);
  mSuccess = CreateBidiContinuation(aFrame, aNewFrame);
}

void
nsBidiPresUtils::RemoveBidiContinuation(nsIFrame*       aFrame,
                                        PRInt32         aFirstIndex,
                                        PRInt32         aLastIndex,
                                        PRInt32&        aOffset) const
{
  FrameProperties props = aFrame->Properties();
  nsBidiLevel embeddingLevel =
    (nsBidiLevel)NS_PTR_TO_INT32(props.Get(nsIFrame::EmbeddingLevelProperty()));
  nsBidiLevel baseLevel =
    (nsBidiLevel)NS_PTR_TO_INT32(props.Get(nsIFrame::BaseLevelProperty()));

  for (PRInt32 index = aFirstIndex + 1; index <= aLastIndex; index++) {
    nsIFrame* frame = mLogicalFrames[index];
    if (nsGkAtoms::directionalFrame == frame->GetType()) {
      frame->Destroy();
      ++aOffset;
    }
    else {
      // Make the frame and its continuation ancestors fluid,
      // so they can be reused or deleted by normal reflow code
      FrameProperties frameProps = frame->Properties();
      frameProps.Set(nsIFrame::EmbeddingLevelProperty(),
                     NS_INT32_TO_PTR(embeddingLevel));
      frameProps.Set(nsIFrame::BaseLevelProperty(),
                     NS_INT32_TO_PTR(baseLevel));
      frame->AddStateBits(NS_FRAME_IS_BIDI);
      while (frame) {
        nsIFrame* prev = frame->GetPrevContinuation();
        if (prev) {
          NS_ASSERTION (!frame->GetPrevInFlow() || frame->GetPrevInFlow() == prev, 
                        "prev-in-flow is not prev continuation!");
          frame->SetPrevInFlow(prev);

          NS_ASSERTION (!prev->GetNextInFlow() || prev->GetNextInFlow() == frame,
                        "next-in-flow is not next continuation!");
          prev->SetNextInFlow(frame);

          frame = frame->GetParent();
        } else {
          break;
        }
      }
    }
  }
}

nsresult
nsBidiPresUtils::FormatUnicodeText(nsPresContext*  aPresContext,
                                   PRUnichar*       aText,
                                   PRInt32&         aTextLength,
                                   nsCharType       aCharType,
                                   PRBool           aIsOddLevel)
{
  NS_ASSERTION(aIsOddLevel == 0 || aIsOddLevel == 1, "aIsOddLevel should be 0 or 1");
  nsresult rv = NS_OK;
  // ahmed 
  //adjusted for correct numeral shaping  
  PRUint32 bidiOptions = aPresContext->GetBidi();
  switch (GET_BIDI_OPTION_NUMERAL(bidiOptions)) {

    case IBMBIDI_NUMERAL_HINDI:
      HandleNumbers(aText,aTextLength,IBMBIDI_NUMERAL_HINDI);
      break;

    case IBMBIDI_NUMERAL_ARABIC:
      HandleNumbers(aText,aTextLength,IBMBIDI_NUMERAL_ARABIC);
      break;

    case IBMBIDI_NUMERAL_PERSIAN:
      HandleNumbers(aText,aTextLength,IBMBIDI_NUMERAL_PERSIAN);
      break;

    case IBMBIDI_NUMERAL_REGULAR:

      switch (aCharType) {

        case eCharType_EuropeanNumber:
          HandleNumbers(aText,aTextLength,IBMBIDI_NUMERAL_ARABIC);
          break;

        case eCharType_ArabicNumber:
          HandleNumbers(aText,aTextLength,IBMBIDI_NUMERAL_HINDI);
          break;

        default:
          break;
      }
      break;

    case IBMBIDI_NUMERAL_HINDICONTEXT:
      if ( ( (GET_BIDI_OPTION_DIRECTION(bidiOptions)==IBMBIDI_TEXTDIRECTION_RTL) && (IS_ARABIC_DIGIT (aText[0])) ) || (eCharType_ArabicNumber == aCharType) )
        HandleNumbers(aText,aTextLength,IBMBIDI_NUMERAL_HINDI);
      else if (eCharType_EuropeanNumber == aCharType)
        HandleNumbers(aText,aTextLength,IBMBIDI_NUMERAL_ARABIC);
      break;

    case IBMBIDI_NUMERAL_PERSIANCONTEXT:
      if ( ( (GET_BIDI_OPTION_DIRECTION(bidiOptions)==IBMBIDI_TEXTDIRECTION_RTL) && (IS_ARABIC_DIGIT (aText[0])) ) || (eCharType_ArabicNumber == aCharType) )
        HandleNumbers(aText,aTextLength,IBMBIDI_NUMERAL_PERSIAN);
      else if (eCharType_EuropeanNumber == aCharType)
        HandleNumbers(aText,aTextLength,IBMBIDI_NUMERAL_ARABIC);
      break;

    case IBMBIDI_NUMERAL_NOMINAL:
    default:
      break;
  }

  StripBidiControlCharacters(aText, aTextLength);
  return rv;
}

void
nsBidiPresUtils::StripBidiControlCharacters(PRUnichar* aText,
                                            PRInt32&   aTextLength) const
{
  if ( (nsnull == aText) || (aTextLength < 1) ) {
    return;
  }

  PRInt32 stripLen = 0;

  for (PRInt32 i = 0; i < aTextLength; i++) {
    // XXX: This silently ignores surrogate characters.
    //      As of Unicode 4.0, all Bidi control characters are within the BMP.
    if (IsBidiControl((PRUint32)aText[i])) {
      ++stripLen;
    }
    else {
      aText[i - stripLen] = aText[i];
    }
  }
  aTextLength -= stripLen;
}
 
#if 0 // XXX: for the future use ???
void
RemoveDiacritics(PRUnichar* aText,
                 PRInt32&   aTextLength)
{
  if (aText && (aTextLength > 0) ) {
    PRInt32 offset = 0;

    for (PRInt32 i = 0; i < aTextLength && aText[i]; i++) {
      if (IS_BIDI_DIACRITIC(aText[i]) ) {
        ++offset;
        continue;
      }
      aText[i - offset] = aText[i];
    }
    aTextLength = i - offset;
    aText[aTextLength] = 0;
  }
}
#endif

void
nsBidiPresUtils::CalculateCharType(PRInt32& aOffset,
                                   PRInt32  aCharTypeLimit,
                                   PRInt32& aRunLimit,
                                   PRInt32& aRunLength,
                                   PRInt32& aRunCount,
                                   PRUint8& aCharType,
                                   PRUint8& aPrevCharType) const

{
  PRBool     strongTypeFound = PR_FALSE;
  PRInt32    offset;
  nsCharType charType;

  aCharType = eCharType_OtherNeutral;

  for (offset = aOffset; offset < aCharTypeLimit; offset++) {
    // Make sure we give RTL chartype to all characters that would be classified
    // as Right-To-Left by a bidi platform.
    // (May differ from the UnicodeData, eg we set RTL chartype to some NSMs.)
    if (IS_HEBREW_CHAR(mBuffer[offset]) ) {
      charType = eCharType_RightToLeft;
    }
    else if (IS_ARABIC_ALPHABETIC(mBuffer[offset]) ) {
      charType = eCharType_RightToLeftArabic;
    }
    else {
      mBidiEngine->GetCharTypeAt(offset, &charType);
    }

    if (!CHARTYPE_IS_WEAK(charType) ) {

      if (strongTypeFound
          && (charType != aPrevCharType)
          && (CHARTYPE_IS_RTL(charType) || CHARTYPE_IS_RTL(aPrevCharType) ) ) {
        // Stop at this point to ensure uni-directionality of the text
        // (from platform's point of view).
        // Also, don't mix Arabic and Hebrew content (since platform may
        // provide BIDI support to one of them only).
        aRunLength = offset - aOffset;
        aRunLimit = offset;
        ++aRunCount;
        break;
      }

      if ( (eCharType_RightToLeftArabic == aPrevCharType
            || eCharType_ArabicNumber == aPrevCharType)
          && eCharType_EuropeanNumber == charType) {
        charType = eCharType_ArabicNumber;
      }

      // Set PrevCharType to the last strong type in this frame
      // (for correct numeric shaping)
      aPrevCharType = charType;

      strongTypeFound = PR_TRUE;
      aCharType = charType;
    }
  }
  aOffset = offset;
}

nsresult nsBidiPresUtils::ProcessText(const PRUnichar*       aText,
                                      PRInt32                aLength,
                                      nsBidiDirection        aBaseDirection,
                                      nsPresContext*         aPresContext,
                                      BidiProcessor&         aprocessor,
                                      Mode                   aMode,
                                      nsBidiPositionResolve* aPosResolve,
                                      PRInt32                aPosResolveCount,
                                      nscoord*               aWidth)
{
  NS_ASSERTION((aPosResolve == nsnull) != (aPosResolveCount > 0), "Incorrect aPosResolve / aPosResolveCount arguments");

  PRInt32 runCount;

  mBuffer.Assign(aText, aLength);

  nsresult rv = mBidiEngine->SetPara(mBuffer.get(), aLength, aBaseDirection, nsnull);
  if (NS_FAILED(rv))
    return rv;

  rv = mBidiEngine->CountRuns(&runCount);
  if (NS_FAILED(rv))
    return rv;

  nscoord xOffset = 0;
  nscoord width, xEndRun;
  nscoord totalWidth = 0;
  PRInt32 i, start, limit, length;
  PRUint32 visualStart = 0;
  PRUint8 charType;
  PRUint8 prevType = eCharType_LeftToRight;
  nsBidiLevel level;
      
  for(int nPosResolve=0; nPosResolve < aPosResolveCount; ++nPosResolve)
  {
    aPosResolve[nPosResolve].visualIndex = kNotFound;
    aPosResolve[nPosResolve].visualLeftTwips = kNotFound;
    aPosResolve[nPosResolve].visualWidth = kNotFound;
  }

  for (i = 0; i < runCount; i++) {
    rv = mBidiEngine->GetVisualRun(i, &start, &length, &aBaseDirection);
    if (NS_FAILED(rv))
      return rv;

    rv = mBidiEngine->GetLogicalRun(start, &limit, &level);
    if (NS_FAILED(rv))
      return rv;

    PRInt32 subRunLength = limit - start;
    PRInt32 lineOffset = start;
    PRInt32 typeLimit = NS_MIN(limit, aLength);
    PRInt32 subRunCount = 1;
    PRInt32 subRunLimit = typeLimit;

    /*
     * If |level| is even, i.e. the direction of the run is left-to-right, we
     * render the subruns from left to right and increment the x-coordinate
     * |xOffset| by the width of each subrun after rendering.
     *
     * If |level| is odd, i.e. the direction of the run is right-to-left, we
     * render the subruns from right to left. We begin by incrementing |xOffset| by
     * the width of the whole run, and then decrement it by the width of each
     * subrun before rendering. After rendering all the subruns, we restore the
     * x-coordinate of the end of the run for the start of the next run.
     */

    if (level & 1) {
      aprocessor.SetText(aText + start, subRunLength, nsBidiDirection(level & 1));
      width = aprocessor.GetWidth();
      xOffset += width;
      xEndRun = xOffset;
    }

    while (subRunCount > 0) {
      // CalculateCharType can increment subRunCount if the run
      // contains mixed character types
      CalculateCharType(lineOffset, typeLimit, subRunLimit, subRunLength, subRunCount, charType, prevType);
      
      nsAutoString runVisualText;
      runVisualText.Assign(aText + start, subRunLength);
      if (PRInt32(runVisualText.Length()) < subRunLength)
        return NS_ERROR_OUT_OF_MEMORY;
      FormatUnicodeText(aPresContext, runVisualText.BeginWriting(), subRunLength,
                        (nsCharType)charType, level & 1);

      aprocessor.SetText(runVisualText.get(), subRunLength, nsBidiDirection(level & 1));
      width = aprocessor.GetWidth();
      totalWidth += width;
      if (level & 1) {
        xOffset -= width;
      }
      if (aMode == MODE_DRAW) {
        aprocessor.DrawText(xOffset, width);
      }

      /*
       * The caller may request to calculate the visual position of one
       * or more characters.
       */
      for(int nPosResolve=0; nPosResolve<aPosResolveCount; ++nPosResolve)
      {
        nsBidiPositionResolve* posResolve = &aPosResolve[nPosResolve];
        /*
         * Did we already resolve this position's visual metric? If so, skip.
         */
        if (posResolve->visualLeftTwips != kNotFound)
           continue;
           
        /*
         * First find out if the logical position is within this run.
         */
        if (start <= posResolve->logicalIndex &&
            start + subRunLength > posResolve->logicalIndex) {
          /*
           * If this run is only one character long, we have an easy case:
           * the visual position is the x-coord of the start of the run
           * less the x-coord of the start of the whole text.
           */
          if (subRunLength == 1) {
            posResolve->visualIndex = visualStart;
            posResolve->visualLeftTwips = xOffset;
            posResolve->visualWidth = width;
          }
          /*
           * Otherwise, we need to measure the width of the run's part
           * which is to the visual left of the index.
           * In other words, the run is broken in two, around the logical index,
           * and we measure the part which is visually left.
           * If the run is right-to-left, this part will span from after the index
           * up to the end of the run; if it is left-to-right, this part will span
           * from the start of the run up to (and inclduing) the character before the index.
           */
          else {
            /*
             * Here is a description of how the width of the current character
             * (posResolve->visualWidth) is calculated:
             *
             * LTR (current char: "P"):
             *    S A M P L E          (logical index: 3, visual index: 3)
             *    ^ (visualLeftPart)
             *    ^ (visualRightSide)
             *    visualLeftLength == 3
             *    ^^^^^^ (subWidth)
             *    ^^^^^^^^ (aprocessor.GetWidth() -- with visualRightSide)
             *          ^^ (posResolve->visualWidth)
             *
             * RTL (current char: "M"):
             *    E L P M A S          (logical index: 2, visual index: 3)
             *        ^ (visualLeftPart)
             *          ^ (visualRightSide)
             *    visualLeftLength == 3
             *    ^^^^^^ (subWidth)
             *    ^^^^^^^^ (aprocessor.GetWidth() -- with visualRightSide)
             *          ^^ (posResolve->visualWidth)
             */
            nscoord subWidth;
            // The position in the text where this run's "left part" begins.
            const PRUnichar* visualLeftPart, *visualRightSide;
            if (level & 1) {
              // One day, son, this could all be replaced with mBidiEngine.GetVisualIndex ...
              posResolve->visualIndex = visualStart + (subRunLength - (posResolve->logicalIndex + 1 - start));
              // Skipping to the "left part".
              visualLeftPart = aText + posResolve->logicalIndex + 1;
              // Skipping to the right side of the current character
              visualRightSide = visualLeftPart - 1;
            }
            else {
              posResolve->visualIndex = visualStart + (posResolve->logicalIndex - start);
              // Skipping to the "left part".
              visualLeftPart = aText + start;
              // In LTR mode this is the same as visualLeftPart
              visualRightSide = visualLeftPart;
            }
            // The delta between the start of the run and the left part's end.
            PRInt32 visualLeftLength = posResolve->visualIndex - visualStart;
            aprocessor.SetText(visualLeftPart, visualLeftLength, nsBidiDirection(level & 1));
            subWidth = aprocessor.GetWidth();
            aprocessor.SetText(visualRightSide, visualLeftLength + 1, nsBidiDirection(level & 1));
            posResolve->visualLeftTwips = xOffset + subWidth;
            posResolve->visualWidth = aprocessor.GetWidth() - subWidth;
          }
        }
      }

      if (!(level & 1)) {
        xOffset += width;
      }

      --subRunCount;
      start = lineOffset;
      subRunLimit = typeLimit;
      subRunLength = typeLimit - lineOffset;
    } // while
    if (level & 1) {
      xOffset = xEndRun;
    }
    
    visualStart += length;
  } // for

  if (aWidth) {
    *aWidth = totalWidth;
  }
  return NS_OK;
}

class NS_STACK_CLASS nsIRenderingContextBidiProcessor : public nsBidiPresUtils::BidiProcessor {
public:
  nsIRenderingContextBidiProcessor(nsIRenderingContext* aCtx,
                                   const nsPoint&       aPt)
                                   : mCtx(aCtx), mPt(aPt) { }

  ~nsIRenderingContextBidiProcessor()
  {
    mCtx->SetRightToLeftText(PR_FALSE);
  }

  virtual void SetText(const PRUnichar* aText,
                       PRInt32          aLength,
                       nsBidiDirection  aDirection)
  {
    mCtx->SetTextRunRTL(aDirection==NSBIDI_RTL);
    mText = aText;
    mLength = aLength;
  }

  virtual nscoord GetWidth()
  {
    nscoord width;
    mCtx->GetWidth(mText, mLength, width, nsnull);
    return width;
  }

  virtual void DrawText(nscoord aXOffset,
                        nscoord)
  {
    mCtx->DrawString(mText, mLength, mPt.x + aXOffset, mPt.y);
  }

private:
  nsIRenderingContext* mCtx;
  nsPoint mPt;
  const PRUnichar* mText;
  PRInt32 mLength;
  nsBidiDirection mDirection;
};

nsresult nsBidiPresUtils::ProcessTextForRenderingContext(const PRUnichar*       aText,
                                                         PRInt32                aLength,
                                                         nsBidiDirection        aBaseDirection,
                                                         nsPresContext*         aPresContext,
                                                         nsIRenderingContext&   aRenderingContext,
                                                         Mode                   aMode,
                                                         nscoord                aX,
                                                         nscoord                aY,
                                                         nsBidiPositionResolve* aPosResolve,
                                                         PRInt32                aPosResolveCount,
                                                         nscoord*               aWidth)
{
  nsIRenderingContextBidiProcessor processor(&aRenderingContext, nsPoint(aX, aY));

  return ProcessText(aText, aLength, aBaseDirection, aPresContext, processor,
                     aMode, aPosResolve, aPosResolveCount, aWidth);
}

PRUint32 nsBidiPresUtils::EstimateMemoryUsed()
{
  PRUint32 size = 0;

  size += sizeof(nsBidiPresUtils);
  size += mBuffer.Length() * sizeof(PRUnichar);
  size += moz_malloc_usable_size(mBidiEngine->mDirPropsMemory);
  size += moz_malloc_usable_size(mBidiEngine->mLevelsMemory);
  size += moz_malloc_usable_size(mBidiEngine->mRunsMemory);

  return size;
}


#endif // IBMBIDI
