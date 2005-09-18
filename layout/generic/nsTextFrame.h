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
 * The Original Code is mozilla.org code.
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
#ifndef nsTextFrame_h___
#define nsTextFrame_h___

#include "nsFrame.h"
#include "nsTextTransformer.h"
#include "nsITextContent.h"

struct nsAutoIndexBuffer;
struct nsAutoPRUint8Buffer;

class nsTextFrame : public nsFrame {
public:
  nsTextFrame();
  
  // nsIFrame
  NS_IMETHOD Paint(nsPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const                nsRect& aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags);
  
  NS_IMETHOD Destroy(nsPresContext* aPresContext);
  
  NS_IMETHOD GetCursor(const nsPoint& aPoint,
                       nsIFrame::Cursor& aCursor);
  
  NS_IMETHOD CharacterDataChanged(nsPresContext* aPresContext,
                                  nsIContent*     aChild,
                                  PRBool          aAppend);
  
  virtual nsIFrame* GetNextInFlow() const {
    return mNextInFlow;
  }
  NS_IMETHOD SetNextInFlow(nsIFrame* aNextInFlow) {
    mNextInFlow = aNextInFlow;
    return NS_OK;
  }
  virtual nsIFrame* GetLastInFlow() const;
  
  NS_IMETHOD  IsSplittable(nsSplittableType& aIsSplittable) const {
    aIsSplittable = NS_FRAME_SPLITTABLE;
    return NS_OK;
  }
  
  /**
    * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::textFrame
   */
  virtual nsIAtom* GetType() const;
  
#ifdef DEBUG
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
  NS_IMETHOD_(nsFrameState) GetDebugStateBits() const ;
#endif
  
  NS_IMETHOD GetPosition(nsPresContext*  aPresContext,
                         const nsPoint&  aPoint,
                         nsIContent **   aNewContent,
                         PRInt32&        aContentOffset,
                         PRInt32&        aContentOffsetEnd);
  
  NS_IMETHOD GetContentAndOffsetsFromPoint(nsPresContext* aPresContext,
                                           const nsPoint&  aPoint,
                                           nsIContent **   aNewContent,
                                           PRInt32&        aContentOffset,
                                           PRInt32&        aContentOffsetEnd,
                                           PRBool&         aBeginFrameContent);
  
  NS_IMETHOD GetPositionSlowly(nsPresContext*  aPresContext,
                               nsIRenderingContext * aRendContext,
                               const nsPoint&        aPoint,
                               nsIContent **         aNewContent,
                               PRInt32&              aOffset);
  
  
  NS_IMETHOD SetSelected(nsPresContext* aPresContext,
                         nsIDOMRange *aRange,
                         PRBool aSelected,
                         nsSpread aSpread);
  
  NS_IMETHOD PeekOffset(nsPresContext* aPresContext, nsPeekOffsetStruct *aPos);
  NS_IMETHOD CheckVisibility(nsPresContext* aContext, PRInt32 aStartIndex, PRInt32 aEndIndex, PRBool aRecurse, PRBool *aFinished, PRBool *_retval);
  
  NS_IMETHOD HandleMultiplePress(nsPresContext* aPresContext,
                                 nsGUIEvent *    aEvent,
                                 nsEventStatus*  aEventStatus);
  
  NS_IMETHOD GetOffsets(PRInt32 &start, PRInt32 &end)const;
  
  virtual void AdjustOffsetsForBidi(PRInt32 start, PRInt32 end);
  
  NS_IMETHOD GetPointFromOffset(nsPresContext*         inPresContext,
                                nsIRenderingContext*    inRendContext,
                                PRInt32                 inOffset,
                                nsPoint*                outPoint);
  
  NS_IMETHOD  GetChildFrameContainingOffset(PRInt32     inContentOffset,
                                            PRBool                  inHint,
                                            PRInt32*                outFrameContentOffset,
                                            nsIFrame*               *outChildFrame);
  
  NS_IMETHOD IsVisibleForPainting(nsPresContext *     aPresContext, 
                                  nsIRenderingContext& aRenderingContext,
                                  PRBool               aCheckVis,
                                  PRBool*              aIsVisible);
  
  virtual PRBool IsEmpty();
  virtual PRBool IsSelfEmpty() { return IsEmpty(); }
  
  /**
    * Does this text frame end with a newline character?
   */
  virtual PRBool HasTerminalNewline() const;
  
#ifdef ACCESSIBILITY
  NS_IMETHOD GetAccessible(nsIAccessible** aAccessible);
#endif
  
  // nsIHTMLReflow
  NS_IMETHOD Reflow(nsPresContext* aPresContext,
                    nsHTMLReflowMetrics& aMetrics,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);
  NS_IMETHOD CanContinueTextRun(PRBool& aContinueTextRun) const;
  NS_IMETHOD AdjustFrameSize(nscoord aExtraSpace, nscoord& aUsedSpace);
  NS_IMETHOD TrimTrailingWhiteSpace(nsPresContext* aPresContext,
                                    nsIRenderingContext& aRC,
                                    nscoord& aDeltaWidth,
                                    PRBool& aLastCharIsJustifiable);
  
  struct TextStyle {
    const nsStyleFont* mFont;
    const nsStyleText* mText;
    nsIFontMetrics* mNormalFont;
    nsIFontMetrics* mSmallFont;
    nsIFontMetrics* mLastFont;
    PRBool mSmallCaps;
    nscoord mWordSpacing;
    nscoord mLetterSpacing;
    nscoord mSpaceWidth;
    nscoord mAveCharWidth;
    PRBool mJustifying;
    PRBool mPreformatted;
    PRInt32 mNumJustifiableCharacterToRender;
    PRInt32 mNumJustifiableCharacterToMeasure;
    nscoord mExtraSpacePerJustifiableCharacter;
    PRInt32 mNumJustifiableCharacterReceivingExtraJot;

    TextStyle(nsPresContext* aPresContext,
              nsIRenderingContext& aRenderingContext,
              nsStyleContext* sc);
    
    ~TextStyle();
  };
  
  // Contains extra style data needed only for painting (not reflowing)
  struct TextPaintStyle : TextStyle {
    const nsStyleColor* mColor;
    nscolor mSelectionTextColor;
    nscolor mSelectionBGColor;
    
    TextPaintStyle(nsPresContext* aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   nsStyleContext* sc);

    ~TextPaintStyle();
  };
  
  struct TextReflowData {
    PRInt32             mX;                   // OUT
    PRInt32             mOffset;              // IN/OUT How far along we are in the content
    nscoord             mMaxWordWidth;        // OUT
    nscoord             mAscent;              // OUT
    nscoord             mDescent;             // OUT
    PRPackedBool        mWrapping;            // IN
    PRPackedBool        mSkipWhitespace;      // IN
    PRPackedBool        mMeasureText;         // IN
    PRPackedBool        mInWord;              // IN
    PRPackedBool        mFirstLetterOK;       // IN
    PRPackedBool        mCanBreakBefore;         // IN
    PRPackedBool        mComputeMaxWordWidth; // IN
    PRPackedBool        mTrailingSpaceTrimmed; // IN/OUT
    
    TextReflowData(PRInt32 aStartingOffset,
                   PRBool  aWrapping,
                   PRBool  aSkipWhitespace,
                   PRBool  aMeasureText,
                   PRBool  aInWord,
                   PRBool  aFirstLetterOK,
                   PRBool  aCanBreakBefore,
                   PRBool  aComputeMaxWordWidth,
                   PRBool  aTrailingSpaceTrimmed)
      : mX(0),
      mOffset(aStartingOffset),
      mMaxWordWidth(0),
      mAscent(0),
      mDescent(0),
      mWrapping(aWrapping),
      mSkipWhitespace(aSkipWhitespace),
      mMeasureText(aMeasureText),
      mInWord(aInWord),
      mFirstLetterOK(aFirstLetterOK),
      mCanBreakBefore(aCanBreakBefore),
      mComputeMaxWordWidth(aComputeMaxWordWidth),
      mTrailingSpaceTrimmed(aTrailingSpaceTrimmed)
    {}
  };
  
  nsIDocument* GetDocument(nsPresContext* aPresContext);
  
  void PrepareUnicodeText(nsTextTransformer& aTransformer,
                          nsAutoIndexBuffer* aIndexBuffer,
                          nsAutoTextBuffer* aTextBuffer,
                          PRInt32* aTextLen,
                          PRBool aForceArabicShaping = PR_FALSE,
                          PRIntn* aJustifiableCharCount = nsnull);
  void ComputeExtraJustificationSpacing(nsIRenderingContext& aRenderingContext,
                                        TextStyle& aTextStyle,
                                        PRUnichar* aBuffer, PRInt32 aLength, PRInt32 aNumJustifiableCharacter);
  
  void PaintTextDecorations(nsIRenderingContext& aRenderingContext,
                            nsStyleContext* aStyleContext,
                            nsPresContext* aPresContext,
                            TextPaintStyle& aStyle,
                            nscoord aX, nscoord aY, nscoord aWidth,
                            PRUnichar* aText = nsnull,
                            SelectionDetails *aDetails = nsnull,
                            PRUint32 aIndex = 0,
                            PRUint32 aLength = 0,
                            const nscoord* aSpacing = nsnull);
  
  void PaintTextSlowly(nsPresContext* aPresContext,
                       nsIRenderingContext& aRenderingContext,
                       nsStyleContext* aStyleContext,
                       TextPaintStyle& aStyle,
                       nscoord aX, nscoord aY);
  
  // The passed-in rendering context must have its color set to the color the
  // text should be rendered in.
  void RenderString(nsIRenderingContext& aRenderingContext,
                    nsStyleContext* aStyleContext,
                    nsPresContext* aPresContext,
                    TextPaintStyle& aStyle,
                    PRUnichar* aBuffer, PRInt32 aLength, PRBool aIsEndOfFrame,
                    nscoord aX, nscoord aY,
                    nscoord aWidth,
                    SelectionDetails *aDetails = nsnull);
  
  void MeasureSmallCapsText(const nsHTMLReflowState& aReflowState,
                            TextStyle& aStyle,
                            PRUnichar* aWord,
                            PRInt32 aWordLength,
                            PRBool aIsEndOfFrame,
                            nsTextDimensions* aDimensionsResult);
  
  PRUint32 EstimateNumChars(PRUint32 aAvailableWidth,
                            PRUint32 aAverageCharWidth);
  
  nsReflowStatus MeasureText(nsPresContext*          aPresContext,
                             const nsHTMLReflowState& aReflowState,
                             nsTextTransformer&       aTx,
                             TextStyle&               aTs,
                             TextReflowData&          aTextData);
  
  void GetTextDimensions(nsIRenderingContext& aRenderingContext,
                         TextStyle& aStyle,
                         PRUnichar* aBuffer, PRInt32 aLength, PRBool aIsEndOfFrame,
                         nsTextDimensions* aDimensionsResult);
  
  //this returns the index into the PAINTBUFFER of the x coord aWidth(based on 0 as far left) 
  //also note: this is NOT added to mContentOffset since that would imply that this return is
  //meaningful to content yet. use index buffer from prepareunicodestring to find the content offset.
  PRInt32 GetLengthSlowly(nsIRenderingContext& aRenderingContext,
                          TextStyle& aStyle,
                          PRUnichar* aBuffer, PRInt32 aLength, PRBool aIsEndOfFrame,
                          nscoord aWidth);
  
  PRBool IsTextInSelection(nsPresContext* aPresContext,
                           nsIRenderingContext& aRenderingContext);
  
  nsresult GetTextInfoForPainting(nsPresContext*          aPresContext,
                                  nsIRenderingContext&     aRenderingContext,
                                  nsIPresShell**           aPresShell,
                                  nsISelectionController** aSelectionController,
                                  PRBool&                  aDisplayingSelection,
                                  PRBool&                  aIsPaginated,
                                  PRBool&                  aIsSelected,
                                  PRBool&                  aHideStandardSelection,
                                  PRInt16&                 aSelectionValue);
  
  void PaintUnicodeText(nsPresContext* aPresContext,
                        nsIRenderingContext& aRenderingContext,
                        nsStyleContext* aStyleContext,
                        TextPaintStyle& aStyle,
                        nscoord dx, nscoord dy);
  
  void PaintAsciiText(nsPresContext* aPresContext,
                      nsIRenderingContext& aRenderingContext,
                      nsStyleContext* aStyleContext,
                      TextPaintStyle& aStyle,
                      nscoord dx, nscoord dy);
  
  nsTextDimensions ComputeTotalWordDimensions(nsPresContext* aPresContext,
                                              nsLineLayout& aLineLayout,
                                              const nsHTMLReflowState& aReflowState,
                                              nsIFrame* aNextFrame,
                                              const nsTextDimensions& aBaseDimensions,
                                              PRUnichar* aWordBuf,
                                              PRUint32   aWordBufLen,
                                              PRUint32   aWordBufSize,
                                              PRBool     aCanBreakBefore);
  
  nsTextDimensions ComputeWordFragmentDimensions(nsPresContext* aPresContext,
                                                 nsLineLayout& aLineLayout,
                                                 const nsHTMLReflowState& aReflowState,
                                                 nsIFrame* aNextFrame,
                                                 nsIContent* aContent,
                                                 nsITextContent* aText,
                                                 PRBool* aStop,
                                                 const PRUnichar* aWordBuf,
                                                 PRUint32 &aWordBufLen,
                                                 PRUint32 aWordBufSize,
                                                 PRBool aCanBreakBefore);
  
#ifdef DEBUG
  void ToCString(nsString& aBuf, PRInt32* aTotalContentLength) const;
#endif
  
protected:
    virtual ~nsTextFrame();
  
  nsIFrame* mNextInFlow;
  PRInt32   mContentOffset;
  PRInt32   mContentLength;
  PRInt32   mColumn;
  nscoord   mAscent;
  //factored out method for GetTextDimensions and getlengthslowly. if aGetTextDimensions is non-zero number then measure to the width field and return the length. else shove total dimensions into result
  PRInt32 GetTextDimensionsOrLength(nsIRenderingContext& aRenderingContext,
                                    TextStyle& aStyle,
                                    PRUnichar* aBuffer, PRInt32 aLength, PRBool aIsEndOfFrame,
                                    nsTextDimensions* aDimensionsResult,
                                    PRBool aGetTextDimensions/* true=get dimensions false = return length up to aDimensionsResult->width size*/);
  nsresult GetContentAndOffsetsForSelection(nsPresContext*  aPresContext,nsIContent **aContent, PRInt32 *aOffset, PRInt32 *aLength);
  
  void AdjustSelectionPointsForBidi(SelectionDetails *sdptr,
                                    PRInt32 textLength,
                                    PRBool isRTLChars,
                                    PRBool isOddLevel,
                                    PRBool isBidiSystem);
  
  void SetOffsets(PRInt32 start, PRInt32 end);
  
  PRBool IsChineseJapaneseLangGroup();
  PRBool IsJustifiableCharacter(PRUnichar aChar, PRBool aLangIsCJ);
  
  nsresult FillClusterBuffer(nsPresContext *aPresContext, const PRUnichar *aText,
                             PRUint32 aLength, nsAutoPRUint8Buffer& aClusterBuffer);
};

PRBool
BinarySearchForPosition(nsIRenderingContext* acx, 
                        const PRUnichar* aText,
                        PRInt32    aBaseWidth,
                        PRInt32    aBaseInx,
                        PRInt32    aStartInx, 
                        PRInt32    aEndInx, 
                        PRInt32    aCursorPos, 
                        PRInt32&   aIndex,
                        PRInt32&   aTextWidth);

#endif /* nsTextFrame_h___ */
