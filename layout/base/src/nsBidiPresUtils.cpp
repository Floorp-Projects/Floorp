/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef IBMBIDI

#define FIX_FOR_BUG_40882

#include "nsBidiPresUtils.h"
#include "nsITextContent.h"
#include "nsTextFragment.h"
#include "nsLayoutAtoms.h"
#include "nsIPresContext.h"
#include "nsIRenderingContext.h"
#include "nsIServiceManager.h"
#include "nsIFrameManager.h"
#include "nsBidiFrames.h"
#include "nsITextFrame.h"
#include "nsIUBidiUtils.h"

static const PRUnichar kSpace            = 0x0020;
static const PRUnichar kLineSeparator    = 0x2028;
static const PRUnichar kObjectSubstitute = 0xFFFC;
static const PRUnichar kLRE              = 0x202A;
static const PRUnichar kRLE              = 0x202B;
static const PRUnichar kLRO              = 0x202D;
static const PRUnichar kRLO              = 0x202E;
static const PRUnichar kPDF              = 0x202C;

#ifdef FIX_FOR_BUG_40882
static const PRUnichar ALEF              = 0x05D0;
#endif

extern nsresult
NS_NewContinuingTextFrame(nsIPresShell* aPresShell, nsIFrame** aResult);
extern nsresult
NS_NewDirectionalFrame(nsIFrame** aNewFrame, PRUnichar aChar);

nsBidiPresUtils::nsBidiPresUtils() : mArraySize(8),
                                     mIndexMap(nsnull),
                                     mLevels(nsnull),
                                     mSuccess(NS_ERROR_FAILURE),
                                     mBidiEngine(nsnull),
                                     mUnicodeUtils(nsnull)
{
  mBidiEngine = do_GetService("@mozilla.org/intl/bidi;1");
  if (mBidiEngine) {
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
}

PRBool
nsBidiPresUtils::IsSuccessful() const
{ 
  return NS_SUCCEEDED(mSuccess); 
}

/* Some helper methods for Resolve() */

static nsresult
CreateBidiContinuation(nsIPresContext* aPresContext,
                       nsIContent*     aContent,
                       nsIFrame*       aFrame,
                       nsIFrame**      aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");

  *aNewFrame = nsnull;

  NS_PRECONDITION(aFrame, "null ptr");

  nsIFrame* parent;

  nsCOMPtr<nsIPresShell>   presShell;
  aPresContext->GetShell(getter_AddRefs(presShell) );

  NS_ASSERTION(presShell, "PresShell must be set on PresContext before calling nsBidiPresUtils::CreateBidiContinuation");

  NS_NewContinuingTextFrame(presShell, aNewFrame);
  if (!(*aNewFrame) ) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsCOMPtr<nsIStyleContext> styleContext;
  aFrame->GetStyleContext(getter_AddRefs(styleContext) );

  NS_ASSERTION(presShell, "Frame has no styleContext in nsBidiPresUtils::CreateBidiContinuation");
  
  aFrame->GetParent(&parent);

  NS_ASSERTION(presShell, "Couldn't get frame parent in nsBidiPresUtils::CreateBidiContinuation");

  (*aNewFrame)->Init(aPresContext, aContent, parent, styleContext, nsnull);

  // XXX: TODO: Instead, create and insert entire frame list
  (*aNewFrame)->SetNextSibling(nsnull);

  // The list name nsLayoutAtoms::nextBidi would indicate we don't want reflow
  parent->InsertFrames(aPresContext, *presShell, nsLayoutAtoms::nextBidi,
                       aFrame, *aNewFrame);

  return NS_OK;
}

nsresult
nsBidiPresUtils::Resolve(nsIPresContext* aPresContext,
                         nsIFrame*       aBlockFrame,
                         nsIFrame*       aFirstChild,
                         PRBool&         aForceReflow)
{
  aForceReflow = PR_FALSE;
  mLogicalFrames.Clear();

  // handle bidi-override being set on the block itself before calling
  // InitLogicalArray.
  const nsStyleVisibility* vis;
  aBlockFrame->GetStyleData(eStyleStruct_Visibility,
                            NS_REINTERPRET_CAST(const nsStyleStruct*&, vis));
  const nsStyleTextReset* text;
  aBlockFrame->GetStyleData(eStyleStruct_TextReset,
                            NS_REINTERPRET_CAST(const nsStyleStruct*&, text));

  if (text->mUnicodeBidi == NS_STYLE_UNICODE_BIDI_OVERRIDE) {
    nsresult rv;
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
  aPresContext->IsVisualMode(isVisual);

  mSuccess = mBidiEngine->CountRuns(&runCount);
  if (NS_FAILED(mSuccess) ) {
    return mSuccess;
  }
  PRInt32                  runLength      = 0;
  PRInt32                  fragmentLength = 0;
  PRInt32                  temp;
  PRInt32                  frameIndex     = -1;
  PRInt32                  frameCount     = mLogicalFrames.Count();
  PRInt32                  contentOffset;        // offset within current frame
  PRInt32                  lineOffset     = 0;   // offset within mBuffer
  PRInt32                  logicalLimit   = 0;
  PRInt32                  numRun         = -1;
  PRUint8                  charType;
  PRUint8                  prevType       = eCharType_LeftToRight;
  PRBool                   isTextFrame;
  nsIFrame*                frame = nsnull;
  nsIFrame*                nextBidi;
  nsITextFrame*            textFrame;
  nsCOMPtr<nsIAtom>        frameType;
  nsCOMPtr<nsIContent>     content;
  nsCOMPtr<nsITextContent> textContent;
  const nsTextFragment*    fragment;

  for (; ;) {
    textFrame = nsnull;

    if (fragmentLength <= 0) {
      if (++frameIndex >= frameCount) {
        break;
      }
      contentOffset = 0;
      
      frame = (nsIFrame*) (mLogicalFrames[frameIndex]);

      frame->GetFrameType(getter_AddRefs(frameType) );
      if (nsLayoutAtoms::textFrame == frameType.get() ) {
        mSuccess = frame->GetContent(getter_AddRefs(content) ); 
        if (NS_FAILED(mSuccess) || (!content) ) {
          break;
        }
        textContent = do_QueryInterface(content, &mSuccess);
        if (NS_FAILED(mSuccess) || (!textContent) ) {
          break;
        }
        textContent->GetText(&fragment);
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

    if (nsLayoutAtoms::directionalFrame == frameType.get()) {
      delete frame;
      ++lineOffset;
    }
    else {
      frame->SetBidiProperty(aPresContext, nsLayoutAtoms::embeddingLevel,
                             (void *)embeddingLevel);
      frame->SetBidiProperty(aPresContext, nsLayoutAtoms::baseLevel,
                             (void *)paraLevel);
      if (isTextFrame) {
        PRInt32 typeLimit = PR_MIN(logicalLimit, lineOffset + fragmentLength);
        CalculateCharType(lineOffset, typeLimit, logicalLimit, runLength,
                           runCount, charType, prevType);
        // IBMBIDI - Egypt - Start
        frame->SetBidiProperty(aPresContext,nsLayoutAtoms::charType,(void*)charType);
        // IBMBIDI - Egypt - End

        if ( (runLength > 0) && (runLength < fragmentLength) ) {
          if (!EnsureBidiContinuation(aPresContext, content.get(), frame,
                                      &nextBidi, frameIndex) ) {
            break;
          }
          frame->QueryInterface(NS_GET_IID(nsITextFrame), (void**) &textFrame);
          if (textFrame) {
            textFrame->SetOffsets(contentOffset, contentOffset + runLength);
            nsFrameState frameState;
            frame->GetFrameState(&frameState);
            frameState |= NS_FRAME_IS_BIDI;
            frame->SetFrameState(frameState);
          }
          frame = nextBidi;
          contentOffset += runLength;
        } // if (runLength < fragmentLength)
        else {
          frame->QueryInterface(NS_GET_IID(nsITextFrame), (void**) &textFrame);
          if (textFrame) {
            textFrame->SetOffsets(contentOffset, contentOffset + fragmentLength);
            nsFrameState frameState;
            frame->GetFrameState(&frameState);
            frameState |= NS_FRAME_IS_BIDI;
            frame->SetFrameState(frameState);
          }
          frame->GetBidiProperty(aPresContext, nsLayoutAtoms::nextBidi,
                                 (void**) &nextBidi,sizeof(nextBidi));
          if (RemoveBidiContinuation(aPresContext, frame, nextBidi,
                                     content.get(), frameIndex, temp) ) {
            aForceReflow = PR_TRUE;
            runLength -= temp;
            lineOffset += temp;
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
nsBidiPresUtils::InitLogicalArray(nsIPresContext* aPresContext,
                                  nsIFrame*       aCurrentFrame,
                                  nsIFrame*       aNextInFlow,
                                  PRBool          aAddMarkers)
{
  nsIFrame*             frame;
  nsIFrame*             directionalFrame;
  nsIFrame*             kid;
  nsCOMPtr<nsIAtom>     frameType;
  const nsStyleDisplay* display;
  nsresult              rv;
  nsresult              res = NS_OK;

  for (frame = aCurrentFrame;
       frame && frame != aNextInFlow;
       frame->GetNextSibling(&frame) ) {

    rv = NS_ERROR_FAILURE;
    frame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&) display);
    
    if (aAddMarkers && !display->IsBlockLevel() ) {
      const nsStyleVisibility* vis;
      frame->GetStyleData(eStyleStruct_Visibility,
                          NS_REINTERPRET_CAST(const nsStyleStruct*&, vis));
      const nsStyleTextReset* text;
      frame->GetStyleData(eStyleStruct_TextReset,
                          NS_REINTERPRET_CAST(const nsStyleStruct*&, text));
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

    frame->GetFrameType(getter_AddRefs(frameType) );

    if ( (!display->IsBlockLevel() )
        && ( (nsLayoutAtoms::inlineFrame == frameType.get() )
          || (nsLayoutAtoms::letterFrame == frameType.get() )
          || (nsLayoutAtoms::blockFrame == frameType.get() ) ) ) {
      frame->FirstChild(aPresContext, nsnull, &kid);
      res = InitLogicalArray(aPresContext, kid, aNextInFlow, aAddMarkers);
    }
    else { // bidi leaf
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
nsBidiPresUtils::CreateBlockBuffer(nsIPresContext* aPresContext)
{
  mBuffer.SetLength(0);

  nsIFrame*                 frame;
  nsIContent*               prevContent = nsnull;
  nsCOMPtr<nsIContent>      content;
  nsCOMPtr<nsITextContent>  textContent;
  nsCOMPtr<nsIAtom>         frameType;
  const nsTextFragment*     frag;
  PRUint32                  i;
  PRUint32                  count = mLogicalFrames.Count();

  for (i = 0; i < count; i++) {
    frame = (nsIFrame*) (mLogicalFrames[i]);
    frame->GetFrameType(getter_AddRefs(frameType) );

    if (nsLayoutAtoms::textFrame == frameType.get() ) {
      mSuccess = frame->GetContent(getter_AddRefs(content) );
      if ( (NS_FAILED(mSuccess) ) || (!content) ) {
        break;
      }
      if (content.get() == prevContent) {
        continue;
      }
      prevContent = content.get();
      textContent = do_QueryInterface(content, &mSuccess);
      if ( (NS_FAILED(mSuccess) ) || (!textContent) ) {
        break;
      }
      textContent->GetText(&frag);
      if (!frag) {
        mSuccess = NS_ERROR_FAILURE;
        break;
      }
      frag->AppendTo(mBuffer);
    }
    else if (nsLayoutAtoms::brFrame == frameType.get() ) { // break frame
      // Append line separator
      mBuffer.Append( (PRUnichar) kLineSeparator);
    }
    else if (nsLayoutAtoms::directionalFrame == frameType.get() ) {
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
nsBidiPresUtils::ReorderFrames(nsIPresContext*      aPresContext,
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
nsBidiPresUtils::Reorder(nsIPresContext* aPresContext,
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
  nsCRT::zero(mLevels, sizeof(PRUint8) * mArraySize);

  nsIFrame* frame;
  PRInt32   i;

  for (i = 0; i < count; i++) {
    frame = (nsIFrame*) (mLogicalFrames[i]);
    frame->GetBidiProperty(aPresContext, nsLayoutAtoms::embeddingLevel,
                           (void**)&mLevels[i], sizeof(mLevels[i]) );
  }
  if (!mIndexMap) {
    mIndexMap = new PRInt32[mArraySize];
  }
  if (!mIndexMap) {
    mSuccess = NS_ERROR_OUT_OF_MEMORY;
  }
  else {
    nsCRT::zero(mIndexMap, sizeof(PRUint32) * mArraySize);

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
nsBidiPresUtils::RepositionInlineFrames(nsIPresContext*      aPresContext,
                                        nsIRenderingContext* aRendContext,
                                        nsIFrame*            aFirstChild,
                                        PRInt32              aChildCount) const
{
  PRInt32 count = mVisualFrames.Count();
  if (count < 2) {
    return;
  }
  nsIFrame* frame = (nsIFrame*) (mVisualFrames[0]);
  nsPoint origin;
  nsRect rect;
  PRInt32 i;

#ifdef FIX_FOR_BUG_40882
  PRInt32 ch;
  PRInt32 charType;
  nscoord width, dWidth, alefWidth = 0, dx = 0;
  PRUnichar buf[2] = {ALEF, 0x0000};

  PRBool isBidiSystem;
  PRUint32 hints = 0;
  aRendContext->GetHints(hints);
  isBidiSystem = (hints & NS_RENDERING_HINT_BIDI_REORDERING);
#endif // bug

  frame->GetRect(rect);

  if (frame != aFirstChild) {
    aFirstChild->GetOrigin(origin);
    rect.x = origin.x;
    frame->MoveTo(aPresContext, rect.x, rect.y);
  }

  for (i = 1; i < count; i++) {

#ifdef FIX_FOR_BUG_40882
    ch = 0;
    ( (nsIFrame*)mVisualFrames[i])->GetBidiProperty(aPresContext,
                             nsLayoutAtoms::charType, (void**)&charType,sizeof(charType));
    if (CHARTYPE_IS_RTL(charType) ) {
      frame->GetBidiProperty(aPresContext,
                             nsLayoutAtoms::endsInDiacritic, (void**)&ch,sizeof(ch));
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
          frame->MoveTo(aPresContext, rect.x + (nscoord)((float)width/8), rect.y);
        }
      }
    }
#endif // bug
    frame = (nsIFrame*) (mVisualFrames[i]);
    frame->GetOrigin(origin);
    frame->MoveTo(aPresContext, rect.x + rect.width, origin.y);
#ifdef FIX_FOR_BUG_40882
    if (ch) {
      dx += (rect.width - dWidth);
      frame->MoveTo(aPresContext, rect.x + dWidth, origin.y);
    }
#endif // bug 40882
    frame->GetRect(rect);
  } // for

#ifdef FIX_FOR_BUG_40882
  if (dx > 0) {
    PRInt32 alignRight;
    frame->GetBidiProperty(aPresContext, nsLayoutAtoms::baseLevel,
                           (void**) &alignRight,sizeof(alignRight));
    if (0 == (alignRight & 1) ) {
      const nsStyleText* styleText;
      frame->GetStyleData(eStyleStruct_Text, (const nsStyleStruct*&) styleText);
      
      if (NS_STYLE_TEXT_ALIGN_RIGHT == styleText->mTextAlign
          || NS_STYLE_TEXT_ALIGN_MOZ_RIGHT == styleText->mTextAlign) {
        alignRight = 1;
      }
    }
    if (alignRight & 1) {
      for (i = 0; i < count; i++) {
        frame = (nsIFrame*) (mVisualFrames[i]);
        frame->GetOrigin(origin);
        frame->MoveTo(aPresContext, origin.x + dx, origin.y);
      }
    }
  }
#endif // bug
  
  // Now adjust inline container frames.
  // Example: LTR paragraph 
  //                <p><b>english HEBREW</b> 123</p>
  // should be displayed as
  //                <p><b>english </b>123 <b>WERBEH</b></p>

  // We assume that <b></b> rectangle takes all the room from "english" left edge to
  // "WERBEH" right edge.

  nsCOMPtr<nsIAtom> frameType;

  frame = aFirstChild;
  for (i = 0; i < aChildCount; i++) {
    frame->GetFrameType(getter_AddRefs(frameType) );
    if ( frameType.get()
        && ( (nsLayoutAtoms::inlineFrame == frameType.get() )
          || (nsLayoutAtoms::letterFrame == frameType.get() )
          || (nsLayoutAtoms::blockFrame == frameType.get() ) ) ) {
      PRInt32 minX = 0x7FFFFFFF;
      PRInt32 maxX = 0;
      RepositionContainerFrame(aPresContext, frame, minX, maxX);
    }
    frame->GetNextSibling(&frame);
  } // for
}

void
nsBidiPresUtils::RepositionContainerFrame(nsIPresContext* aPresContext,
                                          nsIFrame* aContainer,
                                          PRInt32& aMinX,
                                          PRInt32& aMaxX) const
{
  nsIFrame* frame;
  nsIFrame* firstChild;
  nsCOMPtr<nsIAtom> frameType;
  nsRect rect;
  PRInt32 minX = 0x7FFFFFFF;
  PRInt32 maxX = 0;

  aContainer->FirstChild(aPresContext, nsnull, &firstChild);

  for (frame = firstChild; frame; frame->GetNextSibling(&frame) ) {
    frame->GetFrameType(getter_AddRefs(frameType) );
    if ( (frameType.get() )
      && ( (nsLayoutAtoms::inlineFrame == frameType.get() )
        || (nsLayoutAtoms::letterFrame == frameType.get() )
        || (nsLayoutAtoms::blockFrame == frameType.get() ) ) ) {
      RepositionContainerFrame(aPresContext, frame, minX, maxX);
    }
    else {
      frame->GetRect(rect);
      minX = PR_MIN(minX, rect.x);
      maxX = PR_MAX(maxX, rect.x + rect.width);
    }
  }

  aMinX = PR_MIN(minX, aMinX);
  aMaxX = PR_MAX(maxX, aMaxX);

  if (minX < maxX) {
    aContainer->GetRect(rect);
    rect.x = minX;
    rect.width = maxX - minX;
    aContainer->SetRect(aPresContext, rect);
  }

  // Now adjust all the kids (kid's coordinates are relative to the parent's)
  nsPoint origin;

  for (frame = firstChild; frame; frame->GetNextSibling(&frame) ) {
    frame->GetOrigin(origin);
    frame->MoveTo(aPresContext, origin.x - minX, origin.y);
  }
}

PRBool
nsBidiPresUtils::EnsureBidiContinuation(nsIPresContext* aPresContext,
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
    nsCOMPtr<nsIContent> content;

    nsresult rv = frame->GetContent(getter_AddRefs(content) );

    if (NS_SUCCEEDED(rv) && (content.get() == aContent) ) {
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
  aFrame->SetBidiProperty(aPresContext, nsLayoutAtoms::nextBidi, 
                          (void*) *aNewFrame);
  return PR_TRUE;
}

PRBool
nsBidiPresUtils::RemoveBidiContinuation(nsIPresContext* aPresContext,
                                        nsIFrame*       aFrame,
                                        nsIFrame*       aNextFrame,
                                        nsIContent*     aContent,
                                        PRInt32&        aFrameIndex,
                                        PRInt32&        aOffset) const
{
  if (!aFrame) {
    return PR_FALSE;
  }
  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsIAtom>    frameType;
  nsIFrame*            frame;
  PRInt32              index;
  PRInt32              newIndex = -1;
  PRInt32              frameCount = mLogicalFrames.Count();

  for (index = aFrameIndex + 1; index < frameCount; index++) {
    // frames with the same content may not be adjacent. So check all frames.
    frame = (nsIFrame*) mLogicalFrames[index];
    frame->GetContent(getter_AddRefs(content) );
    if (content.get() == aContent) {
      newIndex = index;
    }
  }
  if (-1 == newIndex) {
    return PR_FALSE;
  }
  nsIFrame* nextBidi;
  nsIFrame* parent;
  aFrame->GetParent(&parent);
      
  nsCOMPtr<nsIPresShell> presShell;
  aPresContext->GetShell(getter_AddRefs(presShell) );

  aOffset = 0;

  for (index = newIndex; index > aFrameIndex; index--) {
    frame = (nsIFrame*) mLogicalFrames[index];
    frame->GetFrameType(getter_AddRefs(frameType) );
    if (nsLayoutAtoms::directionalFrame == frameType.get() ) {
      delete frame;
      ++aOffset;
    }
    else if (parent != nsnull) {
      parent->RemoveFrame(aPresContext, *presShell, nsLayoutAtoms::nextBidi,
                          frame);
    }
    else {
      frame->Destroy(aPresContext);
    }
  }
  if (aNextFrame) {
    nsCOMPtr<nsIFrameManager> frameManager;
    presShell->GetFrameManager(getter_AddRefs(frameManager) );
    if (frameManager) {
      // Remove nextBidi property, associated with the current frame
      // and with all of its prev-in-flow.
      frame = aFrame;
      do {
        frameManager->RemoveFrameProperty(frame, nsLayoutAtoms::nextBidi);
        frame->GetPrevInFlow(&frame);
        if (!frame) {
          break;
        }
        frameManager->GetFrameProperty(frame, nsLayoutAtoms::nextBidi,
                                       0, (void**) &nextBidi);
      } while (aNextFrame == nextBidi);
    } // if (frameManager)
  } // if (aNextFrame)
  
  aFrameIndex = newIndex;

  return PR_TRUE;
}

nsresult
nsBidiPresUtils::FormatUnicodeText(nsIPresContext*  aPresContext,
                                   PRUnichar*       aText,
                                   PRInt32&         aTextLength,
                                   nsCharType       aCharType,
                                   PRBool           aIsOddLevel,
                                   PRBool           aIsBidiSystem)
{
  nsresult rv = NS_OK;
  if (!mUnicodeUtils) {
    mUnicodeUtils = do_GetService("@mozilla.org/intl/unicharbidiutil;1");
    if (!mUnicodeUtils) {
      return NS_ERROR_FAILURE;
    }
  }
  // ahmed 
  //adjusted for correct numeral shaping  
  PRUint32 bidiOptions;
  aPresContext->GetBidi(&bidiOptions);
  switch (GET_BIDI_OPTION_NUMERAL(bidiOptions)) {

    case IBMBIDI_NUMERAL_HINDI:
      mUnicodeUtils->HandleNumbers(aText,aTextLength,IBMBIDI_NUMERAL_HINDI);
      break;

    case IBMBIDI_NUMERAL_ARABIC:
      mUnicodeUtils->HandleNumbers(aText,aTextLength,IBMBIDI_NUMERAL_ARABIC);
      break;

    case IBMBIDI_NUMERAL_REGULAR:

      switch (aCharType) {

        case eCharType_EuropeanNumber:
          mUnicodeUtils->HandleNumbers(aText,aTextLength,IBMBIDI_NUMERAL_ARABIC);
          break;

        case eCharType_ArabicNumber:
          mUnicodeUtils->HandleNumbers(aText,aTextLength,IBMBIDI_NUMERAL_HINDI);
          break;

        default:
          break;
      }
      break;
      
    case IBMBIDI_NUMERAL_HINDICONTEXT:
      if ( ( (GET_BIDI_OPTION_DIRECTION(bidiOptions)==IBMBIDI_TEXTDIRECTION_RTL) && (IS_ARABIC_DIGIT (aText[0])) ) || (eCharType_ArabicNumber == aCharType) )
        mUnicodeUtils->HandleNumbers(aText,aTextLength,IBMBIDI_NUMERAL_HINDI);
      else if (eCharType_EuropeanNumber == aCharType)
        mUnicodeUtils->HandleNumbers(aText,aTextLength,IBMBIDI_NUMERAL_ARABIC);
      break;

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
    PRUnichar* buffer = (PRUnichar*)mBuffer.get();

    if (doReverse) {
      rv = mBidiEngine->WriteReverse(aText, aTextLength, buffer,
                                     NSBIDI_DO_MIRRORING, &newLen);
      if (NS_SUCCEEDED(rv) ) {
        aTextLength = newLen;
        nsCRT::memcpy(aText, buffer, aTextLength * sizeof(PRUnichar) );
      }
    }
    if (doShape) {
      rv = mUnicodeUtils->ArabicShaping(aText, aTextLength, buffer, (PRUint32 *)&newLen);
      if (NS_SUCCEEDED(rv) ) {
        aTextLength = newLen;
        nsCRT::memcpy(aText, buffer, aTextLength * sizeof(PRUnichar) );
      }
    }
  }
  return rv;
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

nsresult nsBidiPresUtils::GetBidiEngine(nsIBidi** aBidiEngine)
{
  nsresult rv = NS_ERROR_FAILURE;
  if (mBidiEngine) {
    *aBidiEngine = mBidiEngine;
    NS_ADDREF(*aBidiEngine);
    rv = NS_OK;
  }
  return rv; 
}

nsresult nsBidiPresUtils::RenderText(PRUnichar*           aText,
                                     PRInt32              aLength,
                                     nsBidiDirection      aBaseDirection,
                                     nsIPresContext*      aPresContext,
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

  nscoord width;
  PRBool isRTL = PR_FALSE;
  PRInt32 i, start, limit, length;
  PRUint8 charType;
  PRUint8 prevType = eCharType_LeftToRight;
  nsBidiLevel level;
  PRInt32 lineOffset     = 0;
  PRInt32 runLength      = 0;

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

    runLength = limit - start;
    lineOffset = start;
    PRInt32 typeLimit = PR_MIN(limit, aLength);
    CalculateCharType(lineOffset, typeLimit, limit, runLength, runCount, charType, prevType);

    if (eCharType_RightToLeftArabic == charType) {
      isBidiSystem = (hints & NS_RENDERING_HINT_ARABIC_SHAPING);
    }
    if (isBidiSystem && (CHARTYPE_IS_RTL(charType) ^ isRTL) ) {
      // set reading order into DC
      isRTL = !isRTL;
      aRenderingContext.SetRightToLeftText(isRTL);
    }
    FormatUnicodeText(aPresContext, aText + start, length,
                      (nsCharType)charType, level & 1,
                      isBidiSystem);

    aRenderingContext.GetWidth(aText + start, length, width, nsnull);
    aRenderingContext.DrawString(aText + start, length, aX, aY, width);
    aX += width;
  } // for

  // Restore original reading order
  if (isRTL) {
    aRenderingContext.SetRightToLeftText(PR_FALSE);
  }
  return NS_OK;
}
  
#endif // IBMBIDI
