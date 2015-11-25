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
 *  doc++ -H nsBidi.h
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
 * <li><code>aParaLevel</code> can be set to the
 * pseudo-level values <code>NSBIDI_DEFAULT_LTR</code>
 * and <code>NSBIDI_DEFAULT_RTL</code>.</li></ul>
 *
 * @see nsBidi::SetPara
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
#define NSBIDI_MAX_EXPLICIT_LEVEL 125

/** Bit flag for level input.
 *  Overrides directional properties.
 */
#define NSBIDI_LEVEL_OVERRIDE 0x80

/**
 * Special value which can be returned by the mapping functions when a logical
 * index has no corresponding visual index or vice-versa.
 * @see GetVisualIndex
 * @see GetVisualMap
 * @see GetLogicalIndex
 * @see GetLogicalMap
 */
#define NSBIDI_MAP_NOWHERE (-1)

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

/* helper macros for each allocated array member */
#define GETDIRPROPSMEMORY(length) \
                                  GetMemory((void **)&mDirPropsMemory, &mDirPropsSize, \
                                  (length))

#define GETLEVELSMEMORY(length) \
                                GetMemory((void **)&mLevelsMemory, &mLevelsSize, \
                                (length))

#define GETRUNSMEMORY(length) \
                              GetMemory((void **)&mRunsMemory, &mRunsSize, \
                              (length)*sizeof(Run))

#define GETISOLATESMEMORY(length) \
                                  GetMemory((void **)&mIsolatesMemory, &mIsolatesSize, \
                                  (length)*sizeof(Isolate))

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
#define MASK_LTR (DIRPROP_FLAG(L)|DIRPROP_FLAG(EN)|DIRPROP_FLAG(AN)|DIRPROP_FLAG(LRE)|DIRPROP_FLAG(LRO)|DIRPROP_FLAG(LRI))
#define MASK_RTL (DIRPROP_FLAG(R)|DIRPROP_FLAG(AL)|DIRPROP_FLAG(RLE)|DIRPROP_FLAG(RLO)|DIRPROP_FLAG(RLI))
#define MASK_R_AL (DIRPROP_FLAG(R)|DIRPROP_FLAG(AL))

/* explicit embedding codes */
#define MASK_EXPLICIT (DIRPROP_FLAG(LRE)|DIRPROP_FLAG(LRO)|DIRPROP_FLAG(RLE)|DIRPROP_FLAG(RLO)|DIRPROP_FLAG(PDF))

/* explicit isolate codes */
#define MASK_ISO (DIRPROP_FLAG(LRI)|DIRPROP_FLAG(RLI)|DIRPROP_FLAG(FSI)|DIRPROP_FLAG(PDI))

#define MASK_BN_EXPLICIT (DIRPROP_FLAG(BN)|MASK_EXPLICIT)

/* paragraph and segment separators */
#define MASK_B_S (DIRPROP_FLAG(B)|DIRPROP_FLAG(S))

/* all types that are counted as White Space or Neutral in some steps */
#define MASK_WS (MASK_B_S|DIRPROP_FLAG(WS)|MASK_BN_EXPLICIT|MASK_ISO)

/* types that are neutrals or could becomes neutrals in (Wn) */
#define MASK_POSSIBLE_N (DIRPROP_FLAG(O_N)|DIRPROP_FLAG(CS)|DIRPROP_FLAG(ES)|DIRPROP_FLAG(ET)|MASK_WS)

/*
 * These types may be changed to "e",
 * the embedding type (L or R) of the run,
 * in the Bidi algorithm (N2)
 */
#define MASK_EMBEDDING (DIRPROP_FLAG(NSM)|MASK_POSSIBLE_N)

/* the dirProp's L and R are defined to 0 and 1 values in nsCharType */
#define GET_LR_FROM_LEVEL(level) ((DirProp)((level)&1))

#define IS_DEFAULT_LEVEL(level) (((level)&0xfe)==0xfe)

/*
 * The following bit is ORed to the property of directional control
 * characters which are ignored: unmatched PDF or PDI; LRx, RLx or FSI
 * which would exceed the maximum explicit bidi level.
 */
#define IGNORE_CC 0x40

#define PURE_DIRPROP(prop) ((prop)&~IGNORE_CC)

/*
 * The following bit is used for the directional isolate status.
 * Stack entries corresponding to isolate sequences are greater than ISOLATE.
 */
#define ISOLATE 0x0100

/* number of isolate entries allocated initially without malloc */
#define SIMPLE_ISOLATES_SIZE 5

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
                                         (s)[(i)++]=(char16_t)(c); \
                                         } else { \
                                         (s)[(i)++]=(char16_t)((c)>>10)+0xd7c0; \
                                         (s)[(i)++]=(char16_t)(c)&0x3ff|0xdc00; \
                                         } \
}

/* safe versions with error-checking and optional regularity-checking */

#define UTF16_APPEND_CHAR_SAFE(s, i, length, c) { \
                                                if((PRUInt32)(c)<=0xffff) { \
                                                (s)[(i)++]=(char16_t)(c); \
                                                } else if((PRUInt32)(c)<=0x10ffff) { \
                                                if((i)+1<(length)) { \
                                                (s)[(i)++]=(char16_t)((c)>>10)+0xd7c0; \
                                                (s)[(i)++]=(char16_t)(c)&0x3ff|0xdc00; \
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
                                                     char16_t __c2; \
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

struct Isolate {
  int32_t start1;
  int16_t stateImp;
  int16_t state;
};

/* Run structure for reordering --------------------------------------------- */

typedef struct Run {
  int32_t logicalStart;  /* first character of the run; b31 indicates even/odd level */
  int32_t visualLimit;   /* last visual position of the run +1 */
} Run;

/* in a Run, logicalStart will get this bit set if the run level is odd */
#define INDEX_ODD_BIT (1UL<<31)

#define MAKE_INDEX_ODD_PAIR(index, level) (index|((uint32_t)level<<31))
#define ADD_ODD_BIT_FROM_LEVEL(x, level)  ((x)|=((uint32_t)level<<31))
#define REMOVE_ODD_BIT(x)          ((x)&=~INDEX_ODD_BIT)

#define GET_INDEX(x)   ((x)&~INDEX_ODD_BIT)
#define GET_ODD_BIT(x) ((uint32_t)(x)>>31)
#define IS_ODD_RUN(x)  (((x)&INDEX_ODD_BIT)!=0)
#define IS_EVEN_RUN(x) (((x)&INDEX_ODD_BIT)==0)

typedef uint32_t Flags;

enum { DirProp_L=0, DirProp_R=1, DirProp_EN=2, DirProp_AN=3, DirProp_ON=4, DirProp_S=5, DirProp_B=6 }; /* reduced dirProp */

#define IMPTABLEVELS_COLUMNS (DirProp_B + 2)
typedef const uint8_t ImpTab[][IMPTABLEVELS_COLUMNS];
typedef const uint8_t (*PImpTab)[IMPTABLEVELS_COLUMNS];

typedef const uint8_t ImpAct[];
typedef const uint8_t *PImpAct;

struct LevState {
    PImpTab pImpTab;                    /* level table pointer          */
    PImpAct pImpAct;                    /* action map array             */
    int32_t startON;                    /* start of ON sequence         */
    int32_t state;                      /* current state                */
    int32_t runStart;                   /* start position of the run    */
    nsBidiLevel runLevel;               /* run level before implicit solving */
};

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
   */
  nsresult SetPara(const char16_t *aText, int32_t aLength, nsBidiLevel aParaLevel);

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
   *      This pointer can be <code>nullptr</code> if this
   *      value is not necessary.
   *
   * @param aLevel will receive the level of the run.
   *      This pointer can be <code>nullptr</code> if this
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
   *      The pointer may be <code>nullptr</code> if this index is not needed.
   *
   * @param aLength is the number of characters (at least one) in the run.
   *      The pointer may be <code>nullptr</code> if this is not needed.
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
  nsresult WriteReverse(const char16_t *aSrc, int32_t aSrcLength, char16_t *aDest, uint16_t aOptions, int32_t *aDestSize);

protected:
  friend class nsBidiPresUtils;

  /** length of the current text */
  int32_t mLength;

  /** memory sizes in bytes */
  size_t mDirPropsSize, mLevelsSize, mRunsSize;
  size_t mIsolatesSize;

  /** allocated memory */
  DirProp* mDirPropsMemory;
  nsBidiLevel* mLevelsMemory;
  Run* mRunsMemory;
  Isolate* mIsolatesMemory;

  DirProp* mDirProps;
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

  /* maxium of current nesting depth of isolate sequences */
  /* Within ResolveExplicitLevels() and checkExpicitLevels(), this is the maximal
     nesting encountered.
     Within ResolveImplicitLevels(), this is the index of the current isolates
     stack entry. */
  int32_t mIsolateCount;
  Isolate* mIsolates;

  /** for simple text, have a small stack (no malloc()) */
  Isolate mSimpleIsolates[SIMPLE_ISOLATES_SIZE];

private:

  void Init();

  bool GetMemory(void **aMemory, size_t* aSize, size_t aSizeNeeded);

  void Free();

  void GetDirProps(const char16_t *aText);

  void ResolveExplicitLevels(nsBidiDirection *aDirection);

  nsBidiDirection DirectionFromFlags(Flags aFlags);

  void ProcessPropertySeq(LevState *pLevState, uint8_t _prop, int32_t start, int32_t limit);

  void ResolveImplicitLevels(int32_t aStart, int32_t aLimit, DirProp aSOR, DirProp aEOR);

  void AdjustWSLevels();

  void SetTrailingWSStart();

  bool GetRuns();

  void GetSingleRun(nsBidiLevel aLevel);

  void ReorderLine(nsBidiLevel aMinLevel, nsBidiLevel aMaxLevel);

  static bool PrepareReorder(const nsBidiLevel *aLevels, int32_t aLength, int32_t *aIndexMap, nsBidiLevel *aMinLevel, nsBidiLevel *aMaxLevel);
};

#endif // _nsBidi_h_
