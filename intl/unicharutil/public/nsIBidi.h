/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.  Portions created by IBM are
 * Copyright (C) 2000 IBM Corporation. All
 * Rights Reserved.
 *
 * This module is based on the ICU (International Components for Unicode)
 *
 *   Copyright (C) 2000, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 *
 * Contributor(s):
 */
#ifdef IBMBIDI

#ifndef nsIBidi_h__
#define nsIBidi_h__

#include "nsISupports.h"
#include "prtypes.h"

#define NS_BIDI_CID \
                    { 0xd9123b91, 0xf4f2, 0x11d3, \
                    {0xb6, 0xf1, 0x0, 0x10, 0x4b, 0x41, 0x19, 0xf8} }

#define NS_BIDI_CONTRACTID "@mozilla.org/intl/bidi;1" 

#define NS_BIDI_IID \
                    { 0xd9123b92, 0xf4f2, 0x11d3, \
                    { 0xb6, 0xf1, 0x0, 0x10, 0x4b, 0x41, 0x19, 0xf8 } }

/*
 * javadoc-style comments are intended to be transformed into HTML
 * using DOC++ - see
 * http://www.zib.de/Visual/software/doc++/index.html .
 *
 * The HTML documentation is created with
 *  doc++ -H nsIBidi.h
 */

/**
 * @mainpage BIDI algorithm for Mozilla (from ICU)
 *
 * <h2>BIDI algorithm for Mozilla</h2>
 *
 * This is an implementation of the Unicode Bidirectional algorithm.
 * The algorithm is defined in the
 * <a href="http://www.unicode.org/unicode/reports/tr9/">Unicode Technical Report 9</a>,
 * version 5, also described in The Unicode Standard, Version 3.0 .<p>
 *
 * <h3>General remarks about the API:</h3>
 *
 * The <quote>limit</quote> of a sequence of characters is the position just after their
 * last character, i.e., one more than that position.<p>
 *
 * Some of the API functions provide access to <quote>runs</quote>.
 * Such a <quote>run</quote> is defined as a sequence of characters
 * that are at the same embedding level
 * after performing the BIDI algorithm.<p>
 *
 * @author Markus W. Scherer. Ported to Mozilla by Simon Montagu
 * @version 1.0
 */

/**
 * nsBidiLevel is the type of the level values in this
 * Bidi implementation.
 * It holds an embedding level and indicates the visual direction
 * by its bit 0 (even/odd value).<p>
 *
 * It can also hold non-level values for the
 * <code>aParaLevel</code> and <code>aEmbeddingLevels</code>
 * arguments of <code>SetPara</code>; there:
 * <ul>
 * <li>bit 7 of an <code>aEmbeddingLevels[]</code>
 * value indicates whether the using application is
 * specifying the level of a character to <i>override</i> whatever the
 * Bidi implementation would resolve it to.</li>
 * <li><code>aParaLevel</code> can be set to the
 * pseudo-level values <code>NSBIDI_DEFAULT_LTR</code>
 * and <code>NSBIDI_DEFAULT_RTL</code>.</li></ul>
 *
 * @see nsIBidi::SetPara
 *
 * <p>The related constants are not real, valid level values.
 * <code>NSBIDI_DEFAULT_XXX</code> can be used to specify
 * a default for the paragraph level for
 * when the <code>SetPara</code> function
 * shall determine it but there is no
 * strongly typed character in the input.<p>
 *
 * Note that the value for <code>NSBIDI_DEFAULT_LTR</code> is even
 * and the one for <code>NSBIDI_DEFAULT_RTL</code> is odd,
 * just like with normal LTR and RTL level values -
 * these special values are designed that way. Also, the implementation
 * assumes that NSBIDI_MAX_EXPLICIT_LEVEL is odd.
 *
 * @see NSBIDI_DEFAULT_LTR
 * @see NSBIDI_DEFAULT_RTL
 * @see NSBIDI_LEVEL_OVERRIDE
 * @see NSBIDI_MAX_EXPLICIT_LEVEL
 */
typedef PRUint8 nsBidiLevel;

/** Paragraph level setting.
 *  If there is no strong character, then set the paragraph level to 0 (left-to-right).
 */
#define NSBIDI_DEFAULT_LTR 0xfe

/** Paragraph level setting.
 *  If there is no strong character, then set the paragraph level to 1 (right-to-left).
 */
#define NSBIDI_DEFAULT_RTL 0xff

/**
 * Maximum explicit embedding level.
 * (The maximum resolved level can be up to <code>NSBIDI_MAX_EXPLICIT_LEVEL+1</code>).
 *
 */
#define NSBIDI_MAX_EXPLICIT_LEVEL 61

/** Bit flag for level input. 
 *  Overrides directional properties. 
 */
#define NSBIDI_LEVEL_OVERRIDE 0x80

/**
 * <code>nsBidiDirection</code> values indicate the text direction.
 */
enum nsBidiDirection {
  /** All left-to-right text This is a 0 value. */
  NSBIDI_LTR,
  /** All right-to-left text This is a 1 value. */
  NSBIDI_RTL,
  /** Mixed-directional text. */
  NSBIDI_MIXED
};

typedef enum nsBidiDirection nsBidiDirection;

/* miscellaneous definitions ------------------------------------------------ */

enum nsCharType   { 
  eCharType_LeftToRight              = 0, 
  eCharType_RightToLeft              = 1, 
  eCharType_EuropeanNumber           = 2,
  eCharType_EuropeanNumberSeparator  = 3,
  eCharType_EuropeanNumberTerminator = 4,
  eCharType_ArabicNumber             = 5,
  eCharType_CommonNumberSeparator    = 6,
  eCharType_BlockSeparator           = 7,
  eCharType_SegmentSeparator         = 8,
  eCharType_WhiteSpaceNeutral        = 9, 
  eCharType_OtherNeutral             = 10, 
  eCharType_LeftToRightEmbedding     = 11,
  eCharType_LeftToRightOverride      = 12,
  eCharType_RightToLeftArabic        = 13,
  eCharType_RightToLeftEmbedding     = 14,
  eCharType_RightToLeftOverride      = 15,
  eCharType_PopDirectionalFormat     = 16,
  eCharType_DirNonSpacingMark        = 17,
  eCharType_BoundaryNeutral          = 18,
  eCharType_CharTypeCount
};

/**
 * This specifies the language directional property of a character set.
 */
typedef enum nsCharType nsCharType;

/**
 * definitions of bidirection character types by category
 */

#define CHARTYPE_IS_RTL(val) ( ( (val) == eCharType_RightToLeft) || ( (val) == eCharType_RightToLeftArabic) )

#define CHARTYPE_IS_WEAK(val) ( ( (val) == eCharType_EuropeanNumberSeparator)    \
                           || ( (val) == eCharType_EuropeanNumberTerminator) \
                           || ( ( (val) > eCharType_ArabicNumber) && ( (val) != eCharType_RightToLeftArabic) ) )

/** option flags for WriteReverse() */
/**
 * option bit for WriteReverse():
 * keep combining characters after their base characters in RTL runs
 *
 * @see WriteReverse
 */
#define NSBIDI_KEEP_BASE_COMBINING       1

/**
 * option bit for WriteReverse():
 * replace characters with the "mirrored" property in RTL runs
 * by their mirror-image mappings
 *
 * @see WriteReverse
 */
#define NSBIDI_DO_MIRRORING              2

/**
 * option bit for WriteReverse():
 * remove Bidi control characters
 *
 * @see WriteReverse
 */
#define NSBIDI_REMOVE_BIDI_CONTROLS      8

/**
 * This class holds information about a paragraph of text
 * with Bidi-algorithm-related details, or about one line of
 * such a paragraph.<p>
 * Reordering can be done on a line, or on a paragraph which is
 * then interpreted as one single line.<p>
 *
 * On construction, the class is initially empty. It is assigned
 * the Bidi properties of a paragraph by <code>SetPara</code>
 * or the Bidi properties of a line of a paragraph by
 * <code>SetLine</code>.<p>
 * A Bidi class can be reused for as long as it is not deallocated
 * by calling its destructor.<p>
 * <code>SetPara</code> will allocate additional memory for
 * internal structures as necessary.
 */
class nsIBidi : public nsISupports
{
public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_BIDI_IID)
  /**
   * Perform the Unicode Bidi algorithm. It is defined in the
   * <a href="http://www.unicode.org/unicode/reports/tr9/">Unicode Technical Report 9</a>,
   * version 5,
   * also described in The Unicode Standard, Version 3.0 .<p>
   *
   * This function takes a single plain text paragraph with or without
   * externally specified embedding levels from <quote>styled</quote> text
   * and computes the left-right-directionality of each character.<p>
   *
   * If the entire paragraph consists of text of only one direction, then
   * the function may not perform all the steps described by the algorithm,
   * i.e., some levels may not be the same as if all steps were performed.
   * This is not relevant for unidirectional text.<br>
   * For example, in pure LTR text with numbers the numbers would get
   * a resolved level of 2 higher than the surrounding text according to
   * the algorithm. This implementation may set all resolved levels to
   * the same value in such a case.<p>
   *
   * The text must be externally split into separate paragraphs (rule P1).
   * Paragraph separators (B) should appear at most at the very end.
   *
   * @param aText is a pointer to the single-paragraph text that the
   *      Bidi algorithm will be performed on
   *      (step (P1) of the algorithm is performed externally).
   *      <strong>The text must be (at least) <code>aLength</code> long.</strong>
   *
   * @param aLength is the length of the text; if <code>aLength==-1</code> then
   *      the text must be zero-terminated.
   *
   * @param aParaLevel specifies the default level for the paragraph;
   *      it is typically 0 (LTR) or 1 (RTL).
   *      If the function shall determine the paragraph level from the text,
   *      then <code>aParaLevel</code> can be set to
   *      either <code>NSBIDI_DEFAULT_LTR</code>
   *      or <code>NSBIDI_DEFAULT_RTL</code>;
   *      if there is no strongly typed character, then
   *      the desired default is used (0 for LTR or 1 for RTL).
   *      Any other value between 0 and <code>NSBIDI_MAX_EXPLICIT_LEVEL</code> is also valid,
   *      with odd levels indicating RTL.
   *
   * @param aEmbeddingLevels (in) may be used to preset the embedding and override levels,
   *      ignoring characters like LRE and PDF in the text.
   *      A level overrides the directional property of its corresponding
   *      (same index) character if the level has the
   *      <code>NSBIDI_LEVEL_OVERRIDE</code> bit set.<p>
   *      Except for that bit, it must be
   *      <code>aParaLevel<=aEmbeddingLevels[]<=NSBIDI_MAX_EXPLICIT_LEVEL</code>.<p>
   *      <strong>Caution: </strong>A copy of this pointer, not of the levels,
   *      will be stored in the <code>nsBidi</code> object;
   *      the <code>aEmbeddingLevels</code> array must not be
   *      deallocated before the <code>nsBidi</code> object is destroyed or reused,
   *      and the <code>aEmbeddingLevels</code>
   *      should not be modified to avoid unexpected results on subsequent Bidi operations.
   *      However, the <code>SetPara</code> and
   *      <code>SetLine</code> functions may modify some or all of the levels.<p>
   *      After the <code>nsBidi</code> object is reused or destroyed, the caller
   *      must take care of the deallocation of the <code>aEmbeddingLevels</code> array.<p>
   *      <strong>The <code>aEmbeddingLevels</code> array must be
   *      at least <code>aLength</code> long.</strong>
   */
  NS_IMETHOD SetPara(const PRUnichar *aText, PRInt32 aLength, nsBidiLevel aParaLevel, nsBidiLevel *aEmbeddingLevels) = 0;

#ifdef FULL_BIDI_ENGINE
  /**
   * <code>SetLine</code> sets an <code>nsBidi</code> to
   * contain the reordering information, especially the resolved levels,
   * for all the characters in a line of text. This line of text is
   * specified by referring to an <code>nsBidi</code> object representing
   * this information for a paragraph of text, and by specifying
   * a range of indexes in this paragraph.<p>
   * In the new line object, the indexes will range from 0 to <code>aLimit-aStart</code>.<p>
   *
   * This is used after calling <code>SetPara</code>
   * for a paragraph, and after line-breaking on that paragraph.
   * It is not necessary if the paragraph is treated as a single line.<p>
   *
   * After line-breaking, rules (L1) and (L2) for the treatment of
   * trailing WS and for reordering are performed on
   * an <code>nsBidi</code> object that represents a line.<p>
   *
   * <strong>Important:</strong> the line <code>nsBidi</code> object shares data with
   * <code>aParaBidi</code>.
   * You must destroy or reuse this object before <code>aParaBidi</code>.
   * In other words, you must destroy or reuse the <code>nsBidi</code> object for a line
   * before the object for its parent paragraph.
   *
   * @param aParaBidi is the parent paragraph object.
   *
   * @param aStart is the line's first index into the paragraph text.
   *
   * @param aLimit is just behind the line's last index into the paragraph text
   *      (its last index +1).<br>
   *      It must be <code>0<=aStart<=aLimit<=</code>paragraph length.
   *
   * @see SetPara
   */
  NS_IMETHOD SetLine(nsIBidi* aParaBidi, PRInt32 aStart, PRInt32 aLimit) = 0;  

  /**
   * Get the directionality of the text.
   *
   * @param aDirection receives a <code>NSBIDI_XXX</code> value that indicates if the entire text
   *       represented by this object is unidirectional,
   *       and which direction, or if it is mixed-directional.
   *
   * @see nsBidiDirection
   */
  NS_IMETHOD GetDirection(nsBidiDirection* aDirection) = 0;

  /**
   * Get the length of the text.
   *
   * @param aLength receives the length of the text that the nsBidi object was created for.
   */
  NS_IMETHOD GetLength(PRInt32* aLength) = 0;

  /**
   * Get the paragraph level of the text.
   *
   * @param aParaLevel receives a <code>NSBIDI_XXX</code> value indicating the paragraph level
   *
   * @see nsBidiLevel
   */
  NS_IMETHOD GetParaLevel(nsBidiLevel* aParaLevel) = 0;

  /**
   * Get the level for one character.
   *
   * @param aCharIndex the index of a character.
   *
   * @param aLevel receives the level for the character at aCharIndex.
   *
   * @see nsBidiLevel
   */
  NS_IMETHOD GetLevelAt(PRInt32 aCharIndex,  nsBidiLevel* aLevel) = 0;

  /**
   * Get an array of levels for each character.<p>
   *
   * Note that this function may allocate memory under some
   * circumstances, unlike <code>GetLevelAt</code>.
   *
   * @param aLevels receives a pointer to the levels array for the text,
   *       or <code>NULL</code> if an error occurs.
   *
   * @see nsBidiLevel
   */
  NS_IMETHOD GetLevels(nsBidiLevel** aLevels) = 0;
#endif // FULL_BIDI_ENGINE
  /**
   * Get the bidirectional type for one character.
   *
   * @param aCharIndex the index of a character.
   *
   * @param aType receives the bidirectional type of the character at aCharIndex.
   */
  NS_IMETHOD GetCharTypeAt(PRInt32 aCharIndex,  nsCharType* aType) = 0;

  /**
   * Get a logical run.
   * This function returns information about a run and is used
   * to retrieve runs in logical order.<p>
   * This is especially useful for line-breaking on a paragraph.
   *
   * @param aLogicalStart is the first character of the run.
   *
   * @param aLogicalLimit will receive the limit of the run.
   *      The l-value that you point to here may be the
   *      same expression (variable) as the one for
   *      <code>aLogicalStart</code>.
   *      This pointer can be <code>NULL</code> if this
   *      value is not necessary.
   *
   * @param aLevel will receive the level of the run.
   *      This pointer can be <code>NULL</code> if this
   *      value is not necessary.
   */
  NS_IMETHOD GetLogicalRun(PRInt32 aLogicalStart, PRInt32* aLogicalLimit, nsBidiLevel* aLevel) = 0;

  /**
   * Get the number of runs.
   * This function may invoke the actual reordering on the
   * <code>nsBidi</code> object, after <code>SetPara</code>
   * may have resolved only the levels of the text. Therefore,
   * <code>CountRuns</code> may have to allocate memory,
   * and may fail doing so.
   *
   * @param aRunCount will receive the number of runs.
   */
  NS_IMETHOD CountRuns(PRInt32* aRunCount) = 0;

  /**
   * Get one run's logical start, length, and directionality,
   * which can be 0 for LTR or 1 for RTL.
   * In an RTL run, the character at the logical start is
   * visually on the right of the displayed run.
   * The length is the number of characters in the run.<p>
   * <code>CountRuns</code> should be called
   * before the runs are retrieved.
   *
   * @param aRunIndex is the number of the run in visual order, in the
   *      range <code>[0..CountRuns-1]</code>.
   *
   * @param aLogicalStart is the first logical character index in the text.
   *      The pointer may be <code>NULL</code> if this index is not needed.
   *
   * @param aLength is the number of characters (at least one) in the run.
   *      The pointer may be <code>NULL</code> if this is not needed.
   *
   * @param aDirection will receive the directionality of the run,
   *       <code>NSBIDI_LTR==0</code> or <code>NSBIDI_RTL==1</code>,
   *       never <code>NSBIDI_MIXED</code>.
   *
   * @see CountRuns<p>
   *
   * Example:
   * @code
   *  PRInt32 i, count, logicalStart, visualIndex=0, length;
   *  nsBidiDirection dir;
   *  pBidi->CountRuns(&count);
   *  for(i=0; i<count; ++i) {
   *    pBidi->GetVisualRun(i, &logicalStart, &length, &dir);
   *    if(NSBIDI_LTR==dir) {
   *      do { // LTR
   *        show_char(text[logicalStart++], visualIndex++);
   *      } while(--length>0);
   *    } else {
   *      logicalStart+=length;  // logicalLimit
   *      do { // RTL
   *        show_char(text[--logicalStart], visualIndex++);
   *      } while(--length>0);
   *    }
   *  }
   * @endcode
   *
   * Note that in right-to-left runs, code like this places
   * modifier letters before base characters and second surrogates
   * before first ones.
   */
  NS_IMETHOD GetVisualRun(PRInt32 aRunIndex, PRInt32* aLogicalStart, PRInt32* aLength, nsBidiDirection* aDirection) = 0;

#ifdef FULL_BIDI_ENGINE
  /**
   * Get the visual position from a logical text position.
   * If such a mapping is used many times on the same
   * <code>nsBidi</code> object, then calling
   * <code>GetLogicalMap</code> is more efficient.<p>
   *
   * Note that in right-to-left runs, this mapping places
   * modifier letters before base characters and second surrogates
   * before first ones.
   *
   * @param aLogicalIndex is the index of a character in the text.
   *
   * @param aVisualIndex will receive the visual position of this character.
   *
   * @see GetLogicalMap
   * @see GetLogicalIndex
   */
  NS_IMETHOD GetVisualIndex(PRInt32 aLogicalIndex, PRInt32* aVisualIndex) = 0;

  /**
   * Get the logical text position from a visual position.
   * If such a mapping is used many times on the same
   * <code>nsBidi</code> object, then calling
   * <code>GetVisualMap</code> is more efficient.<p>
   *
   * This is the inverse function to <code>GetVisualIndex</code>.
   *
   * @param aVisualIndex is the visual position of a character.
   *
   * @param aLogicalIndex will receive the index of this character in the text.
   *
   * @see GetVisualMap
   * @see GetVisualIndex
   */
  NS_IMETHOD GetLogicalIndex(PRInt32 aVisualIndex, PRInt32* aLogicalIndex) = 0;

  /**
   * Get a logical-to-visual index map (array) for the characters in the nsBidi
   * (paragraph or line) object.
   *
   * @param aIndexMap is a pointer to an array of <code>GetLength</code>
   *      indexes which will reflect the reordering of the characters.
   *      The array does not need to be initialized.<p>
   *      The index map will result in <code>aIndexMap[aLogicalIndex]==aVisualIndex</code>.<p>
   *
   * @see GetVisualMap
   * @see GetVisualIndex
   */
  NS_IMETHOD GetLogicalMap(PRInt32 *aIndexMap) = 0;

  /**
   * Get a visual-to-logical index map (array) for the characters in the nsBidi
   * (paragraph or line) object.
   *
   * @param aIndexMap is a pointer to an array of <code>GetLength</code>
   *      indexes which will reflect the reordering of the characters.
   *      The array does not need to be initialized.<p>
   *      The index map will result in <code>aIndexMap[aVisualIndex]==aLogicalIndex</code>.<p>
   *
   * @see GetLogicalMap
   * @see GetLogicalIndex
   */
  NS_IMETHOD GetVisualMap(PRInt32 *aIndexMap) = 0;

  /**
   * This is a convenience function that does not use a nsBidi object.
   * It is intended to be used for when an application has determined the levels
   * of objects (character sequences) and just needs to have them reordered (L2).
   * This is equivalent to using <code>GetLogicalMap</code> on a
   * <code>nsBidi</code> object.
   *
   * @param aLevels is an array with <code>aLength</code> levels that have been determined by
   *      the application.
   *
   * @param aLength is the number of levels in the array, or, semantically,
   *      the number of objects to be reordered.
   *      It must be <code>aLength>0</code>.
   *
   * @param aIndexMap is a pointer to an array of <code>aLength</code>
   *      indexes which will reflect the reordering of the characters.
   *      The array does not need to be initialized.<p>
   *      The index map will result in <code>aIndexMap[aLogicalIndex]==aVisualIndex</code>.
   */
  NS_IMETHOD ReorderLogical(const nsBidiLevel *aLevels, PRInt32 aLength, PRInt32 *aIndexMap) = 0;
#endif // FULL_BIDI_ENGINE
  /**
   * This is a convenience function that does not use a nsBidi object.
   * It is intended to be used for when an application has determined the levels
   * of objects (character sequences) and just needs to have them reordered (L2).
   * This is equivalent to using <code>GetVisualMap</code> on a
   * <code>nsBidi</code> object.
   *
   * @param aLevels is an array with <code>aLength</code> levels that have been determined by
   *      the application.
   *
   * @param aLength is the number of levels in the array, or, semantically,
   *      the number of objects to be reordered.
   *      It must be <code>aLength>0</code>.
   *
   * @param aIndexMap is a pointer to an array of <code>aLength</code>
   *      indexes which will reflect the reordering of the characters.
   *      The array does not need to be initialized.<p>
   *      The index map will result in <code>aIndexMap[aVisualIndex]==aLogicalIndex</code>.
   */
  NS_IMETHOD ReorderVisual(const nsBidiLevel *aLevels, PRInt32 aLength, PRInt32 *aIndexMap) = 0;

#ifdef FULL_BIDI_ENGINE
  /**
   * Invert an index map.
   * The one-to-one index mapping of the first map is inverted and written to
   * the second one.
   *
   * @param aSrcMap is an array with <code>aLength</code> indexes
   *      which define the original mapping.
   *
   * @param aDestMap is an array with <code>aLength</code> indexes
   *      which will be filled with the inverse mapping.
   *
   * @param aLength is the length of each array.
   */
  NS_IMETHOD InvertMap(const PRInt32 *aSrcMap, PRInt32 *aDestMap, PRInt32 aLength) = 0;
#endif // FULL_BIDI_ENGINE
  /**
   * Reverse a Right-To-Left run of Unicode text.
   *
   * This function preserves the integrity of characters with multiple
   * code units and (optionally) modifier letters.
   * Characters can be replaced by mirror-image characters
   * in the destination buffer. Note that "real" mirroring has
   * to be done in a rendering engine by glyph selection
   * and that for many "mirrored" characters there are no
   * Unicode characters as mirror-image equivalents.
   * There are also options to insert or remove Bidi control
   * characters; see the description of the <code>aDestSize</code>
   * and <code>aOptions</code> parameters and of the option bit flags.
   *
   * Since no Bidi controls are inserted here, this function will never
   * write more than <code>aSrcLength</code> characters to <code>aDest</code>.
   *
   * @param aSrc A pointer to the RTL run text.
   *
   * @param aSrcLength The length of the RTL run.
   *                 If the <code>NSBIDI_REMOVE_BIDI_CONTROLS</code> option
   *                 is set, then the destination length may be less than
   *                 <code>aSrcLength</code>.
   *                 If this option is not set, then the destination length
   *                 will be exactly <code>aSrcLength</code>.
   *
   * @param aDest A pointer to where the reordered text is to be copied.
   *             <code>aSrc[aSrcLength]</code> and <code>aDest[aSrcLength]</code>
   *             must not overlap.
   *
   * @param aOptions A bit set of options for the reordering that control
   *                how the reordered text is written.
   *
   * @param aDestSize will receive the number of characters that were written to <code>aDest</code>.
   */
  NS_IMETHOD WriteReverse(const PRUnichar *aSrc, PRInt32 aSrcLength, PRUnichar *aDest, PRUint16 aOptions, PRInt32 *aDestSize) = 0;
};

#endif  /* nsIBidi_h__ */

#endif /* IBMBIDI */

