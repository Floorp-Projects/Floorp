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

#ifndef nsBidiImp_h__
#define nsBidiImp_h__

#include "nsIBidi.h"

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
         PR_TRUE, (length))

#define GETINITIALLEVELSMEMORY(length) \
         GetMemory((void **)&mLevelsMemory, &mLevelsSize, \
         PR_TRUE, (length))

#define GETINITIALRUNSMEMORY(length) \
         GetMemory((void **)&mRunsMemory, &mRunsSize, \
         PR_TRUE, (length)*sizeof(Run))

/*
 * Sometimes, bit values are more appropriate
 * to deal with directionality properties.
 * Abbreviations in these macro names refer to names
 * used in the Bidi algorithm.
 */
typedef PRUint8 DirProp;

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
                                         if((PRUint32)(c)<=0xffff) { \
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
                                     PRInt32 __N=(n); \
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
                                          PRInt32 __N=(n); \
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

#define UTF_PREV_CHAR(s, start, i, c)                UTF_PREV_CHAR_SAFE(s, start, i, c, PR_FALSE)
#define UTF_BACK_1(s, start, i)                      UTF_BACK_1_SAFE(s, start, i)
#define UTF_BACK_N(s, start, i, n)                   UTF_BACK_N_SAFE(s, start, i, n)
#define UTF_APPEND_CHAR(s, i, length, c)             UTF_APPEND_CHAR_SAFE(s, i, length, c)

/* Run structure for reordering --------------------------------------------- */

typedef struct Run {
    PRInt32 logicalStart,  /* first character of the run; b31 indicates even/odd level */
    visualLimit;  /* last visual position of the run +1 */
} Run;

/* in a Run, logicalStart will get this bit set if the run level is odd */
#define INDEX_ODD_BIT (1UL<<31)

#define MAKE_INDEX_ODD_PAIR(index, level) (index|((PRUint32)level<<31))
#define ADD_ODD_BIT_FROM_LEVEL(x, level)  ((x)|=((PRUint32)level<<31))
#define REMOVE_ODD_BIT(x)          ((x)&=~INDEX_ODD_BIT)

#define GET_INDEX(x)   (x&~INDEX_ODD_BIT)
#define GET_ODD_BIT(x) ((PRUint32)x>>31)
#define IS_ODD_RUN(x)  ((x&INDEX_ODD_BIT)!=0)
#define IS_EVEN_RUN(x) ((x&INDEX_ODD_BIT)==0)

typedef PRUint32 Flags;

 /**
  * Implementation of the nsIBidi interface
  */
class nsBidi : public nsIBidi {
  NS_DECL_ISUPPORTS
      
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

  /** @brief Preallocating constructor
   * Allocate an <code>nsBidi</code>
   * object with preallocated memory for internal structures.   This
   * constructor provides an <code>nsBidi</code> object like
   * the default constructor, but it also
   * preallocates memory for internal structures according to the sizings
   * supplied by the caller.<p> Subsequent functions will not allocate
   * any more memory, and are thus guaranteed not to fail because of lack
   * of memory.<p> The preallocation can be limited to some of the
   * internal memory by setting some values to 0 here. That means that
   * if, e.g., <code>aMaxRunCount</code> cannot be reasonably
   * predetermined and should not be set to <code>aMaxLength</code> (the
   * only failproof value) to avoid wasting memory, then
   * <code>aMaxRunCount</code> could be set to 0 here and the internal
   * structures that are associated with it will be allocated on demand,
   * just like with the default constructor.
   *
   * If sufficient memory could not be allocated, no exception is thrown.
   * Test whether mDirPropsSize == aMaxLength and/or mRunsSize == aMaxRunCount.
   *
   * @param aMaxLength is the maximum paragraph or line length that internal memory
   *      will be preallocated for. An attempt to associate this object with a
   *      longer text will fail, unless this value is 0, which leaves the allocation
   *      up to the implementation.
   *
   * @param aMaxRunCount is the maximum anticipated number of same-level runs
   *      that internal memory will be preallocated for. An attempt to access
   *      visual runs on an object that was not preallocated for as many runs
   *      as the text was actually resolved to will fail,
   *      unless this value is 0, which leaves the allocation up to the implementation.<p>
   *      The number of runs depends on the actual text and maybe anywhere between
   *      1 and <code>aMaxLength</code>. It is typically small.<p>
   */
  nsBidi(PRUint32 aMaxLength, PRUint32 aMaxRunCount);

  /** @brief Destructor. */
  virtual ~nsBidi();

  NS_IMETHOD SetPara(const PRUnichar *aText, PRInt32 aLength, nsBidiLevel aParaLevel, nsBidiLevel *aEmbeddingLevels);

#ifdef FULL_BIDI_ENGINE
  NS_IMETHOD SetLine(nsIBidi* aParaBidi, PRInt32 aStart, PRInt32 aLimit);
  
  NS_IMETHOD GetDirection(nsBidiDirection* aDirection);
  
  NS_IMETHOD GetLength(PRInt32* aLength);
  
  NS_IMETHOD GetParaLevel(nsBidiLevel* aParaLevel);

  NS_IMETHOD GetLevelAt(PRInt32 aCharIndex,  nsBidiLevel* aLevel);
  
  NS_IMETHOD GetLevels(nsBidiLevel** aLevels);
#endif // FULL_BIDI_ENGINE
  NS_IMETHOD GetCharTypeAt(PRInt32 aCharIndex,  nsCharType* aType);

  NS_IMETHOD GetLogicalRun(PRInt32 aLogicalStart, PRInt32* aLogicalLimit, nsBidiLevel* aLevel);
  
  NS_IMETHOD CountRuns(PRInt32* aRunCount);
  
  NS_IMETHOD GetVisualRun(PRInt32 aRunIndex, PRInt32* aLogicalStart, PRInt32* aLength, nsBidiDirection* aDirection);
#ifdef FULL_BIDI_ENGINE  
  NS_IMETHOD GetVisualIndex(PRInt32 aLogicalIndex, PRInt32* aVisualIndex);
  
  NS_IMETHOD GetLogicalIndex(PRInt32 aVisualIndex, PRInt32* aLogicalIndex);
  
  NS_IMETHOD GetLogicalMap(PRInt32 *aIndexMap);
  
  NS_IMETHOD GetVisualMap(PRInt32 *aIndexMap);
  
  NS_IMETHOD ReorderLogical(const nsBidiLevel *aLevels, PRInt32 aLength, PRInt32 *aIndexMap);

  NS_IMETHOD InvertMap(const PRInt32 *aSrcMap, PRInt32 *aDestMap, PRInt32 aLength);
#endif // FULL_BIDI_ENGINE  
  NS_IMETHOD ReorderVisual(const nsBidiLevel *aLevels, PRInt32 aLength, PRInt32 *aIndexMap);

  NS_IMETHOD WriteReverse(const PRUnichar *aSrc, PRInt32 aSrcLength, PRUnichar *aDest, PRUint16 aOptions, PRInt32 *aDestSize);

protected:
  /** length of the current text */
  PRInt32 mLength;

  /** memory sizes in bytes */
  PRSize mDirPropsSize, mLevelsSize, mRunsSize;

  /** allocated memory */
  DirProp* mDirPropsMemory;
  nsBidiLevel* mLevelsMemory;
  Run* mRunsMemory;

  /** indicators for whether memory may be allocated after construction */
  PRBool mMayAllocateText, mMayAllocateRuns;

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
  PRInt32 mTrailingWSStart;
  
  /** fields for line reordering */
  PRInt32 mRunCount;     /* ==-1: runs not set up yet */
  Run* mRuns;

  /** for non-mixed text, we only need a tiny array of runs (no malloc()) */
  Run mSimpleRuns[1];

private:

  void Init();
  
  PRBool GetMemory(void **aMemory, PRSize* aSize, PRBool aMayAllocate, PRSize aSizeNeeded);

  void Free();
  
  void GetDirProps(const PRUnichar *aText);

  nsBidiDirection ResolveExplicitLevels();

  nsresult CheckExplicitLevels(nsBidiDirection *aDirection);

  nsBidiDirection DirectionFromFlags(Flags aFlags);

  void ResolveImplicitLevels(PRInt32 aStart, PRInt32 aLimit, DirProp aSOR, DirProp aEOR);

  void AdjustWSLevels();

  void SetTrailingWSStart();

  PRBool GetRuns();

  void GetSingleRun(nsBidiLevel aLevel);

  void ReorderLine(nsBidiLevel aMinLevel, nsBidiLevel aMaxLevel);

  PRBool PrepareReorder(const nsBidiLevel *aLevels, PRInt32 aLength, PRInt32 *aIndexMap, nsBidiLevel *aMinLevel, nsBidiLevel *aMaxLevel);
};
#endif

#endif // IBMBIDI
