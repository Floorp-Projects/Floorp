/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBidi_h__
#define nsBidi_h__

#include "nsBidiUtils.h"

// Bidi reordering engine from ICU
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
typedef uint8_t nsBidiLevel;

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

/* helper macros for each allocated array member */
#define GETDIRPROPSMEMORY(length) \
                                  GetMemory((void **)&mDirPropsMemory, &mDirPropsSize, \
                                  mMayAllocateText, (length))

#define GETLEVELSMEMORY(length) \
                                GetMemory((void **)&mLevelsMemory, &mLevelsSize, \
                                mMayAllocateText, (length))

#define GETRUNSMEMORY(length) \
                              GetMemory((void **)&mRunsMemory, &mRunsSize, \
                              mMayAllocateRuns, (length)*sizeof(Run))

/* additional macros used by constructor - always allow allocation */
#define GETINITIALDIRPROPSMEMORY(length) \
                                         GetMemory((void **)&mDirPropsMemory, &mDirPropsSize, \
                                         true, (length))

#define GETINITIALLEVELSMEMORY(length) \
                                       GetMemory((void **)&mLevelsMemory, &mLevelsSize, \
                                       true, (length))

#define GETINITIALRUNSMEMORY(length) \
                                     GetMemory((void **)&mRunsMemory, &mRunsSize, \
                                     true, (length)*sizeof(Run))

/*
 * Sometimes, bit values are more appropriate
 * to deal with directionality properties.
 * Abbreviations in these macro names refer to names
 * used in the Bidi algorithm.
 */
typedef uint8_t DirProp;

#define DIRPROP_FLAG(dir) (1UL<<(dir))

/* special flag for multiple runs from explicit embedding codes */
#define DIRPROP_FLAG_MULTI_RUNS (1UL<<31)

/* are there any characters that are LTR or RTL? */
#define MASK_LTR (DIRPROP_FLAG(L)|DIRPROP_FLAG(EN)|DIRPROP_FLAG(AN)|DIRPROP_FLAG(LRE)|DIRPROP_FLAG(LRO))
#define MASK_RTL (DIRPROP_FLAG(R)|DIRPROP_FLAG(AL)|DIRPROP_FLAG(RLE)|DIRPROP_FLAG(RLO))

/* explicit embedding codes */
#define MASK_LRX (DIRPROP_FLAG(LRE)|DIRPROP_FLAG(LRO))
#define MASK_RLX (DIRPROP_FLAG(RLE)|DIRPROP_FLAG(RLO))
#define MASK_OVERRIDE (DIRPROP_FLAG(LRO)|DIRPROP_FLAG(RLO))

#define MASK_EXPLICIT (MASK_LRX|MASK_RLX|DIRPROP_FLAG(PDF))
#define MASK_BN_EXPLICIT (DIRPROP_FLAG(BN)|MASK_EXPLICIT)

/* paragraph and segment separators */
#define MASK_B_S (DIRPROP_FLAG(B)|DIRPROP_FLAG(S))

/* all types that are counted as White Space or Neutral in some steps */
#define MASK_WS (MASK_B_S|DIRPROP_FLAG(WS)|MASK_BN_EXPLICIT)
#define MASK_N (DIRPROP_FLAG(O_N)|MASK_WS)

/* all types that are included in a sequence of European Terminators for (W5) */
#define MASK_ET_NSM_BN (DIRPROP_FLAG(ET)|DIRPROP_FLAG(NSM)|MASK_BN_EXPLICIT)

/* types that are neutrals or could becomes neutrals in (Wn) */
#define MASK_POSSIBLE_N (DIRPROP_FLAG(CS)|DIRPROP_FLAG(ES)|DIRPROP_FLAG(ET)|MASK_N)

/*
 * These types may be changed to "e",
 * the embedding type (L or R) of the run,
 * in the Bidi algorithm (N2)
 */
#define MASK_EMBEDDING (DIRPROP_FLAG(NSM)|MASK_POSSIBLE_N)

/* the dirProp's L and R are defined to 0 and 1 values in nsCharType */
#define GET_LR_FROM_LEVEL(level) ((DirProp)((level)&1))

#define IS_DEFAULT_LEVEL(level) (((level)&0xfe)==0xfe)

/* handle surrogate pairs --------------------------------------------------- */

#define IS_FIRST_SURROGATE(uchar) (((uchar)&0xfc00)==0xd800)
#define IS_SECOND_SURROGATE(uchar) (((uchar)&0xfc00)==0xdc00)

/* get the UTF-32 value directly from the surrogate pseudo-characters */
#define SURROGATE_OFFSET ((0xd800<<10UL)+0xdc00-0x10000)
#define GET_UTF_32(first, second) (((first)<<10UL)+(second)-SURROGATE_OFFSET)


#define UTF_ERROR_VALUE 0xffff
/* definitions with forward iteration --------------------------------------- */

/*
 * all the macros that go forward assume that
 * the initial offset is 0<=i<length;
 * they update the offset
 */

/* fast versions, no error-checking */

#define UTF16_APPEND_CHAR_UNSAFE(s, i, c){ \
                                         if((uint32_t)(c)<=0xffff) { \
                                         (s)[(i)++]=(PRUnichar)(c); \
                                         } else { \
                                         (s)[(i)++]=(PRUnichar)((c)>>10)+0xd7c0; \
                                         (s)[(i)++]=(PRUnichar)(c)&0x3ff|0xdc00; \
                                         } \
}

/* safe versions with error-checking and optional regularity-checking */

#define UTF16_APPEND_CHAR_SAFE(s, i, length, c) { \
                                                if((PRUInt32)(c)<=0xffff) { \
                                                (s)[(i)++]=(PRUnichar)(c); \
                                                } else if((PRUInt32)(c)<=0x10ffff) { \
                                                if((i)+1<(length)) { \
                                                (s)[(i)++]=(PRUnichar)((c)>>10)+0xd7c0; \
                                                (s)[(i)++]=(PRUnichar)(c)&0x3ff|0xdc00; \
                                                } else /* not enough space */ { \
                                                (s)[(i)++]=UTF_ERROR_VALUE; \
                                                } \
                                                } else /* c>0x10ffff, write error value */ { \
                                                (s)[(i)++]=UTF_ERROR_VALUE; \
                                                } \
}

/* definitions with backward iteration -------------------------------------- */

/*
 * all the macros that go backward assume that
 * the valid buffer range starts at offset 0
 * and that the initial offset is 0<i<=length;
 * they update the offset
 */

/* fast versions, no error-checking */

/*
 * Get a single code point from an offset that points behind the last
 * of the code units that belong to that code point.
 * Assume 0<=i<length.
 */
#define UTF16_PREV_CHAR_UNSAFE(s, i, c) { \
                                        (c)=(s)[--(i)]; \
                                        if(IS_SECOND_SURROGATE(c)) { \
                                        (c)=GET_UTF_32((s)[--(i)], (c)); \
                                        } \
}

#define UTF16_BACK_1_UNSAFE(s, i) { \
                                  if(IS_SECOND_SURROGATE((s)[--(i)])) { \
                                  --(i); \
                                  } \
}

#define UTF16_BACK_N_UNSAFE(s, i, n) { \
                                     int32_t __N=(n); \
                                     while(__N>0) { \
                                     UTF16_BACK_1_UNSAFE(s, i); \
                                     --__N; \
                                     } \
}

/* safe versions with error-checking and optional regularity-checking */

#define UTF16_PREV_CHAR_SAFE(s, start, i, c, strict) { \
                                                     (c)=(s)[--(i)]; \
                                                     if(IS_SECOND_SURROGATE(c)) { \
                                                     PRUnichar __c2; \
                                                     if((i)>(start) && IS_FIRST_SURROGATE(__c2=(s)[(i)-1])) { \
                                                     --(i); \
                                                     (c)=GET_UTF_32(__c2, (c)); \
      /* strict: ((c)&0xfffe)==0xfffe is caught by UTF_IS_ERROR() */ \
                                                     } else if(strict) {\
      /* unmatched second surrogate */ \
                                                     (c)=UTF_ERROR_VALUE; \
                                                     } \
                                                     } else if(strict && IS_FIRST_SURROGATE(c)) { \
      /* unmatched first surrogate */ \
                                                     (c)=UTF_ERROR_VALUE; \
  /* else strict: (c)==0xfffe is caught by UTF_IS_ERROR() */ \
                                                     } \
}

#define UTF16_BACK_1_SAFE(s, start, i) { \
                                       if(IS_SECOND_SURROGATE((s)[--(i)]) && (i)>(start) && IS_FIRST_SURROGATE((s)[(i)-1])) { \
                                       --(i); \
                                       } \
}

#define UTF16_BACK_N_SAFE(s, start, i, n) { \
                                          int32_t __N=(n); \
                                          while(__N>0 && (i)>(start)) { \
                                          UTF16_BACK_1_SAFE(s, start, i); \
                                          --__N; \
                                          } \
}

#define UTF_PREV_CHAR_UNSAFE(s, i, c)                UTF16_PREV_CHAR_UNSAFE(s, i, c)
#define UTF_PREV_CHAR_SAFE(s, start, i, c, strict)   UTF16_PREV_CHAR_SAFE(s, start, i, c, strict)
#define UTF_BACK_1_UNSAFE(s, i)                      UTF16_BACK_1_UNSAFE(s, i)
#define UTF_BACK_1_SAFE(s, start, i)                 UTF16_BACK_1_SAFE(s, start, i)
#define UTF_BACK_N_UNSAFE(s, i, n)                   UTF16_BACK_N_UNSAFE(s, i, n)
#define UTF_BACK_N_SAFE(s, start, i, n)              UTF16_BACK_N_SAFE(s, start, i, n)
#define UTF_APPEND_CHAR_UNSAFE(s, i, c)              UTF16_APPEND_CHAR_UNSAFE(s, i, c)
#define UTF_APPEND_CHAR_SAFE(s, i, length, c)        UTF16_APPEND_CHAR_SAFE(s, i, length, c)

#define UTF_PREV_CHAR(s, start, i, c)                UTF_PREV_CHAR_SAFE(s, start, i, c, false)
#define UTF_BACK_1(s, start, i)                      UTF_BACK_1_SAFE(s, start, i)
#define UTF_BACK_N(s, start, i, n)                   UTF_BACK_N_SAFE(s, start, i, n)
#define UTF_APPEND_CHAR(s, i, length, c)             UTF_APPEND_CHAR_SAFE(s, i, length, c)

/* Run structure for reordering --------------------------------------------- */

typedef struct Run {
  int32_t logicalStart,  /* first character of the run; b31 indicates even/odd level */
  visualLimit;  /* last visual position of the run +1 */
} Run;

/* in a Run, logicalStart will get this bit set if the run level is odd */
#define INDEX_ODD_BIT (1UL<<31)

#define MAKE_INDEX_ODD_PAIR(index, level) (index|((uint32_t)level<<31))
#define ADD_ODD_BIT_FROM_LEVEL(x, level)  ((x)|=((uint32_t)level<<31))
#define REMOVE_ODD_BIT(x)          ((x)&=~INDEX_ODD_BIT)

#define GET_INDEX(x)   (x&~INDEX_ODD_BIT)
#define GET_ODD_BIT(x) ((uint32_t)x>>31)
#define IS_ODD_RUN(x)  ((x&INDEX_ODD_BIT)!=0)
#define IS_EVEN_RUN(x) ((x&INDEX_ODD_BIT)==0)

typedef uint32_t Flags;

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
class nsBidi
{
public: 
  /** @brief Default constructor.
   * 
   * The nsBidi object is initially empty. It is assigned
   * the Bidi properties of a paragraph by <code>SetPara()</code>
   * or the Bidi properties of a line of a paragraph by
   * <code>GetLine()</code>.<p>
   * This object can be reused for as long as it is not destroyed.<p>
   * <code>SetPara()</code> will allocate additional memory for
   * internal structures as necessary.
   *
   */
  nsBidi();

  /** @brief Destructor. */
  virtual ~nsBidi();


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
  nsresult SetPara(const PRUnichar *aText, int32_t aLength, nsBidiLevel aParaLevel, nsBidiLevel *aEmbeddingLevels);

  /**
   * Get the directionality of the text.
   *
   * @param aDirection receives a <code>NSBIDI_XXX</code> value that indicates if the entire text
   *       represented by this object is unidirectional,
   *       and which direction, or if it is mixed-directional.
   *
   * @see nsBidiDirection
   */
  nsresult GetDirection(nsBidiDirection* aDirection);

  /**
   * Get the paragraph level of the text.
   *
   * @param aParaLevel receives a <code>NSBIDI_XXX</code> value indicating the paragraph level
   *
   * @see nsBidiLevel
   */
  nsresult GetParaLevel(nsBidiLevel* aParaLevel);

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
  nsresult SetLine(nsIBidi* aParaBidi, int32_t aStart, int32_t aLimit);  

  /**
   * Get the length of the text.
   *
   * @param aLength receives the length of the text that the nsBidi object was created for.
   */
  nsresult GetLength(int32_t* aLength);

  /**
   * Get the level for one character.
   *
   * @param aCharIndex the index of a character.
   *
   * @param aLevel receives the level for the character at aCharIndex.
   *
   * @see nsBidiLevel
   */
  nsresult GetLevelAt(int32_t aCharIndex,  nsBidiLevel* aLevel);

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
  nsresult GetLevels(nsBidiLevel** aLevels);
#endif // FULL_BIDI_ENGINE
  /**
   * Get the bidirectional type for one character.
   *
   * @param aCharIndex the index of a character.
   *
   * @param aType receives the bidirectional type of the character at aCharIndex.
   */
  nsresult GetCharTypeAt(int32_t aCharIndex,  nsCharType* aType);

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
  nsresult GetLogicalRun(int32_t aLogicalStart, int32_t* aLogicalLimit, nsBidiLevel* aLevel);

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
  nsresult CountRuns(int32_t* aRunCount);

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
   *  int32_t i, count, logicalStart, visualIndex=0, length;
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
  nsresult GetVisualRun(int32_t aRunIndex, int32_t* aLogicalStart, int32_t* aLength, nsBidiDirection* aDirection);

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
  nsresult GetVisualIndex(int32_t aLogicalIndex, int32_t* aVisualIndex);

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
  nsresult GetLogicalIndex(int32_t aVisualIndex, int32_t* aLogicalIndex);

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
  nsresult GetLogicalMap(int32_t *aIndexMap);

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
  nsresult GetVisualMap(int32_t *aIndexMap);

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
  static nsresult ReorderLogical(const nsBidiLevel *aLevels, int32_t aLength, int32_t *aIndexMap);
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
  static nsresult ReorderVisual(const nsBidiLevel *aLevels, int32_t aLength, int32_t *aIndexMap);

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
  nsresult InvertMap(const int32_t *aSrcMap, int32_t *aDestMap, int32_t aLength);
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
  nsresult WriteReverse(const PRUnichar *aSrc, int32_t aSrcLength, PRUnichar *aDest, uint16_t aOptions, int32_t *aDestSize);

protected:
  friend class nsBidiPresUtils;

  /** length of the current text */
  int32_t mLength;

  /** memory sizes in bytes */
  size_t mDirPropsSize, mLevelsSize, mRunsSize;

  /** allocated memory */
  DirProp* mDirPropsMemory;
  nsBidiLevel* mLevelsMemory;
  Run* mRunsMemory;

  /** indicators for whether memory may be allocated after construction */
  bool mMayAllocateText, mMayAllocateRuns;

  const DirProp* mDirProps;
  nsBidiLevel* mLevels;

  /** the paragraph level */
  nsBidiLevel mParaLevel;

  /** flags is a bit set for which directional properties are in the text */
  Flags mFlags;

  /** the overall paragraph or line directionality - see nsBidiDirection */
  nsBidiDirection mDirection;

  /** characters after trailingWSStart are WS and are */
  /* implicitly at the paraLevel (rule (L1)) - levels may not reflect that */
  int32_t mTrailingWSStart;

  /** fields for line reordering */
  int32_t mRunCount;     /* ==-1: runs not set up yet */
  Run* mRuns;

  /** for non-mixed text, we only need a tiny array of runs (no malloc()) */
  Run mSimpleRuns[1];

private:

  void Init();

  bool GetMemory(void **aMemory, size_t* aSize, bool aMayAllocate, size_t aSizeNeeded);

  void Free();

  void GetDirProps(const PRUnichar *aText);

  nsBidiDirection ResolveExplicitLevels();

  nsresult CheckExplicitLevels(nsBidiDirection *aDirection);

  nsBidiDirection DirectionFromFlags(Flags aFlags);

  void ResolveImplicitLevels(int32_t aStart, int32_t aLimit, DirProp aSOR, DirProp aEOR);

  void AdjustWSLevels();

  void SetTrailingWSStart();

  bool GetRuns();

  void GetSingleRun(nsBidiLevel aLevel);

  void ReorderLine(nsBidiLevel aMinLevel, nsBidiLevel aMaxLevel);

  static bool PrepareReorder(const nsBidiLevel *aLevels, int32_t aLength, int32_t *aIndexMap, nsBidiLevel *aMinLevel, nsBidiLevel *aMaxLevel);

  int32_t doWriteReverse(const PRUnichar *src, int32_t srcLength,
                         PRUnichar *dest, uint16_t options);

};

#endif // _nsBidi_h_
