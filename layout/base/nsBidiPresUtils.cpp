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

static NS_DEFINE_IID(kInlineFrameCID, NS_INLINE_FRAME_CID);

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
SplitInlineAncestors(nsPresContext* aPresContext,
                      nsIFrame*     aFrame)
{
  nsIPresShell *presShell = aPresContext->PresShell();
  nsIFrame* frame = aFrame;
  nsIFrame* parent = aFrame->GetParent();
  nsIFrame* newFrame = aFrame->GetNextSibling();
  nsIFrame* newParent;

  while (IsBidiSplittable(parent)) {
    nsIFrame* grandparent = parent->GetParent();
    NS_ASSERTION(grandparent, "Couldn't get parent's parent in nsBidiPresUtils::SplitInlineAncestors");
    
    nsresult rv = presShell->FrameConstructor()->
      CreateContinuingFrame(aPresContext, parent, grandparent, &newParent, PR_FALSE);
    if (NS_FAILED(rv)) {
      return rv;
    }
    
    // The new parent adopts the new frame
    frame->SetNextSibling(nsnull);
    rv = newParent->InsertFrames(nsGkAtoms::nextBidi, nsnull, newFrame);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Reparent views as necessary
    rv = nsHTMLContainerFrame::ReparentFrameViewList(aPresContext, newFrame, parent, newParent);
    if (NS_FAILED(rv)) {
      return rv;
    }
    
    // The list name nsGkAtoms::nextBidi would indicate we don't want reflow
    rv = grandparent->InsertFrames(nsGkAtoms::nextBidi, parent, newParent);
    if (NS_FAILED(rv)) {
      return rv;
    }
    
    frame = parent;
    newFrame = newParent;
    parent = grandparent;
  }
  
  return NS_OK;
}

static nsresult
CreateBidiContinuation(nsPresContext* aPresContext,
                       nsIFrame*       aFrame,
                       nsIFrame**      aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  NS_PRECONDITION(aFrame, "null ptr");

  *aNewFrame = nsnull;

  nsIPresShell *presShell = aPresContext->PresShell();
  NS_ASSERTION(presShell, "PresShell must be set on PresContext before calling nsBidiPresUtils::CreateBidiContinuation");

  nsIFrame* parent = aFrame->GetParent();
  NS_ASSERTION(parent, "Couldn't get frame parent in nsBidiPresUtils::CreateBidiContinuation");
  
  nsresult rv = presShell->FrameConstructor()->
    CreateContinuingFrame(aPresContext, aFrame, parent, aNewFrame, PR_FALSE);
  if (NS_FAILED(rv)) {
    return rv;
  }
  
  // The list name nsGkAtoms::nextBidi would indicate we don't want reflow
  rv = parent->InsertFrames(nsGkAtoms::nextBidi, aFrame, *aNewFrame);
  if (NS_FAILED(rv)) {
    return rv;
  }
  
  // Split inline ancestor frames
  rv = SplitInlineAncestors(aPresContext, aFrame);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

static void
AdvanceLineIteratorToFrame(nsIFrame* aFrame,
                           nsIFrame* aBlockFrame,
                           nsBlockFrame::line_iterator& aLine,
                           nsIFrame*& aPrevFrame,
                           const nsBlockFrame::line_iterator& aEndLines)
{
  // Advance aLine to the line containing aFrame
  nsIFrame* child = aFrame;
  nsIFrame* parent = child->GetParent();
  while (parent && parent != aBlockFrame) {
    child = parent;
    parent = child->GetParent();
  }
  NS_ASSERTION (parent, "aFrame is not a descendent of aBlockFrame");
  while (aLine != aEndLines && !aLine->ContainsAfter(aPrevFrame, child, aLine, aEndLines)) {
    ++aLine;
    aPrevFrame = nsnull;
  }
  aPrevFrame = child;
  NS_ASSERTION (aLine != aEndLines, "frame not found on any line");
}

/*
 * Overview of the implementation of Resolve():
 *
 *  Walk through the descendants of aBlockFrame and build:
 *   * mLogicalArray: an nsVoidArray of nsIFrame* pointers in logical order
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
 *  correlate them with the frames indexed in mLogicalArray, setting the
 *  baseLevel, embeddingLevel, and charType properties according to the results
 *  returned by the Bidi engine and CalculateCharType().
 *
 *  The rendering layer requires each text frame to contain text in only one
 *  direction and of only one character type, so we may need to call
 *  EnsureBidiContinuation() to split frames. We may also need to call
 *  RemoveBidiContinuation() to convert frames created by
 *  EnsureBidiContinuation() in previous reflows into fluid continuations.
 */
nsresult
nsBidiPresUtils::Resolve(nsPresContext* aPresContext,
                         nsBlockFrame*   aBlockFrame,
                         nsIFrame*       aFirstChild,
                         PRBool          aIsVisualFormControl)
{
  mLogicalFrames.Clear();
  mContentToFrameIndex.Clear();
  
  nsIPresShell* shell = aPresContext->PresShell();
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
  mSuccess = InitLogicalArray(aPresContext, aFirstChild, nsnull, PR_TRUE);

  if (text->mUnicodeBidi == NS_STYLE_UNICODE_BIDI_OVERRIDE) {
    nsIFrame* directionalFrame = NS_NewDirectionalFrame(shell, styleContext, kPDF);
    if (directionalFrame) {
      mLogicalFrames.AppendElement(directionalFrame);
    }
  }
  if (NS_FAILED(mSuccess) ) {
    return mSuccess;
  }

  CreateBlockBuffer(aPresContext);

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

  PRBool isVisual;
  if (aIsVisualFormControl) {
    isVisual = PR_FALSE;
  } else {
    isVisual = aPresContext->IsVisualMode();
  }
  mSuccess = mBidiEngine->CountRuns(&runCount);
  if (NS_FAILED(mSuccess) ) {
    return mSuccess;
  }
  PRInt32                  runLength      = 0;
  PRInt32                  fragmentLength = 0;
  PRInt32                  temp;
  PRInt32                  frameIndex     = -1;
  PRInt32                  frameCount     = mLogicalFrames.Count();
  PRInt32                  contentOffset  = 0;   // offset within current frame
  PRInt32                  lineOffset     = 0;   // offset within mBuffer
  PRInt32                  logicalLimit   = 0;
  PRInt32                  numRun         = -1;
  PRUint8                  charType;
  PRUint8                  prevType       = eCharType_LeftToRight;
  PRBool                   isTextFrame    = PR_FALSE;
  nsIFrame*                frame = nsnull;
  nsIFrame*                nextBidi;
  nsIContent*              content = nsnull;
  const nsTextFragment*    fragment;
  nsIAtom*                 frameType = nsnull;

  nsPropertyTable *propTable = aPresContext->PropertyTable();

  nsBlockFrame::line_iterator line = aBlockFrame->begin_lines();
  nsBlockFrame::line_iterator endLines = aBlockFrame->end_lines();
  nsIFrame* prevFrame = nsnull;
  PRBool lineNeedsUpdate = PR_FALSE;
  
  for (; ;) {
    if (fragmentLength <= 0) {
      if (++frameIndex >= frameCount) {
        break;
      }
      contentOffset = 0;
      
      frame = (nsIFrame*) (mLogicalFrames[frameIndex]);
      frameType = frame->GetType();
      lineNeedsUpdate = PR_TRUE;
      if (nsGkAtoms::textFrame == frameType) {
        content = frame->GetContent();
        if (!content) {
          mSuccess = NS_OK;
          break;
        }
        fragment = content->GetText();
        if (!fragment) {
          mSuccess = NS_ERROR_FAILURE;
          break;
        }
        fragmentLength = fragment->GetLength();
        isTextFrame = PR_TRUE;
      } // if text frame
      else {
        isTextFrame = PR_FALSE;
        fragmentLength = 1;
      }
    } // if (fragmentLength <= 0)
    if (runLength <= 0) {
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
      propTable->SetProperty(frame, nsGkAtoms::embeddingLevel,
                             NS_INT32_TO_PTR(embeddingLevel), nsnull, nsnull);
      propTable->SetProperty(frame, nsGkAtoms::baseLevel,
                             NS_INT32_TO_PTR(paraLevel), nsnull, nsnull);
      if (isTextFrame) {
        PRInt32 typeLimit = PR_MIN(logicalLimit, lineOffset + fragmentLength);
        CalculateCharType(lineOffset, typeLimit, logicalLimit, runLength,
                           runCount, charType, prevType);
        // IBMBIDI - Egypt - Start
        propTable->SetProperty(frame, nsGkAtoms::charType,
                               NS_INT32_TO_PTR(charType), nsnull, nsnull);
        // IBMBIDI - Egypt - End

        if ( (runLength > 0) && (runLength < fragmentLength) ) {
          if (!EnsureBidiContinuation(aPresContext, frame,
                                      &nextBidi, frameIndex) ) {
            break;
          }
          if (lineNeedsUpdate) {
            AdvanceLineIteratorToFrame(frame, aBlockFrame, line, prevFrame, endLines);
            lineNeedsUpdate = PR_FALSE;
          }
          line->MarkDirty();
          frame->AdjustOffsetsForBidi(contentOffset, contentOffset + runLength);
          frame = nextBidi;
          contentOffset += runLength;
        } // if (runLength < fragmentLength)
        else {
          frame->AdjustOffsetsForBidi(contentOffset, contentOffset + fragmentLength);
          PRInt32 newIndex = 0;
          mContentToFrameIndex.Get(content, &newIndex);
          if (newIndex > frameIndex) {
            RemoveBidiContinuation(aPresContext, frame,
                                   frameIndex, newIndex, temp);
            if (lineNeedsUpdate) {
              AdvanceLineIteratorToFrame(frame, aBlockFrame, line, prevFrame, endLines);
              lineNeedsUpdate = PR_FALSE;
            }
            line->MarkDirty();
            runLength -= temp;
            fragmentLength -= temp;
            lineOffset += temp;
            frameIndex = newIndex;
          }
        }
      } // isTextFrame
      else {
        ++lineOffset;
      }
    } // not directionalFrame
    temp = runLength;
    runLength -= fragmentLength;
    fragmentLength -= temp;

    // If the frame is at the end of a run, split all ancestor inlines that need splitting.
    if (frame && fragmentLength <= 0 && runLength <= 0) {
      // As long as we're on the last sibling, the parent doesn't have to be split.
      nsIFrame* child = frame;
      nsIFrame* parent = frame->GetParent();
      while (parent &&
             IsBidiSplittable(parent) &&
             !child->GetNextSibling()) {
        child = parent;
        parent = child->GetParent();
      }
      if (parent && IsBidiSplittable(parent))
        SplitInlineAncestors(aPresContext, child);
    }
  } // for
  return mSuccess;
}

// Should this frame be treated as a leaf (e.g. when building mLogicalArray)?
PRBool IsBidiLeaf(nsIFrame* aFrame) {
  nsIFrame* kid = aFrame->GetFirstChild(nsnull);
  // Need the IsBlockLevel() check because nsFirstLetterFrame is
  // always of type eBidiInlineContainer, even if it's floating.
  return !kid
    || !aFrame->IsFrameOfType(nsIFrame::eBidiInlineContainer)
    || aFrame->GetStyleDisplay()->IsBlockLevel();
}

nsresult
nsBidiPresUtils::InitLogicalArray(nsPresContext* aPresContext,
                                  nsIFrame*       aCurrentFrame,
                                  nsIFrame*       aNextInFlow,
                                  PRBool          aAddMarkers)
{
  nsIFrame*             frame;
  nsIFrame*             directionalFrame;
  nsresult              res = NS_OK;

  nsIPresShell* shell = aPresContext->PresShell();
  nsStyleContext* styleContext;

  for (frame = aCurrentFrame;
       frame && frame != aNextInFlow;
       frame = frame->GetNextSibling()) {
    directionalFrame = nsnull;

    if (aAddMarkers &&
        frame->IsFrameOfType(nsIFrame::eBidiInlineContainer) &&
        !frame->GetPrevContinuation()) {
      const nsStyleVisibility* vis = frame->GetStyleVisibility();
      const nsStyleTextReset* text = frame->GetStyleTextReset();
      switch (text->mUnicodeBidi) {
        case NS_STYLE_UNICODE_BIDI_NORMAL:
          break;
        case NS_STYLE_UNICODE_BIDI_EMBED:
          styleContext = frame->GetStyleContext();

          if (NS_STYLE_DIRECTION_RTL == vis->mDirection) {
            directionalFrame = NS_NewDirectionalFrame(shell, styleContext, kRLE);
          }
          else if (NS_STYLE_DIRECTION_LTR == vis->mDirection) {
            directionalFrame = NS_NewDirectionalFrame(shell, styleContext, kLRE);
          }
          break;
        case NS_STYLE_UNICODE_BIDI_OVERRIDE:
          styleContext = frame->GetStyleContext();

          if (NS_STYLE_DIRECTION_RTL == vis->mDirection) {
            directionalFrame = NS_NewDirectionalFrame(shell, styleContext, kRLO);
          }
          else if (NS_STYLE_DIRECTION_LTR == vis->mDirection) {
            directionalFrame = NS_NewDirectionalFrame(shell, styleContext, kLRO);
          }
          break;
      }

      // Create a directional frame before the first frame of an
      // element specifying embedding or override
      if (directionalFrame) {
        mLogicalFrames.AppendElement(directionalFrame);
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
        mContentToFrameIndex.Put(content, mLogicalFrames.Count());
      }
      mLogicalFrames.AppendElement(frame);
    }
    else {
      nsIFrame* kid = frame->GetFirstChild(nsnull);
      res = InitLogicalArray(aPresContext, kid, aNextInFlow, aAddMarkers);
    }

    // If the element is attributed by dir, indicate direction pop (add PDF frame)
    if (directionalFrame && !frame->GetNextContinuation()) {
      directionalFrame = NS_NewDirectionalFrame(shell, styleContext, kPDF);
   
      // Create a directional frame after the last frame of an
      // element specifying embedding or override
      if (directionalFrame) {
        mLogicalFrames.AppendElement(directionalFrame);
      }
    }
  } // for
  return res;       
}

void
nsBidiPresUtils::CreateBlockBuffer(nsPresContext* aPresContext)
{
  mBuffer.SetLength(0);

  nsIFrame*                 frame;
  nsIContent*               prevContent = nsnull;
  PRUint32                  i;
  PRUint32                  count = mLogicalFrames.Count();

  for (i = 0; i < count; i++) {
    frame = (nsIFrame*) (mLogicalFrames[i]);
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
      mBuffer.Append( (PRUnichar) kLineSeparator);
    }
    else if (nsGkAtoms::directionalFrame == frameType) {
      nsDirectionalFrame* dirFrame;
      frame->QueryInterface(NS_GET_IID(nsDirectionalFrame),
                            (void**) &dirFrame);
      mBuffer.Append(dirFrame->GetChar() );
    }
    else { // not text frame
      // See the Unicode Bidi Algorithm:
      // "...inline objects (such as graphics) are treated as if they are ... U+FFFC"
      mBuffer.Append( (PRUnichar) kObjectSubstitute);
    }
  }
  // XXX: TODO: Handle preformatted text ('\n')
  mBuffer.ReplaceChar("\t\r\n", kSpace);
}

void
nsBidiPresUtils::ReorderFrames(nsPresContext*       aPresContext,
                               nsIRenderingContext* aRendContext,
                               nsIFrame*            aFirstFrameOnLine,
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
  RepositionInlineFrames(aPresContext, aRendContext, aFirstFrameOnLine, isReordered);
}

nsresult
nsBidiPresUtils::Reorder(PRBool& aReordered, PRBool& aHasRTLFrames)
{
  aReordered = PR_FALSE;
  aHasRTLFrames = PR_FALSE;
  PRInt32 count = mLogicalFrames.Count();

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
    frame = (nsIFrame*) (mLogicalFrames[i]);
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
    firstLeaf = firstLeaf->GetFirstChild(nsnull);
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

  nsIFrame* testFrame;
  aFrame->QueryInterface(kInlineFrameCID, (void**)&testFrame);

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
  nsMargin margin;
  aFrame->GetMargin(margin);
  if (isLeftMost)
    aLeft += margin.left;

  nscoord start = aLeft;

  if (!IsBidiLeaf(aFrame))
  {
    nscoord x = 0;
    nsMargin borderPadding;
    aFrame->GetBorderAndPadding(borderPadding);
    if (isLeftMost) {
      x += borderPadding.left;
    }

    // If aIsOddLevel is true, so we need to traverse the child list
    // in reverse order, to make it O(n) we store the list locally and
    // iterate the list reversely
    nsVoidArray childList;
    nsIFrame *frame = aFrame->GetFirstChild(nsnull);
    if (frame && aIsOddLevel) {
      childList.AppendElement(nsnull);
      while (frame) {
        childList.AppendElement(frame);
        frame = frame->GetNextSibling();
      }
      frame = (nsIFrame*)childList[childList.Count() - 1];
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
                (nsIFrame*)childList[childList.Count() - index - 1] :
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
nsBidiPresUtils::RepositionInlineFrames(nsPresContext*       aPresContext,
                                        nsIRenderingContext* aRendContext,
                                        nsIFrame*            aFirstChild,
                                        PRBool               aReordered) const
{
  nsMargin margin;
  const nsStyleVisibility* vis = aFirstChild->GetStyleVisibility();
  PRBool isLTR = (NS_STYLE_DIRECTION_LTR == vis->mDirection);
  nscoord leftSpace = 0;

  aFirstChild->GetMargin(margin);
  if (!aFirstChild->GetPrevContinuation())
    leftSpace = isLTR ? margin.left : margin.right;

  nscoord left = aFirstChild->GetPosition().x - leftSpace;
  nsIFrame* frame;
  PRInt32 count = mVisualFrames.Count();
  PRInt32 index;
  nsContinuationStates continuationStates;

  continuationStates.Init();

  // Initialize continuation states to (nsnull, 0) for
  // each frame on the line.
  for (index = 0; index < count; index++) {
    InitContinuationStates((nsIFrame*)mVisualFrames[index],
                           &continuationStates);
  }

  // Reposition frames in visual order
  for (index = 0; index < count; index++) {
    frame = (nsIFrame*) (mVisualFrames[index]);
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
  PRInt32 count = mLogicalFrames.Count();
  
  if (aFirstVisual) {
    *aFirstVisual = (nsIFrame*)mVisualFrames[0];
  }
  if (aLastVisual) {
    *aLastVisual = (nsIFrame*)mVisualFrames[count-1];
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
  PRInt32 count = mVisualFrames.Count();

  if (aFrame == nsnull)
    return (nsIFrame*)mVisualFrames[0];
  
  for (PRInt32 i = 0; i < count - 1; i++) {
    if ((nsIFrame*)mVisualFrames[i] == aFrame) {
      return (nsIFrame*)mVisualFrames[i+1];
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
  PRInt32 count = mVisualFrames.Count();
  
  if (aFrame == nsnull)
    return (nsIFrame*)mVisualFrames[count-1];
  
  for (PRInt32 i = 1; i < count; i++) {
    if ((nsIFrame*)mVisualFrames[i] == aFrame) {
      return (nsIFrame*)mVisualFrames[i-1];
    }
  }
  
  return nsnull;
}

PRBool
nsBidiPresUtils::EnsureBidiContinuation(nsPresContext* aPresContext,
                                        nsIFrame*       aFrame,
                                        nsIFrame**      aNewFrame,
                                        PRInt32&        aFrameIndex)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  NS_PRECONDITION(aFrame, "aFrame is null");

  *aNewFrame = nsnull;
  nsBidiLevel embeddingLevel = NS_GET_EMBEDDING_LEVEL(aFrame);
  nsBidiLevel baseLevel = NS_GET_BASE_LEVEL(aFrame);
  nsCharType charType = (nsCharType)NS_PTR_TO_INT32(aFrame->GetProperty(nsGkAtoms::charType));
  
  // Skip fluid continuations
  while (aFrameIndex + 1 < mLogicalFrames.Count()) {
    nsIFrame* frame = (nsIFrame*)mLogicalFrames[aFrameIndex + 1];
    if (frame->GetPrevInFlow() != aFrame) {
      // If we found a non-fluid continuation, use it
      if (frame->GetPrevContinuation() == aFrame) {
        *aNewFrame = frame;
        aFrameIndex++;
      }
      break;
    }
    frame->SetProperty(nsGkAtoms::embeddingLevel, NS_INT32_TO_PTR(embeddingLevel));
    frame->SetProperty(nsGkAtoms::baseLevel, NS_INT32_TO_PTR(baseLevel));
    frame->SetProperty(nsGkAtoms::charType, NS_INT32_TO_PTR(charType));
    frame->AddStateBits(NS_FRAME_IS_BIDI);
    aFrameIndex++;
    aFrame = frame;
  }
  
  if (!*aNewFrame) {
    mSuccess = CreateBidiContinuation(aPresContext, aFrame, aNewFrame);
    if (NS_FAILED(mSuccess) ) {
      return PR_FALSE;
    }
  }

  return PR_TRUE;
}

void
nsBidiPresUtils::RemoveBidiContinuation(nsPresContext* aPresContext,
                                        nsIFrame*       aFrame,
                                        PRInt32         aFirstIndex,
                                        PRInt32         aLastIndex,
                                        PRInt32&        aOffset) const
{
  aOffset = 0;

  nsresult rv;
  nsBidiLevel embeddingLevel = (nsCharType)NS_PTR_TO_INT32(aFrame->GetProperty(nsGkAtoms::embeddingLevel, &rv));
  NS_ASSERTION(NS_SUCCEEDED(rv), "embeddingLevel attribute missing from aFrame");
  nsBidiLevel baseLevel = (nsCharType)NS_PTR_TO_INT32(aFrame->GetProperty(nsGkAtoms::baseLevel, &rv));
  NS_ASSERTION(NS_SUCCEEDED(rv), "baseLevel attribute missing from aFrame");
  nsCharType charType = (nsCharType)NS_PTR_TO_INT32(aFrame->GetProperty(nsGkAtoms::charType, &rv));
  NS_ASSERTION(NS_SUCCEEDED(rv), "charType attribute missing from aFrame");
  
  for (PRInt32 index = aFirstIndex + 1; index <= aLastIndex; index++) {
    nsIFrame* frame = (nsIFrame*) mLogicalFrames[index];
    if (nsGkAtoms::directionalFrame == frame->GetType()) {
      frame->Destroy();
      ++aOffset;
    }
    else {
      // Make the frame and its continuation ancestors fluid,
      // so they can be reused or deleted by normal reflow code
      frame->SetProperty(nsGkAtoms::embeddingLevel, NS_INT32_TO_PTR(embeddingLevel));
      frame->SetProperty(nsGkAtoms::baseLevel, NS_INT32_TO_PTR(baseLevel));
      frame->SetProperty(nsGkAtoms::charType, NS_INT32_TO_PTR(charType));
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
                                   PRBool           aIsOddLevel,
                                   PRBool           aIsBidiSystem,
                                   PRBool           aIsNewTextRunSystem)
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

    case IBMBIDI_NUMERAL_NOMINAL:
    default:
      break;
  }

  PRBool doReverse = PR_FALSE;
  PRBool doShape = PR_FALSE;

  if (!aIsNewTextRunSystem) {
    if (aIsBidiSystem) {
      if ( (CHARTYPE_IS_RTL(aCharType)) ^ (aIsOddLevel) )
        doReverse = PR_TRUE;
    }
    else {
      if (aIsOddLevel)
        doReverse = PR_TRUE;
      if (eCharType_RightToLeftArabic == aCharType) 
        doShape = PR_TRUE;
    }
  }

  if (doReverse || doShape) {
    PRInt32    newLen;

    if (mBuffer.Length() < aTextLength) {
      if (!EnsureStringLength(mBuffer, aTextLength))
        return NS_ERROR_OUT_OF_MEMORY;
    }
    PRUnichar* buffer = mBuffer.BeginWriting();

    if (doReverse) {
      rv = mBidiEngine->WriteReverse(aText, aTextLength, buffer,
                                     NSBIDI_DO_MIRRORING, &newLen);
      if (NS_SUCCEEDED(rv) ) {
        aTextLength = newLen;
        memcpy(aText, buffer, aTextLength * sizeof(PRUnichar) );
      }
    }
    if (doShape) {
      rv = ArabicShaping(aText, aTextLength, buffer, (PRUint32 *)&newLen,
                         PR_FALSE, PR_FALSE);
      if (NS_SUCCEEDED(rv) ) {
        aTextLength = newLen;
        memcpy(aText, buffer, aTextLength * sizeof(PRUnichar) );
      }
    }
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
    if (mBidiEngine->IsBidiControl((PRUint32)aText[i])) {
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

nsresult nsBidiPresUtils::GetBidiEngine(nsBidi** aBidiEngine)
{
  nsresult rv = NS_ERROR_FAILURE;
  if (mBidiEngine) {
    *aBidiEngine = mBidiEngine;
    rv = NS_OK;
  }
  return rv; 
}

nsresult nsBidiPresUtils::ProcessText(const PRUnichar*       aText,
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
  NS_ASSERTION((aPosResolve == nsnull) != (aPosResolveCount > 0), "Incorrect aPosResolve / aPosResolveCount arguments");

  PRInt32 runCount;

  mBuffer.Assign(aText);

  nsresult rv = mBidiEngine->SetPara(mBuffer.get(), aLength, aBaseDirection, nsnull);
  if (NS_FAILED(rv))
    return rv;

  rv = mBidiEngine->CountRuns(&runCount);
  if (NS_FAILED(rv))
    return rv;

  nscoord width, xEndRun, xStartText = aX;
  PRBool isRTL = PR_FALSE;
  nscoord totalWidth = 0;
  PRInt32 i, start, limit, length;
  PRUint32 visualStart = 0;
  PRUint8 charType;
  PRUint8 prevType = eCharType_LeftToRight;
  nsBidiLevel level;

  PRUint32 hints = 0;
  aRenderingContext.GetHints(hints);
  PRBool isBidiSystem = (hints & NS_RENDERING_HINT_BIDI_REORDERING);
      
  for(int nPosResolve=0; nPosResolve < aPosResolveCount; ++nPosResolve)
  {
    aPosResolve[nPosResolve].visualIndex = kNotFound;
    aPosResolve[nPosResolve].visualLeftTwips = kNotFound;
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
    PRInt32 typeLimit = PR_MIN(limit, aLength);
    PRInt32 subRunCount = 1;
    PRInt32 subRunLimit = typeLimit;

    /*
     * If |level| is even, i.e. the direction of the run is left-to-right, we
     * render the subruns from left to right and increment the x-coordinate
     * |aX| by the width of each subrun after rendering.
     *
     * If |level| is odd, i.e. the direction of the run is right-to-left, we
     * render the subruns from right to left. We begin by incrementing |aX| by
     * the width of the whole run, and then decrement it by the width of each
     * subrun before rendering. After rendering all the subruns, we restore the
     * x-coordinate of the end of the run for the start of the next run.
     */
    aRenderingContext.SetTextRunRTL(level & 1);

    if (level & 1) {
      aRenderingContext.GetWidth(aText + start, subRunLength, width, nsnull);
      aX += width;
      xEndRun = aX;
    }

    while (subRunCount > 0) {
      // CalculateCharType can increment subRunCount if the run
      // contains mixed character types
      CalculateCharType(lineOffset, typeLimit, subRunLimit, subRunLength, subRunCount, charType, prevType);

      if (eCharType_RightToLeftArabic == charType) {
        isBidiSystem = (hints & NS_RENDERING_HINT_ARABIC_SHAPING);
      }
      if (isBidiSystem && (CHARTYPE_IS_RTL(charType) ^ isRTL) ) {
        // set reading order into DC
        isRTL = !isRTL;
        aRenderingContext.SetRightToLeftText(isRTL);
      }
      
      nsAutoString runVisualText;
      runVisualText.Assign(aText + start, subRunLength);
      if (runVisualText.Length() < subRunLength)
        return NS_ERROR_OUT_OF_MEMORY;
      FormatUnicodeText(aPresContext, runVisualText.BeginWriting(), subRunLength,
                        (nsCharType)charType, level & 1,
                        isBidiSystem, (hints & NS_RENDERING_HINT_NEW_TEXT_RUNS) != 0);

      aRenderingContext.GetWidth(runVisualText.get(), subRunLength, width, nsnull);
      totalWidth += width;
      if (level & 1) {
        aX -= width;
      }
      if (aMode == MODE_DRAW) {
        aRenderingContext.DrawString(runVisualText.get(), subRunLength, aX, aY);
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
           * less the x-coord of the start of the whole text (saved in xStartText).
           */
          if (subRunLength == 1) {
            posResolve->visualIndex = visualStart;
            posResolve->visualLeftTwips = aX - xStartText;
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
            nscoord subWidth;
            // The position in the text where this run's "left part" begins.
            const PRUnichar* visualLeftPart;
            if (level & 1) {
              // One day, son, this could all be replaced with mBidiEngine.GetVisualIndex ...
              posResolve->visualIndex = visualStart + (subRunLength - (posResolve->logicalIndex + 1 - start));
              // Skipping to the "left part".
              visualLeftPart = aText + posResolve->logicalIndex + 1;
            }
            else {
              posResolve->visualIndex = visualStart + (posResolve->logicalIndex - start);
              // Skipping to the "left part".
              visualLeftPart = aText + start;
            }
            // The delta between the start of the run and the left part's end.
            PRInt32 visualLeftLength = posResolve->visualIndex - visualStart;
            aRenderingContext.GetWidth(visualLeftPart,
                                       visualLeftLength,
                                       subWidth, nsnull);
            posResolve->visualLeftTwips = aX + subWidth - xStartText;
          }
        }
      }

      if (!(level & 1)) {
        aX += width;
      }

      --subRunCount;
      start = lineOffset;
      subRunLimit = typeLimit;
      subRunLength = typeLimit - lineOffset;
    } // while
    if (level & 1) {
      aX = xEndRun;
    }
    
    visualStart += length;
  } // for

  // Restore original reading order
  if (isRTL) {
    aRenderingContext.SetRightToLeftText(PR_FALSE);
  }
  if (aWidth) {
    *aWidth = totalWidth;
  }
  return NS_OK;
}
  
nsresult
nsBidiPresUtils::ReorderUnicodeText(PRUnichar*       aText,
                                    PRInt32&         aTextLength,
                                    nsCharType       aCharType,
                                    PRBool           aIsOddLevel,
                                    PRBool           aIsBidiSystem,
                                    PRBool           aIsNewTextRunSystem)
{
  NS_ASSERTION(aIsOddLevel == 0 || aIsOddLevel == 1, "aIsOddLevel should be 0 or 1");
  nsresult rv = NS_OK;
  PRBool doReverse = PR_FALSE;

  if (!aIsNewTextRunSystem) {
    if (aIsBidiSystem) {
      if ( (CHARTYPE_IS_RTL(aCharType)) ^ (aIsOddLevel) )
        doReverse = PR_TRUE;
    }
    else {
      if (aIsOddLevel)
        doReverse = PR_TRUE;
    }
  }

  if (doReverse) {
    PRInt32    newLen;

    if (mBuffer.Length() < aTextLength) {
      if (!EnsureStringLength(mBuffer, aTextLength))
        return NS_ERROR_OUT_OF_MEMORY;
    }
    PRUnichar* buffer = mBuffer.BeginWriting();

    if (doReverse) {
      rv = mBidiEngine->WriteReverse(aText, aTextLength, buffer,
                                     NSBIDI_DO_MIRRORING, &newLen);
      if (NS_SUCCEEDED(rv) ) {
        aTextLength = newLen;
        memcpy(aText, buffer, aTextLength * sizeof(PRUnichar) );
      }
    }
  }
  return rv;
}

#endif // IBMBIDI
