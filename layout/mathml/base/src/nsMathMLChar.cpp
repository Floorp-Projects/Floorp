/*
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
 * The Original Code is Mozilla MathML Project.
 *
 * The Initial Developer of the Original Code is The University Of
 * Queensland.  Portions created by The University Of Queensland are
 * Copyright (C) 1999 The University Of Queensland.  All Rights Reserved.
 *
 * Contributor(s):
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 *   Shyjan Mahamud <mahamud@cs.cmu.edu>
 */

#include "nsCOMPtr.h"
#include "nsHTMLParts.h"
#include "nsIHTMLContent.h"
#include "nsFrame.h"
#include "nsLineLayout.h"
#include "nsHTMLIIDs.h"
#include "nsIPresContext.h"
#include "nsHTMLAtoms.h"
#include "nsUnitConversion.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsINameSpaceManager.h"
#include "nsString.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#include "nsStyleUtil.h"

#include "nsIDeviceContext.h"
#include "nsCSSRendering.h"
#include "nsQuickSort.h"

#include "nsMathMLOperators.h"
#include "nsMathMLChar.h"
#include "nslog.h"

NS_IMPL_LOG(nsMathMLCharLog)
#define PRINTF NS_LOG_PRINTF(nsMathMLCharLog)
#define FLUSH  NS_LOG_FLUSH(nsMathMLCharLog)

// List of all stretchy MathML chars with their attributes --------------
// ----------------------------------------------------------------------

struct nsCharInfo {
  PRUnichar     mUnicode;
  PRInt32       mDirection;
  nsGlyphTable* mGlyphTable; // placeholder to cache the table with the smallest glue
};

nsCharInfo gCharInfo[] = {
#define WANT_CHAR_INFO
  #include "nsMathMLCharList.h"
#undef WANT_CHAR_INFO
};

// ----------------------------------------------------------------------
// nsGlyphTable is a class that provides an interface for accessing glyphs
// of stretchy chars. It acts like a table that stores the variants of bigger
// sizes (if any) and the partial glyphs needed to build extensible symbols.
// An instance of nsGlyphTable is associated to one font.
//
// A char for which nsGlyphTable::Has(aChar) is true means that the table
// contains some glyphs (bigger and/or partial) that can be used to render
// the char. Bigger sizes (if any) of the char can then be retrieved with
// BigOf(aSize). Partial glyphs can be retrieved with TopOf(), GlueOf(), etc.
// ----------------------------------------------------------------------

// A table consists of "nsGlyphCode"s which are viewed either as Unicode
// points or as direct glyph indices, depending on the type of the table.
// XXX The latter is not yet supported.

// XXX There are many assertions in the code to ensure that callers
//     write correct/efficient/speedy code. As a rule of thumb, you should
//     ckeck for Has(aChar) prior to using a char within a glyphtable.

#define NS_TABLE_TYPE_UNICODE       0
#define NS_TABLE_TYPE_GLYPH_INDEX   1

struct nsCharData {
  nsMathMLCharEnum mEnum;
  PRInt32          mIndex;
};

class nsGlyphTable {
public:
  nsGlyphTable(PRInt32      aType,
               char*        aFontName,
               nsCharData*  aCharArray,
               PRInt32      aCharCount,
               nsGlyphCode* aGlyphArray,
               PRInt32      aGlyphCount)
  {
    mType = aType;
    mFontName.Assign(aFontName);
    mCharArray = aCharArray;
    mCharCount = aCharCount;
    mGlyphArray = aGlyphArray;
    mGlyphCount = aGlyphCount;
    mNextTable = nsnull;
    mCache = mCharArray;
    // quick sort so that we can speedily lookup the index of a char using binary search
    if (mCharArray && (mCharCount > 1)) {
      NS_QuickSort(mCharArray, mCharCount, sizeof(nsCharData), compare, nsnull);
    }
  }

  ~nsGlyphTable() // not a virtual destructor: this class is not intended to be subclassed
  {
  }

  void GetFontName(nsString& aFontName)
  {
    aFontName.AssignWithConversion(mFontName);
  }

  nsGlyphTable* GetNextTable(void)
  {
    return mNextTable;
  }

  void SetNextTable(nsGlyphTable* aNextTable)
  {
    mNextTable = aNextTable;
  }

  PRBool Has(nsMathMLCharEnum aEnum)
  {
    return PRBool(IndexOf(aEnum) < mGlyphCount);
  }

#ifdef NS_DEBUG
  // a linear search function for debug mode only
  PRBool Has(nsGlyphCode aGlyphCode)
  {
    NS_ASSERTION(aGlyphCode, "bad usage of a glyph table");
    for (PRInt32 i = 0; i < mGlyphCount; i++)
      if (mGlyphArray[i] == aGlyphCode) return PR_TRUE;
    return PR_FALSE;
  }
#endif

  nsGlyphCode  TopOf(nsMathMLCharEnum aEnum)
  {
    NS_ASSERTION(Has(aEnum), "bad usage of a glyph table");
    return mGlyphArray[IndexOf(aEnum)];
  }

  nsGlyphCode  MiddleOf(nsMathMLCharEnum aEnum)
  {
    NS_ASSERTION(IndexOf(aEnum) + 1 < mGlyphCount, "bad usage of a glyph table");
    return mGlyphArray[IndexOf(aEnum) + 1];
  }

  nsGlyphCode  BottomOf(nsMathMLCharEnum aEnum)
  {
    NS_ASSERTION(IndexOf(aEnum) + 2 < mGlyphCount, "bad usage of a glyph table");
    return mGlyphArray[IndexOf(aEnum) + 2];
  }

  nsGlyphCode  GlueOf(nsMathMLCharEnum aEnum)
  {
    NS_ASSERTION(IndexOf(aEnum) + 3 < mGlyphCount, "bad usage of a glyph table");
    return mGlyphArray[IndexOf(aEnum) + 3];
  }

  nsGlyphCode  BigOf(nsMathMLCharEnum aEnum, PRInt32 aSize)
  {
    NS_ASSERTION(aSize>=0 && IndexOf(aEnum) + 4 + aSize < mGlyphCount, "bad usage of a glyph table");
    return mGlyphArray[IndexOf(aEnum) + 4 + aSize];
  }

  nsGlyphCode  LeftOf(nsMathMLCharEnum aEnum)
  {
    NS_ASSERTION(Has(aEnum), "bad usage of a glyph table");
    return mGlyphArray[IndexOf(aEnum)];
  }

  nsGlyphCode  RightOf(nsMathMLCharEnum aEnum)
  {
    NS_ASSERTION(IndexOf(aEnum) + 2 < mGlyphCount, "bad usage of a glyph table");
    return mGlyphArray[IndexOf(aEnum) + 2];
  }

  // use these to measure/render a glyph that comes from this table
  nsresult GetBoundingMetrics(nsIRenderingContext& aRenderingContext,
                              nsGlyphCode          aGlyphCode,
                              nsBoundingMetrics&   aBoundingMetrics)
  {
    NS_ASSERTION(Has(aGlyphCode), "bad usage of a glyph table");
    // if (mType == NS_TABLE_TYPE_UNICODE)
      return aRenderingContext.GetBoundingMetrics((PRUnichar*)&aGlyphCode, PRUint32(1), aBoundingMetrics);
    // else mType == NS_TABLE_TYPE_GLYPH_INDEX
    // return NS_ERROR_NOT_IMPLEMENTED;
    // return aRenderingContext.GetBoundingMetricsI((PRUint16*)&aGlyphCode, PRUint32(1), aBoundingMetrics);
  }

  void
  DrawGlyph(nsIRenderingContext& aRenderingContext,
            nsGlyphCode          aGlyphCode,
            nscoord              aX,
            nscoord              aY,
            nsRect*              aClipRect = nsnull);

private:
  PRInt32       mType;       // NS_TABLE_TYPE_UNICODE
                             // NS_TABLE_TYPE_GLYPH_INDEX

  nsCString     mFontName;   // The name of the font associated to this table.

  PRInt32       mCharCount;
  nsCharData*   mCharArray;  // All the stretchy chars supported by the font associated to
                             // this table.

  PRInt32       mGlyphCount;
  nsGlyphCode*  mGlyphArray; // For each stretchy char in this font, this array contains
                             // the unicode points (or glyph-indices) of the partial glyphs
                             // needed to build extensible symbols, and of the readily-available
                             // variants of bigger sizes (if any) that the font already contains.

  nsGlyphTable* mNextTable;  // Pointer to another table --to enable walking/trying several fonts.

  nsCharData*   mCache;      // For speedy re-use, always cache the last char used in IndexOf() ...

  // ---------------------------
  // helper functions

  // compare function for doing a quick sort of our chars in ascending order
  static int compare( const void *a, const void *b , void *unused)
  {
    return (PRInt32)(((nsCharData*)a)->mEnum) - (PRInt32)(((nsCharData*)b)->mEnum);
  }

  // we use this a lot... to lookup the starting index (in mGlyphArray) of the
  // list of glyphs corresponding to a char. This list consists of the 4 partial
  // glyphs: top (or left), middle, bottom (or right), glue, followed by the
  // bigger sizes (if any): size0, ..., size{N-1}, *and* the null separator.
  // (Note: retrieval with BigOf(aSize) is 0-based, BigOf(0) gives the first
  // glyph size which is the normal size.)
  PRInt32 IndexOf(nsMathMLCharEnum aEnum);
};

PRInt32
nsGlyphTable::IndexOf(nsMathMLCharEnum aEnum)
{
  // quick return if it is identical with our cache ...
  if (mCache->mEnum == aEnum) return mCache->mIndex;
  // binary search ...
  PRInt32 low = 0;
  PRInt32 high = mCharCount-1;
  while (low <= high) {
    PRInt32 middle = (low + high) >> 1;
    PRInt32 result = (PRInt32)aEnum - (PRInt32)mCharArray[middle].mEnum;
    if (result == 0) {
      mCache = &mCharArray[middle]; // update our cache ...
      return mCache->mIndex;
    }
    if (result > 0)
      low = middle + 1;
    else
      high = middle - 1;
  }
  return mGlyphCount;
}

// draw a glyph in a clipped area so that we don't have hairy chars pending outside
void
nsGlyphTable::DrawGlyph(nsIRenderingContext& aRenderingContext,
                        nsGlyphCode          aGlyphCode,
                        nscoord              aX,
                        nscoord              aY,
                        nsRect*              aClipRect)
{
  NS_ASSERTION(Has(aGlyphCode), "bad usage of a glyph table");
  PRBool clipState;
  if (aClipRect) {
    aRenderingContext.PushState();
    aRenderingContext.SetClipRect(*aClipRect, nsClipCombine_kIntersect, clipState);
  }

  //if (mType == NS_TABLE_TYPE_UNICODE)
    aRenderingContext.DrawString((PRUnichar*)&aGlyphCode, PRUint32(1), aX, aY);
  //else
  //  NS_ASSERTION(0, "Error *** Not yet implemented");
    //aRenderingContext.DrawStringI((PRUint16*)&aGlyphCode, PRUint32(1), aX, aY);

  if (aClipRect) {
    aRenderingContext.PopState(clipState);
  }
}

// Class used to walk/try all the glyph tables that we have.
// Glyph tables are singly linked together through their next-table pointer.
// -----------------------------------------------------------------------------------

class nsGlyphTableList {
public:
  nsGlyphTableList(void)
  {
    mFirstTable = nsnull;
  }

  nsGlyphTable*
  FirstTable(void)
  {
    return mFirstTable;
  }

  // Create a list of glyph tables. The list will only select glyph tables that
  // are associated to <b>fonts currently installed on the system</b>...
  void Init(nsIPresContext* aPresContext,
            nsGlyphTable**  aGlyphTables);

  // Find a glyph table that has a glyph for a char.
  nsGlyphTable*
  FindTableFor(nsMathMLCharEnum aCharEnum);

  // Check for existence of a glyph table.
  PRBool
  Has(nsGlyphTable* aGlyphTable);

private:
  nsGlyphTable* mFirstTable;
};

void
nsGlyphTableList::Init(nsIPresContext* aPresContext,
                       nsGlyphTable**  aGlyphTables)
{
  nsCOMPtr<nsIDeviceContext> deviceContext;
  aPresContext->GetDeviceContext(getter_AddRefs(deviceContext));

  PRBool aliased;
  nsAutoString fontName, localName;
  nsGlyphTable* lastTable = nsnull;
  nsGlyphTable** glyphTable = aGlyphTables;

  while (*glyphTable) {
    // check for existence of the font
    (*glyphTable)->GetFontName(fontName);
    deviceContext->GetLocalFontName(fontName, localName, aliased);
    if (aliased || (NS_OK == deviceContext->CheckFontExistence(localName)))
    {
      if (mFirstTable == nsnull) {
        mFirstTable = lastTable = *glyphTable;
      }
      else {
        lastTable->SetNextTable(*glyphTable);
        lastTable = *glyphTable;
      }
    }
    else { // the font is not installed
#ifdef NS_DEBUG
      char str[50];
      localName.ToCString(str, sizeof(str));
      PRINTF("WARNING *** Missing the %s font to stretch MathML symbols!\n", str);
      PRINTF("            Why don't you install the %s font on your system for a better MathML experience!\n", str);
#endif
    }
    glyphTable++;
  }
}

nsGlyphTable*
nsGlyphTableList::FindTableFor(nsMathMLCharEnum aCharEnum)
{
  nsGlyphTable* glyphTable = mFirstTable;
  while (glyphTable) {
    if (glyphTable->Has(aCharEnum)) {
      return glyphTable;
    }
    glyphTable = glyphTable->GetNextTable();
  }
  return nsnull;
}

PRBool
nsGlyphTableList::Has(nsGlyphTable* aGlyphTable)
{
  nsGlyphTable* glyphTable = mFirstTable;
  while (glyphTable) {
    if (glyphTable == aGlyphTable) {
      return PR_TRUE;
    }
    glyphTable = glyphTable->GetNextTable();
  }
  return PR_FALSE;
}

// -----------------------------------------------------------------------------------
// Here is the global list of glyph tables that we will be using ...
nsGlyphTableList gGlyphTableList;

// And this is a dummy glyph table that we will use as a flag for gCharInfo[].mGlyphTable
nsGlyphTable gGlyphTableUNDEFINED =
nsGlyphTable(NS_TABLE_TYPE_UNICODE, "UNDEFINED",
             nsnull, 0, nsnull, 0);


// Data sets that we have ...

// -----------------------------------------------------------------------------------
// Data for stretchy chars that are supported by the Symbol font ---------------------

static nsCharData gCharDataSymbol[] = {
#define WANT_SYMBOL_DATA
#define WANT_CHAR_DATA
   #include "nsMathMLCharList.h"
#undef WANT_CHAR_DATA
#undef WANT_SYMBOL_DATA
};

static nsGlyphCode gGlyphCodeSymbol[] = {
#define WANT_SYMBOL_DATA
#define WANT_GLYPH_DATA
   #include "nsMathMLCharList.h"
#undef WANT_GLYPH_DATA
#undef WANT_SYMBOL_DATA
};

nsGlyphTable gGlyphTableSymbol =
nsGlyphTable(NS_TABLE_TYPE_UNICODE, "Symbol",
             gCharDataSymbol,  sizeof(gCharDataSymbol)  / sizeof(gCharDataSymbol[0]),
             gGlyphCodeSymbol, sizeof(gGlyphCodeSymbol) / sizeof(gGlyphCodeSymbol[0]));

// -----------------------------------------------------------------------------------
// Data for stretchy chars that are supported by the MT Extra font -------------------

static nsCharData gCharDataMTExtra[] = {
#define WANT_MTEXTRA_DATA
#define WANT_CHAR_DATA
   #include "nsMathMLCharList.h"
#undef WANT_CHAR_DATA
#undef WANT_MTEXTRA_DATA
};

static nsGlyphCode gGlyphCodeMTExtra[] = {
#define WANT_MTEXTRA_DATA
#define WANT_GLYPH_DATA
   #include "nsMathMLCharList.h"
#undef WANT_GLYPH_DATA
#undef WANT_MTEXTRA_DATA
};

nsGlyphTable gGlyphTableMTExtra = 
nsGlyphTable(NS_TABLE_TYPE_UNICODE, "MT Extra",
             gCharDataMTExtra,  sizeof(gCharDataMTExtra)  / sizeof(gCharDataMTExtra[0]),
             gGlyphCodeMTExtra, sizeof(gGlyphCodeMTExtra) / sizeof(gGlyphCodeMTExtra[0]));

// -----------------------------------------------------------------------------------
// Data for stretchy chars that are supported by TeX's CMEX font ---------------------

static nsCharData gCharDataCMEX[] = {
#define WANT_CMEX_DATA
#define WANT_CHAR_DATA
   #include "nsMathMLCharList.h"
#undef WANT_CHAR_DATA
#undef WANT_CMEX_DATA
};

static nsGlyphCode gGlyphCodeCMEX[] = {
#define WANT_CMEX_DATA
#define WANT_GLYPH_DATA
   #include "nsMathMLCharList.h"
#undef WANT_GLYPH_DATA
#undef WANT_CMEX_DATA
};

nsGlyphTable gGlyphTableCMEX =
nsGlyphTable(NS_TABLE_TYPE_UNICODE, "CMEX10",
             gCharDataCMEX,  sizeof(gCharDataCMEX)  / sizeof(gCharDataCMEX[0]),
             gGlyphCodeCMEX, sizeof(gGlyphCodeCMEX) / sizeof(gGlyphCodeCMEX[0]));

// -----------------------------------------------------------------------------------
// Data for stretchy chars that are supported by TeX's CMSY font ---------------------

static nsCharData gCharDataCMSY[] = {
#define WANT_CMSY_DATA
#define WANT_CHAR_DATA
   #include "nsMathMLCharList.h"
#undef WANT_CHAR_DATA
#undef WANT_CMSY_DATA
};

static nsGlyphCode gGlyphCodeCMSY[] = {
#define WANT_CMSY_DATA
#define WANT_GLYPH_DATA
   #include "nsMathMLCharList.h"
#undef WANT_GLYPH_DATA
#undef WANT_CMSY_DATA
};

nsGlyphTable gGlyphTableCMSY =
nsGlyphTable(NS_TABLE_TYPE_UNICODE, "CMSY10",
             gCharDataCMSY,  sizeof(gCharDataCMSY)  / sizeof(gCharDataCMSY[0]),
             gGlyphCodeCMSY, sizeof(gGlyphCodeCMSY) / sizeof(gGlyphCodeCMSY[0]));

// -----------------------------------------------------------------------------------
// Data for stretchy chars that are supported by the Math4 font ----------------------

static nsCharData gCharDataMath4[] = {
#define WANT_MATH4_DATA
#define WANT_CHAR_DATA
   #include "nsMathMLCharList.h"
#undef WANT_CHAR_DATA
#undef WANT_MATH4_DATA
};

static nsGlyphCode gGlyphCodeMath4[] = {
#define WANT_MATH4_DATA
#define WANT_GLYPH_DATA
   #include "nsMathMLCharList.h"
#undef WANT_GLYPH_DATA
#undef WANT_MATH4_DATA
};

nsGlyphTable gGlyphTableMath4 =
nsGlyphTable(NS_TABLE_TYPE_UNICODE, "Math4",
             gCharDataMath4,  sizeof(gCharDataMath4)  / sizeof(gCharDataMath4[0]),
             gGlyphCodeMath4, sizeof(gGlyphCodeMath4) / sizeof(gGlyphCodeMath4[0]));

// -----------------------------------------------------------------------------------

// All the glyph tables in order of preference ...
nsGlyphTable* gAllGlyphTables[] = {
  &gGlyphTableCMSY, // should be first to pick the single sqrt glyph therein
  &gGlyphTableCMEX,
  &gGlyphTableMTExtra,
  &gGlyphTableMath4,
  &gGlyphTableSymbol,

  nsnull
};

static PRBool gInitialized = PR_FALSE;

void InitGlobals(nsIPresContext* aPresContext)
{
  gInitialized = PR_TRUE;
  gGlyphTableList.Init(aPresContext, gAllGlyphTables);
  for (PRInt32 i = 0; i < eMathMLChar_COUNT; i++) {
    gCharInfo[i].mGlyphTable = &gGlyphTableUNDEFINED;
  }
  // let some particular chars have their preferred extension tables
  if (gGlyphTableList.Has(&gGlyphTableMTExtra)) {
    gCharInfo[eMathMLChar_OverCurlyBracket].mGlyphTable = &gGlyphTableMTExtra;
    gCharInfo[eMathMLChar_UnderCurlyBracket].mGlyphTable = &gGlyphTableMTExtra;
  }
#ifdef NS_DEBUG
  // sanity check
  for (PRInt32 j = 0; j < eMathMLChar_COUNT; j++) {
    PRUnichar cj = gCharInfo[j].mUnicode;
    for (PRInt32 k = 0; k < eMathMLChar_COUNT; k++) {
      // hitting this assertion? 
      // check nsMathMLCharList to ensure that the same Unicode point
      // is not associated to different enums
      PRUnichar ck = gCharInfo[k].mUnicode;
      NS_ASSERTION(!(cj <= ck && j > k), "nsMathMLCharList is not sorted");
      NS_ASSERTION(!(cj >= ck && j < k), "nsMathMLCharList is not sorted");
    }
  }
#endif
}


// -----------------------------------------------------------------------------------
// And now the implementation of nsMathMLChar

nsresult
nsMathMLChar::GetStyleContext(nsIStyleContext** aStyleContext) const
{
  NS_PRECONDITION(aStyleContext, "null OUT ptr");
  NS_ASSERTION(mStyleContext, "chars shoud always have style context");
  NS_IF_ADDREF(mStyleContext);
  *aStyleContext = mStyleContext;
  return NS_OK;
}

nsresult
nsMathMLChar::SetStyleContext(nsIStyleContext* aStyleContext)
{
  NS_PRECONDITION(aStyleContext, "null ptr");
  if (aStyleContext != mStyleContext) {
    NS_IF_RELEASE(mStyleContext);
    if (aStyleContext) {
      mStyleContext = aStyleContext;
      NS_ADDREF(aStyleContext);
    }
  }
  return NS_OK;
}

// If you call SetData(), it will lookup the enum
// of the char and set it for you.
void
nsMathMLChar::SetData(nsIPresContext* aPresContext,
                      nsString&       aData)
{
  if (!gInitialized) {
    InitGlobals(aPresContext);
  }
  mData = aData;
  // some assumptions until proven otherwise!
  // note that mGlyph is not initialized
  mEnum = eMathMLChar_DONT_STRETCH;
  mDirection = NS_STRETCH_DIRECTION_UNSUPPORTED;
  mBoundingMetrics.Clear();
  mGlyphTable = nsnull;
  // lookup the enum ...
  if (1 == mData.Length()) {
    PRUnichar ch = mData[0];
    PRInt32 i = eMathMLChar_COUNT;
    // binary search
    PRInt32 low = 0;
    PRInt32 high = eMathMLChar_COUNT-1;
    while (low <= high) {
      PRInt32 middle = (low + high) >> 1;
      PRInt32 result = (PRInt32)ch - (PRInt32)gCharInfo[middle].mUnicode;
      if (result == 0) {
        i = middle;
        break;
      }
      if (result > 0)
        low = middle + 1;
      else
        high = middle - 1;
    }
    if (i != eMathMLChar_COUNT) {
      // found ...
      mEnum = nsMathMLCharEnum(i);
      mDirection = gCharInfo[i].mDirection;
      if (mDirection != NS_STRETCH_DIRECTION_UNSUPPORTED) {
        // ... now see if there is a glyph table for us
        mGlyphTable = gGlyphTableList.FindTableFor(mEnum);
        // don't bother with the stretching if there is no glyph table for us...
        if (!mGlyphTable) {
          gCharInfo[i].mDirection = NS_STRETCH_DIRECTION_UNSUPPORTED; // update to never try again
          mDirection = NS_STRETCH_DIRECTION_UNSUPPORTED;
          mEnum = eMathMLChar_DONT_STRETCH;
        }
#ifdef NS_DEBUG
        // hitting this assertion? 
        // check nsMathMLCharList to ensure that enum and Unicode (of size0) match in MATHML_CHAR(index, enum, ...)
        else NS_ASSERTION(mGlyphTable->Has(nsGlyphCode(mData[0])), "Something is wrong somewhere");
#endif
      }
    }
  }
}

// If you call SetEnum(), it will lookup the actual value
// of the char and set it for you.
void
nsMathMLChar::SetEnum(nsIPresContext*  aPresContext,
                      nsMathMLCharEnum aEnum)
{
  NS_ASSERTION(aEnum < eMathMLChar_COUNT, "Something is wrong somewhere");
  if (!gInitialized) {
    InitGlobals(aPresContext);
  }
  mEnum = aEnum;
  // some assumptions until proven otherwise!
  // note that mGlyph is not initialized
  mData = nsAutoString();
  mDirection = NS_STRETCH_DIRECTION_UNSUPPORTED;
  mBoundingMetrics.Clear();
  mGlyphTable = nsnull;
  if (mEnum != eMathMLChar_DONT_STRETCH) {
    mData = gCharInfo[mEnum].mUnicode;
    mDirection = gCharInfo[mEnum].mDirection;
    if (mDirection != NS_STRETCH_DIRECTION_UNSUPPORTED) {
      // ... now see if there is a glyph table for us
      mGlyphTable = gGlyphTableList.FindTableFor(mEnum);
      // don't bother with the stretching if there is no glyph table for us...
      if (!mGlyphTable) {
        gCharInfo[mEnum].mDirection = NS_STRETCH_DIRECTION_UNSUPPORTED; // update to never try again
        mDirection = NS_STRETCH_DIRECTION_UNSUPPORTED;
        mEnum = eMathMLChar_DONT_STRETCH;
      }
#ifdef NS_DEBUG
      // hitting this assertion? 
      // check nsMathMLCharList to ensure that enum and Unicode (of size0) match in MATHML_CHAR(index, enum, ...)
      else NS_ASSERTION(mGlyphTable->Has(nsGlyphCode(mData[0])), "Something is wrong somewhere");
#endif
    }
    NS_ASSERTION(mData.Length(), "Something is wrong somewhere");
  }
}

/*
 The Stretch:
 @param aContainerSize - suggested size for the stretched char
 @param aDesiredStretchSize - IN/OUT parameter. On input
 our current size or zero if current size is unknown, on output
 the size after stretching. If no stretching is done, and the
 input was zero, the output will simply give the default size.

 How it works?
 The Stretch() method first looks for a glyph of appropriate
 size; If a glyph is found, it is cached by this object
 and its size is returned in aDesiredStretchSize. The cached
 glyph will then be used at the painting stage.

 If no glyph of appropriate size is found, aDesiredStretchSize
 is set to aContainerSize and the char is later built by
 parts, at the painting stage, to fit the size.

 * When a glyph is found, its size can actually be slighthly
   larger or smaller than aContainerSize.

 * When built by parts, the char is stretched to *exactly*
   fit aContainerSize.

 In either case, it is the responsibility of the caller
 to account for the spacing when setting aContainerSize, and
 to leave the extra spacing when placing the stretched char.
*/

//#define SHOW_BORDERS 1
//#define NOISY_SEARCH 1

// **********************************************************************
// **********************************************************************
// **********************************************************************

#undef PR_ABS
#define PR_ABS(x) ((x) < 0 ? -(x) : (x))

static PRBool
IsSizeOK(nscoord a, nscoord b, PRInt32 aHint)
{
  if (aHint == NS_STRETCH_NORMAL)
    return PRBool(float(PR_ABS(a - b)) < 0.15f * float(b));
  else if (aHint == NS_STRETCH_SMALLER)
    return PRBool((0.85f * float(b) <= float(a)) && (a <= b));
  else if (aHint == NS_STRETCH_LARGER)
    return PRBool(a >= b);
  else
    return PR_FALSE;
}

static PRBool
IsSizeBetter(nscoord a, nscoord olda, nscoord b, PRInt32 aHint)
{
  if (0 == olda)
    return PR_TRUE;
  else if (PR_ABS(a - b) < PR_ABS(olda - b))
  {
    if (aHint == NS_STRETCH_NORMAL)
      return PR_TRUE;
    else if (aHint == NS_STRETCH_SMALLER)
      return PRBool(a <= olda);
    else if (aHint == NS_STRETCH_LARGER)
      return PRBool(a >= olda);
  }
  return PR_FALSE;
}

static PRBool
FontEnumCallback(const nsString& aFamily, PRBool aGeneric, void *aData)
{
  nsAutoString* familyList = (nsAutoString*)aData;
  // XXX unreliable if aFamily is a substring of another family already in the list
  if (familyList->Find(aFamily, PR_TRUE) == kNotFound) {
    familyList->Append(PRUnichar(','));
    // XXX could enclose in quotes if weird font problems develop
    familyList->Append(aFamily);
  }
  return PR_TRUE; // don't stop
}

// re-order the font-family list of aFont to put aFamily in first position
static void SetFirstFamily(nsFont& aFont, const nsString& aFamily)
{
  // put aFamily in first position
  nsAutoString familyList(aFamily);
  // XXX hack to force CMSY10 to be at least the second best choice !
  if (!aFamily.EqualsIgnoreCase("CMSY10"))
    familyList.AppendWithConversion(",CMSY10");
  // loop over font-family: (skipping aFamily if present)
  aFont.EnumerateFamilies(FontEnumCallback, &familyList);
  // overwrite the old value of font-family:
  aFont.name.Assign(familyList);
}

nsresult
nsMathMLChar::Stretch(nsIPresContext*      aPresContext,
                      nsIRenderingContext& aRenderingContext,
                      nsStretchDirection   aStretchDirection,
                      nsBoundingMetrics&   aContainerSize,
                      nsBoundingMetrics&   aDesiredStretchSize,
                      PRInt32              aStretchHint)
{
  nsresult rv = NS_OK;
  nsStretchDirection aDirection = aStretchDirection;

  ////////////////
  // if no specified direction, attempt to stretch in our preferred direction
  if (aDirection == NS_STRETCH_DIRECTION_DEFAULT) {
    aDirection = mDirection;
  }

  ///////////////
  // Set font
  nsAutoString fontName;
  nsStyleFont font;
  mStyleContext->GetStyle(eStyleStruct_Font, font);
  if (mGlyphTable) {
    mGlyphTable->GetFontName(fontName);
    SetFirstFamily(font.mFont, fontName); // force precedence on this font
  }
  aRenderingContext.SetFont(font.mFont);

  //////////////
  // set the default bounding metrics
  rv = aRenderingContext.GetBoundingMetrics(mData.GetUnicode(),
                                            PRUint32(mData.Length()),
                                            mBoundingMetrics);
  if (NS_FAILED(rv)) {
      PRINTF ("GetBoundingMetrics failed\n");
    // ensure that the char later behaves like a normal char
    mEnum = eMathMLChar_DONT_STRETCH; // XXX need to reset in dynamic updates
    return rv;
  }

  //////////////
  // set the default desired metrics for special cases where
  // the caller doesn't know the default size
  if ((0 == aDesiredStretchSize.width) ||
      (0 == aDesiredStretchSize.ascent && 0 == aDesiredStretchSize.descent))
  {
    aDesiredStretchSize = mBoundingMetrics;
  }

  ///////////////
  // quickly return if there is nothing special about this char
  if (!mGlyphTable || 
      eMathMLChar_DONT_STRETCH == mEnum || 
      aDirection != mDirection) {
    // ensure that the char later behaves like a normal char
    mEnum = eMathMLChar_DONT_STRETCH; // XXX need to reset in dynamic updates
    return NS_OK;
  }

  // hitting these assertions?
  // make sure to clobber-build the MathML directory if nsMathMLCharList.h has changed
  NS_ASSERTION(gCharInfo[mEnum].mUnicode == mData[0], "Mystery!");
  NS_ASSERTION(gCharInfo[mEnum].mDirection == mDirection, "Mystery!");
  NS_ASSERTION(gCharInfo[mEnum].mDirection != NS_STRETCH_DIRECTION_UNSUPPORTED, "Mystery!");

  ////////////////
  // check the common situations where stretching is not actually needed
  if (aDirection == NS_STRETCH_DIRECTION_VERTICAL) {
    if (IsSizeOK(aDesiredStretchSize.ascent + aDesiredStretchSize.descent, 
                 aContainerSize.ascent + aContainerSize.descent,
                 aStretchHint)) {
      // ensure that the char later behaves like a normal char
      mEnum = eMathMLChar_DONT_STRETCH; // XXX need to reset in dynamic updates
      return NS_OK;
    }
  }
  else if (aDirection == NS_STRETCH_DIRECTION_HORIZONTAL) {
    if (IsSizeOK(aDesiredStretchSize.width,
                 aContainerSize.width,
                 aStretchHint)) {
      // ensure that the char later behaves like a normal char
      mEnum = eMathMLChar_DONT_STRETCH; // XXX need to reset in dynamic updates
      return NS_OK;
    }
  }
  else {
    NS_ASSERTION(0, "What are you doing here?");
    return NS_OK;
  }

  //////////////////
  // try first to search if there is a glyph of appropriate size ...

  PRInt32 size;
  PRBool sizeOK = PR_FALSE;
  nsBoundingMetrics bm;
  nsGlyphCode ch;

  // this will be the best glyph that we encounter during the search...
  nsGlyphCode bestGlyph = mData[0]; // XXX need to also take care of the glyph index case 
  nsGlyphTable* bestGlyphTable = mGlyphTable;
  nsBoundingMetrics bestbm = mBoundingMetrics;

#ifdef NOISY_SEARCH
  PRINTF("Searching a font with a glyph of appropriate size for: 0x%04X:%c\n",
         mData[0], mData[0]&0x00FF);
#endif
  nsGlyphTable* glyphTable = gGlyphTableList.FirstTable();
  while (glyphTable && !sizeOK) {
    if (glyphTable->Has(mEnum) && glyphTable->BigOf(mEnum, 1)) {
      // see if this table has a glyph that matches the size
      glyphTable->GetFontName(fontName);
      SetFirstFamily(font.mFont, fontName); // force precedence on this font
      aRenderingContext.SetFont(font.mFont);
#ifdef NOISY_SEARCH
      char str[50];
      fontName.ToCString(str, sizeof(str));
      PRINTF("  searching in %s ...\n", str);
#endif
      size = 1; // size=0 is the char at its normal size
      ch = glyphTable->BigOf(mEnum, size++);
      while (ch) {
        NS_ASSERTION(ch != mData[0], "glyph table incorrectly set -- duplicate found");
        rv = glyphTable->GetBoundingMetrics(aRenderingContext, ch, bm);
        if (NS_SUCCEEDED(rv)) {
          nscoord h = bm.ascent + bm.descent;
          nscoord w = bm.rightBearing - bm.leftBearing;
          if ((aDirection == NS_STRETCH_DIRECTION_VERTICAL &&
               IsSizeOK(h, aContainerSize.ascent + aContainerSize.descent, aStretchHint)) ||
              (aDirection == NS_STRETCH_DIRECTION_HORIZONTAL &&
               IsSizeOK(w, aContainerSize.width, aStretchHint)))
          {
#ifdef NOISY_SEARCH
              PRINTF("    size:%d OK!\n", size-1);
#endif
            bestbm = bm;
            bestGlyphTable = glyphTable;
            bestGlyph = ch;

            sizeOK = PR_TRUE;
            break; // get out...
          }
          if ((aDirection == NS_STRETCH_DIRECTION_VERTICAL &&
               IsSizeBetter(h, bestbm.ascent + bestbm.descent,
                            aContainerSize.ascent + aContainerSize.descent, aStretchHint)) ||
              (aDirection == NS_STRETCH_DIRECTION_HORIZONTAL &&
               IsSizeBetter(w, bestbm.rightBearing - bestbm.leftBearing,
                            aContainerSize.width, aStretchHint)))
          {
#ifdef NOISY_SEARCH
              PRINTF("    size:%d Current best\n", size-1);
#endif
            bestGlyphTable = glyphTable;
            bestGlyph = ch;
            bestbm = bm;
          }
#ifdef NOISY_SEARCH
          else {
              PRINTF("    size:%d Rejected!\n", size-1);
          }
#endif
        }
        ch = glyphTable->BigOf(mEnum, size++);
      }
    }
    glyphTable = glyphTable->GetNextTable();
  }

  //////////////////
  // if no glyph of appropriate size, see if we can build by parts...
  // search for the table with the smallest glue

  if (!sizeOK) {
#ifdef NOISY_SEARCH
      PRINTF("  searching for the font with the smallest glue\n");
#endif
    if (gCharInfo[mEnum].mGlyphTable == &gGlyphTableUNDEFINED) { // first time
      // gCharInfo[mEnum].mGlyphTable is not yet initialized, scan the global list
      nscoord lengthGlue = 0;
      gCharInfo[mEnum].mGlyphTable = nsnull; // clear the first time flag
      glyphTable = gGlyphTableList.FirstTable();
      while (glyphTable) {
        if (glyphTable->Has(mEnum) && glyphTable->GlueOf(mEnum)) {
          glyphTable->GetFontName(fontName);
          SetFirstFamily(font.mFont, fontName); // force precedence on this font
          aRenderingContext.SetFont(font.mFont);
          ch = glyphTable->GlueOf(mEnum);
          rv = glyphTable->GetBoundingMetrics(aRenderingContext, ch, bm);
          if (NS_SUCCEEDED(rv)) {
            nscoord length = (aDirection == NS_STRETCH_DIRECTION_VERTICAL)
                           ? bm.ascent + bm.descent
                           : bm.rightBearing - bm.leftBearing;
            NS_ASSERTION(length, "glue should never be zero *** Erroneous glue in glyph table!");
            if (IsSizeBetter(length, lengthGlue, 0, NS_STRETCH_SMALLER)) {
              // current glyph table is the one with the smallest glue, update the cache...
              gCharInfo[mEnum].mGlyphTable = glyphTable;
              lengthGlue = length;
#ifdef NOISY_SEARCH
              char str[50];
              fontName.ToCString(str, sizeof(str));
              PRINTF("    %s glue:%d Current best\n", str, lengthGlue);
#endif
            }
#ifdef NOISY_SEARCH
            else {
              char str[50];
              fontName.ToCString(str, sizeof(str));
              PRINTF("    %s glue:%d Rejected!\n", str, length);
            }
#endif
          }
        }
        glyphTable = glyphTable->GetNextTable();
      }
#ifdef NOISY_SEARCH
      if (gCharInfo[mEnum].mGlyphTable) {
        gCharInfo[mEnum].mGlyphTable->GetFontName(fontName);
        char str[50];
        fontName.ToCString(str, sizeof(str));
        PRINTF("    Found %s in the global list\n", str);
      }
#endif
    }
#ifdef NOISY_SEARCH
    if (gCharInfo[mEnum].mGlyphTable) {
      gCharInfo[mEnum].mGlyphTable->GetFontName(fontName);
      char str[50];
      fontName.ToCString(str, sizeof(str));
      PRINTF("    Found %s in the cache\n", str);
    }
    else {
        PRINTF("    no font found\n");
    }
#endif
  }

  if (sizeOK || !gCharInfo[mEnum].mGlyphTable) {
    // if we enter here, it means either that the best glyph encountered
    // is of appropriate size, or that it is not possible to build by
    // parts (i.e., a font with a glue wasn't found).
    // so return the best glyph that we have
  }
  else {
    // we are going to build by parts...
    // using the glyph table with the smallest glue
    glyphTable = gCharInfo[mEnum].mGlyphTable;
    glyphTable->GetFontName(fontName);
    SetFirstFamily(font.mFont, fontName); // force precedence on this font
    aRenderingContext.SetFont(font.mFont);

    PRInt32 i;

    // compute the bounding metrics of all partial glyphs
    nsGlyphCode chdata[4];
    nsBoundingMetrics bmdata[4];
    for (i = 0; i < 4; i++) {
      switch (i) {
        case 0: ch = glyphTable->TopOf(mEnum);    break;
        case 1: ch = glyphTable->MiddleOf(mEnum); break;
        case 2: ch = glyphTable->BottomOf(mEnum); break;
        case 3: ch = glyphTable->GlueOf(mEnum);   break;
      }
      if (!ch) ch = glyphTable->GlueOf(mEnum); // empty slots are filled with the glue
      rv = glyphTable->GetBoundingMetrics(aRenderingContext, ch, bm);
      if (NS_FAILED(rv)) {
          PRINTF("GetBoundingMetrics failed for %04X:%c\n", ch, ch&0x00FF);
        // stop if we failed to compute the bounding metrics of a part.
        // we will use the best glyph encountered earlier
        break;
      }
      chdata[i] = ch;
      bmdata[i] = bm;
    }

    // build by parts if we have successfully computed the
    // bounding metrics of all partial glyphs
    if (NS_SUCCEEDED(rv)) {
      // We want to place the glyphs even if they don't fit at their
      // full extent, i.e., we may clip to tolerate a small amount of
      // overlap between the parts. This is important to cater for fonts
      // with long glues.
      float flex[3] = {0.7f, 0.3f, 0.7f};
      // refine the flexibility depending on whether some parts are not there
      if ((chdata[1] == chdata[0]) || // mid == top (or mid == left) 
          (chdata[1] == chdata[2]) || // mid == bot (or mid == right)
          (chdata[1] == chdata[3]))   // mid == glue
      {
        flex[0] = 0.5f;
        flex[1] = 0.0f;
        flex[2] = 0.5f;
      }
  
      if (aDirection == NS_STRETCH_DIRECTION_VERTICAL) {
        // default is to fill-up the area given to us
        nscoord lbearing = bmdata[0].leftBearing;
        nscoord rbearing = bmdata[0].rightBearing;
        nscoord h = 0;
        nscoord w = bmdata[0].width;
        for (i = 0; i < 4; i++) {
          bm = bmdata[i];
          if (w < bm.width) w = bm.width;
          if (i < 3) {
            h += nscoord(flex[i]*(bm.ascent + bm.descent)); // sum heights of the parts...
            // compute and cache our bounding metrics (vertical stacking)
            if (lbearing > bm.leftBearing) lbearing = bm.leftBearing;
            if (rbearing < bm.rightBearing) rbearing = bm.rightBearing;
          }
        }
        if (h <= aContainerSize.ascent + aContainerSize.descent) { 
          // can nicely fit in the available space...
          bestbm.width = w;
          bestbm.ascent = aContainerSize.ascent;
          bestbm.descent = aContainerSize.descent;
          bestbm.leftBearing = lbearing;
          bestbm.rightBearing = rbearing;
          // reset
          bestGlyph = 0; // this will tell paint to build by parts
          bestGlyphTable = glyphTable;
        }
        else {
          // sum of parts doesn't fit in the space... we will use a single glyph
          // we will use the best glyph encountered earlier
        }
      }
      else if (aDirection == NS_STRETCH_DIRECTION_HORIZONTAL) {
        // default is to fill-up the area given to us
        nscoord a = bmdata[0].ascent;
        nscoord d = bmdata[0].descent;
        nscoord w = 0;
        for (i = 0; i < 4; i++) {
          bm = bmdata[i];
          if (a < bm.ascent) a = bm.ascent;
          if (d < bm.descent) d = bm.descent;
          if (i < 3) {
            w += nscoord(flex[i]*(bm.rightBearing - bm.leftBearing)); // sum widths of the parts...
          }
        }
        if (w <= aContainerSize.width) { 
          // can nicely fit in the available space...
          bestbm.width = aContainerSize.width;
          bestbm.ascent = a;
          bestbm.descent = d;
          bestbm.leftBearing = 0;
          bestbm.rightBearing = aContainerSize.width;
          // reset
          bestGlyph = 0; // this will tell paint to build by parts
          bestGlyphTable = glyphTable;
        }
        else {
          // sum of parts doesn't fit in the space... we will use a single glyph
          // we will use the best glyph encountered earlier
        }
      }
    }
  }

  if (bestGlyph == mData[0]) { // nothing happened
    // ensure that the char behaves like a normal char
    mEnum = eMathMLChar_DONT_STRETCH; // XXX need to reset in dynamic updates
  }
  else {
    // will stretch
    mGlyph = bestGlyph; // note that this can be 0 in order to tell paint to build by parts
    mGlyphTable = bestGlyphTable;
    mBoundingMetrics = bestbm;
    aDesiredStretchSize = bestbm;
  }
  return NS_OK;
}

nsresult
nsMathMLChar::Paint(nsIPresContext*      aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect&        aDirtyRect,
                    nsFramePaintLayer    aWhichLayer,
                    nsIFrame*            aForFrame)
{
  nsStyleDisplay display;
  nsStyleColor color;
  mStyleContext->GetStyle(eStyleStruct_Display, display);
  mStyleContext->GetStyle(eStyleStruct_Color, color);

  if (display.IsVisible() && NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer)
  {
    if (mRect.width && mRect.height) {
      // Paint our background and border
      PRIntn skipSides = 0; //aForFrame->GetSkipSides();
      nsStyleSpacing spacing;
      mStyleContext->GetStyle(eStyleStruct_Spacing, spacing);

      nsRect  rect(mRect); //0, 0, mRect.width, mRect.height);
      nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, aForFrame,
                                      aDirtyRect, rect, color, spacing, 0, 0);
      nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, aForFrame,
                                  aDirtyRect, rect, spacing, mStyleContext, skipSides);
      nsCSSRendering::PaintOutline(aPresContext, aRenderingContext, aForFrame,
                                   aDirtyRect, rect, spacing, mStyleContext, 0);
    }
  }

  if (display.IsVisible() && NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer)
  {
    // Set color and font ...
    nsStyleFont font;
    mStyleContext->GetStyle(eStyleStruct_Font, font);
    aRenderingContext.SetColor(color.mColor);

    if (mGlyphTable) {
      nsAutoString fontName;
      mGlyphTable->GetFontName(fontName);
      SetFirstFamily(font.mFont, fontName); // force precedence on this font
    }
    aRenderingContext.SetFont(font.mFont);

    // grab some metrics that will help to adjust the placements ...
    nscoord fontAscent;
    nsCOMPtr<nsIFontMetrics> fm;
    aRenderingContext.GetFontMetrics(*getter_AddRefs(fm));
    fm->GetMaxAscent(fontAscent);

    if (eMathMLChar_DONT_STRETCH == mEnum || NS_STRETCH_DIRECTION_UNSUPPORTED == mDirection) {
      // normal drawing if there is nothing special about this char ...
        //PRINTF("Painting %04X like a normal char\n", mData[0]);
//aRenderingContext.SetColor(NS_RGB(255,0,0));
      aRenderingContext.DrawString(mData.GetUnicode(), PRUint32(mData.Length()), 
                                   mRect.x, 
                                   mRect.y - (fontAscent - mBoundingMetrics.ascent));
    }
    else if (0 < mGlyph) { // wow, there is a glyph of appropriate size!
        //PRINTF("Painting %04X with a glyph of appropriate size\n", mData[0]);
//aRenderingContext.SetColor(NS_RGB(0,0,255));
      mGlyphTable->DrawGlyph(aRenderingContext, mGlyph,
                             mRect.x,
                             mRect.y - (fontAscent - mBoundingMetrics.ascent));
    }
    else { // paint by parts
//aRenderingContext.SetColor(NS_RGB(0,255,0));
      if (NS_STRETCH_DIRECTION_VERTICAL == mDirection)
        return PaintVertically(aPresContext, aRenderingContext, fontAscent,
                               mStyleContext, mGlyphTable, mEnum, mRect);
      else if (NS_STRETCH_DIRECTION_HORIZONTAL == mDirection)
        return PaintHorizontally(aPresContext, aRenderingContext, fontAscent,
                                 mStyleContext, mGlyphTable, mEnum, mRect);
    }
  }
  return NS_OK;
}

/* =================================================================================
  And now the helper routines that actually do the job of painting the char by parts
 */

// paint a stretchy char by assembling glyphs vertically
nsresult
nsMathMLChar::PaintVertically(nsIPresContext*      aPresContext,
                              nsIRenderingContext& aRenderingContext,
                              nscoord              aFontAscent,
                              nsIStyleContext*     aStyleContext,
                              nsGlyphTable*        aGlyphTable,
                              nsMathMLCharEnum     aCharEnum,
                              nsRect               aRect)
{
  nsresult rv = NS_OK;
  nsRect clipRect;
  nscoord dx, dy;

  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);
  nscoord onePixel = NSIntPixelsToTwips(1, p2t);

  // get metrics data to be re-used later
  PRInt32 i;
  nsGlyphCode ch, chdata[4];
  nsBoundingMetrics bm, bmdata[4];
  nscoord stride, offset[3], start[3], end[3];
  for (i = 0; i < 4; i++) {
    switch (i) {
      case 0: ch = aGlyphTable->TopOf(aCharEnum);    break;
      case 1: ch = aGlyphTable->MiddleOf(aCharEnum); break;
      case 2: ch = aGlyphTable->BottomOf(aCharEnum); break;
      case 3: ch = aGlyphTable->GlueOf(aCharEnum);   break;
    }
    if (!ch) ch = aGlyphTable->GlueOf(aCharEnum);
    rv = aGlyphTable->GetBoundingMetrics(aRenderingContext, ch, bm);
    if (NS_FAILED(rv)) {
        PRINTF("GetBoundingMetrics failed for %04X:%c\n", ch, ch&0x00FF);
      return rv;
    }
    chdata[i] = ch;
    bmdata[i] = bm;
  }
  dx = aRect.x;
  for (i = 0; i < 3; i++) {
    ch = chdata[i];
    bm = bmdata[i];
    if (0 == i) { // top
      dy = aRect.y - aFontAscent + bm.ascent;
    }
    else if (1 == i) { // middle
      dy = aRect.y - aFontAscent + bm.ascent + 
           (aRect.height - (bm.ascent + bm.descent))/2;
    }
    else if (2 == i) { // bottom
      dy = aRect.y - aFontAscent + aRect.height - bm.descent;
    }
    // abcissa passed to DrawString
    offset[i] = dy;
    // *exact* abcissa where the *top-most* pixel of the glyph is painted
    start[i] = dy + aFontAscent - bm.ascent;
    // *exact* abcissa where the *bottom-most* pixel of the glyph is painted
    end[i] = dy + aFontAscent + bm.descent; // end = start + height
  }

  /////////////////////////////////////
  // draw top, middle, bottom
  for (i = 0; i < 3; i++) {
    ch = chdata[i];
#ifdef SHOW_BORDERS
    // bounding box of the part
    aRenderingContext.SetColor(NS_RGB(0,0,0));
    aRenderingContext.DrawRect(nsRect(dx,start[i],aRect.width+30*(i+1),end[i]-start[i]));
#endif
    dy = offset[i];
         if (i==0) clipRect = nsRect(dx, aRect.y, aRect.width, aRect.height);
    else if (i==1) clipRect = nsRect(dx, end[0], aRect.width, start[2]-end[0]);
    else if (i==2) clipRect = nsRect(dx, start[2], aRect.width, end[2]-start[2]);

    if (!clipRect.IsEmpty()) {
      clipRect.Inflate(onePixel, onePixel);
      aGlyphTable->DrawGlyph(aRenderingContext, ch, dx, dy, &clipRect);
    }
  }

  ///////////////
  // fill the gap between top and middle, and between middle and bottom.
  ch = aGlyphTable->GlueOf(aCharEnum);
  for (i = 0; i < 2; i++) {
    PRInt32 count = 0;
    dy = offset[i];
    clipRect = nsRect(dx, end[i], aRect.width, start[i+1]-end[i]);
    clipRect.Inflate(onePixel, onePixel);
#ifdef SHOW_BORDERS
    // exact area to fill
    aRenderingContext.SetColor(NS_RGB(255,0,0));
    aRenderingContext.DrawRect(clipRect);
#endif
    bm = bmdata[i];
    while (dy + aFontAscent + bm.descent < start[i+1]) {
      if (2 > count) {
        stride = bm.descent;
        bm = bmdata[3]; // glue
        stride += bm.ascent;
      }
      count++;
      dy += stride;
      aGlyphTable->DrawGlyph(aRenderingContext, ch, dx, dy, &clipRect);
//    NS_ASSERTION(5000 == count, "Error - glyph table is incorrectly set");
      if (1000 == count) return NS_ERROR_UNEXPECTED;
    }
#ifdef SHOW_BORDERS
    // last glyph that may cross past its boundary and collide with the next
    nscoord height = bm.ascent + bm.descent;
    aRenderingContext.SetColor(NS_RGB(0,255,0));
    aRenderingContext.DrawRect(nsRect(dx, dy+aFontAscent-bm.ascent, aRect.width, height));
#endif
  }
  return NS_OK;
}

// paint a stretchy char by assembling glyphs horizontally
nsresult
nsMathMLChar::PaintHorizontally(nsIPresContext*      aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                nscoord              aFontAscent,
                                nsIStyleContext*     aStyleContext,
                                nsGlyphTable*        aGlyphTable,
                                nsMathMLCharEnum     aCharEnum,
                                nsRect               aRect)
{
  nsresult rv = NS_OK;
  nsRect clipRect;
  nscoord dx, dy;

  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);
  nscoord onePixel = NSIntPixelsToTwips(1, p2t);

  // get metrics data to be re-used later
  PRInt32 i;
  nsGlyphCode ch, chdata[4];
  nsBoundingMetrics bm, bmdata[4];
  nscoord ascent, stride, offset[3], start[3], end[3];
  for (i = 0; i < 4; i++) {
    switch (i) {
      case 0: ch = aGlyphTable->LeftOf(aCharEnum);   break;
      case 1: ch = aGlyphTable->MiddleOf(aCharEnum); break;
      case 2: ch = aGlyphTable->RightOf(aCharEnum);  break;
      case 3: ch = aGlyphTable->GlueOf(aCharEnum);   break;
    }
    if (!ch) ch = aGlyphTable->GlueOf(aCharEnum);
    rv = aGlyphTable->GetBoundingMetrics(aRenderingContext, ch, bm);
    if (NS_FAILED(rv)) {
        PRINTF("GetBoundingMetrics failed for %04X:%c\n", ch, ch&0x00FF);
      return rv;
    }
    chdata[i] = ch;
    bmdata[i] = bm;
    if (0 == i || ascent < bm.ascent) ascent = bm.ascent;
  }
  dy = aRect.y - aFontAscent + ascent;
  for (i = 0; i < 3; i++) {
    ch = chdata[i];
    bm = bmdata[i];
    if (0 == i) { // left
      dx = aRect.x - bm.leftBearing;
    }
    else if (1 == i) { // middle
      dx = aRect.x + (aRect.width - bm.width)/2;
    }
    else if (2 == i) { // right
      dx = aRect.x + aRect.width - bm.rightBearing;
    }
    // abcissa that DrawString used
    offset[i] = dx;
    // *exact* abcissa where the *left-most* pixel of the glyph is painted
    start[i] = dx + bm.leftBearing;
    // *exact* abcissa where the *right-most* pixel of the glyph is painted
    end[i] = dx + bm.rightBearing; // note: end = start + width
  }

  ///////////////////////////
  // draw left, middle, right
  for (i = 0; i < 3; i++) {
    ch = chdata[i];
#ifdef SHOW_BORDERS
    aRenderingContext.SetColor(NS_RGB(0,0,0));
    aRenderingContext.DrawRect(nsRect(start[i], aRect.y, end[i]-start[i], aRect.height+30*(i+1)));
#endif
    dx = offset[i];
         if (i==0) clipRect = nsRect(dx, aRect.y, aRect.width, aRect.height);
    else if (i==1) clipRect = nsRect(end[0], aRect.y, start[2]-end[0], aRect.height);
    else if (i==2) clipRect = nsRect(start[2], aRect.y, end[2]-start[2], aRect.height);

    if (!clipRect.IsEmpty()) {
      clipRect.Inflate(onePixel, onePixel);
      aGlyphTable->DrawGlyph(aRenderingContext, ch, dx, dy, &clipRect);
    }
  }

  ////////////////
  // fill the gap between left and middle, and between middle and right.
  ch = aGlyphTable->GlueOf(aCharEnum);
  for (i = 0; i < 2; i++) {
    PRInt32 count = 0;
    dx = offset[i];
    clipRect = nsRect(end[i], aRect.y, start[i+1]-end[i], aRect.height);
    clipRect.Inflate(onePixel, onePixel);
#ifdef SHOW_BORDERS
    // rectangles in-between that are to be filled
    aRenderingContext.SetColor(NS_RGB(255,0,0));
    aRenderingContext.DrawRect(clipRect);
#endif
    bm = bmdata[i];
    while (dx + bm.rightBearing < start[i+1]) {
      if (2 > count) {
        stride = bm.rightBearing;
        bm = bmdata[3]; // glue
        stride -= bm.leftBearing;
      }
      count++;
      dx += stride;
      aGlyphTable->DrawGlyph(aRenderingContext, ch, dx, dy, &clipRect);
//    NS_ASSERTION(5000 == count, "Error - glyph table is incorrectly set");
      if (1000 == count) return NS_ERROR_UNEXPECTED;
    }
#ifdef SHOW_BORDERS
    // last glyph that may cross past its boundary and collide with the next
    nscoord width = bm.rightBearing - bm.leftBearing;
    aRenderingContext.SetColor(NS_RGB(0,255,0));
    aRenderingContext.DrawRect(nsRect(dx + bm.leftBearing, aRect.y, width, aRect.height));
#endif
  }
  return NS_OK;
}
