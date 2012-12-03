/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifdef IBMBIDI

#include "nsBidi.h"
#include "nsUnicodeProperties.h"
#include "nsCRT.h"

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
 * after the initial setup.
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
  /* reset the object, all pointers NULL, all flags false, all sizes 0 */
  mLength = 0;
  mParaLevel = 0;
  mFlags = 0;
  mDirection = NSBIDI_LTR;
  mTrailingWSStart = 0;

  mDirPropsSize = 0;
  mLevelsSize = 0;
  mRunsSize = 0;
  mRunCount = -1;

  mDirProps=NULL;
  mLevels=NULL;
  mRuns=NULL;

  mDirPropsMemory=NULL;
  mLevelsMemory=NULL;
  mRunsMemory=NULL;

  mMayAllocateText=false;
  mMayAllocateRuns=false;
  
}

/*
 * We are allowed to allocate memory if aMemory==NULL or
 * aMayAllocate==true for each array that we need.
 * We also try to grow and shrink memory as needed if we
 * allocate it.
 *
 * Assume aSizeNeeded>0.
 * If *aMemory!=NULL, then assume *aSize>0.
 *
 * ### this realloc() may unnecessarily copy the old data,
 * which we know we don't need any more;
 * is this the best way to do this??
 */
bool nsBidi::GetMemory(void **aMemory, size_t *aSize, bool aMayAllocate, size_t aSizeNeeded)
{
  /* check for existing memory */
  if(*aMemory==NULL) {
    /* we need to allocate memory */
    if(!aMayAllocate) {
      return false;
    } else {
      *aMemory=moz_malloc(aSizeNeeded);
      if (*aMemory!=NULL) {
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
      void *memory=moz_realloc(*aMemory, aSizeNeeded);

      if(memory!=NULL) {
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
  moz_free(mDirPropsMemory);
  mDirPropsMemory = nullptr;
  moz_free(mLevelsMemory);
  mLevelsMemory = nullptr;
  moz_free(mRunsMemory);
  mRunsMemory = nullptr;
}

/* SetPara ------------------------------------------------------------ */

nsresult nsBidi::SetPara(const PRUnichar *aText, int32_t aLength,
                         nsBidiLevel aParaLevel, nsBidiLevel *aEmbeddingLevels)
{
  nsBidiDirection direction;

  /* check the argument values */
  if(aText==NULL ||
     ((NSBIDI_MAX_EXPLICIT_LEVEL<aParaLevel) && !IS_DEFAULT_LEVEL(aParaLevel)) ||
     aLength<-1
    ) {
    return NS_ERROR_INVALID_ARG;
  }

  if(aLength==-1) {
    aLength = NS_strlen(aText);
  }

  /* initialize member data */
  mLength=aLength;
  mParaLevel=aParaLevel;
  mDirection=NSBIDI_LTR;
  mTrailingWSStart=aLength;  /* the levels[] will reflect the WS run */

  mDirProps=NULL;
  mLevels=NULL;
  mRuns=NULL;

  if(aLength==0) {
    /*
     * For an empty paragraph, create an nsBidi object with the aParaLevel and
     * the flags and the direction set but without allocating zero-length arrays.
     * There is nothing more to do.
     */
    if(IS_DEFAULT_LEVEL(aParaLevel)) {
      mParaLevel&=1;
    }
    if(aParaLevel&1) {
      mFlags=DIRPROP_FLAG(R);
      mDirection=NSBIDI_RTL;
    } else {
      mFlags=DIRPROP_FLAG(L);
      mDirection=NSBIDI_LTR;
    }

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
  if(aEmbeddingLevels==NULL) {
    /* no: determine explicit levels according to the (Xn) rules */\
    if(GETLEVELSMEMORY(aLength)) {
      mLevels=mLevelsMemory;
      direction=ResolveExplicitLevels();
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

  /*
   * The steps after (X9) in the Bidi algorithm are performed only if
   * the paragraph text has mixed directionality!
   */
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
      if(aEmbeddingLevels==NULL && !(mFlags&DIRPROP_FLAG_MULTI_RUNS)) {
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
          }
        } while(limit<aLength);
      }

      /* reset the embedding levels for some non-graphic characters (L1), (X9) */
      AdjustWSLevels();
      break;
  }

  mDirection=direction;
  return NS_OK;
}

/* perform (P2)..(P3) ------------------------------------------------------- */

/*
 * Get the directional properties for the text,
 * calculate the flags bit-set, and
 * determine the partagraph level if necessary.
 */
void nsBidi::GetDirProps(const PRUnichar *aText)
{
  DirProp *dirProps=mDirPropsMemory;    /* mDirProps is const */

  int32_t i=0, length=mLength;
  Flags flags=0;      /* collect all directionalities in the text */
  PRUnichar uchar;
  DirProp dirProp;

  if(IS_DEFAULT_LEVEL(mParaLevel)) {
    /* determine the paragraph level (P2..P3) */
    for(;;) {
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
      if(dirProp==L) {
        mParaLevel=0;
        break;
      } else if(dirProp==R || dirProp==AL) {
        mParaLevel=1;
        break;
      } else if(i==length) {
        /*
         * see comment in nsIBidi.h:
         * the DEFAULT_XXX values are designed so that
         * their bit 0 alone yields the intended default
         */
        mParaLevel&=1;
        break;
      }
    }
  }

  /* get the rest of the directional properties and the flags bits */
  while(i<length) {
    uchar=aText[i];
    if(!IS_FIRST_SURROGATE(uchar) || i+1==length || !IS_SECOND_SURROGATE(aText[i+1])) {
      /* not a surrogate pair */
      flags|=DIRPROP_FLAG(dirProps[i]=GetBidiCat((uint32_t)uchar));
    } else {
      /* a surrogate pair */
      dirProps[i++]=BN;   /* second surrogate in the pair gets the BN type */
      flags|=DIRPROP_FLAG(dirProps[i]=GetBidiCat(GET_UTF_32(uchar, aText[i])))|DIRPROP_FLAG(BN);
    }
    ++i;
  }
  if(flags&MASK_EMBEDDING) {
    flags|=DIRPROP_FLAG_LR(mParaLevel);
  }
  mFlags=flags;
}

/* perform (X1)..(X9) ------------------------------------------------------- */

/*
 * Resolve the explicit levels as specified by explicit embedding codes.
 * Recalculate the flags to have them reflect the real properties
 * after taking the explicit embeddings into account.
 *
 * The Bidi algorithm is designed to result in the same behavior whether embedding
 * levels are externally specified (from "styled text", supposedly the preferred
 * method) or set by explicit embedding codes (LRx, RLx, PDF) in the plain text.
 * That is why (X9) instructs to remove all explicit codes (and BN).
 * However, in a real implementation, this removal of these codes and their index
 * positions in the plain text is undesirable since it would result in
 * reallocated, reindexed text.
 * Instead, this implementation leaves the codes in there and just ignores them
 * in the subsequent processing.
 * In order to get the same reordering behavior, positions with a BN or an
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
 * With the Bidi stack of explicit levels,
 * as pushed with each LRE, RLE, LRO, and RLO and popped with each PDF,
 * the explicit level must never exceed NSBIDI_MAX_EXPLICIT_LEVEL==61.
 *
 * In order to have a correct push-pop semantics even in the case of overflows,
 * there are two overflow counters:
 * - countOver60 is incremented with each LRx at level 60
 * - from level 60, one RLx increases the level to 61
 * - countOver61 is incremented with each LRx and RLx at level 61
 *
 * Popping levels with PDF must work in the opposite order so that level 61
 * is correct at the correct point. Underflows (too many PDFs) must be checked.
 *
 * This implementation assumes that NSBIDI_MAX_EXPLICIT_LEVEL is odd.
 */

nsBidiDirection nsBidi::ResolveExplicitLevels()
{
  const DirProp *dirProps=mDirProps;
  nsBidiLevel *levels=mLevels;

  int32_t i=0, length=mLength;
  Flags flags=mFlags;       /* collect all directionalities in the text */
  DirProp dirProp;
  nsBidiLevel level=mParaLevel;

  nsBidiDirection direction;

  /* determine if the text is mixed-directional or single-directional */
  direction=DirectionFromFlags(flags);

  /* we may not need to resolve any explicit levels */
  if(direction!=NSBIDI_MIXED) {
    /* not mixed directionality: levels don't matter - trailingWSStart will be 0 */
  } else if(!(flags&MASK_EXPLICIT)) {
    /* mixed, but all characters are at the same embedding level */
    /* set all levels to the paragraph level */
    for(i=0; i<length; ++i) {
      levels[i]=level;
    }
  } else {
    /* continue to perform (Xn) */

    /* (X1) level is set for all codes, embeddingLevel keeps track of the push/pop operations */
    /* both variables may carry the NSBIDI_LEVEL_OVERRIDE flag to indicate the override status */
    nsBidiLevel embeddingLevel=level, newLevel, stackTop=0;

    nsBidiLevel stack[NSBIDI_MAX_EXPLICIT_LEVEL];        /* we never push anything >=NSBIDI_MAX_EXPLICIT_LEVEL */
    uint32_t countOver60=0, countOver61=0;  /* count overflows of explicit levels */

    /* recalculate the flags */
    flags=0;

    /* since we assume that this is a single paragraph, we ignore (X8) */
    for(i=0; i<length; ++i) {
      dirProp=dirProps[i];
      switch(dirProp) {
        case LRE:
        case LRO:
          /* (X3, X5) */
          newLevel=(embeddingLevel+2)&~(NSBIDI_LEVEL_OVERRIDE|1);    /* least greater even level */
          if(newLevel<=NSBIDI_MAX_EXPLICIT_LEVEL) {
            stack[stackTop]=embeddingLevel;
            ++stackTop;
            embeddingLevel=newLevel;
            if(dirProp==LRO) {
              embeddingLevel|=NSBIDI_LEVEL_OVERRIDE;
            } else {
              embeddingLevel&=~NSBIDI_LEVEL_OVERRIDE;
            }
          } else if((embeddingLevel&~NSBIDI_LEVEL_OVERRIDE)==NSBIDI_MAX_EXPLICIT_LEVEL) {
            ++countOver61;
          } else /* (embeddingLevel&~NSBIDI_LEVEL_OVERRIDE)==NSBIDI_MAX_EXPLICIT_LEVEL-1 */ {
            ++countOver60;
          }
          flags|=DIRPROP_FLAG(BN);
          break;
        case RLE:
        case RLO:
          /* (X2, X4) */
          newLevel=((embeddingLevel&~NSBIDI_LEVEL_OVERRIDE)+1)|1;    /* least greater odd level */
          if(newLevel<=NSBIDI_MAX_EXPLICIT_LEVEL) {
            stack[stackTop]=embeddingLevel;
            ++stackTop;
            embeddingLevel=newLevel;
            if(dirProp==RLO) {
              embeddingLevel|=NSBIDI_LEVEL_OVERRIDE;
            } else {
              embeddingLevel&=~NSBIDI_LEVEL_OVERRIDE;
            }
          } else {
            ++countOver61;
          }
          flags|=DIRPROP_FLAG(BN);
          break;
        case PDF:
          /* (X7) */
          /* handle all the overflow cases first */
          if(countOver61>0) {
            --countOver61;
          } else if(countOver60>0 && (embeddingLevel&~NSBIDI_LEVEL_OVERRIDE)!=NSBIDI_MAX_EXPLICIT_LEVEL) {
            /* handle LRx overflows from level 60 */
            --countOver60;
          } else if(stackTop>0) {
            /* this is the pop operation; it also pops level 61 while countOver60>0 */
            --stackTop;
            embeddingLevel=stack[stackTop];
            /* } else { (underflow) */
          }
          flags|=DIRPROP_FLAG(BN);
          break;
        case B:
          /*
           * We do not really expect to see a paragraph separator (B),
           * but we should do something reasonable with it,
           * especially at the end of the text.
           */
          stackTop=0;
          countOver60=countOver61=0;
          embeddingLevel=level=mParaLevel;
          flags|=DIRPROP_FLAG(B);
          break;
        case BN:
          /* BN, LRE, RLE, and PDF are supposed to be removed (X9) */
          /* they will get their levels set correctly in AdjustWSLevels() */
          flags|=DIRPROP_FLAG(BN);
          break;
        default:
          /* all other types get the "real" level */
          if(level!=embeddingLevel) {
            level=embeddingLevel;
            if(level&NSBIDI_LEVEL_OVERRIDE) {
              flags|=DIRPROP_FLAG_O(level)|DIRPROP_FLAG_MULTI_RUNS;
            } else {
              flags|=DIRPROP_FLAG_E(level)|DIRPROP_FLAG_MULTI_RUNS;
            }
          }
          if(!(level&NSBIDI_LEVEL_OVERRIDE)) {
            flags|=DIRPROP_FLAG(dirProp);
          }
          break;
      }

      /*
       * We need to set reasonable levels even on BN codes and
       * explicit codes because we will later look at same-level runs (X10).
       */
      levels[i]=level;
    }
    if(flags&MASK_EMBEDDING) {
      flags|=DIRPROP_FLAG_LR(mParaLevel);
    }

    /* subsequently, ignore the explicit codes and BN (X9) */

    /* again, determine if the text is mixed-directional or single-directional */
    mFlags=flags;
    direction=DirectionFromFlags(flags);
  }
  return direction;
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
  nsBidiLevel *levels=mLevels;

  int32_t i, length=mLength;
  Flags flags=0;  /* collect all directionalities in the text */
  nsBidiLevel level, paraLevel=mParaLevel;

  for(i=0; i<length; ++i) {
    level=levels[i];
    if(level&NSBIDI_LEVEL_OVERRIDE) {
      /* keep the override flag in levels[i] but adjust the flags */
      level&=~NSBIDI_LEVEL_OVERRIDE;     /* make the range check below simpler */
      flags|=DIRPROP_FLAG_O(level);
    } else {
      /* set the flags */
      flags|=DIRPROP_FLAG_E(level)|DIRPROP_FLAG(dirProps[i]);
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

/* perform rules (Wn), (Nn), and (In) on a run of the text ------------------ */

/*
 * This implementation of the (Wn) rules applies all rules in one pass.
 * In order to do so, it needs a look-ahead of typically 1 character
 * (except for W5: sequences of ET) and keeps track of changes
 * in a rule Wp that affect a later Wq (p<q).
 *
 * historyOfEN is a variable-saver: it contains 4 boolean states;
 * a bit in it set to 1 means:
 *  bit 0: the current code is an EN after W2
 *  bit 1: the current code is an EN after W4
 *  bit 2: the previous code was an EN after W2
 *  bit 3: the previous code was an EN after W4
 * In other words, b0..1 have transitions of EN in the current iteration,
 * while b2..3 have the transitions of EN in the previous iteration.
 * A simple historyOfEN<<=2 suffices for the propagation.
 *
 * The (Nn) and (In) rules are also performed in that same single loop,
 * but effectively one iteration behind for white space.
 *
 * Since all implicit rules are performed in one step, it is not necessary
 * to actually store the intermediate directional properties in dirProps[].
 */

#define EN_SHIFT 2
#define EN_AFTER_W2 1
#define EN_AFTER_W4 2
#define EN_ALL 3
#define PREV_EN_AFTER_W2 4
#define PREV_EN_AFTER_W4 8

void nsBidi::ResolveImplicitLevels(int32_t aStart, int32_t aLimit,
                   DirProp aSOR, DirProp aEOR)
{
  const DirProp *dirProps=mDirProps;
  nsBidiLevel *levels=mLevels;

  int32_t i, next, neutralStart=-1;
  DirProp prevDirProp, dirProp, nextDirProp, lastStrong, beforeNeutral;
  uint8_t historyOfEN;

  /* initialize: current at aSOR, next at aStart (it is aStart<aLimit) */
  next=aStart;
  beforeNeutral=dirProp=lastStrong=aSOR;
  nextDirProp=dirProps[next];
  historyOfEN=0;

  /*
   * In all steps of this implementation, BN and explicit embedding codes
   * must be treated as if they didn't exist (X9).
   * They will get levels set before a non-neutral character, and remain
   * undefined before a neutral one, but AdjustWSLevels() will take care
   * of all of them.
   */
  while(DIRPROP_FLAG(nextDirProp)&MASK_BN_EXPLICIT) {
    if(++next<aLimit) {
      nextDirProp=dirProps[next];
    } else {
      nextDirProp=aEOR;
      break;
    }
  }

  /* loop for entire run */
  while(next<aLimit) {
    /* advance */
    prevDirProp=dirProp;
    dirProp=nextDirProp;
    i=next;
    do {
      if(++next<aLimit) {
        nextDirProp=dirProps[next];
      } else {
        nextDirProp=aEOR;
        break;
      }
    } while(DIRPROP_FLAG(nextDirProp)&MASK_BN_EXPLICIT);
    historyOfEN<<=EN_SHIFT;

    /* (W1..W7) */
    switch(dirProp) {
      case L:
        lastStrong=L;
        break;
      case R:
        lastStrong=R;
        break;
      case AL:
        /* (W3) */
        lastStrong=AL;
        dirProp=R;
        break;
      case EN:
        /* we have to set historyOfEN correctly */
        if(lastStrong==AL) {
          /* (W2) */
          dirProp=AN;
        } else {
          if(lastStrong==L) {
            /* (W7) */
            dirProp=L;
          }
          /* this EN stays after (W2) and (W4) - at least before (W7) */
          historyOfEN|=EN_ALL;
        }
        break;
      case ES:
        if( historyOfEN&PREV_EN_AFTER_W2 &&     /* previous was EN before (W4) */
            nextDirProp==EN && lastStrong!=AL   /* next is EN and (W2) won't make it AN */
          ) {
          /* (W4) */
          if(lastStrong!=L) {
            dirProp=EN;
          } else {
            /* (W7) */
            dirProp=L;
          }
          historyOfEN|=EN_AFTER_W4;
        } else {
          /* (W6) */
          dirProp=O_N;
        }
        break;
      case CS:
        if( historyOfEN&PREV_EN_AFTER_W2 &&     /* previous was EN before (W4) */
            nextDirProp==EN && lastStrong!=AL   /* next is EN and (W2) won't make it AN */
          ) {
          /* (W4) */
          if(lastStrong!=L) {
            dirProp=EN;
          } else {
            /* (W7) */
            dirProp=L;
          }
          historyOfEN|=EN_AFTER_W4;
        } else if(prevDirProp==AN &&                    /* previous was AN */
              (nextDirProp==AN ||                   /* next is AN */
               (nextDirProp==EN && lastStrong==AL))   /* or (W2) will make it one */
             ) {
          /* (W4) */
          dirProp=AN;
        } else {
          /* (W6) */
          dirProp=O_N;
        }
        break;
      case ET:
        /* get sequence of ET; advance only next, not current, previous or historyOfEN */
        while(next<aLimit && DIRPROP_FLAG(nextDirProp)&MASK_ET_NSM_BN /* (W1), (X9) */) {
          if(++next<aLimit) {
            nextDirProp=dirProps[next];
          } else {
            nextDirProp=aEOR;
            break;
          }
        }

        if( historyOfEN&PREV_EN_AFTER_W4 ||     /* previous was EN before (W5) */
            (nextDirProp==EN && lastStrong!=AL)   /* next is EN and (W2) won't make it AN */
          ) {
          /* (W5) */
          if(lastStrong!=L) {
            dirProp=EN;
          } else {
            /* (W7) */
            dirProp=L;
          }
        } else {
          /* (W6) */
          dirProp=O_N;
        }

        /* apply the result of (W1), (W5)..(W7) to the entire sequence of ET */
        break;
      case NSM:
        /* (W1) */
        dirProp=prevDirProp;
        /* set historyOfEN back to prevDirProp's historyOfEN */
        historyOfEN>>=EN_SHIFT;
        /*
         * Technically, this should be done before the switch() in the form
         *      if(nextDirProp==NSM) {
         *          dirProps[next]=nextDirProp=dirProp;
         *      }
         *
         * - effectively one iteration ahead.
         * However, whether the next dirProp is NSM or is equal to the current dirProp
         * does not change the outcome of any condition in (W2)..(W7).
         */
        break;
      default:
        break;
    }

    /* here, it is always [prev,this,next]dirProp!=BN; it may be next>i+1 */

    /* perform (Nn) - here, only L, R, EN, AN, and neutrals are left */
    /* this is one iteration late for the neutrals */
    if(DIRPROP_FLAG(dirProp)&MASK_N) {
      if(neutralStart<0) {
        /* start of a sequence of neutrals */
        neutralStart=i;
        beforeNeutral=prevDirProp;
      }
    } else /* not a neutral, can be only one of { L, R, EN, AN } */ {
      /*
       * Note that all levels[] values are still the same at this
       * point because this function is called for an entire
       * same-level run.
       * Therefore, we need to read only one actual level.
       */
      nsBidiLevel level=levels[i];

      if(neutralStart>=0) {
        nsBidiLevel final;
        /* end of a sequence of neutrals (dirProp is "afterNeutral") */
        if(beforeNeutral==L) {
          if(dirProp==L) {
            final=0;                /* make all neutrals L (N1) */
          } else {
            final=level;            /* make all neutrals "e" (N2) */
          }
        } else /* beforeNeutral is one of { R, EN, AN } */ {
          if(dirProp==L) {
            final=level;            /* make all neutrals "e" (N2) */
          } else {
            final=1;                /* make all neutrals R (N1) */
          }
        }
        /* perform (In) on the sequence of neutrals */
        if((level^final)&1) {
          /* do something only if we need to _change_ the level */
          do {
            ++levels[neutralStart];
          } while(++neutralStart<i);
        }
        neutralStart=-1;
      }

      /* perform (In) on the non-neutral character */
      /*
       * in the cases of (W5), processing a sequence of ET,
       * and of (X9), skipping BN,
       * there may be multiple characters from i to <next
       * that all get (virtually) the same dirProp and (really) the same level
       */
      if(dirProp==L) {
        if(level&1) {
          ++level;
        } else {
          i=next;     /* we keep the levels */
        }
      } else if(dirProp==R) {
        if(!(level&1)) {
          ++level;
        } else {
          i=next;     /* we keep the levels */
        }
      } else /* EN or AN */ {
        level=(level+2)&~1;     /* least greater even level */
      }

      /* apply the new level to the sequence, if necessary */
      while(i<next) {
        levels[i++]=level;
      }
    }
  }

  /* perform (Nn) - here,
  the character after the neutrals is aEOR, which is either L or R */
  /* this is one iteration late for the neutrals */
  if(neutralStart>=0) {
    /*
     * Note that all levels[] values are still the same at this
     * point because this function is called for an entire
     * same-level run.
     * Therefore, we need to read only one actual level.
     */
    nsBidiLevel level=levels[neutralStart], final;

    /* end of a sequence of neutrals (aEOR is "afterNeutral") */
    if(beforeNeutral==L) {
      if(aEOR==L) {
        final=0;                /* make all neutrals L (N1) */
      } else {
        final=level;            /* make all neutrals "e" (N2) */
      }
    } else /* beforeNeutral is one of { R, EN, AN } */ {
      if(aEOR==L) {
        final=level;            /* make all neutrals "e" (N2) */
      } else {
        final=1;                /* make all neutrals R (N1) */
      }
    }
    /* perform (In) on the sequence of neutrals */
    if((level^final)&1) {
      /* do something only if we need to _change_ the level */
      do {
        ++levels[neutralStart];
      } while(++neutralStart<aLimit);
    }
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
      while(i>0 && DIRPROP_FLAG(dirProps[--i])&MASK_WS) {
        levels[i]=paraLevel;
      }

      /* reset BN to the next character's paraLevel until B/S, which restarts above loop */
      /* here, i+1 is guaranteed to be <length */
      while(i>0) {
        flag=DIRPROP_FLAG(dirProps[--i]);
        if(flag&MASK_BN_EXPLICIT) {
          levels[i]=levels[i+1];
        } else if(flag&MASK_B_S) {
          levels[i]=paraLevel;
          break;
        }
      }
    }
  }

  /* now remove the NSBIDI_LEVEL_OVERRIDE flags, if any */
  /* (a separate loop can be optimized more easily by a compiler) */
  if(mFlags&MASK_OVERRIDE) {
    for(i=mTrailingWSStart; i>0;) {
      levels[--i]&=~NSBIDI_LEVEL_OVERRIDE;
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
 * the Unicode 3.0 Bidi algorithm as defined in
 * http://www.unicode.org/unicode/reports/tr9/ , version 5,
 * also described in The Unicode Standard, Version 3.0 .
 *
 * This means that there is a nsBidi object with a levels
 * and a dirProps array.
 * paraLevel and direction are also set.
 * Only if the length of the text is zero, then levels==dirProps==NULL.
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
nsresult nsBidi::SetLine(nsIBidi* aParaBidi, int32_t aStart, int32_t aLimit)
{
  nsBidi* pParent = (nsBidi*)aParaBidi;
  int32_t length;

  /* check the argument values */
  if(pParent==NULL) {
    return NS_ERROR_INVALID_POINTER;
  } else if(aStart<0 || aStart>aLimit || aLimit>pParent->mLength) {
    return NS_ERROR_INVALID_ARG;
  }

  /* set members from our aParaBidi parent */
  length=mLength=aLimit-aStart;
  mParaLevel=pParent->mParaLevel;

  mRuns=NULL;
  mFlags=0;

  if(length>0) {
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
      Flags flags=0;

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
  } else {
    /* create an object for a zero-length line */
    mDirection=mParaLevel&1 ? NSBIDI_RTL : NSBIDI_LTR;
    mTrailingWSStart=mRunCount=0;

    mDirProps=NULL;
    mLevels=NULL;
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
    *aLevels = NULL;
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
    *aLevels = NULL;
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

  if(mDirection!=NSBIDI_MIXED || aLogicalStart>=mTrailingWSStart) {
    if(aLogicalLimit!=NULL) {
      *aLogicalLimit=length;
    }
    if(aLevel!=NULL) {
      *aLevel=mParaLevel;
    }
  } else {
    nsBidiLevel *levels=mLevels;
    nsBidiLevel level=levels[aLogicalStart];

    /* search for the end of the run */
    length=mTrailingWSStart;
    while(++aLogicalStart<length && level==levels[aLogicalStart]) {}

    if(aLogicalLimit!=NULL) {
      *aLogicalLimit=aLogicalStart;
    }
    if(aLevel!=NULL) {
      *aLevel=level;
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
    if(aLogicalStart!=NULL) {
      *aLogicalStart=GET_INDEX(start);
    }
    if(aLength!=NULL) {
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
  if(mDirection!=NSBIDI_MIXED) {
    /* simple, single-run case - this covers length==0 */
    GetSingleRun(mParaLevel);
  } else /* NSBIDI_MIXED, length>0 */ {
    /* mixed directionality */
    int32_t length=mLength, limit=length;

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
    limit=mTrailingWSStart;
    if(limit==0) {
      /* there is only WS on this line */
      GetSingleRun(mParaLevel);
    } else {
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
        start=0;
        level=levels[0];
        if(level<minLevel) {
          minLevel=level;
        }
        if(level>maxLevel) {
          maxLevel=level;
        }

        /* initialize visualLimit values with the run lengths */
        for(i=1; i<limit; ++i) {
          if(levels[i]!=level) {
            /* i is another run limit */
            runs[runIndex].logicalStart=start;
            runs[runIndex].visualLimit=i-start;
            start=i;

            level=levels[i];
            if(level<minLevel) {
              minLevel=level;
            }
            if(level>maxLevel) {
              maxLevel=level;
            }
            ++runIndex;
          }
        }

        /* finish the last run at i==limit */
        runs[runIndex].logicalStart=start;
        runs[runIndex].visualLimit=limit-start;
        ++runIndex;

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
        ADD_ODD_BIT_FROM_LEVEL(runs[0].logicalStart, levels[runs[0].logicalStart]);
        limit=runs[0].visualLimit;
        for(i=1; i<runIndex; ++i) {
          ADD_ODD_BIT_FROM_LEVEL(runs[i].logicalStart, levels[runs[i].logicalStart]);
          limit=runs[i].visualLimit+=limit;
        }

        /* same for the trailing WS run */
        if(runIndex<runCount) {
          ADD_ODD_BIT_FROM_LEVEL(runs[i].logicalStart, mParaLevel);
          runs[runIndex].visualLimit+=limit;
        }
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
  Run *runs;
  nsBidiLevel *levels;
  int32_t firstRun, endRun, limitRun, runCount, temp;

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
        temp=runs[firstRun].logicalStart;
        runs[firstRun].logicalStart=runs[endRun].logicalStart;
        runs[endRun].logicalStart=temp;

        temp=runs[firstRun].visualLimit;
        runs[firstRun].visualLimit=runs[endRun].visualLimit;
        runs[endRun].visualLimit=temp;

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
      temp=runs[firstRun].logicalStart;
      runs[firstRun].logicalStart=runs[runCount].logicalStart;
      runs[runCount].logicalStart=temp;

      temp=runs[firstRun].visualLimit;
      runs[firstRun].visualLimit=runs[runCount].visualLimit;
      runs[runCount].visualLimit=temp;

      ++firstRun;
      --runCount;
    }
  }
}

nsresult nsBidi::ReorderVisual(const nsBidiLevel *aLevels, int32_t aLength, int32_t *aIndexMap)
{
  int32_t start, end, limit, temp;
  nsBidiLevel minLevel, maxLevel;

  if(aIndexMap==NULL || !PrepareReorder(aLevels, aLength, aIndexMap, &minLevel, &maxLevel)) {
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

  if(aLevels==NULL || aLength<=0) {
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
          for(i=0;; ++i) {
            length=runs[i].visualLimit-visualStart;
            offset=aLogicalIndex-GET_INDEX(runs[i].logicalStart);
            if(offset>=0 && offset<length) {
              if(IS_EVEN_RUN(runs[i].logicalStart)) {
                /* LTR */
                *aVisualIndex = visualStart+offset;
                return NS_OK;
              } else {
                /* RTL */
                *aVisualIndex = visualStart+length-offset-1;
                return NS_OK;
              }
            }
            visualStart+=length;
          }
        }
    }
  }
}

nsresult nsBidi::GetLogicalIndex(int32_t aVisualIndex, int32_t *aLogicalIndex)
{
  if(aVisualIndex<0 || mLength<=aVisualIndex) {
    return NS_ERROR_INVALID_ARG;
  } else {
    /* we can do the trivial cases without the runs array */
    switch(mDirection) {
      case NSBIDI_LTR:
        *aLogicalIndex = aVisualIndex;
        return NS_OK;
      case NSBIDI_RTL:
        *aLogicalIndex = mLength-aVisualIndex-1;
        return NS_OK;
      default:
        if(mRunCount<0 && !GetRuns()) {
          return NS_ERROR_OUT_OF_MEMORY;
        } else {
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
    }
  }
}

nsresult nsBidi::GetLogicalMap(int32_t *aIndexMap)
{
  nsBidiLevel *levels;
  nsresult rv;

  /* GetLevels() checks all of its and our arguments */
  rv = GetLevels(&levels);
  if(NS_FAILED(rv)) {
    return rv;
  } else if(aIndexMap==NULL) {
    return NS_ERROR_INVALID_ARG;
  } else {
    return ReorderLogical(levels, mLength, aIndexMap);
  }
}

nsresult nsBidi::GetVisualMap(int32_t *aIndexMap)
{
  int32_t* runCount=NULL;
  nsresult rv;

  /* CountRuns() checks all of its and our arguments */
  rv = CountRuns(runCount);
  if(NS_FAILED(rv)) {
    return rv;
  } else if(aIndexMap==NULL) {
    return NS_ERROR_INVALID_ARG;
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

  if(aIndexMap==NULL || !PrepareReorder(aLevels, aLength, aIndexMap, &minLevel, &maxLevel)) {
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
  if(aSrcMap!=NULL && aDestMap!=NULL) {
    aSrcMap+=aLength;
    while(aLength>0) {
      aDestMap[*--aSrcMap]=--aLength;
    }
  }
  return NS_OK;
}

int32_t nsBidi::doWriteReverse(const PRUnichar *src, int32_t srcLength,
                               PRUnichar *dest, uint16_t options) {
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
        } while(srcLength>0 && IsBidiCategory(c, eBidiCat_NSM));

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
        PRUnichar ch;

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
          while(srcLength>0 && IsBidiCategory(c, eBidiCat_NSM)) {
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
          c = SymmSwap(c);

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

nsresult nsBidi::WriteReverse(const PRUnichar *aSrc, int32_t aSrcLength, PRUnichar *aDest, uint16_t aOptions, int32_t *aDestSize)
{
  if( aSrc==NULL || aSrcLength<0 ||
      aDest==NULL
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
#endif // IBMBIDI
