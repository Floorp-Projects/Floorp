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
#include "nsITextContent.h"
#include "nsTextFragment.h"
#include "nsLayoutAtoms.h"
#include "nsPresContext.h"
#include "nsIRenderingContext.h"
#include "nsIServiceManager.h"
#include "nsFrameManager.h"
#include "nsBidiFrames.h"
#include "nsBidiUtils.h"

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

nsresult
NS_NewContinuingTextFrame(nsIPresShell* aPresShell, nsIFrame** aResult);
nsresult
NS_NewDirectionalFrame(nsIFrame** aNewFrame, PRUnichar aChar);

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

static nsresult
CreateBidiContinuation(nsPresContext* aPresContext,
                       nsIContent*     aContent,
                       nsIFrame*       aFrame,
                       nsIFrame**      aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");

  *aNewFrame = nsnull;

  NS_PRECONDITION(aFrame, "null ptr");

  nsIPresShell *presShell = aPresContext->PresShell();

  NS_ASSERTION(presShell, "PresShell must be set on PresContext before calling nsBidiPresUtils::CreateBidiContinuation");

  NS_NewContinuingTextFrame(presShell, aNewFrame);
  if (!(*aNewFrame) ) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsStyleContext* styleContext = aFrame->GetStyleContext();

  NS_ASSERTION(styleContext, "Frame has no styleContext in nsBidiPresUtils::CreateBidiContinuation");
  
  nsIFrame* parent = aFrame->GetParent();
  NS_ASSERTION(parent, "Couldn't get frame parent in nsBidiPresUtils::CreateBidiContinuation");

  (*aNewFrame)->Init(aPresContext, aContent, parent, styleContext, nsnull);

  // XXX: TODO: Instead, create and insert entire frame list
  (*aNewFrame)->SetNextSibling(nsnull);

  // The list name nsLayoutAtoms::nextBidi would indicate we don't want reflow
  parent->InsertFrames(aPresContext, *presShell, nsLayoutAtoms::nextBidi,
                       aFrame, *aNewFrame);

  return NS_OK;
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
 *  RemoveBidiContinuation() to delete frames created by
 *  EnsureBidiContinuation() in previous reflows.
 */
nsresult
nsBidiPresUtils::Resolve(nsPresContext* aPresContext,
                         nsIFrame*       aBlockFrame,
                         nsIFrame*       aFirstChild,
                         PRBool&         aForceReflow,
                         PRBool          aIsVisualFormControl)
{
  aForceReflow = PR_FALSE;
  mLogicalFrames.Clear();
  mContentToFrameIndex.Clear();

  // handle bidi-override being set on the block itself before calling
  // InitLogicalArray.
  const nsStyleVisibility* vis = aBlockFrame->GetStyleVisibility();
  const nsStyleTextReset* text = aBlockFrame->GetStyleTextReset();

  if (text->mUnicodeBidi == NS_STYLE_UNICODE_BIDI_OVERRIDE) {
    nsresult rv = NS_OK;
    nsIFrame *directionalFrame = nsnull;
    if (NS_STYLE_DIRECTION_RTL == vis->mDirection) {
      rv = NS_NewDirectionalFrame(&directionalFrame, kRLO);
    }
    else if (NS_STYLE_DIRECTION_LTR == vis->mDirection) {
      rv = NS_NewDirectionalFrame(&directionalFrame, kLRO);
    }
    if (directionalFrame && NS_SUCCEEDED(rv)) {
      mLogicalFrames.AppendElement(directionalFrame);
    }
  }
  mSuccess = InitLogicalArray(aPresContext, aFirstChild, nsnull, PR_TRUE);
  if (text->mUnicodeBidi == NS_STYLE_UNICODE_BIDI_OVERRIDE) {
    nsIFrame *directionalFrame = nsnull;
    nsresult rv = NS_NewDirectionalFrame(&directionalFrame, kPDF);
    if (directionalFrame && NS_SUCCEEDED(rv)) {
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
  nsCOMPtr<nsITextContent> textContent;
  const nsTextFragment*    fragment;
  nsIAtom*                 frameType = nsnull;

  for (; ;) {
    if (fragmentLength <= 0) {
      if (++frameIndex >= frameCount) {
        break;
      }
      contentOffset = 0;
      
      frame = (nsIFrame*) (mLogicalFrames[frameIndex]);
      frameType = frame->GetType();
      if (nsLayoutAtoms::textFrame == frameType) {
        content = frame->GetContent();
        if (!content) {
          mSuccess = NS_OK;
          break;
        }
        textContent = do_QueryInterface(content, &mSuccess);
        if (NS_FAILED(mSuccess) || (!textContent) ) {
          break;
        }
        fragment = textContent->Text();
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

    if (nsLayoutAtoms::directionalFrame == frameType) {
      delete frame;
      ++lineOffset;
    }
    else {
      frame->SetProperty(nsLayoutAtoms::embeddingLevel,
                         NS_INT32_TO_PTR(embeddingLevel));
      frame->SetProperty(nsLayoutAtoms::baseLevel,
                         NS_INT32_TO_PTR(paraLevel));
      if (isTextFrame) {
        PRInt32 typeLimit = PR_MIN(logicalLimit, lineOffset + fragmentLength);
        CalculateCharType(lineOffset, typeLimit, logicalLimit, runLength,
                           runCount, charType, prevType);
        // IBMBIDI - Egypt - Start
        frame->SetProperty(nsLayoutAtoms::charType,
                           NS_INT32_TO_PTR(charType));
        // IBMBIDI - Egypt - End

        if ( (runLength > 0) && (runLength < fragmentLength) ) {
          if (!EnsureBidiContinuation(aPresContext, content, frame,
                                      &nextBidi, frameIndex) ) {
            break;
          }
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
            aForceReflow = PR_TRUE;
            runLength -= temp;
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
  } // for
  return mSuccess;
}

nsresult
nsBidiPresUtils::InitLogicalArray(nsPresContext* aPresContext,
                                  nsIFrame*       aCurrentFrame,
                                  nsIFrame*       aNextInFlow,
                                  PRBool          aAddMarkers)
{
  nsIFrame*             frame;
  nsIFrame*             directionalFrame;
  nsresult              rv;
  nsresult              res = NS_OK;

  for (frame = aCurrentFrame;
       frame && frame != aNextInFlow;
       frame = frame->GetNextSibling()) {
    rv = NS_ERROR_FAILURE;
    const nsStyleDisplay* display = frame->GetStyleDisplay();
    
    if (aAddMarkers && !display->IsBlockLevel() ) {
      const nsStyleVisibility* vis = frame->GetStyleVisibility();
      const nsStyleTextReset* text = frame->GetStyleTextReset();
      switch (text->mUnicodeBidi) {
        case NS_STYLE_UNICODE_BIDI_NORMAL:
          break;
        case NS_STYLE_UNICODE_BIDI_EMBED:
          if (NS_STYLE_DIRECTION_RTL == vis->mDirection) {
            rv = NS_NewDirectionalFrame(&directionalFrame, kRLE);
          }
          else if (NS_STYLE_DIRECTION_LTR == vis->mDirection) {
            rv = NS_NewDirectionalFrame(&directionalFrame, kLRE);
          }
          break;
        case NS_STYLE_UNICODE_BIDI_OVERRIDE:
          if (NS_STYLE_DIRECTION_RTL == vis->mDirection) {
            rv = NS_NewDirectionalFrame(&directionalFrame, kRLO);
          }
          else if (NS_STYLE_DIRECTION_LTR == vis->mDirection) {
            rv = NS_NewDirectionalFrame(&directionalFrame, kLRO);
          }
          break;
      }
      if (NS_SUCCEEDED(rv) ) {
        mLogicalFrames.AppendElement(directionalFrame);
      }
    }

    nsIAtom* frameType = frame->GetType();

    if ( (!display->IsBlockLevel() )
        && ( (nsLayoutAtoms::inlineFrame == frameType)
          || (nsLayoutAtoms::positionedInlineFrame == frameType)
          || (nsLayoutAtoms::letterFrame == frameType)
          || (nsLayoutAtoms::blockFrame == frameType) ) ) {
      nsIFrame* kid = frame->GetFirstChild(nsnull);
      res = InitLogicalArray(aPresContext, kid, aNextInFlow, aAddMarkers);
    }
    else {
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

    // If the element is attributed by dir, indicate direction pop (add PDF frame)
    if (NS_SUCCEEDED(rv) ) {
      rv = NS_NewDirectionalFrame(&directionalFrame, kPDF);
      if (NS_SUCCEEDED(rv) ) {
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
  nsCOMPtr<nsITextContent>  textContent;
  PRUint32                  i;
  PRUint32                  count = mLogicalFrames.Count();

  for (i = 0; i < count; i++) {
    frame = (nsIFrame*) (mLogicalFrames[i]);
    nsIAtom* frameType = frame->GetType();

    if (nsLayoutAtoms::textFrame == frameType) {
      nsIContent* content = frame->GetContent();
      if (!content) {
        mSuccess = NS_OK;
        break;
      }
      if (content == prevContent) {
        continue;
      }
      prevContent = content;
      textContent = do_QueryInterface(content, &mSuccess);
      if ( (NS_FAILED(mSuccess) ) || (!textContent) ) {
        break;
      }
      textContent->Text()->AppendTo(mBuffer);
    }
    else if (nsLayoutAtoms::brFrame == frameType) { // break frame
      // Append line separator
      mBuffer.Append( (PRUnichar) kLineSeparator);
    }
    else if (nsLayoutAtoms::directionalFrame == frameType) {
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
nsBidiPresUtils::ReorderFrames(nsPresContext*      aPresContext,
                               nsIRenderingContext* aRendContext,
                               nsIFrame*            aFirstChild,
                               nsIFrame*            aNextInFlow,
                               PRInt32              aChildCount)
{
  mLogicalFrames.Clear();

  if (NS_SUCCEEDED(InitLogicalArray(aPresContext, aFirstChild, aNextInFlow))
      && (mLogicalFrames.Count() > 1)) {
    PRBool bidiEnabled;
    // Set bidiEnabled to true if the line is reordered
    Reorder(aPresContext, bidiEnabled);
    if (bidiEnabled) {
      RepositionInlineFrames(aPresContext, aRendContext, aFirstChild, aChildCount);
    }
  }
}

nsresult
nsBidiPresUtils::Reorder(nsPresContext* aPresContext,
                         PRBool&         aBidiEnabled)
{
  aBidiEnabled = PR_FALSE;
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
    mLevels[i] = NS_GET_EMBEDDING_LEVEL(frame);
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
          aBidiEnabled = PR_TRUE;
        }
      }
    } // NS_SUCCEEDED(mSuccess)
  } // indexMap

  if (NS_FAILED(mSuccess) ) {
    aBidiEnabled = PR_FALSE;
  }
  return mSuccess;
}

void
nsBidiPresUtils::RepositionInlineFrames(nsPresContext*      aPresContext,
                                        nsIRenderingContext* aRendContext,
                                        nsIFrame*            aFirstChild,
                                        PRInt32              aChildCount) const
{
  PRInt32 count = mVisualFrames.Count();
  if (count < 2) {
    return;
  }
  nsIFrame* frame = (nsIFrame*) (mVisualFrames[0]);
  PRInt32 i;

  PRInt32 ch;
  PRInt32 charType;
  nscoord width, dWidth, alefWidth, dx;
  PRUnichar buf[2] = {ALEF, 0x0000};

  PRBool isBidiSystem;
  PRUint32 hints = 0;

  dWidth = alefWidth = dx = 0;
  aRendContext->GetHints(hints);
  isBidiSystem = (hints & NS_RENDERING_HINT_BIDI_REORDERING);

  nsRect rect = frame->GetRect();

  if (frame != aFirstChild) {
    rect.x = aFirstChild->GetPosition().x;
    frame->SetPosition(nsPoint(rect.x, rect.y));
  }

  for (i = 1; i < count; i++) {

    ch = 0;
    charType = NS_PTR_TO_INT32(((nsIFrame*)mVisualFrames[i])->
                               GetProperty(nsLayoutAtoms::charType));
    if (CHARTYPE_IS_RTL(charType) ) {
      ch = NS_PTR_TO_INT32(frame->GetProperty(nsLayoutAtoms::endsInDiacritic));
      if (ch) {
        if (!alefWidth) {
          aRendContext->GetWidth(buf, 1, alefWidth, nsnull);
        }
        dWidth = 0;
        if (isBidiSystem) {
          buf[1] = (PRUnichar) ch;
          aRendContext->GetWidth(buf, 2, width, nsnull);
          dWidth = width - alefWidth;
        }
        if (dWidth <= 0) {
          frame->SetPosition(nsPoint(rect.x + (nscoord)((float)width/8), rect.y));
        }
      }
    }
    frame = (nsIFrame*) (mVisualFrames[i]);
    if (ch) {
      dx += (rect.width - dWidth);
      frame->SetPosition(nsPoint(rect.x + dWidth, frame->GetPosition().y));
    } else
      frame->SetPosition(nsPoint(rect.XMost(), frame->GetPosition().y));
    rect = frame->GetRect();
  } // for

  if (dx > 0) {
    PRInt32 alignRight = NS_GET_BASE_LEVEL(frame);
    if (0 == (alignRight & 1) ) {
      const nsStyleText* styleText = frame->GetStyleText();
      
      if (NS_STYLE_TEXT_ALIGN_RIGHT == styleText->mTextAlign
          || NS_STYLE_TEXT_ALIGN_MOZ_RIGHT == styleText->mTextAlign) {
        alignRight = 1;
      }
    }
    if (alignRight & 1) {
      for (i = 0; i < count; i++) {
        frame = (nsIFrame*) (mVisualFrames[i]);
        frame->SetPosition(frame->GetPosition() + nsPoint(dx, 0));
      }
    }
  }
  
  // Now adjust inline container frames.
  // Example: LTR paragraph 
  //                <p><b>english HEBREW</b> 123</p>
  // should be displayed as
  //                <p><b>english </b>123 <b>WERBEH</b></p>

  // We assume that <b></b> rectangle takes all the room from "english" left edge to
  // "WERBEH" right edge.

  frame = aFirstChild;
  for (i = 0; i < aChildCount; i++) {
    nsIAtom* frameType = frame->GetType();
    if ( (nsLayoutAtoms::inlineFrame == frameType)
          || (nsLayoutAtoms::positionedInlineFrame == frameType)
          || (nsLayoutAtoms::letterFrame == frameType)
          || (nsLayoutAtoms::blockFrame == frameType) ) {
      PRInt32 minX = 0x7FFFFFFF;
      PRInt32 maxX = 0;
      RepositionContainerFrame(aPresContext, frame, minX, maxX);
    }
    frame = frame->GetNextSibling();
  } // for
}

void
nsBidiPresUtils::RepositionContainerFrame(nsPresContext* aPresContext,
                                          nsIFrame* aContainer,
                                          PRInt32& aMinX,
                                          PRInt32& aMaxX) const
{
  nsIFrame* frame;
  PRInt32 minX = 0x7FFFFFFF;
  PRInt32 maxX = 0;

  nsIFrame* firstChild = aContainer->GetFirstChild(nsnull);

  for (frame = firstChild; frame; frame = frame->GetNextSibling()) {
    nsIAtom* frameType = frame->GetType();
    if ( (nsLayoutAtoms::inlineFrame == frameType)
        || (nsLayoutAtoms::positionedInlineFrame == frameType)
        || (nsLayoutAtoms::letterFrame == frameType)
        || (nsLayoutAtoms::blockFrame == frameType) ) {
      RepositionContainerFrame(aPresContext, frame, minX, maxX);
    }
    else {
      nsRect rect = frame->GetRect();
      minX = PR_MIN(minX, rect.x);
      maxX = PR_MAX(maxX, rect.XMost());
    }
  }

  aMinX = PR_MIN(minX, aMinX);
  aMaxX = PR_MAX(maxX, aMaxX);

  if (minX < maxX) {
    nsRect rect = aContainer->GetRect();
    rect.x = minX;
    rect.width = maxX - minX;
    aContainer->SetRect(rect);
  }

  // Now adjust all the kids (kid's coordinates are relative to the parent's)
  for (frame = firstChild; frame; frame = frame->GetNextSibling()) {
    frame->SetPosition(frame->GetPosition() - nsPoint(minX, 0));
  }
}

PRBool
nsBidiPresUtils::EnsureBidiContinuation(nsPresContext* aPresContext,
                                        nsIContent*     aContent,
                                        nsIFrame*       aFrame,
                                        nsIFrame**      aNewFrame,
                                        PRInt32&        aFrameIndex)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (!aNewFrame) {
    return PR_FALSE;
  }
  *aNewFrame = nsnull;

  if (!aFrame) {
    return PR_FALSE;
  }
  if (aFrameIndex + 1 < mLogicalFrames.Count() ) {
    nsIFrame* frame = (nsIFrame*)mLogicalFrames[aFrameIndex + 1];
    if (frame->GetContent() == aContent) {
      *aNewFrame = frame;
      ++aFrameIndex;
      aFrame->SetNextInFlow(nsnull);
      frame->SetPrevInFlow(nsnull);
    }
  }
  if (nsnull == *aNewFrame) {
    mSuccess = CreateBidiContinuation(aPresContext, aContent, aFrame, aNewFrame);
    if (NS_FAILED(mSuccess) ) {
      return PR_FALSE;
    }
  }
  aFrame->SetProperty(nsLayoutAtoms::nextBidi, (void*) *aNewFrame);
  return PR_TRUE;
}

void
nsBidiPresUtils::RemoveBidiContinuation(nsPresContext* aPresContext,
                                        nsIFrame*       aFrame,
                                        PRInt32         aFirstIndex,
                                        PRInt32         aLastIndex,
                                        PRInt32&        aOffset) const
{
  nsIFrame*         frame;
  PRInt32           index;
  nsIFrame*         parent = aFrame->GetParent();
      
  nsIPresShell *presShell = aPresContext->PresShell();

  aOffset = 0;

  for (index = aLastIndex; index > aFirstIndex; index--) {
    frame = (nsIFrame*) mLogicalFrames[index];
    if (nsLayoutAtoms::directionalFrame == frame->GetType()) {
      delete frame;
      ++aOffset;
    }
    else {
      if (frame->GetStateBits() & NS_FRAME_IS_BIDI) {
        // only delete Bidi frames
        if (parent) {
          parent->RemoveFrame(aPresContext, *presShell,
                              nsLayoutAtoms::nextBidi, frame);
        }
        else {
          frame->Destroy(aPresContext);
        }
      }
    }
  }
  nsIFrame* thisFramesNextBidiFrame;
  nsIFrame* previousFramesNextBidiFrame;

  nsFrameManager* frameManager = presShell->FrameManager();
  thisFramesNextBidiFrame = NS_STATIC_CAST(nsIFrame*,
    frameManager->GetFrameProperty(aFrame, nsLayoutAtoms::nextBidi, 0));

  if (thisFramesNextBidiFrame) {
    // Remove nextBidi property, associated with the current frame
    // and with all of its prev-in-flow.
    frame = aFrame;
    do {
      frameManager->RemoveFrameProperty(frame, nsLayoutAtoms::nextBidi);
      frame->GetPrevInFlow(&frame);
      if (!frame) {
        break;
      }
      previousFramesNextBidiFrame = NS_STATIC_CAST(nsIFrame*,
        frameManager->GetFrameProperty(frame, nsLayoutAtoms::nextBidi, 0));
    } while (thisFramesNextBidiFrame == previousFramesNextBidiFrame);
  } // if (thisFramesNextBidiFrame)
}

nsresult
nsBidiPresUtils::FormatUnicodeText(nsPresContext*  aPresContext,
                                   PRUnichar*       aText,
                                   PRInt32&         aTextLength,
                                   nsCharType       aCharType,
                                   PRBool           aIsOddLevel,
                                   PRBool           aIsBidiSystem)
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

  if (doReverse || doShape) {
    PRInt32    newLen;

    if (mBuffer.Length() < aTextLength) {
      mBuffer.SetLength(aTextLength);
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
  PRBool     inFERange = PR_FALSE;
  PRInt32    offset;
  nsCharType charType;

  aCharType = eCharType_OtherNeutral;

  for (offset = aOffset; offset < aCharTypeLimit; offset++) {
    // Make sure we give RTL chartype to all that stuff that would be classified
    // as Right-To-Left by a bidi platform.
    // (May differ from the UnicodeData, eg we set RTL chartype to some NSM's,
    // and set LTR chartype to FE-ranged Arabic.)
    if (IS_HEBREW_CHAR(mBuffer[offset]) ) {
      charType = eCharType_RightToLeft;
    }
    else if (IS_ARABIC_ALPHABETIC(mBuffer[offset]) ) {
      charType = eCharType_RightToLeftArabic;
    }
    else if (IS_FE_CHAR(mBuffer[offset]) ) {
      charType = eCharType_LeftToRight;
      inFERange = PR_TRUE;
    }
    else {
      // IBMBIDI - Egypt - Start
      mBidiEngine->GetCharTypeAt(offset, &charType);
      // IBMBIDI - Egypt - End
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
  if (inFERange) {
    aPrevCharType = eCharType_RightToLeftArabic;
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

nsresult nsBidiPresUtils::RenderText(PRUnichar*           aText,
                                     PRInt32              aLength,
                                     nsBidiDirection      aBaseDirection,
                                     nsPresContext*      aPresContext,
                                     nsIRenderingContext& aRenderingContext,
                                     nscoord              aX,
                                     nscoord              aY)
{
  PRInt32 runCount;

  mBuffer.Assign(aText);

  nsresult rv = mBidiEngine->SetPara(mBuffer.get(), aLength, aBaseDirection, nsnull);
  if (NS_FAILED(rv))
    return rv;

  rv = mBidiEngine->CountRuns(&runCount);
  if (NS_FAILED(rv))
    return rv;

  nscoord width, xEndRun;
  PRBool isRTL = PR_FALSE;
  PRInt32 i, start, limit, length;
  PRUint8 charType;
  PRUint8 prevType = eCharType_LeftToRight;
  nsBidiLevel level;

  PRUint32 hints = 0;
  aRenderingContext.GetHints(hints);
  PRBool isBidiSystem = (hints & NS_RENDERING_HINT_BIDI_REORDERING);

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
      FormatUnicodeText(aPresContext, aText + start, subRunLength,
                        (nsCharType)charType, level & 1,
                        isBidiSystem);

      aRenderingContext.GetWidth(aText + start, subRunLength, width, nsnull);
      if (level & 1) {
        aX -= width;
      }
      aRenderingContext.DrawString(aText + start, subRunLength, aX, aY, width);
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
  } // for

  // Restore original reading order
  if (isRTL) {
    aRenderingContext.SetRightToLeftText(PR_FALSE);
  }
  return NS_OK;
}
  
nsresult
nsBidiPresUtils::ReorderUnicodeText(PRUnichar*       aText,
                                    PRInt32&         aTextLength,
                                    nsCharType       aCharType,
                                    PRBool           aIsOddLevel,
                                    PRBool           aIsBidiSystem)
{
  NS_ASSERTION(aIsOddLevel == 0 || aIsOddLevel == 1, "aIsOddLevel should be 0 or 1");
  nsresult rv = NS_OK;
  PRBool doReverse = PR_FALSE;

  if (aIsBidiSystem) {
    if ( (CHARTYPE_IS_RTL(aCharType)) ^ (aIsOddLevel) )
      doReverse = PR_TRUE;
  }
  else {
    if (aIsOddLevel)
      doReverse = PR_TRUE;
  }

  if (doReverse) {
    PRInt32    newLen;

    if (mBuffer.Length() < aTextLength) {
      mBuffer.SetLength(aTextLength);
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
