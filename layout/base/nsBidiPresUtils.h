/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBidiPresUtils_h___
#define nsBidiPresUtils_h___

#include "nsBidi.h"
#include "nsBidiUtils.h"
#include "nsHashKeys.h"
#include "nsCoord.h"

#ifdef DrawText
#undef DrawText
#endif

struct BidiParagraphData;
struct BidiLineData;
class nsFontMetrics;
class nsIFrame;
class nsBlockFrame;
class nsPresContext;
class nsRenderingContext;
class nsBlockInFlowLineIterator;
class nsStyleContext;
struct nsSize;
template<class T> class nsTHashtable;
namespace mozilla { class WritingMode; }

/**
 * A structure representing some continuation state for each frame on the line,
 * used to determine the first and the last continuation frame for each
 * continuation chain on the line.
 */
struct nsFrameContinuationState : public nsVoidPtrHashKey
{
  explicit nsFrameContinuationState(const void *aFrame) : nsVoidPtrHashKey(aFrame) {}

  /**
   * The first visual frame in the continuation chain containing this frame, or
   * nullptr if this frame is the first visual frame in the chain.
   */
  nsIFrame* mFirstVisualFrame;

  /**
   * The number of frames in the continuation chain containing this frame, if
   * this frame is the first visual frame of the chain, or 0 otherwise.
   */
  uint32_t mFrameCount;

  /**
   * TRUE if this frame is the first visual frame of its continuation chain on
   * this line and the chain has some frames on the previous lines.
   */
  bool mHasContOnPrevLines;

  /**
   * TRUE if this frame is the first visual frame of its continuation chain on
   * this line and the chain has some frames left for next lines.
   */
  bool mHasContOnNextLines;
};

/*
 * Following type is used to pass needed hashtable to reordering methods
 */
typedef nsTHashtable<nsFrameContinuationState> nsContinuationStates;

/**
 * A structure representing a logical position which should be resolved
 * into its visual position during BiDi processing.
 */
struct nsBidiPositionResolve
{
  // [in] Logical index within string.
  int32_t logicalIndex;
  // [out] Visual index within string.
  // If the logical position was not found, set to kNotFound.
  int32_t visualIndex;
  // [out] Visual position of the character, from the left (on the X axis), in twips.
  // Eessentially, this is the X position (relative to the rendering context) where the text was drawn + the font metric of the visual string to the left of the given logical position.
  // If the logical position was not found, set to kNotFound.
  int32_t visualLeftTwips;
  // [out] Visual width of the character, in twips.
  // If the logical position was not found, set to kNotFound.
  int32_t visualWidth;
};

class nsBidiPresUtils {
public:
  nsBidiPresUtils();
  ~nsBidiPresUtils();
  
  /**
   * Interface for the processor used by ProcessText. Used by process text to
   * collect information about the width of subruns and to notify where each
   * subrun should be rendered.
   */
  class BidiProcessor {
  public:
    virtual ~BidiProcessor() { }

    /**
     * Sets the current text with the given length and the given direction.
     *
     * @remark The reason that the function gives a string instead of an index
     *  is that ProcessText copies and modifies the string passed to it, so
     *  passing an index would be impossible.
     *
     * @param aText The string of text.
     * @param aLength The length of the string of text.
     * @param aDirection The direction of the text. The string will never have
     *  mixed direction.
     */
    virtual void SetText(const char16_t*   aText,
                         int32_t           aLength,
                         nsBidiDirection   aDirection) = 0;

    /**
     * Returns the measured width of the text given in SetText. If SetText was
     * not called with valid parameters, the result of this call is undefined.
     * This call is guaranteed to only be called once between SetText calls.
     * Will be invoked before DrawText.
     */
    virtual nscoord GetWidth() = 0;

    /**
     * Draws the text given in SetText to a rendering context. If SetText was
     * not called with valid parameters, the result of this call is undefined.
     * This call is guaranteed to only be called once between SetText calls.
     * 
     * @param aXOffset The offset of the left side of the substring to be drawn
     *  from the beginning of the overall string passed to ProcessText.
     * @param aWidth The width returned by GetWidth.
     */
    virtual void DrawText(nscoord   aXOffset,
                          nscoord   aWidth) = 0;
  };

  /**
   * Make Bidi engine calculate the embedding levels of the frames that are
   * descendants of a given block frame.
   *
   * @param aBlockFrame          The block frame
   *
   *  @lina 06/18/2000
   */
  static nsresult Resolve(nsBlockFrame* aBlockFrame);
  static nsresult ResolveParagraph(nsBlockFrame* aBlockFrame,
                                   BidiParagraphData* aBpd);
  static void ResolveParagraphWithinBlock(nsBlockFrame* aBlockFrame,
                                          BidiParagraphData* aBpd);

  /**
   * Reorder this line using Bidi engine.
   * Update frame array, following the new visual sequence.
   * 
   * @lina 05/02/2000
   */
  static void ReorderFrames(nsIFrame*            aFirstFrameOnLine,
                            int32_t              aNumFramesOnLine,
                            mozilla::WritingMode aLineWM,
                            const nsSize&        aContainerSize,
                            nscoord              aStart);

  /**
   * Format Unicode text, taking into account bidi capabilities
   * of the platform. The formatting includes: reordering, Arabic shaping,
   * symmetric and numeric swapping, removing control characters.
   *
   * @lina 06/18/2000 
   */
  static nsresult FormatUnicodeText(nsPresContext*  aPresContext,
                                    char16_t*       aText,
                                    int32_t&        aTextLength,
                                    nsCharType      aCharType,
                                    nsBidiDirection aDir);

  /**
   * Reorder plain text using the Unicode Bidi algorithm and send it to
   * a rendering context for rendering.
   *
   * @param[in] aText  the string to be rendered (in logical order)
   * @param aLength the number of characters in the string
   * @param aBaseLevel the base embedding level of the string
   *  odd values are right-to-left; even values are left-to-right, plus special
   *  constants as follows (defined in nsBidi.h)
   *  NSBIDI_LTR - left-to-right string
   *  NSBIDI_RTL - right-to-left string
   *  NSBIDI_DEFAULT_LTR - auto direction determined by first strong character,
   *                       default is left-to-right
   *  NSBIDI_DEFAULT_RTL - auto direction determined by first strong character,
   *                       default is right-to-left
   *
   * @param aPresContext the presentation context
   * @param aRenderingContext the rendering context to render to
   * @param aTextRunConstructionContext the rendering context to be used to construct the textrun (affects font hinting)
   * @param aX the x-coordinate to render the string
   * @param aY the y-coordinate to render the string
   * @param[in,out] aPosResolve array of logical positions to resolve into visual positions; can be nullptr if this functionality is not required
   * @param aPosResolveCount number of items in the aPosResolve array
   */
  static nsresult RenderText(const char16_t*       aText,
                             int32_t                aLength,
                             nsBidiLevel            aBaseLevel,
                             nsPresContext*         aPresContext,
                             nsRenderingContext&    aRenderingContext,
                             nsRenderingContext&    aTextRunConstructionContext,
                             nsFontMetrics&         aFontMetrics,
                             nscoord                aX,
                             nscoord                aY,
                             nsBidiPositionResolve* aPosResolve = nullptr,
                             int32_t                aPosResolveCount = 0)
  {
    return ProcessTextForRenderingContext(aText, aLength, aBaseLevel, aPresContext, aRenderingContext,
                                          aTextRunConstructionContext,
                                          aFontMetrics,
                                          MODE_DRAW, aX, aY, aPosResolve, aPosResolveCount, nullptr);
  }
  
  static nscoord MeasureTextWidth(const char16_t*     aText,
                                  int32_t              aLength,
                                  nsBidiLevel          aBaseLevel,
                                  nsPresContext*       aPresContext,
                                  nsRenderingContext&  aRenderingContext,
                                  nsFontMetrics&       aFontMetrics)
  {
    nscoord length;
    nsresult rv = ProcessTextForRenderingContext(aText, aLength, aBaseLevel, aPresContext,
                                                 aRenderingContext, aRenderingContext,
                                                 aFontMetrics,
                                                 MODE_MEASURE, 0, 0, nullptr, 0, &length);
    return NS_SUCCEEDED(rv) ? length : 0;
  }

  /**
   * Check if a line is reordered, i.e., if the child frames are not
   * all laid out left-to-right.
   * @param aFirstFrameOnLine : first frame of the line to be tested
   * @param aNumFramesOnLine : number of frames on this line
   * @param[out] aLeftMost : leftmost frame on this line
   * @param[out] aRightMost : rightmost frame on this line
   */
  static bool CheckLineOrder(nsIFrame*  aFirstFrameOnLine,
                               int32_t    aNumFramesOnLine,
                               nsIFrame** aLeftmost,
                               nsIFrame** aRightmost);

  /**
   * Get the frame to the right of the given frame, on the same line.
   * @param aFrame : We're looking for the frame to the right of this frame.
   *                 If null, return the leftmost frame on the line.
   * @param aFirstFrameOnLine : first frame of the line to be tested
   * @param aNumFramesOnLine : number of frames on this line
   */
  static nsIFrame* GetFrameToRightOf(const nsIFrame*  aFrame,
                                     nsIFrame*        aFirstFrameOnLine,
                                     int32_t          aNumFramesOnLine);
    
  /**
   * Get the frame to the left of the given frame, on the same line.
   * @param aFrame : We're looking for the frame to the left of this frame.
   *                 If null, return the rightmost frame on the line.
   * @param aFirstFrameOnLine : first frame of the line to be tested
   * @param aNumFramesOnLine : number of frames on this line
   */
  static nsIFrame* GetFrameToLeftOf(const nsIFrame*  aFrame,
                                    nsIFrame*        aFirstFrameOnLine,
                                    int32_t          aNumFramesOnLine);

  static nsIFrame* GetFirstLeaf(nsIFrame* aFrame);
    
  /**
   * Get the bidi embedding level of the given (inline) frame.
   */
  static nsBidiLevel GetFrameEmbeddingLevel(nsIFrame* aFrame);
    
  /**
   * Get the paragraph depth of the given (inline) frame.
   */
  static uint8_t GetParagraphDepth(nsIFrame* aFrame);

  /**
   * Get the bidi base level of the given (inline) frame.
   */
  static nsBidiLevel GetFrameBaseLevel(nsIFrame* aFrame);

  /**
   * Get an nsBidiDirection representing the direction implied by the
   * bidi base level of the frame.
   * @return NSBIDI_LTR (left-to-right) or NSBIDI_RTL (right-to-left)
   *  NSBIDI_MIXED will never be returned.
   */
  static nsBidiDirection ParagraphDirection(nsIFrame* aFrame) {
    return DIRECTION_FROM_LEVEL(GetFrameBaseLevel(aFrame));
  }

  /**
   * Get an nsBidiDirection representing the direction implied by the
   * bidi embedding level of the frame.
   * @return NSBIDI_LTR (left-to-right) or NSBIDI_RTL (right-to-left)
   *  NSBIDI_MIXED will never be returned.
   */
  static nsBidiDirection FrameDirection(nsIFrame* aFrame) {
    return DIRECTION_FROM_LEVEL(GetFrameEmbeddingLevel(aFrame));
  }

  static bool IsFrameInParagraphDirection(nsIFrame* aFrame) {
    return ParagraphDirection(aFrame) == FrameDirection(aFrame);
  }

  enum Mode { MODE_DRAW, MODE_MEASURE };

  /**
   * Reorder plain text using the Unicode Bidi algorithm and send it to
   * a processor for rendering or measuring
   *
   * @param[in] aText  the string to be processed (in logical order)
   * @param aLength the number of characters in the string
   * @param aBaseLevel the base embedding level of the string
   *  odd values are right-to-left; even values are left-to-right, plus special
   *  constants as follows (defined in nsBidi.h)
   *  NSBIDI_LTR - left-to-right string
   *  NSBIDI_RTL - right-to-left string
   *  NSBIDI_DEFAULT_LTR - auto direction determined by first strong character,
   *                       default is left-to-right
   *  NSBIDI_DEFAULT_RTL - auto direction determined by first strong character,
   *                       default is right-to-left
   *
   * @param aPresContext the presentation context
   * @param aprocessor the bidi processor
   * @param aMode the operation to process
   *  MODE_DRAW - invokes DrawText on the processor for each substring
   *  MODE_MEASURE - does not invoke DrawText on the processor
   *  Note that the string is always measured, regardless of mode
   * @param[in,out] aPosResolve array of logical positions to resolve into
   *  visual positions; can be nullptr if this functionality is not required
   * @param aPosResolveCount number of items in the aPosResolve array
   * @param[out] aWidth Pointer to where the width will be stored (may be null)
   */
  static nsresult ProcessText(const char16_t*       aText,
                              int32_t                aLength,
                              nsBidiLevel            aBaseLevel,
                              nsPresContext*         aPresContext,
                              BidiProcessor&         aprocessor,
                              Mode                   aMode,
                              nsBidiPositionResolve* aPosResolve,
                              int32_t                aPosResolveCount,
                              nscoord*               aWidth,
                              nsBidi*                aBidiEngine);

  /**
   * Make a copy of a string, converting from logical to visual order
   *
   * @param aSource the source string
   * @param aDest the destination string
   * @param aBaseDirection the base direction of the string
   *       (NSBIDI_LTR or NSBIDI_RTL to force the base direction;
   *        NSBIDI_DEFAULT_LTR or NSBIDI_DEFAULT_RTL to let the bidi engine
   *        determine the direction from rules P2 and P3 of the bidi algorithm.
   *  @see nsBidi::GetPara
   * @param aOverride if TRUE, the text has a bidi override, according to
   *                    the direction in aDir
   */
  static void CopyLogicalToVisual(const nsAString& aSource,
                                  nsAString& aDest,
                                  nsBidiLevel aBaseDirection,
                                  bool aOverride);

  /**
   * Use style attributes to determine the base paragraph level to pass to the
   * bidi algorithm.
   *
   * If |unicode-bidi| is set to "[-moz-]plaintext", returns NSBIDI_DEFAULT_LTR,
   * in other words the direction is determined from the first strong character
   * in the text according to rules P2 and P3 of the bidi algorithm, or LTR if
   * there is no strong character.
   *
   * Otherwise returns NSBIDI_LTR or NSBIDI_RTL depending on the value of
   * |direction|
   */
  static nsBidiLevel BidiLevelFromStyle(nsStyleContext* aStyleContext);

private:
  static nsresult
  ProcessTextForRenderingContext(const char16_t*       aText,
                                 int32_t                aLength,
                                 nsBidiLevel            aBaseLevel,
                                 nsPresContext*         aPresContext,
                                 nsRenderingContext&    aRenderingContext,
                                 nsRenderingContext&    aTextRunConstructionContext,
                                 nsFontMetrics&         aFontMetrics,
                                 Mode                   aMode,
                                 nscoord                aX, // DRAW only
                                 nscoord                aY, // DRAW only
                                 nsBidiPositionResolve* aPosResolve,  /* may be null */
                                 int32_t                aPosResolveCount,
                                 nscoord*               aWidth /* may be null */);

  /**
   * Traverse the child frames of the block element and:
   *  Set up an array of the frames in logical order
   *  Create a string containing the text content of all the frames
   *  If we encounter content that requires us to split the element into more
   *  than one paragraph for bidi resolution, resolve the paragraph up to that
   *  point.
   */
  static void TraverseFrames(nsBlockFrame*              aBlockFrame,
                             nsBlockInFlowLineIterator* aLineIter,
                             nsIFrame*                  aCurrentFrame,
                             BidiParagraphData*         aBpd);

  /*
   * Position aFrame and its descendants to their visual places. Also if aFrame
   * is not leaf, resize it to embrace its children.
   *
   * @param aFrame               The frame which itself and its children are
   *                             going to be repositioned
   * @param aIsEvenLevel         TRUE means the embedding level of this frame
   *                             is even (LTR)
   * @param[in,out] aStart       IN value is the starting position of aFrame
   *                             (without considering its inline-start margin)
   *                             OUT value will be the ending position of aFrame
   *                             (after adding its inline-end margin)
   * @param aContinuationStates  A map from nsIFrame* to nsFrameContinuationState
   */
  static void RepositionFrame(nsIFrame*              aFrame,
                              bool                   aIsEvenLevel,
                              nscoord&               aStart,
                              nsContinuationStates*  aContinuationStates,
                              mozilla::WritingMode   aContainerWM,
                              const nsSize&          aContainerSize);

  /*
   * Initialize the continuation state(nsFrameContinuationState) to
   * (nullptr, 0) for aFrame and its descendants.
   *
   * @param aFrame               The frame which itself and its descendants will
   *                             be initialized
   * @param aContinuationStates  A map from nsIFrame* to nsFrameContinuationState
   */
  static void InitContinuationStates(nsIFrame*              aFrame,
                                     nsContinuationStates*  aContinuationStates);

  /*
   * Determine if aFrame is first or last, and set aIsFirst and
   * aIsLast values. Also set continuation states of
   * aContinuationStates.
   *
   * A frame is first if it's the first appearance of its continuation
   * chain on the line and the chain is on its first line.
   * A frame is last if it's the last appearance of its continuation
   * chain on the line and the chain is on its last line.
   *
   * N.B: "First appearance" and "Last appearance" in the previous
   * paragraph refer to the frame's inline direction, not necessarily
   * the line's.
   *
   * @param aContinuationStates        A map from nsIFrame* to
   *                                    nsFrameContinuationState
   * @param[in] aSpanDirMatchesLineDir TRUE means that the inline
   *                                    direction of aFrame is the same
   *                                    as its container
   * @param[out] aIsFirst              TRUE means aFrame is first frame
   *                                    or continuation
   * @param[out] aIsLast               TRUE means aFrame is last frame
   *                                    or continuation
   */
   static void IsFirstOrLast(nsIFrame*              aFrame,
                             nsContinuationStates*  aContinuationStates,
                             bool                   aSpanInLineOrder /* in */,
                             bool&                  aIsFirst /* out */,
                             bool&                  aIsLast /* out */);

  /**
   *  Adjust frame positions following their visual order
   *
   *  @param aFirstChild the first kid
   *
   *  @lina 04/11/2000
   */
  static void RepositionInlineFrames(BidiLineData* aBld,
                                     nsIFrame* aFirstChild,
                                     mozilla::WritingMode aLineWM,
                                     const nsSize& aContainerSize,
                                     nscoord aStart);
  
  /**
   * Helper method for Resolve()
   * Truncate a text frame to the end of a single-directional run and possibly
   * create a continuation frame for the remainder of its content.
   *
   * @param aFrame       the original frame
   * @param aNewFrame    [OUT] the new frame that was created
   * @param aFrameIndex  [IN/OUT] index of aFrame in mLogicalFrames
   * @param aStart       [IN] the start of the content mapped by aFrame (and 
   *                          any fluid continuations)
   * @param aEnd         [IN] the offset of the end of the single-directional
   *                          text run.
   * @see Resolve()
   * @see RemoveBidiContinuation()
   */
  static inline
  nsresult EnsureBidiContinuation(nsIFrame*       aFrame,
                                  nsIFrame**      aNewFrame,
                                  int32_t&        aFrameIndex,
                                  int32_t         aStart,
                                  int32_t         aEnd);

  /**
   * Helper method for Resolve()
   * Convert one or more bidi continuation frames created in a previous reflow by
   * EnsureBidiContinuation() into fluid continuations.
   * @param aFrame       the frame whose continuations are to be removed
   * @param aFirstIndex  index of aFrame in mLogicalFrames
   * @param aLastIndex   index of the last frame to be removed
   * @param aOffset      [OUT] count of directional frames removed. Since
   *                     directional frames have control characters
   *                     corresponding to them in mBuffer, the pointers to
   *                     mBuffer in Resolve() will need to be updated after
   *                     deleting the frames.
   *
   * @see Resolve()
   * @see EnsureBidiContinuation()
   */
  static void RemoveBidiContinuation(BidiParagraphData* aBpd,
                                     nsIFrame*          aFrame,
                                     int32_t            aFirstIndex,
                                     int32_t            aLastIndex,
                                     int32_t&           aOffset);
  static void CalculateCharType(nsBidi*          aBidiEngine,
                                const char16_t* aText,
                                int32_t&         aOffset,
                                int32_t          aCharTypeLimit,
                                int32_t&         aRunLimit,
                                int32_t&         aRunLength,
                                int32_t&         aRunCount,
                                uint8_t&         aCharType,
                                uint8_t&         aPrevCharType);
  
  static void StripBidiControlCharacters(char16_t* aText,
                                         int32_t&   aTextLength);

  static bool WriteLogicalToVisual(const char16_t* aSrc,
                                     uint32_t aSrcLength,
                                     char16_t* aDest,
                                     nsBidiLevel aBaseDirection,
                                     nsBidi* aBidiEngine);

  static void WriteReverse(const char16_t* aSrc,
                           uint32_t aSrcLength,
                           char16_t* aDest);
};

#endif /* nsBidiPresUtils_h___ */
