/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsBidi.h"
#include "nsUnicodeProperties.h"
#include "nsCRTGlue.h"

using namespace mozilla::unicode;

// These are #defined in <sys/regset.h> under Solaris 10 x86
#undef CS
#undef ES

/*  Comparing the description of the Bidi algorithm with this implementation
    is easier with the same names for the Bidi types in the code as there.
*/
enum {
    L =   eCharType_LeftToRight,
    R =   eCharType_RightToLeft,
    EN =  eCharType_EuropeanNumber,
    ES =  eCharType_EuropeanNumberSeparator,
    ET =  eCharType_EuropeanNumberTerminator,
    AN =  eCharType_ArabicNumber,
    CS =  eCharType_CommonNumberSeparator,
    B =   eCharType_BlockSeparator,
    S =   eCharType_SegmentSeparator,
    WS =  eCharType_WhiteSpaceNeutral,
    O_N = eCharType_OtherNeutral,
    LRE = eCharType_LeftToRightEmbedding,
    LRO = eCharType_LeftToRightOverride,
    AL =  eCharType_RightToLeftArabic,
    RLE = eCharType_RightToLeftEmbedding,
    RLO = eCharType_RightToLeftOverride,
    PDF = eCharType_PopDirectionalFormat,
    NSM = eCharType_DirNonSpacingMark,
    BN =  eCharType_BoundaryNeutral,
    LRI = eCharType_LeftToRightIsolate,
    RLI = eCharType_RightToLeftIsolate,
    FSI = eCharType_FirstStrongIsolate,
    PDI = eCharType_PopDirectionalIsolate,
    dirPropCount
};

/* to avoid some conditional statements, use tiny constant arrays */
static Flags flagLR[2]={ DIRPROP_FLAG(L), DIRPROP_FLAG(R) };
static Flags flagE[2]={ DIRPROP_FLAG(LRE), DIRPROP_FLAG(RLE) };
static Flags flagO[2]={ DIRPROP_FLAG(LRO), DIRPROP_FLAG(RLO) };

#define DIRPROP_FLAG_LR(level) flagLR[(level)&1]
#define DIRPROP_FLAG_E(level) flagE[(level)&1]
#define DIRPROP_FLAG_O(level) flagO[(level)&1]

/*
 * General implementation notes:
 *
 * Throughout the implementation, there are comments like (W2) that refer to
 * rules of the Bidi algorithm in its version 5, in this example to the second
 * rule of the resolution of weak types.
 *
 * For handling surrogate pairs, where two UChar's form one "abstract" (or UTF-32)
 * character according to UTF-16, the second UChar gets the directional property of
 * the entire character assigned, while the first one gets a BN, a boundary
 * neutral, type, which is ignored by most of the algorithm according to
 * rule (X9) and the implementation suggestions of the Bidi algorithm.
 *
 * Later, AdjustWSLevels() will set the level for each BN to that of the
 * following character (UChar), which results in surrogate pairs getting the
 * same level on each of their surrogates.
 *
 * In a UTF-8 implementation, the same thing could be done: the last byte of
 * a multi-byte sequence would get the "real" property, while all previous
 * bytes of that sequence would get BN.
 *
 * It is not possible to assign all those parts of a character the same real
 * property because this would fail in the resolution of weak types with rules
 * that look at immediately surrounding types.
 *
 * As a related topic, this implementation does not remove Boundary Neutral
 * types from the input, but ignores them whenever this is relevant.
 * For example, the loop for the resolution of the weak types reads
 * types until it finds a non-BN.
 * Also, explicit embedding codes are neither changed into BN nor removed.
 * They are only treated the same way real BNs are.
 * As stated before, AdjustWSLevels() takes care of them at the end.
 * For the purpose of conformance, the levels of all these codes
 * do not matter.
 *
 * Note that this implementation never modifies the dirProps
 * after the initial setup, except for FSI which is changed to either
 * LRI or RLI in GetDirProps(), and paired brackets which may be changed
 * to L or R according to N0.
 *
 *
 * In this implementation, the resolution of weak types (Wn),
 * neutrals (Nn), and the assignment of the resolved level (In)
 * are all done in one single loop, in ResolveImplicitLevels().
 * Changes of dirProp values are done on the fly, without writing
 * them back to the dirProps array.
 *
 *
 * This implementation contains code that allows to bypass steps of the
 * algorithm that are not needed on the specific paragraph
 * in order to speed up the most common cases considerably,
 * like text that is entirely LTR, or RTL text without numbers.
 *
 * Most of this is done by setting a bit for each directional property
 * in a flags variable and later checking for whether there are
 * any LTR characters or any RTL characters, or both, whether
 * there are any explicit embedding codes, etc.
 *
 * If the (Xn) steps are performed, then the flags are re-evaluated,
 * because they will then not contain the embedding codes any more
 * and will be adjusted for override codes, so that subsequently
 * more bypassing may be possible than what the initial flags suggested.
 *
 * If the text is not mixed-directional, then the
 * algorithm steps for the weak type resolution are not performed,
 * and all levels are set to the paragraph level.
 *
 * If there are no explicit embedding codes, then the (Xn) steps
 * are not performed.
 *
 * If embedding levels are supplied as a parameter, then all
 * explicit embedding codes are ignored, and the (Xn) steps
 * are not performed.
 *
 * White Space types could get the level of the run they belong to,
 * and are checked with a test of (flags&MASK_EMBEDDING) to
 * consider if the paragraph direction should be considered in
 * the flags variable.
 *
 * If there are no White Space types in the paragraph, then
 * (L1) is not necessary in AdjustWSLevels().
 */
nsBidi::nsBidi()
{
  Init();

  mMayAllocateText=true;
  mMayAllocateRuns=true;
}

nsBidi::~nsBidi()
{
  Free();
}

void nsBidi::Init()
{
  /* reset the object, all pointers nullptr, all flags false, all sizes 0 */
  mLength = 0;
  mParaLevel = 0;
  mFlags = 0;
  mDirection = NSBIDI_LTR;
  mTrailingWSStart = 0;

  mDirPropsSize = 0;
  mLevelsSize = 0;
  mRunsSize = 0;
  mIsolatesSize = 0;

  mRunCount = -1;
  mIsolateCount = -1;

  mDirProps=nullptr;
  mLevels=nullptr;
  mRuns=nullptr;
  mIsolates=nullptr;

  mDirPropsMemory=nullptr;
  mLevelsMemory=nullptr;
  mRunsMemory=nullptr;
  mIsolatesMemory=nullptr;

  mMayAllocateText=false;
  mMayAllocateRuns=false;
}

/*
 * We are allowed to allocate memory if aMemory==nullptr or
 * aMayAllocate==true for each array that we need.
 * We also try to grow and shrink memory as needed if we
 * allocate it.
 *
 * Assume aSizeNeeded>0.
 * If *aMemory!=nullptr, then assume *aSize>0.
 *
 * ### this realloc() may unnecessarily copy the old data,
 * which we know we don't need any more;
 * is this the best way to do this??
 */
bool nsBidi::GetMemory(void **aMemory, size_t *aSize, bool aMayAllocate, size_t aSizeNeeded)
{
  /* check for existing memory */
  if(*aMemory==nullptr) {
    /* we need to allocate memory */
    if(!aMayAllocate) {
      return false;
    } else {
      *aMemory=malloc(aSizeNeeded);
      if (*aMemory!=nullptr) {
        *aSize=aSizeNeeded;
        return true;
      } else {
        *aSize=0;
        return false;
      }
    }
  } else {
    /* there is some memory, is it enough or too much? */
    if(aSizeNeeded>*aSize && !aMayAllocate) {
      /* not enough memory, and we must not allocate */
      return false;
    } else if(aSizeNeeded!=*aSize && aMayAllocate) {
      /* we may try to grow or shrink */
      void *memory=realloc(*aMemory, aSizeNeeded);

      if(memory!=nullptr) {
        *aMemory=memory;
        *aSize=aSizeNeeded;
        return true;
      } else {
        /* we failed to grow */
        return false;
      }
    } else {
      /* we have at least enough memory and must not allocate */
      return true;
    }
  }
}

void nsBidi::Free()
{
  free(mDirPropsMemory);
  mDirPropsMemory = nullptr;
  free(mLevelsMemory);
  mLevelsMemory = nullptr;
  free(mRunsMemory);
  mRunsMemory = nullptr;
  free(mIsolatesMemory);
  mIsolatesMemory = nullptr;
}

/* SetPara ------------------------------------------------------------ */

nsresult nsBidi::SetPara(const char16_t *aText, int32_t aLength,
                         nsBidiLevel aParaLevel, nsBidiLevel *aEmbeddingLevels)
{
  nsBidiDirection direction;

  /* check the argument values */
  if(aText==nullptr ||
     ((NSBIDI_MAX_EXPLICIT_LEVEL<aParaLevel) && !IS_DEFAULT_LEVEL(aParaLevel)) ||
     aLength<-1
    ) {
    return NS_ERROR_INVALID_ARG;
  }

  if(aLength==-1) {
    aLength = NS_strlen(aText);
  }

  /* initialize member data */
  mLength = aLength;
  mParaLevel=aParaLevel;
  mDirection=aParaLevel & 1 ? NSBIDI_RTL : NSBIDI_LTR;
  mTrailingWSStart=aLength;  /* the levels[] will reflect the WS run */

  mDirProps=nullptr;
  mLevels=nullptr;
  mRuns=nullptr;

  if(aLength==0) {
    /*
     * For an empty paragraph, create an nsBidi object with the aParaLevel and
     * the flags and the direction set but without allocating zero-length arrays.
     * There is nothing more to do.
     */
    if(IS_DEFAULT_LEVEL(aParaLevel)) {
      mParaLevel&=1;
    }
    mFlags=DIRPROP_FLAG_LR(aParaLevel);
    mRunCount=0;
    return NS_OK;
  }

  mRunCount=-1;

  /*
   * Get the directional properties,
   * the flags bit-set, and
   * determine the partagraph level if necessary.
   */
  if(GETDIRPROPSMEMORY(aLength)) {
    mDirProps=mDirPropsMemory;
    GetDirProps(aText);
  } else {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  /* are explicit levels specified? */
  if(aEmbeddingLevels==nullptr) {
    /* no: determine explicit levels according to the (Xn) rules */\
    if(GETLEVELSMEMORY(aLength)) {
      mLevels=mLevelsMemory;
      ResolveExplicitLevels(&direction);
    } else {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  } else {
    /* set BN for all explicit codes, check that all levels are aParaLevel..NSBIDI_MAX_EXPLICIT_LEVEL */
    mLevels=aEmbeddingLevels;
    nsresult rv = CheckExplicitLevels(&direction);
    if(NS_FAILED(rv)) {
      return rv;
    }
  }

  /* allocate isolate memory */
  if (mIsolateCount <= SIMPLE_ISOLATES_SIZE) {
    mIsolates = mSimpleIsolates;
  } else {
    if (mIsolateCount * sizeof(Isolate) <= mIsolatesSize) {
      mIsolates = mIsolatesMemory;
    } else {
      if (GETINITIALISOLATESMEMORY(mIsolateCount)) {
        mIsolates = mIsolatesMemory;
      } else {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }
  mIsolateCount = -1;  /* current isolates stack entry == none */

  /*
   * The steps after (X9) in the Bidi algorithm are performed only if
   * the paragraph text has mixed directionality!
   */
  mDirection = direction;
  switch(direction) {
    case NSBIDI_LTR:
      /* make sure paraLevel is even */
      mParaLevel=(mParaLevel+1)&~1;

      /* all levels are implicitly at paraLevel (important for GetLevels()) */
      mTrailingWSStart=0;
      break;
    case NSBIDI_RTL:
      /* make sure paraLevel is odd */
      mParaLevel|=1;

      /* all levels are implicitly at paraLevel (important for GetLevels()) */
      mTrailingWSStart=0;
      break;
    default:
      /*
       * If there are no external levels specified and there
       * are no significant explicit level codes in the text,
       * then we can treat the entire paragraph as one run.
       * Otherwise, we need to perform the following rules on runs of
       * the text with the same embedding levels. (X10)
       * "Significant" explicit level codes are ones that actually
       * affect non-BN characters.
       * Examples for "insignificant" ones are empty embeddings
       * LRE-PDF, LRE-RLE-PDF-PDF, etc.
       */
      if(aEmbeddingLevels==nullptr && !(mFlags&DIRPROP_FLAG_MULTI_RUNS)) {
        ResolveImplicitLevels(0, aLength,
                    GET_LR_FROM_LEVEL(mParaLevel),
                    GET_LR_FROM_LEVEL(mParaLevel));
      } else {
        /* sor, eor: start and end types of same-level-run */
        nsBidiLevel *levels=mLevels;
        int32_t start, limit=0;
        nsBidiLevel level, nextLevel;
        DirProp sor, eor;

        /* determine the first sor and set eor to it because of the loop body (sor=eor there) */
        level=mParaLevel;
        nextLevel=levels[0];
        if(level<nextLevel) {
          eor=GET_LR_FROM_LEVEL(nextLevel);
        } else {
          eor=GET_LR_FROM_LEVEL(level);
        }

        do {
          /* determine start and limit of the run (end points just behind the run) */

          /* the values for this run's start are the same as for the previous run's end */
          sor=eor;
          start=limit;
          level=nextLevel;

          /* search for the limit of this run */
          while(++limit<aLength && levels[limit]==level) {}

          /* get the correct level of the next run */
          if(limit<aLength) {
            nextLevel=levels[limit];
          } else {
            nextLevel=mParaLevel;
          }

          /* determine eor from max(level, nextLevel); sor is last run's eor */
          if((level&~NSBIDI_LEVEL_OVERRIDE)<(nextLevel&~NSBIDI_LEVEL_OVERRIDE)) {
            eor=GET_LR_FROM_LEVEL(nextLevel);
          } else {
            eor=GET_LR_FROM_LEVEL(level);
          }

          /* if the run consists of overridden directional types, then there
          are no implicit types to be resolved */
          if(!(level&NSBIDI_LEVEL_OVERRIDE)) {
            ResolveImplicitLevels(start, limit, sor, eor);
          } else {
            do {
              levels[start++] &= ~NSBIDI_LEVEL_OVERRIDE;
            } while (start < limit);
          }
        } while(limit<aLength);
      }

      /* reset the embedding levels for some non-graphic characters (L1), (X9) */
      AdjustWSLevels();
      break;
  }

  return NS_OK;
}

/* perform (P2)..(P3) ------------------------------------------------------- */

/*
 * Get the directional properties for the text,
 * calculate the flags bit-set, and
 * determine the partagraph level if necessary.
 */
void nsBidi::GetDirProps(const char16_t *aText)
{
  DirProp *dirProps=mDirPropsMemory;    /* mDirProps is const */

  int32_t i=0, length=mLength;
  Flags flags=0;      /* collect all directionalities in the text */
  char16_t uchar;
  DirProp dirProp;

  bool isDefaultLevel = IS_DEFAULT_LEVEL(mParaLevel);

  enum State {
    NOT_SEEKING_STRONG,       /* 0: not after FSI */
    SEEKING_STRONG_FOR_PARA,  /* 1: looking for first strong char in para */
    SEEKING_STRONG_FOR_FSI,   /* 2: looking for first strong after FSI */
    LOOKING_FOR_PDI           /* 3: found strong after FSI, looking for PDI */
  };
  State state;

  /* The following stacks are used to manage isolate sequences. Those
     sequences may be nested, but obviously never more deeply than the
     maximum explicit embedding level.
     lastStack is the index of the last used entry in the stack. A value of -1
     means that there is no open isolate sequence. */
  /* The following stack contains the position of the initiator of
     each open isolate sequence */
  int32_t isolateStartStack[NSBIDI_MAX_EXPLICIT_LEVEL + 1];
  /* The following stack contains the last known state before
     encountering the initiator of an isolate sequence */
  State previousStateStack[NSBIDI_MAX_EXPLICIT_LEVEL + 1];
  int32_t stackLast = -1;

  if(isDefaultLevel) {
    /*
     * see comment in nsBidi.h:
     * the DEFAULT_XXX values are designed so that
     * their bit 0 alone yields the intended default
     */
    mParaLevel &= 1;
    state = SEEKING_STRONG_FOR_PARA;
  } else {
    state = NOT_SEEKING_STRONG;
  }

  /* determine the paragraph level (P2..P3) */
  for(/* i = 0 above */; i < length;) {
    uchar=aText[i];
    if(!IS_FIRST_SURROGATE(uchar) || i+1==length || !IS_SECOND_SURROGATE(aText[i+1])) {
      /* not a surrogate pair */
      flags|=DIRPROP_FLAG(dirProps[i]=dirProp=GetBidiCat((uint32_t)uchar));
    } else {
      /* a surrogate pair */
      dirProps[i++]=BN;   /* first surrogate in the pair gets the BN type */
      flags|=DIRPROP_FLAG(dirProps[i]=dirProp=GetBidiCat(GET_UTF_32(uchar, aText[i])))|DIRPROP_FLAG(BN);
    }
    ++i;

    switch (dirProp) {
    case L:
      if (state == SEEKING_STRONG_FOR_PARA) {
        mParaLevel = 0;
        state = NOT_SEEKING_STRONG;
      } else if  (state == SEEKING_STRONG_FOR_FSI) {
        if (stackLast <= NSBIDI_MAX_EXPLICIT_LEVEL) {
          dirProps[isolateStartStack[stackLast]] = LRI;
          flags |= LRI;
        }
        state = LOOKING_FOR_PDI;
      }
      break;

    case R: case AL:
      if (state == SEEKING_STRONG_FOR_PARA) {
        mParaLevel = 1;
        state = NOT_SEEKING_STRONG;
      } else if  (state == SEEKING_STRONG_FOR_FSI) {
        if (stackLast <= NSBIDI_MAX_EXPLICIT_LEVEL) {
          dirProps[isolateStartStack[stackLast]] = RLI;
          flags |= RLI;
        }
        state = LOOKING_FOR_PDI;
      }
      break;

    case FSI: case LRI: case RLI:
      stackLast++;
      if (stackLast <= NSBIDI_MAX_EXPLICIT_LEVEL) {
        isolateStartStack[stackLast] = i - 1;
        previousStateStack[stackLast] = state;
      }
      if (dirProp == FSI) {
        state = SEEKING_STRONG_FOR_FSI;
      } else {
        state = LOOKING_FOR_PDI;
      }
      break;

    case PDI:
      if (state == SEEKING_STRONG_FOR_FSI) {
        if (stackLast <= NSBIDI_MAX_EXPLICIT_LEVEL) {
          dirProps[isolateStartStack[stackLast]] = LRI;
          flags |= DIRPROP_FLAG(LRI);
        }
      }
      if (stackLast >= 0) {
        if (stackLast <= NSBIDI_MAX_EXPLICIT_LEVEL) {
          state = previousStateStack[stackLast];
        }
        stackLast--;
      }
      break;

    case B:
      // This shouldn't happen, since we don't support multiple paragraphs.
      NS_NOTREACHED("Unexpected paragraph separator");
      break;

    default:
      break;
    }
  }

  /* Ignore still open isolate sequences with overflow */
  if (stackLast > NSBIDI_MAX_EXPLICIT_LEVEL) {
    stackLast = NSBIDI_MAX_EXPLICIT_LEVEL;
    if (dirProps[previousStateStack[NSBIDI_MAX_EXPLICIT_LEVEL]] != FSI) {
      state = LOOKING_FOR_PDI;
    }
  }

  /* Resolve direction of still unresolved open FSI sequences */
  while (stackLast >= 0) {
    if (state == SEEKING_STRONG_FOR_FSI) {
      dirProps[isolateStartStack[stackLast]] = LRI;
      flags |= DIRPROP_FLAG(LRI);
    }
    state = previousStateStack[stackLast];
    stackLast--;
  }

  flags|=DIRPROP_FLAG_LR(mParaLevel);

  mFlags = flags;
}

/* perform (X1)..(X9) ------------------------------------------------------- */

/*
 * Resolve the explicit levels as specified by explicit embedding codes.
 * Recalculate the flags to have them reflect the real properties
 * after taking the explicit embeddings into account.
 *
 * The Bidi algorithm is designed to result in the same behavior whether embedding
 * levels are externally specified (from "styled text", supposedly the preferred
 * method) or set by explicit embedding codes (LRx, RLx, PDF, FSI, PDI) in the plain text.
 * That is why (X9) instructs to remove all not-isolate explicit codes (and BN).
 * However, in a real implementation, this removal of these codes and their index
 * positions in the plain text is undesirable since it would result in
 * reallocated, reindexed text.
 * Instead, this implementation leaves the codes in there and just ignores them
 * in the subsequent processing.
 * In order to get the same reordering behavior, positions with a BN or a not-isolate
 * explicit embedding code just get the same level assigned as the last "real"
 * character.
 *
 * Some implementations, not this one, then overwrite some of these
 * directionality properties at "real" same-level-run boundaries by
 * L or R codes so that the resolution of weak types can be performed on the
 * entire paragraph at once instead of having to parse it once more and
 * perform that resolution on same-level-runs.
 * This limits the scope of the implicit rules in effectively
 * the same way as the run limits.
 *
 * Instead, this implementation does not modify these codes.
 * On one hand, the paragraph has to be scanned for same-level-runs, but
 * on the other hand, this saves another loop to reset these codes,
 * or saves making and modifying a copy of dirProps[].
 *
 *
 * Note that (Pn) and (Xn) changed significantly from version 4 of the Bidi algorithm.
 *
 *
 * Handling the stack of explicit levels (Xn):
 *
 * With the Bidi stack of explicit levels, as pushed with each
 * LRE, RLE, LRO, and RLO, LRI, RLI, and FSI and popped with each PDF and PDI,
 * the explicit level must never exceed NSBIDI_MAX_EXPLICIT_LEVEL.
 *
 * In order to have a correct push-pop semantics even in the case of overflows,
 * overflow counters and a valid isolate counter are used as described in UAX#9
 * section 3.3.2 "Explicit Levels and Direction".
 *
 * This implementation assumes that NSBIDI_MAX_EXPLICIT_LEVEL is odd.
 */

void nsBidi::ResolveExplicitLevels(nsBidiDirection *aDirection)
{
  DirProp *dirProps=mDirProps;
  nsBidiLevel *levels=mLevels;

  int32_t i=0, length=mLength;
  Flags flags=mFlags;       /* collect all directionalities in the text */
  DirProp dirProp;
  nsBidiLevel level=mParaLevel;
  nsBidiDirection direction;

  mIsolateCount = 0;

  /* determine if the text is mixed-directional or single-directional */
  direction=DirectionFromFlags(flags);

  /* we may not need to resolve any explicit levels */
  if(direction!=NSBIDI_MIXED) {
    /* not mixed directionality: levels don't matter - trailingWSStart will be 0 */
  } else if(!(flags&(MASK_EXPLICIT|MASK_ISO))) {
    /* no embeddings, set all levels to the paragraph level */
    for(i=0; i<length; ++i) {
      levels[i]=level;
    }
  } else {
    /* continue to perform (Xn) */

    /* (X1) level is set for all codes, embeddingLevel keeps track of the push/pop operations */
    /* both variables may carry the NSBIDI_LEVEL_OVERRIDE flag to indicate the override status */
    nsBidiLevel embeddingLevel = level, newLevel;
    nsBidiLevel previousLevel = level;     /* previous level for regular (not CC) characters */

    uint16_t stack[NSBIDI_MAX_EXPLICIT_LEVEL + 2];   /* we never push anything >=NSBIDI_MAX_EXPLICIT_LEVEL
                                                        but we need one more entry as base */
    int32_t stackLast = 0;
    int32_t overflowIsolateCount = 0;
    int32_t overflowEmbeddingCount = 0;
    int32_t validIsolateCount = 0;

    stack[0] = level;

    /* recalculate the flags */
    flags=0;

    /* since we assume that this is a single paragraph, we ignore (X8) */
    for(i=0; i<length; ++i) {
      dirProp=dirProps[i];
      switch(dirProp) {
        case LRE:
        case RLE:
        case LRO:
        case RLO:
          /* (X2, X3, X4, X5) */
          flags |= DIRPROP_FLAG(BN);
          if (dirProp == LRE || dirProp == LRO) {
            newLevel = (embeddingLevel + 2) & ~(NSBIDI_LEVEL_OVERRIDE | 1);    /* least greater even level */
          } else {
            newLevel = ((embeddingLevel & ~NSBIDI_LEVEL_OVERRIDE) + 1) | 1;    /* least greater odd level */
          }
          if(newLevel <= NSBIDI_MAX_EXPLICIT_LEVEL && overflowIsolateCount == 0 && overflowEmbeddingCount == 0) {
            embeddingLevel = newLevel;
            if (dirProp == LRO || dirProp == RLO) {
              embeddingLevel |= NSBIDI_LEVEL_OVERRIDE;
            }
            stackLast++;
            stack[stackLast] = embeddingLevel;
            /* we don't need to set UBIDI_LEVEL_OVERRIDE off for LRE and RLE
               since this has already been done for newLevel which is
               the source for embeddingLevel.
             */
          } else {
            dirProps[i] |= IGNORE_CC;
            if (overflowIsolateCount == 0) {
              overflowEmbeddingCount++;
            }
          }
          break;

        case PDF:
          /* (X7) */
          flags |= DIRPROP_FLAG(BN);
          /* handle all the overflow cases first */
          if (overflowIsolateCount) {
            dirProps[i] |= IGNORE_CC;
            break;
          }
          if (overflowEmbeddingCount) {
            dirProps[i] |= IGNORE_CC;
            overflowEmbeddingCount--;
            break;
          }
          if (stackLast > 0 && stack[stackLast] < ISOLATE) {   /* not an isolate entry */
            stackLast--;
            embeddingLevel = stack[stackLast];
          } else {
            dirProps[i] |= IGNORE_CC;
          }
          break;

        case LRI:
        case RLI:
          if (embeddingLevel != previousLevel) {
            previousLevel = embeddingLevel;
          }
          /* (X5a, X5b) */
          flags |= DIRPROP_FLAG(O_N) | DIRPROP_FLAG(BN) | DIRPROP_FLAG_LR(embeddingLevel);
          level = embeddingLevel;
          if (dirProp == LRI) {
            newLevel = (embeddingLevel + 2) & ~(NSBIDI_LEVEL_OVERRIDE | 1); /* least greater even level */
          } else {
            newLevel = ((embeddingLevel & ~NSBIDI_LEVEL_OVERRIDE) + 1) | 1;  /* least greater odd level */
          }
          if (newLevel <= NSBIDI_MAX_EXPLICIT_LEVEL && overflowIsolateCount == 0 && overflowEmbeddingCount == 0) {
            previousLevel = embeddingLevel;
            validIsolateCount++;
            if (validIsolateCount > mIsolateCount) {
              mIsolateCount = validIsolateCount;
            }
            embeddingLevel = newLevel;
            stackLast++;
            stack[stackLast] = embeddingLevel + ISOLATE;
          } else {
            dirProps[i] |= IGNORE_CC;
            overflowIsolateCount++;
          }
          break;

        case PDI:
          /* (X6a) */
          if (overflowIsolateCount) {
            dirProps[i] |= IGNORE_CC;
            overflowIsolateCount--;
          } else if (validIsolateCount) {
            overflowEmbeddingCount = 0;
            while (stack[stackLast] < ISOLATE) {
              /* pop embedding entries        */
              /* until the last isolate entry */
              stackLast--;

              // Since validIsolateCount is true, there must be an isolate entry
              // on the stack, so the stack is guaranteed to not be empty.
              // Still, to eliminate a warning from coverity, we use an assertion.
              MOZ_ASSERT(stackLast > 0);
            }
            stackLast--;  /* pop also the last isolate entry */
            MOZ_ASSERT(stackLast >= 0);  // For coverity
            validIsolateCount--;
          } else {
            dirProps[i] |= IGNORE_CC;
          }
          embeddingLevel = stack[stackLast] & ~ISOLATE;
          previousLevel = level = embeddingLevel;
          flags |= DIRPROP_FLAG(O_N) | DIRPROP_FLAG(BN) | DIRPROP_FLAG_LR(embeddingLevel);
          break;

        case B:
          /*
           * We do not expect to see a paragraph separator (B),
           */
          NS_NOTREACHED("Unexpected paragraph separator");
          break;

        case BN:
          /* BN, LRE, RLE, and PDF are supposed to be removed (X9) */
          /* they will get their levels set correctly in AdjustWSLevels() */
          flags|=DIRPROP_FLAG(BN);
          break;

        default:
          /* all other types get the "real" level */
          level = embeddingLevel;
          if(embeddingLevel != previousLevel) {
            previousLevel = embeddingLevel;
          }

          if (level & NSBIDI_LEVEL_OVERRIDE) {
            flags |= DIRPROP_FLAG_LR(level);
          } else {
            flags |= DIRPROP_FLAG(dirProp);
          }
          break;
      }

      /*
       * We need to set reasonable levels even on BN codes and
       * explicit codes because we will later look at same-level runs (X10).
       */
      levels[i]=level;
      if (i > 0 && levels[i - 1] != level) {
        flags |= DIRPROP_FLAG_MULTI_RUNS;
        if (level & NSBIDI_LEVEL_OVERRIDE) {
          flags |= DIRPROP_FLAG_O(level);
        } else {
          flags |= DIRPROP_FLAG_E(level);
        }
      }
      if (DIRPROP_FLAG(dirProp) & MASK_ISO) {
        level = embeddingLevel;
      }
    }

    if(flags&MASK_EMBEDDING) {
      flags|=DIRPROP_FLAG_LR(mParaLevel);
    }

    /* subsequently, ignore the explicit codes and BN (X9) */

    /* again, determine if the text is mixed-directional or single-directional */
    mFlags=flags;
    direction=DirectionFromFlags(flags);
  }

  *aDirection = direction;
}

/*
 * Use a pre-specified embedding levels array:
 *
 * Adjust the directional properties for overrides (->LEVEL_OVERRIDE),
 * ignore all explicit codes (X9),
 * and check all the preset levels.
 *
 * Recalculate the flags to have them reflect the real properties
 * after taking the explicit embeddings into account.
 */
nsresult nsBidi::CheckExplicitLevels(nsBidiDirection *aDirection)
{
  const DirProp *dirProps=mDirProps;
  DirProp dirProp;
  nsBidiLevel *levels=mLevels;
  int32_t isolateCount = 0;

  int32_t i, length=mLength;
  Flags flags=0;  /* collect all directionalities in the text */
  nsBidiLevel level, paraLevel=mParaLevel;
  mIsolateCount = 0;

  for(i=0; i<length; ++i) {
    level=levels[i];
    dirProp = dirProps[i];
    if (dirProp == LRI || dirProp == RLI) {
      isolateCount++;
      if (isolateCount > mIsolateCount) {
        mIsolateCount = isolateCount;
      }
    } else if (dirProp == PDI) {
      isolateCount--;
    }
    if(level&NSBIDI_LEVEL_OVERRIDE) {
      /* keep the override flag in levels[i] but adjust the flags */
      level&=~NSBIDI_LEVEL_OVERRIDE;     /* make the range check below simpler */
      flags|=DIRPROP_FLAG_O(level);
    } else {
      /* set the flags */
      flags|=DIRPROP_FLAG_E(level)|DIRPROP_FLAG(dirProp);
    }
    if(level<paraLevel || NSBIDI_MAX_EXPLICIT_LEVEL<level) {
      /* level out of bounds */
      *aDirection = NSBIDI_LTR;
      return NS_ERROR_INVALID_ARG;
    }
  }
  if(flags&MASK_EMBEDDING) {
    flags|=DIRPROP_FLAG_LR(mParaLevel);
  }

  /* determine if the text is mixed-directional or single-directional */
  mFlags=flags;
  *aDirection = DirectionFromFlags(flags);
  return NS_OK;
}

/* determine if the text is mixed-directional or single-directional */
nsBidiDirection nsBidi::DirectionFromFlags(Flags aFlags)
{
  /* if the text contains AN and neutrals, then some neutrals may become RTL */
  if(!(aFlags&MASK_RTL || (aFlags&DIRPROP_FLAG(AN) && aFlags&MASK_POSSIBLE_N))) {
    return NSBIDI_LTR;
  } else if(!(aFlags&MASK_LTR)) {
    return NSBIDI_RTL;
  } else {
    return NSBIDI_MIXED;
  }
}

/******************************************************************
 The Properties state machine table
*******************************************************************

 All table cells are 8 bits:
      bits 0..4:  next state
      bits 5..7:  action to perform (if > 0)

 Cells may be of format "n" where n represents the next state
 (except for the rightmost column).
 Cells may also be of format "s(x,y)" where x represents an action
 to perform and y represents the next state.

*******************************************************************
 Definitions and type for properties state table
*******************************************************************
*/
#define IMPTABPROPS_COLUMNS 16
#define IMPTABPROPS_RES (IMPTABPROPS_COLUMNS - 1)
#define GET_STATEPROPS(cell) ((cell)&0x1f)
#define GET_ACTIONPROPS(cell) ((cell)>>5)
#undef s
#define s(action, newState) ((uint8_t)(newState+(action<<5)))

static const uint8_t groupProp[] =          /* dirProp regrouped */
{
/*  L   R   EN  ES  ET  AN  CS  B   S   WS  ON  LRE LRO AL  RLE RLO PDF NSM BN  FSI LRI RLI PDI ENL ENR */
    0,  1,  2,  7,  8,  3,  9,  6,  5,  4,  4,  10, 10, 12, 10, 10, 10, 11, 10, 4,  4,  4,  4,  13, 14
};

/******************************************************************

      PROPERTIES  STATE  TABLE

 In table impTabProps,
      - the ON column regroups ON and WS, FSI, RLI, LRI and PDI
      - the BN column regroups BN, LRE, RLE, LRO, RLO, PDF
      - the Res column is the reduced property assigned to a run

 Action 1: process current run1, init new run1
        2: init new run2
        3: process run1, process run2, init new run1
        4: process run1, set run1=run2, init new run2

 Notes:
  1) This table is used in ResolveImplicitLevels().
  2) This table triggers actions when there is a change in the Bidi
     property of incoming characters (action 1).
  3) Most such property sequences are processed immediately (in
     fact, passed to ProcessPropertySeq().
  4) However, numbers are assembled as one sequence. This means
     that undefined situations (like CS following digits, until
     it is known if the next char will be a digit) are held until
     following chars define them.
     Example: digits followed by CS, then comes another CS or ON;
              the digits will be processed, then the CS assigned
              as the start of an ON sequence (action 3).
  5) There are cases where more than one sequence must be
     processed, for instance digits followed by CS followed by L:
     the digits must be processed as one sequence, and the CS
     must be processed as an ON sequence, all this before starting
     assembling chars for the opening L sequence.


*/
static const uint8_t impTabProps[][IMPTABPROPS_COLUMNS] =
{
/*                        L ,     R ,    EN ,    AN ,    ON ,     S ,     B ,    ES ,    ET ,    CS ,    BN ,   NSM ,    AL ,   ENL ,   ENR , Res */
/* 0 Init        */ {     1 ,     2 ,     4 ,     5 ,     7 ,    15 ,    17 ,     7 ,     9 ,     7 ,     0 ,     7 ,     3 ,    18 ,    21 , DirProp_ON },
/* 1 L           */ {     1 , s(1,2), s(1,4), s(1,5), s(1,7),s(1,15),s(1,17), s(1,7), s(1,9), s(1,7),     1 ,     1 , s(1,3),s(1,18),s(1,21),  DirProp_L },
/* 2 R           */ { s(1,1),     2 , s(1,4), s(1,5), s(1,7),s(1,15),s(1,17), s(1,7), s(1,9), s(1,7),     2 ,     2 , s(1,3),s(1,18),s(1,21),  DirProp_R },
/* 3 AL          */ { s(1,1), s(1,2), s(1,6), s(1,6), s(1,8),s(1,16),s(1,17), s(1,8), s(1,8), s(1,8),     3 ,     3 ,     3 ,s(1,18),s(1,21),  DirProp_R },
/* 4 EN          */ { s(1,1), s(1,2),     4 , s(1,5), s(1,7),s(1,15),s(1,17),s(2,10),    11 ,s(2,10),     4 ,     4 , s(1,3),    18 ,    21 , DirProp_EN },
/* 5 AN          */ { s(1,1), s(1,2), s(1,4),     5 , s(1,7),s(1,15),s(1,17), s(1,7), s(1,9),s(2,12),     5 ,     5 , s(1,3),s(1,18),s(1,21), DirProp_AN },
/* 6 AL:EN/AN    */ { s(1,1), s(1,2),     6 ,     6 , s(1,8),s(1,16),s(1,17), s(1,8), s(1,8),s(2,13),     6 ,     6 , s(1,3),    18 ,    21 , DirProp_AN },
/* 7 ON          */ { s(1,1), s(1,2), s(1,4), s(1,5),     7 ,s(1,15),s(1,17),     7 ,s(2,14),     7 ,     7 ,     7 , s(1,3),s(1,18),s(1,21), DirProp_ON },
/* 8 AL:ON       */ { s(1,1), s(1,2), s(1,6), s(1,6),     8 ,s(1,16),s(1,17),     8 ,     8 ,     8 ,     8 ,     8 , s(1,3),s(1,18),s(1,21), DirProp_ON },
/* 9 ET          */ { s(1,1), s(1,2),     4 , s(1,5),     7 ,s(1,15),s(1,17),     7 ,     9 ,     7 ,     9 ,     9 , s(1,3),    18 ,    21 , DirProp_ON },
/*10 EN+ES/CS    */ { s(3,1), s(3,2),     4 , s(3,5), s(4,7),s(3,15),s(3,17), s(4,7),s(4,14), s(4,7),    10 , s(4,7), s(3,3),    18 ,    21 , DirProp_EN },
/*11 EN+ET       */ { s(1,1), s(1,2),     4 , s(1,5), s(1,7),s(1,15),s(1,17), s(1,7),    11 , s(1,7),    11 ,    11 , s(1,3),    18 ,    21 , DirProp_EN },
/*12 AN+CS       */ { s(3,1), s(3,2), s(3,4),     5 , s(4,7),s(3,15),s(3,17), s(4,7),s(4,14), s(4,7),    12 , s(4,7), s(3,3),s(3,18),s(3,21), DirProp_AN },
/*13 AL:EN/AN+CS */ { s(3,1), s(3,2),     6 ,     6 , s(4,8),s(3,16),s(3,17), s(4,8), s(4,8), s(4,8),    13 , s(4,8), s(3,3),    18 ,    21 , DirProp_AN },
/*14 ON+ET       */ { s(1,1), s(1,2), s(4,4), s(1,5),     7 ,s(1,15),s(1,17),     7 ,    14 ,     7 ,    14 ,    14 , s(1,3),s(4,18),s(4,21), DirProp_ON },
/*15 S           */ { s(1,1), s(1,2), s(1,4), s(1,5), s(1,7),    15 ,s(1,17), s(1,7), s(1,9), s(1,7),    15 , s(1,7), s(1,3),s(1,18),s(1,21),  DirProp_S },
/*16 AL:S        */ { s(1,1), s(1,2), s(1,6), s(1,6), s(1,8),    16 ,s(1,17), s(1,8), s(1,8), s(1,8),    16 , s(1,8), s(1,3),s(1,18),s(1,21),  DirProp_S },
/*17 B           */ { s(1,1), s(1,2), s(1,4), s(1,5), s(1,7),s(1,15),    17 , s(1,7), s(1,9), s(1,7),    17 , s(1,7), s(1,3),s(1,18),s(1,21),  DirProp_B },
/*18 ENL         */ { s(1,1), s(1,2),    18 , s(1,5), s(1,7),s(1,15),s(1,17),s(2,19),    20 ,s(2,19),    18 ,    18 , s(1,3),    18 ,    21 ,  DirProp_L },
/*19 ENL+ES/CS   */ { s(3,1), s(3,2),    18 , s(3,5), s(4,7),s(3,15),s(3,17), s(4,7),s(4,14), s(4,7),    19 , s(4,7), s(3,3),    18 ,    21 ,  DirProp_L },
/*20 ENL+ET      */ { s(1,1), s(1,2),    18 , s(1,5), s(1,7),s(1,15),s(1,17), s(1,7),    20 , s(1,7),    20 ,    20 , s(1,3),    18 ,    21 ,  DirProp_L },
/*21 ENR         */ { s(1,1), s(1,2),    21 , s(1,5), s(1,7),s(1,15),s(1,17),s(2,22),    23 ,s(2,22),    21 ,    21 , s(1,3),    18 ,    21 , DirProp_AN },
/*22 ENR+ES/CS   */ { s(3,1), s(3,2),    21 , s(3,5), s(4,7),s(3,15),s(3,17), s(4,7),s(4,14), s(4,7),    22 , s(4,7), s(3,3),    18 ,    21 , DirProp_AN },
/*23 ENR+ET      */ { s(1,1), s(1,2),    21 , s(1,5), s(1,7),s(1,15),s(1,17), s(1,7),    23 , s(1,7),    23 ,    23 , s(1,3),    18 ,    21 , DirProp_AN }
};

/*  we must undef macro s because the levels table have a different
 *  structure (4 bits for action and 4 bits for next state.
 */
#undef s

/******************************************************************
 The levels state machine tables
*******************************************************************

 All table cells are 8 bits:
      bits 0..3:  next state
      bits 4..7:  action to perform (if > 0)

 Cells may be of format "n" where n represents the next state
 (except for the rightmost column).
 Cells may also be of format "s(x,y)" where x represents an action
 to perform and y represents the next state.

 This format limits each table to 16 states each and to 15 actions.

*******************************************************************
 Definitions and type for levels state tables
*******************************************************************
*/
#define IMPTABLEVELS_RES (IMPTABLEVELS_COLUMNS - 1)
#define GET_STATE(cell) ((cell)&0x0f)
#define GET_ACTION(cell) ((cell)>>4)
#define s(action, newState) ((uint8_t)(newState+(action<<4)))

/******************************************************************

      LEVELS  STATE  TABLES

 In all levels state tables,
      - state 0 is the initial state
      - the Res column is the increment to add to the text level
        for this property sequence.

 The impAct arrays for each table of a pair map the local action
 numbers of the table to the total list of actions. For instance,
 action 2 in a given table corresponds to the action number which
 appears in entry [2] of the impAct array for that table.
 The first entry of all impAct arrays must be 0.

 Action 1: init conditional sequence
        2: prepend conditional sequence to current sequence
        3: set ON sequence to new level - 1
        4: init EN/AN/ON sequence
        5: fix EN/AN/ON sequence followed by R
        6: set previous level sequence to level 2

 Notes:
  1) These tables are used in ProcessPropertySeq(). The input
     is property sequences as determined by ResolveImplicitLevels.
  2) Most such property sequences are processed immediately
     (levels are assigned).
  3) However, some sequences cannot be assigned a final level till
     one or more following sequences are received. For instance,
     ON following an R sequence within an even-level paragraph.
     If the following sequence is R, the ON sequence will be
     assigned basic run level+1, and so will the R sequence.
  4) S is generally handled like ON, since its level will be fixed
     to paragraph level in AdjustWSLevels().

*/

static const ImpTab impTabL =   /* Even paragraph level */
/*  In this table, conditional sequences receive the higher possible level
    until proven otherwise.
*/
{
/*                         L ,     R ,    EN ,    AN ,    ON ,     S ,     B , Res */
/* 0 : init       */ {     0 ,     1 ,     0 ,     2 ,     0 ,     0 ,     0 ,  0 },
/* 1 : R          */ {     0 ,     1 ,     3 ,     3 , s(1,4), s(1,4),     0 ,  1 },
/* 2 : AN         */ {     0 ,     1 ,     0 ,     2 , s(1,5), s(1,5),     0 ,  2 },
/* 3 : R+EN/AN    */ {     0 ,     1 ,     3 ,     3 , s(1,4), s(1,4),     0 ,  2 },
/* 4 : R+ON       */ { s(2,0),     1 ,     3 ,     3 ,     4 ,     4 , s(2,0),  1 },
/* 5 : AN+ON      */ { s(2,0),     1 , s(2,0),     2 ,     5 ,     5 , s(2,0),  1 }
};
static const ImpTab impTabR =   /* Odd  paragraph level */
/*  In this table, conditional sequences receive the lower possible level
    until proven otherwise.
*/
{
/*                         L ,     R ,    EN ,    AN ,    ON ,     S ,     B , Res */
/* 0 : init       */ {     1 ,     0 ,     2 ,     2 ,     0 ,     0 ,     0 ,  0 },
/* 1 : L          */ {     1 ,     0 ,     1 ,     3 , s(1,4), s(1,4),     0 ,  1 },
/* 2 : EN/AN      */ {     1 ,     0 ,     2 ,     2 ,     0 ,     0 ,     0 ,  1 },
/* 3 : L+AN       */ {     1 ,     0 ,     1 ,     3 ,     5 ,     5 ,     0 ,  1 },
/* 4 : L+ON       */ { s(2,1),     0 , s(2,1),     3 ,     4 ,     4 ,     0 ,  0 },
/* 5 : L+AN+ON    */ {     1 ,     0 ,     1 ,     3 ,     5 ,     5 ,     0 ,  0 }
};

#undef s

static ImpAct impAct0 = {0,1,2,3,4,5,6};
static PImpTab impTab[2] = {impTabL, impTabR};

/*------------------------------------------------------------------------*/

/* perform rules (Wn), (Nn), and (In) on a run of the text ------------------ */

/*
 * This implementation of the (Wn) rules applies all rules in one pass.
 * In order to do so, it needs a look-ahead of typically 1 character
 * (except for W5: sequences of ET) and keeps track of changes
 * in a rule Wp that affect a later Wq (p<q).
 *
 * The (Nn) and (In) rules are also performed in that same single loop,
 * but effectively one iteration behind for white space.
 *
 * Since all implicit rules are performed in one step, it is not necessary
 * to actually store the intermediate directional properties in dirProps[].
 */

void nsBidi::ProcessPropertySeq(LevState *pLevState, uint8_t _prop, int32_t start, int32_t limit)
{
  uint8_t cell, oldStateSeq, actionSeq;
  PImpTab pImpTab = pLevState->pImpTab;
  PImpAct pImpAct = pLevState->pImpAct;
  nsBidiLevel* levels = mLevels;
  nsBidiLevel level, addLevel;
  int32_t start0, k;

  start0 = start;                         /* save original start position */
  oldStateSeq = (uint8_t)pLevState->state;
  cell = pImpTab[oldStateSeq][_prop];
  pLevState->state = GET_STATE(cell);       /* isolate the new state */
  actionSeq = pImpAct[GET_ACTION(cell)]; /* isolate the action */
  addLevel = pImpTab[pLevState->state][IMPTABLEVELS_RES];

  if(actionSeq) {
    switch(actionSeq) {
    case 1:                         /* init ON seq */
      pLevState->startON = start0;
      break;

    case 2:                         /* prepend ON seq to current seq */
      start = pLevState->startON;
      break;

    default:                        /* we should never get here */
      MOZ_ASSERT(false);
      break;
    }
  }
  if(addLevel || (start < start0)) {
    level = pLevState->runLevel + addLevel;
    if (start >= pLevState->runStart) {
      for (k = start; k < limit; k++) {
        levels[k] = level;
      }
    } else {
      DirProp *dirProps = mDirProps, dirProp;
      int32_t isolateCount = 0;
      for (k = start; k < limit; k++) {
        dirProp = dirProps[k];
        if (dirProp == PDI) {
          isolateCount--;
        }
        if (isolateCount == 0) {
          levels[k]=level;
        }
        if (dirProp == LRI || dirProp == RLI) {
          isolateCount++;
        }
      }
    }
  }
}

void nsBidi::ResolveImplicitLevels(int32_t aStart, int32_t aLimit,
                   DirProp aSOR, DirProp aEOR)
{
  const DirProp *dirProps = mDirProps;
  DirProp dirProp;
  LevState levState;
  int32_t i, start1, start2;
  uint16_t oldStateImp, stateImp, actionImp;
  uint8_t gprop, resProp, cell;

  /* initialize for property and levels state tables */
  levState.startON = -1;
  levState.runStart = aStart;
  levState.runLevel = mLevels[aStart];
  levState.pImpTab = impTab[levState.runLevel & 1];
  levState.pImpAct = impAct0;

  /* The isolates[] entries contain enough information to
     resume the bidi algorithm in the same state as it was
     when it was interrupted by an isolate sequence. */
  if (dirProps[aStart] == PDI) {
    start1 = mIsolates[mIsolateCount].start1;
    stateImp = mIsolates[mIsolateCount].stateImp;
    levState.state = mIsolates[mIsolateCount].state;
    mIsolateCount--;
  } else {
    start1 = aStart;
    if (dirProps[aStart] == NSM) {
      stateImp = 1 + aSOR;
    } else {
      stateImp = 0;
    }
    levState.state = 0;
    ProcessPropertySeq(&levState, aSOR, aStart, aStart);
  }
  start2 = aStart;

  for (i = aStart; i <= aLimit; i++) {
    if (i >= aLimit) {
      if (aLimit > aStart) {
        dirProp = mDirProps[aLimit - 1];
        if (dirProp == LRI || dirProp == RLI) {
          break;  /* no forced closing for sequence ending with LRI/RLI */
        }
      }
      gprop = aEOR;
    } else {
      DirProp prop;
      prop = PURE_DIRPROP(dirProps[i]);
      gprop = groupProp[prop];
    }
    oldStateImp = stateImp;
    cell = impTabProps[oldStateImp][gprop];
    stateImp = GET_STATEPROPS(cell);      /* isolate the new state */
    actionImp = GET_ACTIONPROPS(cell);    /* isolate the action */
    if ((i == aLimit) && (actionImp == 0)) {
      /* there is an unprocessed sequence if its property == eor   */
      actionImp = 1;                      /* process the last sequence */
    }
    if (actionImp) {
      resProp = impTabProps[oldStateImp][IMPTABPROPS_RES];
      switch (actionImp) {
      case 1:             /* process current seq1, init new seq1 */
        ProcessPropertySeq(&levState, resProp, start1, i);
        start1 = i;
        break;
      case 2:             /* init new seq2 */
        start2 = i;
        break;
      case 3:             /* process seq1, process seq2, init new seq1 */
        ProcessPropertySeq(&levState, resProp, start1, start2);
        ProcessPropertySeq(&levState, DirProp_ON, start2, i);
        start1 = i;
        break;
      case 4:             /* process seq1, set seq1=seq2, init new seq2 */
        ProcessPropertySeq(&levState, resProp, start1, start2);
        start1 = start2;
        start2 = i;
        break;
      default:            /* we should never get here */
        MOZ_ASSERT(false);
        break;
      }
    }
  }

  dirProp = dirProps[aLimit - 1];
  if ((dirProp == LRI || dirProp == RLI) && aLimit < mLength) {
    mIsolateCount++;
    mIsolates[mIsolateCount].stateImp = stateImp;
    mIsolates[mIsolateCount].state = levState.state;
    mIsolates[mIsolateCount].start1 = start1;
  } else {
    ProcessPropertySeq(&levState, aEOR, aLimit, aLimit);
  }
}


/* perform (L1) and (X9) ---------------------------------------------------- */

/*
 * Reset the embedding levels for some non-graphic characters (L1).
 * This function also sets appropriate levels for BN, and
 * explicit embedding types that are supposed to have been removed
 * from the paragraph in (X9).
 */
void nsBidi::AdjustWSLevels()
{
  const DirProp *dirProps=mDirProps;
  nsBidiLevel *levels=mLevels;
  int32_t i;

  if(mFlags&MASK_WS) {
    nsBidiLevel paraLevel=mParaLevel;
    Flags flag;

    i=mTrailingWSStart;
    while(i>0) {
      /* reset a sequence of WS/BN before eop and B/S to the paragraph paraLevel */
      while (i > 0 && DIRPROP_FLAG(PURE_DIRPROP(dirProps[--i])) & MASK_WS) {
        levels[i]=paraLevel;
      }

      /* reset BN to the next character's paraLevel until B/S, which restarts above loop */
      /* here, i+1 is guaranteed to be <length */
      while(i>0) {
        flag = DIRPROP_FLAG(PURE_DIRPROP(dirProps[--i]));
        if(flag&MASK_BN_EXPLICIT) {
          levels[i]=levels[i+1];
        } else if(flag&MASK_B_S) {
          levels[i]=paraLevel;
          break;
        }
      }
    }
  }
}

nsresult nsBidi::GetDirection(nsBidiDirection* aDirection)
{
  *aDirection = mDirection;
  return NS_OK;
}

nsresult nsBidi::GetParaLevel(nsBidiLevel* aParaLevel)
{
  *aParaLevel = mParaLevel;
  return NS_OK;
}
#ifdef FULL_BIDI_ENGINE

/* -------------------------------------------------------------------------- */

nsresult nsBidi::GetLength(int32_t* aLength)
{
  *aLength = mLength;
  return NS_OK;
}

/*
 * General remarks about the functions in this section:
 *
 * These functions deal with the aspects of potentially mixed-directional
 * text in a single paragraph or in a line of a single paragraph
 * which has already been processed according to
 * the Unicode 6.3 Bidi algorithm as defined in
 * http://www.unicode.org/unicode/reports/tr9/ , version 28,
 * also described in The Unicode Standard, Version 6.3.0 .
 *
 * This means that there is a nsBidi object with a levels
 * and a dirProps array.
 * paraLevel and direction are also set.
 * Only if the length of the text is zero, then levels==dirProps==nullptr.
 *
 * The overall directionality of the paragraph
 * or line is used to bypass the reordering steps if possible.
 * Even purely RTL text does not need reordering there because
 * the getLogical/VisualIndex() functions can compute the
 * index on the fly in such a case.
 *
 * The implementation of the access to same-level-runs and of the reordering
 * do attempt to provide better performance and less memory usage compared to
 * a direct implementation of especially rule (L2) with an array of
 * one (32-bit) integer per text character.
 *
 * Here, the levels array is scanned as soon as necessary, and a vector of
 * same-level-runs is created. Reordering then is done on this vector.
 * For each run of text positions that were resolved to the same level,
 * only 8 bytes are stored: the first text position of the run and the visual
 * position behind the run after reordering.
 * One sign bit is used to hold the directionality of the run.
 * This is inefficient if there are many very short runs. If the average run
 * length is <2, then this uses more memory.
 *
 * In a further attempt to save memory, the levels array is never changed
 * after all the resolution rules (Xn, Wn, Nn, In).
 * Many functions have to consider the field trailingWSStart:
 * if it is less than length, then there is an implicit trailing run
 * at the paraLevel,
 * which is not reflected in the levels array.
 * This allows a line nsBidi object to use the same levels array as
 * its paragraph parent object.
 *
 * When a nsBidi object is created for a line of a paragraph, then the
 * paragraph's levels and dirProps arrays are reused by way of setting
 * a pointer into them, not by copying. This again saves memory and forbids to
 * change the now shared levels for (L1).
 */
nsresult nsBidi::SetLine(const nsBidi* aParaBidi, int32_t aStart, int32_t aLimit)
{
  nsBidi* pParent = (nsBidi*)aParaBidi;
  int32_t length;

  /* check the argument values */
  if(pParent==nullptr) {
    return NS_ERROR_INVALID_POINTER;
  } else if(aStart < 0 || aStart >= aLimit || aLimit > pParent->mLength) {
    return NS_ERROR_INVALID_ARG;
  }

  /* set members from our aParaBidi parent */
  length = mLength = aLimit - aStart;
  mParaLevel=pParent->mParaLevel;

  mRuns=nullptr;
  mFlags=0;

  mDirProps=pParent->mDirProps+aStart;
  mLevels=pParent->mLevels+aStart;
  mRunCount=-1;

  if(pParent->mDirection!=NSBIDI_MIXED) {
    /* the parent is already trivial */
    mDirection=pParent->mDirection;

    /*
     * The parent's levels are all either
     * implicitly or explicitly ==paraLevel;
     * do the same here.
     */
    if(pParent->mTrailingWSStart<=aStart) {
      mTrailingWSStart=0;
    } else if(pParent->mTrailingWSStart<aLimit) {
      mTrailingWSStart=pParent->mTrailingWSStart-aStart;
    } else {
      mTrailingWSStart=length;
    }
  } else {
    const nsBidiLevel *levels=mLevels;
    int32_t i, trailingWSStart;
    nsBidiLevel level;

    SetTrailingWSStart();
    trailingWSStart=mTrailingWSStart;

    /* recalculate pLineBidi->direction */
    if(trailingWSStart==0) {
      /* all levels are at paraLevel */
      mDirection=(nsBidiDirection)(mParaLevel&1);
   } else {
      /* get the level of the first character */
      level=levels[0]&1;

      /* if there is anything of a different level, then the line is mixed */
      if(trailingWSStart<length && (mParaLevel&1)!=level) {
        /* the trailing WS is at paraLevel, which differs from levels[0] */
        mDirection=NSBIDI_MIXED;
      } else {
        /* see if levels[1..trailingWSStart-1] have the same direction as levels[0] and paraLevel */
        i=1;
        for(;;) {
          if(i==trailingWSStart) {
            /* the direction values match those in level */
            mDirection=(nsBidiDirection)level;
            break;
          } else if((levels[i]&1)!=level) {
            mDirection=NSBIDI_MIXED;
            break;
          }
          ++i;
        }
      }
    }

    switch(mDirection) {
      case NSBIDI_LTR:
        /* make sure paraLevel is even */
        mParaLevel=(mParaLevel+1)&~1;

        /* all levels are implicitly at paraLevel (important for GetLevels()) */
        mTrailingWSStart=0;
      break;
      case NSBIDI_RTL:
        /* make sure paraLevel is odd */
        mParaLevel|=1;

        /* all levels are implicitly at paraLevel (important for GetLevels()) */
        mTrailingWSStart=0;
        break;
      default:
        break;
    }
  }
  return NS_OK;
}

/* handle trailing WS (L1) -------------------------------------------------- */

/*
 * SetTrailingWSStart() sets the start index for a trailing
 * run of WS in the line. This is necessary because we do not modify
 * the paragraph's levels array that we just point into.
 * Using trailingWSStart is another form of performing (L1).
 *
 * To make subsequent operations easier, we also include the run
 * before the WS if it is at the paraLevel - we merge the two here.
 */
void nsBidi::SetTrailingWSStart() {
  /* mDirection!=NSBIDI_MIXED */

  const DirProp *dirProps=mDirProps;
  nsBidiLevel *levels=mLevels;
  int32_t start=mLength;
  nsBidiLevel paraLevel=mParaLevel;

  /* go backwards across all WS, BN, explicit codes */
  while(start>0 && DIRPROP_FLAG(dirProps[start-1])&MASK_WS) {
    --start;
  }

  /* if the WS run can be merged with the previous run then do so here */
  while(start>0 && levels[start-1]==paraLevel) {
    --start;
  }

  mTrailingWSStart=start;
}

nsresult nsBidi::GetLevelAt(int32_t aCharIndex, nsBidiLevel* aLevel)
{
  /* return paraLevel if in the trailing WS run, otherwise the real level */
  if(aCharIndex<0 || mLength<=aCharIndex) {
    *aLevel = 0;
  } else if(mDirection!=NSBIDI_MIXED || aCharIndex>=mTrailingWSStart) {
    *aLevel = mParaLevel;
  } else {
    *aLevel = mLevels[aCharIndex];
  }
  return NS_OK;
}

nsresult nsBidi::GetLevels(nsBidiLevel** aLevels)
{
  int32_t start, length;

  length = mLength;
  if(length<=0) {
    *aLevels = nullptr;
    return NS_ERROR_INVALID_ARG;
  }

  start = mTrailingWSStart;
  if(start==length) {
    /* the current levels array reflects the WS run */
    *aLevels = mLevels;
    return NS_OK;
  }

  /*
   * After the previous if(), we know that the levels array
   * has an implicit trailing WS run and therefore does not fully
   * reflect itself all the levels.
   * This must be a nsBidi object for a line, and
   * we need to create a new levels array.
   */

  if(GETLEVELSMEMORY(length)) {
    nsBidiLevel *levels=mLevelsMemory;

    if(start>0 && levels!=mLevels) {
      memcpy(levels, mLevels, start);
    }
    memset(levels+start, mParaLevel, length-start);

    /* this new levels array is set for the line and reflects the WS run */
    mTrailingWSStart=length;
    *aLevels=mLevels=levels;
    return NS_OK;
  } else {
    /* out of memory */
    *aLevels = nullptr;
    return NS_ERROR_OUT_OF_MEMORY;
  }
}
#endif // FULL_BIDI_ENGINE

nsresult nsBidi::GetCharTypeAt(int32_t aCharIndex, nsCharType* pType)
{
  if(aCharIndex<0 || mLength<=aCharIndex) {
    return NS_ERROR_INVALID_ARG;
  }
  *pType = (nsCharType)mDirProps[aCharIndex];
  return NS_OK;
}

nsresult nsBidi::GetLogicalRun(int32_t aLogicalStart, int32_t *aLogicalLimit, nsBidiLevel *aLevel)
{
  int32_t length = mLength;

  if(aLogicalStart<0 || length<=aLogicalStart) {
    return NS_ERROR_INVALID_ARG;
  }

  int32_t runCount, visualStart, logicalLimit, logicalFirst, i;
  Run iRun;

  /* CountRuns will check VALID_PARA_OR_LINE */
  nsresult rv = CountRuns(&runCount);
  if (NS_FAILED(rv)) {
    return rv;
  }

  visualStart = logicalLimit = 0;
  iRun = mRuns[0];

  for (i = 0; i < runCount; i++) {
    iRun = mRuns[i];
    logicalFirst = GET_INDEX(iRun.logicalStart);
    logicalLimit = logicalFirst + iRun.visualLimit - visualStart;
    if ((aLogicalStart >= logicalFirst) && (aLogicalStart < logicalLimit)) {
       break;
    }
    visualStart = iRun.visualLimit;
  }
  if (aLogicalLimit) {
    *aLogicalLimit = logicalLimit;
  }
  if (aLevel) {
    if (mDirection != NSBIDI_MIXED || aLogicalStart >= mTrailingWSStart) {
      *aLevel = mParaLevel;
    } else {
      *aLevel = mLevels[aLogicalStart];
    }
  }
  return NS_OK;
}

/* runs API functions ------------------------------------------------------- */

nsresult nsBidi::CountRuns(int32_t* aRunCount)
{
  if(mRunCount<0 && !GetRuns()) {
    return NS_ERROR_OUT_OF_MEMORY;
  } else {
    if (aRunCount)
      *aRunCount = mRunCount;
    return NS_OK;
  }
}

nsresult nsBidi::GetVisualRun(int32_t aRunIndex, int32_t *aLogicalStart, int32_t *aLength, nsBidiDirection *aDirection)
{
  if( aRunIndex<0 ||
      (mRunCount==-1 && !GetRuns()) ||
      aRunIndex>=mRunCount
    ) {
    *aDirection = NSBIDI_LTR;
    return NS_OK;
  } else {
    int32_t start=mRuns[aRunIndex].logicalStart;
    if(aLogicalStart!=nullptr) {
      *aLogicalStart=GET_INDEX(start);
    }
    if(aLength!=nullptr) {
      if(aRunIndex>0) {
        *aLength=mRuns[aRunIndex].visualLimit-
             mRuns[aRunIndex-1].visualLimit;
      } else {
        *aLength=mRuns[0].visualLimit;
      }
    }
    *aDirection = (nsBidiDirection)GET_ODD_BIT(start);
    return NS_OK;
  }
}

/* compute the runs array --------------------------------------------------- */

/*
 * Compute the runs array from the levels array.
 * After GetRuns() returns true, runCount is guaranteed to be >0
 * and the runs are reordered.
 * Odd-level runs have visualStart on their visual right edge and
 * they progress visually to the left.
 */
bool nsBidi::GetRuns()
{
  /*
   * This method returns immediately if the runs are already set. This
   * includes the case of length==0 (handled in setPara)..
   */
  if (mRunCount >= 0) {
    return true;
  }

  if(mDirection!=NSBIDI_MIXED) {
    /* simple, single-run case - this covers length==0 */
    GetSingleRun(mParaLevel);
  } else /* NSBIDI_MIXED, length>0 */ {
    /* mixed directionality */
    int32_t length=mLength, limit=mTrailingWSStart;

    /*
     * If there are WS characters at the end of the line
     * and the run preceding them has a level different from
     * paraLevel, then they will form their own run at paraLevel (L1).
     * Count them separately.
     * We need some special treatment for this in order to not
     * modify the levels array which a line nsBidi object shares
     * with its paragraph parent and its other line siblings.
     * In other words, for the trailing WS, it may be
     * levels[]!=paraLevel but we have to treat it like it were so.
     */
    nsBidiLevel *levels=mLevels;
    int32_t i, runCount;
    nsBidiLevel level=NSBIDI_DEFAULT_LTR;   /* initialize with no valid level */

    /* count the runs, there is at least one non-WS run, and limit>0 */
    runCount=0;
    for(i=0; i<limit; ++i) {
      /* increment runCount at the start of each run */
      if(levels[i]!=level) {
        ++runCount;
        level=levels[i];
      }
    }

    /*
     * We don't need to see if the last run can be merged with a trailing
     * WS run because SetTrailingWSStart() would have done that.
     */
    if(runCount==1 && limit==length) {
      /* There is only one non-WS run and no trailing WS-run. */
      GetSingleRun(levels[0]);
    } else /* runCount>1 || limit<length */ {
      /* allocate and set the runs */
      Run *runs;
      int32_t runIndex, start;
      nsBidiLevel minLevel=NSBIDI_MAX_EXPLICIT_LEVEL+1, maxLevel=0;

      /* now, count a (non-mergable) WS run */
      if(limit<length) {
        ++runCount;
      }

      /* runCount>1 */
      if(GETRUNSMEMORY(runCount)) {
        runs=mRunsMemory;
      } else {
        return false;
      }

      /* set the runs */
      /* this could be optimized, e.g.: 464->444, 484->444, 575->555, 595->555 */
      /* however, that would take longer and make other functions more complicated */
      runIndex=0;

      /* search for the run ends */
      i = 0;
      do {
        /* prepare this run */
        start = i;
        level = levels[i];
        if(level<minLevel) {
          minLevel=level;
        }
        if(level>maxLevel) {
          maxLevel=level;
        }

        /* look for the run limit */
        while (++i < limit && levels[i] == level) {
        }

        /* i is another run limit */
        runs[runIndex].logicalStart = start;
        runs[runIndex].visualLimit = i - start;
        ++runIndex;
      } while (i < limit);

      if(limit<length) {
        /* there is a separate WS run */
        runs[runIndex].logicalStart=limit;
        runs[runIndex].visualLimit=length-limit;
        if(mParaLevel<minLevel) {
          minLevel=mParaLevel;
        }
      }

      /* set the object fields */
      mRuns=runs;
      mRunCount=runCount;

      ReorderLine(minLevel, maxLevel);

      /* now add the direction flags and adjust the visualLimit's to be just that */
      /* this loop will also handling the trailing WS run */
      limit = 0;
      for (i = 0; i < runCount; ++i) {
        ADD_ODD_BIT_FROM_LEVEL(runs[i].logicalStart, levels[runs[i].logicalStart]);
        limit += runs[i].visualLimit;
        runs[i].visualLimit = limit;
      }

      /* Set the "odd" bit for the trailing WS run. */
      /* For a RTL paragraph, it will be the *first* run in visual order. */
      if (runIndex < runCount) {
        int32_t trailingRun = (mParaLevel & 1) ? 0 : runIndex;
        ADD_ODD_BIT_FROM_LEVEL(runs[trailingRun].logicalStart, mParaLevel);
      }
    }
  }

  return true;
}

/* in trivial cases there is only one trivial run; called by GetRuns() */
void nsBidi::GetSingleRun(nsBidiLevel aLevel)
{
  /* simple, single-run case */
  mRuns=mSimpleRuns;
  mRunCount=1;

  /* fill and reorder the single run */
  mRuns[0].logicalStart=MAKE_INDEX_ODD_PAIR(0, aLevel);
  mRuns[0].visualLimit=mLength;
}

/* reorder the runs array (L2) ---------------------------------------------- */

/*
 * Reorder the same-level runs in the runs array.
 * Here, runCount>1 and maxLevel>=minLevel>=paraLevel.
 * All the visualStart fields=logical start before reordering.
 * The "odd" bits are not set yet.
 *
 * Reordering with this data structure lends itself to some handy shortcuts:
 *
 * Since each run is moved but not modified, and since at the initial maxLevel
 * each sequence of same-level runs consists of only one run each, we
 * don't need to do anything there and can predecrement maxLevel.
 * In many simple cases, the reordering is thus done entirely in the
 * index mapping.
 * Also, reordering occurs only down to the lowest odd level that occurs,
 * which is minLevel|1. However, if the lowest level itself is odd, then
 * in the last reordering the sequence of the runs at this level or higher
 * will be all runs, and we don't need the elaborate loop to search for them.
 * This is covered by ++minLevel instead of minLevel|=1 followed
 * by an extra reorder-all after the reorder-some loop.
 * About a trailing WS run:
 * Such a run would need special treatment because its level is not
 * reflected in levels[] if this is not a paragraph object.
 * Instead, all characters from trailingWSStart on are implicitly at
 * paraLevel.
 * However, for all maxLevel>paraLevel, this run will never be reordered
 * and does not need to be taken into account. maxLevel==paraLevel is only reordered
 * if minLevel==paraLevel is odd, which is done in the extra segment.
 * This means that for the main reordering loop we don't need to consider
 * this run and can --runCount. If it is later part of the all-runs
 * reordering, then runCount is adjusted accordingly.
 */
void nsBidi::ReorderLine(nsBidiLevel aMinLevel, nsBidiLevel aMaxLevel)
{
  Run *runs, tempRun;
  nsBidiLevel *levels;
  int32_t firstRun, endRun, limitRun, runCount;

  /* nothing to do? */
  if(aMaxLevel<=(aMinLevel|1)) {
    return;
  }

  /*
   * Reorder only down to the lowest odd level
   * and reorder at an odd aMinLevel in a separate, simpler loop.
   * See comments above for why aMinLevel is always incremented.
   */
  ++aMinLevel;

  runs=mRuns;
  levels=mLevels;
  runCount=mRunCount;

  /* do not include the WS run at paraLevel<=old aMinLevel except in the simple loop */
  if(mTrailingWSStart<mLength) {
    --runCount;
  }

  while(--aMaxLevel>=aMinLevel) {
    firstRun=0;

    /* loop for all sequences of runs */
    for(;;) {
      /* look for a sequence of runs that are all at >=aMaxLevel */
      /* look for the first run of such a sequence */
      while(firstRun<runCount && levels[runs[firstRun].logicalStart]<aMaxLevel) {
        ++firstRun;
      }
      if(firstRun>=runCount) {
        break;  /* no more such runs */
      }

      /* look for the limit run of such a sequence (the run behind it) */
      for(limitRun=firstRun; ++limitRun<runCount && levels[runs[limitRun].logicalStart]>=aMaxLevel;) {}

      /* Swap the entire sequence of runs from firstRun to limitRun-1. */
      endRun=limitRun-1;
      while(firstRun<endRun) {
        tempRun = runs[firstRun];
        runs[firstRun] = runs[endRun];
        runs[endRun] = tempRun;
        ++firstRun;
        --endRun;
      }

      if(limitRun==runCount) {
        break;  /* no more such runs */
      } else {
        firstRun=limitRun+1;
      }
    }
  }

  /* now do aMaxLevel==old aMinLevel (==odd!), see above */
  if(!(aMinLevel&1)) {
    firstRun=0;

    /* include the trailing WS run in this complete reordering */
    if(mTrailingWSStart==mLength) {
      --runCount;
    }

    /* Swap the entire sequence of all runs. (endRun==runCount) */
    while(firstRun<runCount) {
      tempRun = runs[firstRun];
      runs[firstRun] = runs[runCount];
      runs[runCount] = tempRun;
      ++firstRun;
      --runCount;
    }
  }
}

nsresult nsBidi::ReorderVisual(const nsBidiLevel *aLevels, int32_t aLength, int32_t *aIndexMap)
{
  int32_t start, end, limit, temp;
  nsBidiLevel minLevel, maxLevel;

  if(aIndexMap==nullptr ||
     !PrepareReorder(aLevels, aLength, aIndexMap, &minLevel, &maxLevel)) {
    return NS_OK;
  }

  /* nothing to do? */
  if(minLevel==maxLevel && (minLevel&1)==0) {
    return NS_OK;
  }

  /* reorder only down to the lowest odd level */
  minLevel|=1;

  /* loop maxLevel..minLevel */
  do {
    start=0;

    /* loop for all sequences of levels to reorder at the current maxLevel */
    for(;;) {
      /* look for a sequence of levels that are all at >=maxLevel */
      /* look for the first index of such a sequence */
      while(start<aLength && aLevels[start]<maxLevel) {
        ++start;
      }
      if(start>=aLength) {
        break;  /* no more such runs */
      }

      /* look for the limit of such a sequence (the index behind it) */
      for(limit=start; ++limit<aLength && aLevels[limit]>=maxLevel;) {}

      /*
       * Swap the entire interval of indexes from start to limit-1.
       * We don't need to swap the levels for the purpose of this
       * algorithm: the sequence of levels that we look at does not
       * move anyway.
       */
      end=limit-1;
      while(start<end) {
        temp=aIndexMap[start];
        aIndexMap[start]=aIndexMap[end];
        aIndexMap[end]=temp;

        ++start;
        --end;
      }

      if(limit==aLength) {
        break;  /* no more such sequences */
      } else {
        start=limit+1;
      }
    }
  } while(--maxLevel>=minLevel);

  return NS_OK;
}

bool nsBidi::PrepareReorder(const nsBidiLevel *aLevels, int32_t aLength,
                int32_t *aIndexMap,
                nsBidiLevel *aMinLevel, nsBidiLevel *aMaxLevel)
{
  int32_t start;
  nsBidiLevel level, minLevel, maxLevel;

  if(aLevels==nullptr || aLength<=0) {
    return false;
  }

  /* determine minLevel and maxLevel */
  minLevel=NSBIDI_MAX_EXPLICIT_LEVEL+1;
  maxLevel=0;
  for(start=aLength; start>0;) {
    level=aLevels[--start];
    if(level>NSBIDI_MAX_EXPLICIT_LEVEL+1) {
      return false;
    }
    if(level<minLevel) {
      minLevel=level;
    }
    if(level>maxLevel) {
      maxLevel=level;
    }
  }
  *aMinLevel=minLevel;
  *aMaxLevel=maxLevel;

  /* initialize the index map */
  for(start=aLength; start>0;) {
    --start;
    aIndexMap[start]=start;
  }

  return true;
}

#ifdef FULL_BIDI_ENGINE
/* API functions for logical<->visual mapping ------------------------------- */

nsresult nsBidi::GetVisualIndex(int32_t aLogicalIndex, int32_t* aVisualIndex) {
  int32_t visualIndex = NSBIDI_MAP_NOWHERE;

  if(aLogicalIndex<0 || mLength<=aLogicalIndex) {
    return NS_ERROR_INVALID_ARG;
  } else {
    /* we can do the trivial cases without the runs array */
    switch(mDirection) {
    case NSBIDI_LTR:
      *aVisualIndex = aLogicalIndex;
      return NS_OK;
    case NSBIDI_RTL:
      *aVisualIndex = mLength-aLogicalIndex-1;
      return NS_OK;
    default:
      if(mRunCount<0 && !GetRuns()) {
        return NS_ERROR_OUT_OF_MEMORY;
      } else {
        Run *runs=mRuns;
        int32_t i, visualStart=0, offset, length;

        /* linear search for the run, search on the visual runs */
        for (i = 0; i < mRunCount; ++i) {
          length=runs[i].visualLimit-visualStart;
          offset=aLogicalIndex-GET_INDEX(runs[i].logicalStart);
          if(offset>=0 && offset<length) {
            if(IS_EVEN_RUN(runs[i].logicalStart)) {
              /* LTR */
              visualIndex = visualStart + offset;
            } else {
              /* RTL */
              visualIndex = visualStart + length - offset - 1;
            }
            break;
          }
          visualStart+=length;
        }
        if (i >= mRunCount) {
          *aVisualIndex = NSBIDI_MAP_NOWHERE;
          return NS_OK;
        }
      }
    }
  }

  *aVisualIndex = visualIndex;
  return NS_OK;
}

nsresult nsBidi::GetLogicalIndex(int32_t aVisualIndex, int32_t *aLogicalIndex)
{
  if(aVisualIndex<0 || mLength<=aVisualIndex) {
    return NS_ERROR_INVALID_ARG;
  }

  /* we can do the trivial cases without the runs array */
  if (mDirection == NSBIDI_LTR) {
    *aLogicalIndex = aVisualIndex;
    return NS_OK;
  } else if (mDirection == NSBIDI_RTL) {
    *aLogicalIndex = mLength - aVisualIndex - 1;
    return NS_OK;
  }

  if(mRunCount<0 && !GetRuns()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  Run *runs=mRuns;
  int32_t i, runCount=mRunCount, start;

  if(runCount<=10) {
    /* linear search for the run */
    for(i=0; aVisualIndex>=runs[i].visualLimit; ++i) {}
  } else {
    /* binary search for the run */
    int32_t start=0, limit=runCount;

    /* the middle if() will guaranteed find the run, we don't need a loop limit */
    for(;;) {
      i=(start+limit)/2;
      if(aVisualIndex>=runs[i].visualLimit) {
        start=i+1;
      } else if(i==0 || aVisualIndex>=runs[i-1].visualLimit) {
        break;
      } else {
        limit=i;
      }
    }
  }

  start=runs[i].logicalStart;
  if(IS_EVEN_RUN(start)) {
    /* LTR */
    /* the offset in runs[i] is aVisualIndex-runs[i-1].visualLimit */
    if(i>0) {
      aVisualIndex-=runs[i-1].visualLimit;
    }
    *aLogicalIndex = GET_INDEX(start)+aVisualIndex;
    return NS_OK;
  } else {
    /* RTL */
    *aLogicalIndex = GET_INDEX(start)+runs[i].visualLimit-aVisualIndex-1;
    return NS_OK;
  }
}

nsresult nsBidi::GetLogicalMap(int32_t *aIndexMap)
{
  nsresult rv;

  /* CountRuns() checks for VALID_PARA_OR_LINE */
  rv = CountRuns(nullptr);
  if(NS_FAILED(rv)) {
    return rv;
  } else if(aIndexMap==nullptr) {
    return NS_ERROR_INVALID_ARG;
  } else {
    /* fill a logical-to-visual index map using the runs[] */
    int32_t visualStart, visualLimit, j;
    int32_t logicalStart;
    Run *runs = mRuns;
    if (mLength <= 0) {
      return NS_OK;
    }

    visualStart = 0;
    for (j = 0; j < mRunCount; ++j) {
      logicalStart = GET_INDEX(runs[j].logicalStart);
      visualLimit = runs[j].visualLimit;
      if (IS_EVEN_RUN(runs[j].logicalStart)) {
        do { /* LTR */
          aIndexMap[logicalStart++] = visualStart++;
        } while (visualStart < visualLimit);
      } else {
        logicalStart += visualLimit-visualStart;  /* logicalLimit */
        do { /* RTL */
          aIndexMap[--logicalStart] = visualStart++;
        } while (visualStart < visualLimit);
      }
      /* visualStart==visualLimit; */
    }
  }
  return NS_OK;
}

nsresult nsBidi::GetVisualMap(int32_t *aIndexMap)
{
  int32_t* runCount=nullptr;
  nsresult rv;

  if(aIndexMap==nullptr) {
    return NS_ERROR_INVALID_ARG;
  }

  /* CountRuns() checks all of its and our arguments */
  rv = CountRuns(runCount);
  if(NS_FAILED(rv)) {
    return rv;
  } else {
    /* fill a visual-to-logical index map using the runs[] */
    Run *runs=mRuns, *runsLimit=runs+mRunCount;
    int32_t logicalStart, visualStart, visualLimit;

    visualStart=0;
    for(; runs<runsLimit; ++runs) {
      logicalStart=runs->logicalStart;
      visualLimit=runs->visualLimit;
      if(IS_EVEN_RUN(logicalStart)) {
        do { /* LTR */
          *aIndexMap++ = logicalStart++;
        } while(++visualStart<visualLimit);
      } else {
        REMOVE_ODD_BIT(logicalStart);
        logicalStart+=visualLimit-visualStart;  /* logicalLimit */
        do { /* RTL */
          *aIndexMap++ = --logicalStart;
        } while(++visualStart<visualLimit);
      }
      /* visualStart==visualLimit; */
    }
    return NS_OK;
  }
}

/* reorder a line based on a levels array (L2) ------------------------------ */

nsresult nsBidi::ReorderLogical(const nsBidiLevel *aLevels, int32_t aLength, int32_t *aIndexMap)
{
  int32_t start, limit, sumOfSosEos;
  nsBidiLevel minLevel, maxLevel;

  if(aIndexMap==nullptr ||
     !PrepareReorder(aLevels, aLength, aIndexMap, &minLevel, &maxLevel)) {
    return NS_OK;
  }

  /* nothing to do? */
  if(minLevel==maxLevel && (minLevel&1)==0) {
    return NS_OK;
  }

  /* reorder only down to the lowest odd level */
  minLevel|=1;

  /* loop maxLevel..minLevel */
  do {
    start=0;

    /* loop for all sequences of levels to reorder at the current maxLevel */
    for(;;) {
      /* look for a sequence of levels that are all at >=maxLevel */
      /* look for the first index of such a sequence */
      while(start<aLength && aLevels[start]<maxLevel) {
        ++start;
      }
      if(start>=aLength) {
        break;  /* no more such sequences */
      }

      /* look for the limit of such a sequence (the index behind it) */
      for(limit=start; ++limit<aLength && aLevels[limit]>=maxLevel;) {}

      /*
       * sos=start of sequence, eos=end of sequence
       *
       * The closed (inclusive) interval from sos to eos includes all the logical
       * and visual indexes within this sequence. They are logically and
       * visually contiguous and in the same range.
       *
       * For each run, the new visual index=sos+eos-old visual index;
       * we pre-add sos+eos into sumOfSosEos ->
       * new visual index=sumOfSosEos-old visual index;
       */
      sumOfSosEos=start+limit-1;

      /* reorder each index in the sequence */
      do {
        aIndexMap[start]=sumOfSosEos-aIndexMap[start];
      } while(++start<limit);

      /* start==limit */
      if(limit==aLength) {
        break;  /* no more such sequences */
      } else {
        start=limit+1;
      }
    }
  } while(--maxLevel>=minLevel);

  return NS_OK;
}

nsresult nsBidi::InvertMap(const int32_t *aSrcMap, int32_t *aDestMap, int32_t aLength)
{
  if(aSrcMap!=nullptr && aDestMap!=nullptr && aLength > 0) {
    const int32_t *pi;
    int32_t destLength = -1, count = 0;
    /* find highest value and count positive indexes in srcMap */
    pi = aSrcMap + aLength;
    while (pi > aSrcMap) {
      if (*--pi > destLength) {
        destLength = *pi;
      }
      if (*pi >= 0) {
        count++;
      }
    }
    destLength++;  /* add 1 for origin 0 */
    if (count < destLength) {
      /* we must fill unmatched destMap entries with -1 */
      memset(aDestMap, 0xFF, destLength * sizeof(int32_t));
    }
    pi = aSrcMap + aLength;
    while (aLength > 0) {
      if (*--pi >= 0) {
        aDestMap[*pi] = --aLength;
      } else {
        --aLength;
      }
    }
  }
  return NS_OK;
}

int32_t nsBidi::doWriteReverse(const char16_t *src, int32_t srcLength,
                               char16_t *dest, uint16_t options) {
  /*
   * RTL run -
   *
   * RTL runs need to be copied to the destination in reverse order
   * of code points, not code units, to keep Unicode characters intact.
   *
   * The general strategy for this is to read the source text
   * in backward order, collect all code units for a code point
   * (and optionally following combining characters, see below),
   * and copy all these code units in ascending order
   * to the destination for this run.
   *
   * Several options request whether combining characters
   * should be kept after their base characters,
   * whether Bidi control characters should be removed, and
   * whether characters should be replaced by their mirror-image
   * equivalent Unicode characters.
   */
  int32_t i, j, destSize;
  uint32_t c;

  /* optimize for several combinations of options */
  switch(options&(NSBIDI_REMOVE_BIDI_CONTROLS|NSBIDI_DO_MIRRORING|NSBIDI_KEEP_BASE_COMBINING)) {
    case 0:
        /*
         * With none of the "complicated" options set, the destination
         * run will have the same length as the source run,
         * and there is no mirroring and no keeping combining characters
         * with their base characters.
         */
      destSize=srcLength;

    /* preserve character integrity */
      do {
      /* i is always after the last code unit known to need to be kept in this segment */
        i=srcLength;

      /* collect code units for one base character */
        UTF_BACK_1(src, 0, srcLength);

      /* copy this base character */
        j=srcLength;
        do {
          *dest++=src[j++];
        } while(j<i);
      } while(srcLength>0);
      break;
    case NSBIDI_KEEP_BASE_COMBINING:
    /*
         * Here, too, the destination
         * run will have the same length as the source run,
         * and there is no mirroring.
         * We do need to keep combining characters with their base characters.
         */
      destSize=srcLength;

    /* preserve character integrity */
      do {
      /* i is always after the last code unit known to need to be kept in this segment */
        i=srcLength;

      /* collect code units and modifier letters for one base character */
        do {
          UTF_PREV_CHAR(src, 0, srcLength, c);
        } while(srcLength>0 && GetBidiCat(c) == eCharType_DirNonSpacingMark);

      /* copy this "user character" */
        j=srcLength;
        do {
          *dest++=src[j++];
        } while(j<i);
      } while(srcLength>0);
      break;
    default:
    /*
         * With several "complicated" options set, this is the most
         * general and the slowest copying of an RTL run.
         * We will do mirroring, remove Bidi controls, and
         * keep combining characters with their base characters
         * as requested.
         */
      if(!(options&NSBIDI_REMOVE_BIDI_CONTROLS)) {
        i=srcLength;
      } else {
      /* we need to find out the destination length of the run,
               which will not include the Bidi control characters */
        int32_t length=srcLength;
        char16_t ch;

        i=0;
        do {
          ch=*src++;
          if (!IsBidiControl((uint32_t)ch)) {
            ++i;
          }
        } while(--length>0);
        src-=srcLength;
      }
      destSize=i;

    /* preserve character integrity */
      do {
      /* i is always after the last code unit known to need to be kept in this segment */
        i=srcLength;

      /* collect code units for one base character */
        UTF_PREV_CHAR(src, 0, srcLength, c);
        if(options&NSBIDI_KEEP_BASE_COMBINING) {
        /* collect modifier letters for this base character */
          while(srcLength>0 && GetBidiCat(c) == eCharType_DirNonSpacingMark) {
            UTF_PREV_CHAR(src, 0, srcLength, c);
          }
        }

        if(options&NSBIDI_REMOVE_BIDI_CONTROLS && IsBidiControl(c)) {
        /* do not copy this Bidi control character */
          continue;
        }

      /* copy this "user character" */
        j=srcLength;
        if(options&NSBIDI_DO_MIRRORING) {
          /* mirror only the base character */
          c = GetMirroredChar(c);

          int32_t k=0;
          UTF_APPEND_CHAR_UNSAFE(dest, k, c);
          dest+=k;
          j+=k;
        }
        while(j<i) {
          *dest++=src[j++];
        }
      } while(srcLength>0);
      break;
  } /* end of switch */
  return destSize;
}

nsresult nsBidi::WriteReverse(const char16_t *aSrc, int32_t aSrcLength, char16_t *aDest, uint16_t aOptions, int32_t *aDestSize)
{
  if( aSrc==nullptr || aSrcLength<0 ||
      aDest==nullptr
    ) {
    return NS_ERROR_INVALID_ARG;
  }

  /* do input and output overlap? */
  if( aSrc>=aDest && aSrc<aDest+aSrcLength ||
      aDest>=aSrc && aDest<aSrc+aSrcLength
    ) {
    return NS_ERROR_INVALID_ARG;
  }

  if(aSrcLength>0) {
    *aDestSize = doWriteReverse(aSrc, aSrcLength, aDest, aOptions);
  }
  return NS_OK;
}
#endif // FULL_BIDI_ENGINE
