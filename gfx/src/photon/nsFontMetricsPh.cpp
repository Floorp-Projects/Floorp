/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "xp_core.h"
#include "nsQuickSort.h"
#include "nsFontMetricsPh.h"
#include "nsPhGfxLog.h"

#include <errno.h>

static int gGotAllFontNames = 0;

// XXX many of these statics need to be freed at shutdown time
static PLHashTable* gFamilies = nsnull;

static PLHashTable* gFamilyNames = nsnull;

static NS_DEFINE_IID(kIFontMetricsIID, NS_IFONT_METRICS_IID);

nsFontMetricsPh :: nsFontMetricsPh()
{
  NS_INIT_REFCNT();
  mDeviceContext = nsnull;
  mFont = nsnull;
	  
  mHeight = 0;
  mAscent = 0;
  mDescent = 0;
  mLeading = 0;
  mEmHeight = 0;
  mEmAscent = 0;
  mEmDescent = 0;
  mMaxHeight = 0;
  mMaxAscent = 0;
  mMaxDescent = 0;
  mMaxAdvance = 0;
  mXHeight = 0;
  mSuperscriptOffset = 0;
  mSubscriptOffset = 0;
  mStrikeoutSize = 0;
  mStrikeoutOffset = 0;
  mUnderlineSize = 0;
  mUnderlineOffset = 0;
  mSpaceWidth = 0;
}
  
nsFontMetricsPh :: ~nsFontMetricsPh()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::~nsFontMetricsPh Destructor called\n"));

  if (nsnull != mFont)
  {
    delete mFont;
    mFont = nsnull;
  }

  mDeviceContext = nsnull;
}

NS_IMPL_ISUPPORTS1(nsFontMetricsPh, nsIFontMetrics)

NS_IMETHODIMP
nsFontMetricsPh :: Init ( const nsFont& aFont, nsIAtom* aLangGroup,
                          nsIDeviceContext* aContext )
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::Init with nsFont\n"));

  NS_ASSERTION(!(nsnull == aContext), "attempt to init fontmetrics with null device context");

  nsAutoString  firstFace;
  char          *str = nsnull;
  nsresult      result;  	
  nsresult      ret_code = NS_ERROR_FAILURE;
  int           MAX_FONTDETAIL = 50;
  FontDetails   fDetails[MAX_FONTDETAIL];
  int           fontcount;
  int           index;
  PhRect_t      extent;


	result = aContext->FirstExistingFont(aFont, firstFace);

	str = firstFace.ToNewCString();
	PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::Init with nsFont: after FirstExistingFont, firstFace = <%s> %d\n", str, result));

	if (NS_OK != result)
	{
		aFont.GetFirstFamily(firstFace);
		char *str = nsnull;
		str = firstFace.ToNewCString();
		PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::Init with nsFont: after GetFirstFamily = <%s>\n", str));
		delete [] str;
	}

	if (!str || !str[0])
	{
		delete [] str;
		str = strdup("serif");
	}

	PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::Init with nsFont: aFont.name = <%s>\n", str));
	PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::Init with nsFont: aFont.weight = <%d>\n", aFont.weight));
	PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::Init with nsFont: aFont.style = <%d>\n", aFont.style));
	PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::Init with nsFont: aFont.size = <%d>\n", aFont.size));

	mFont = new nsFont(aFont);
	mLangGroup = aLangGroup;
	mDeviceContext = (nsDeviceContextPh *) aContext;

	float app2dev, app2twip,scale;

	mDeviceContext->GetAppUnitsToDevUnits(app2dev);
	mDeviceContext->GetDevUnitsToTwips(app2twip);
	mDeviceContext->GetCanonicalPixelScale(scale);

	PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::Init with nsFont: app2dev = <%f>\n", app2dev));
	PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::Init with nsFont: app2twip = <%f>\n", app2twip));
	PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::Init with nsFont: scale = <%f>\n", scale));

	app2twip *= (app2dev * scale);

	PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::Init with nsFont: 2 app2twip = <%f>\n", app2twip));

	/* Build the Photon Font Name */

	// this interesting bit of code rounds the font size off to the floor point value
	// this is necessary for proper font scaling under windows
	PRInt32 sizePoints = NSTwipsToFloorIntPoints(nscoord(mFont->size * app2twip));
  
	char NSFontName[64];	/* Local buffer to keep the fontname in */
	char NSFontSuffix[5];
	char NSFullFontName[MAX_FONT_TAG];
    	
	NSFontSuffix[0] = nsnull;

	unsigned int uiFlags = 0L;

	if(aFont.weight > NS_FONT_WEIGHT_NORMAL)
		uiFlags |= PF_STYLE_BOLD;

	if(aFont.style & NS_FONT_STYLE_ITALIC)
		uiFlags |= PF_STYLE_ITALIC;

	if(aFont.style & NS_FONT_STYLE_OBLIQUE)
		uiFlags |= PF_STYLE_ANTIALIAS;

	if(PfGenerateFontName((const uchar_t *)str, uiFlags, sizePoints, (uchar_t *)NSFullFontName) == NULL)
	{
		NS_WARNING("nsFontMetricsPh::Init Name generate failed");
		printf("nsFontMetricsPh::Init Name generate failed:  %s, %d, %d\n", str, uiFlags, sizePoints);
		if (PfGenerateFontName((const uchar_t *)"Courier 10 Pitch BT", uiFlags, sizePoints, (uchar_t *)NSFullFontName) == NULL)
		{
  		  NS_ASSERTION(0,"nsFontMetricsPh::Init Name generate failed for default font\n");
		  printf(" nsFontMetricsPh::Init Name generate failed for default font:  %s, %d, %d\n", str, uiFlags, sizePoints);
 		}
	}

	/* Once the Photon Font String is built get the attributes */
	FontQueryInfo fontInfo;
	int ret;
	PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::Init with nsFont: Lookup the Photon Font Name for <%s>\n", NSFullFontName));

	ret = PfQueryFont(NSFullFontName, &fontInfo);
	if (ret < 0)  
	{
		PR_LOG(PhGfxLog, PR_LOG_ERROR, ("nsFontMetricsPh::Init with nsFont: FontQueryInfo call failed! errno=%d\n", errno));
	}
	else
	{
	  float f;
		mDeviceContext->GetDevUnitsToAppUnits(f);
		PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::Init with nsFont: Internal Name <%s> f=<%f>\n",fontInfo.font, f));
  //mFontHandle = nsString( (const PRUnichar *) NSFullFontName, strlen(NSFullFontName));
  mFontHandle = strdup(NSFullFontName); /* memory leak */

		/* These are in pixels and need to be converted! */
		float height;
		height = fontInfo.descender - fontInfo.ascender + 1.0;
		mEmHeight = mHeight                = nscoord(height * f);
		mEmAscent = mMaxAscent = mAscent   = nscoord(fontInfo.ascender * f * -1.0); // HACK
		mEmDescent = mMaxDescent = mDescent = nscoord(fontInfo.descender * f);
		mMaxAdvance            = nscoord(fontInfo.width * f);  /* max width */
		
        /***** Get the width of a space *****/
        PfExtentText(&extent, NULL, NSFullFontName, " ", 1);
//PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::Init with nsFont mSpaceWidth=<%d> font=<%s> f=<%f> \n", (extent.lr.x - extent.ul.x + 1),  NSFullFontName, f ));
        mSpaceWidth = (int) ((extent.lr.x - extent.ul.x + 1) * f);

		/****** stolen from GTK *******/
		// 56% of ascent, best guess for non-true type
		mXHeight = NSToCoordRound((float) fontInfo.ascender * f * 0.56f * -1.0);
	
		mUnderlineOffset   = -NSToIntRound(PR_MAX (1, floor (0.1 * height + 0.5)) * f);
		mUnderlineSize     =  NSToIntRound(PR_MAX(1, floor (0.05 * height + 0.5)) * f);
		mStrikeoutOffset   =  NSToIntRound((mAscent + 1) / 2.5);
		mStrikeoutSize     =  mUnderlineSize;
		mSuperscriptOffset =  mXHeight;
		mSubscriptOffset   =  mXHeight;
		mLeading           =  0;

		PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::Init with nsFont: Font Metrics for <%s> mHeight=<%d> mMaxAscent=<%d> mMaxDescent=<%d> mMaxAdvance=<%d>\n",
		  fontInfo.font, mHeight, mMaxAscent, mMaxDescent, mMaxAdvance));
	}

	delete [] str;
	return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPh :: Destroy()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::Destroy\n"));

  mDeviceContext = nsnull;
  return NS_OK;
}

static void
apGenericFamilyToFont(const nsString& aGenericFamily,
                       nsIDeviceContext* aDC,
                       nsString& aFontFace)
{
  char *str = aGenericFamily.ToNewCString();
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::MapGenericFamilyToFont  GenericFamily = <%s> - Not Implemented\n", str));
  delete [] str;
}

struct FontEnumData
{
  FontEnumData(nsIDeviceContext* aContext, char* aFaceName)
  {
    mContext = aContext;
    mFaceName = aFaceName;
  }
  nsIDeviceContext* mContext;
  char* mFaceName;
};

static PRBool
FontEnumCallback(const nsString& aFamily, PRBool aGeneric, void *aData)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::FontEnumCallback - Not Implemented\n"));

  return PR_TRUE;
}

void
nsFontMetricsPh::RealizeFont()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::RealizeFont - Not Implemented\n"));
}

NS_IMETHODIMP
nsFontMetricsPh :: GetXHeight(nscoord& aResult)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::GetXHeight - Not Implemented\n"));
  aResult = mXHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPh :: GetSuperscriptOffset(nscoord& aResult)
{
PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::GetSuperscriptOffset mSuperscriptOffset=<%d> - Not Implemented\n", mSuperscriptOffset));
  aResult = mSuperscriptOffset;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPh :: GetSubscriptOffset(nscoord& aResult)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::GetSubscriptOffset mSubscriptOffset=<%d> - Not Implemented\n", mSubscriptOffset));
  aResult = mSubscriptOffset;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPh :: GetStrikeout(nscoord& aOffset, nscoord& aSize)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::GetStrikeout  mStrikeoutOffset=<%d> mStrikeoutSize=<%d>\n",mStrikeoutOffset, mStrikeoutSize));

  aOffset = mStrikeoutOffset;
  aSize = mStrikeoutSize;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPh :: GetUnderline(nscoord& aOffset, nscoord& aSize)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::GetUnderline mUnderlineSize=<%d> mUnderlineOffset=<%d>\n",mUnderlineSize, mUnderlineOffset));

  aOffset = mUnderlineOffset;
  aSize = mUnderlineSize;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPh :: GetHeight(nscoord &aHeight)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::GetHeight mHeight=<%d>\n", mHeight));
  aHeight = mHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPh ::GetNormalLineHeight(nscoord &aHeight)
{
  aHeight = mEmHeight + mLeading;
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::GetNormalLineHeight aHeight=<%d>\n", aHeight));
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPh :: GetLeading(nscoord &aLeading)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::GetLeading mLeading=<%d>- Not Implemented\n", mLeading));
  aLeading = mLeading;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsPh::GetEmHeight(nscoord &aHeight)
{
  aHeight = mEmHeight;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsPh::GetEmAscent(nscoord &aAscent)
{
  aAscent = mEmAscent;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsPh::GetEmDescent(nscoord &aDescent)
{
  aDescent = mEmDescent;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsPh::GetMaxHeight(nscoord &aHeight)
{
  aHeight = mMaxHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPh :: GetMaxAscent(nscoord &aAscent)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::GetMaxAscent <%d>\n", mMaxAscent));
  aAscent = mMaxAscent;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPh :: GetMaxDescent(nscoord &aDescent)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::GetMaxDescent <%d>\n", mMaxDescent));
  aDescent = mMaxDescent;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPh :: GetMaxAdvance(nscoord &aAdvance)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::GetMaxAdvance <%d>\n", mMaxAdvance));
  aAdvance = mMaxAdvance;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPh :: GetFont(const nsFont *&aFont)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::GetFont\n"));
  aFont = mFont;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsPh::GetLangGroup(nsIAtom** aLangGroup)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::GetLangGroup this=<%p> aLangGroup=<%p>\n", this, aLangGroup));

  if (!aLangGroup) {
    return NS_ERROR_NULL_POINTER;
  }

  *aLangGroup = mLangGroup;
  NS_IF_ADDREF(*aLangGroup);

  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPh::GetFontHandle(nsFontHandle &aHandle)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::GetFontHandle mFontHandle=<%s>\n", mFontHandle));
  aHandle = (nsFontHandle) mFontHandle;
  return NS_OK;
}

nsresult
nsFontMetricsPh::GetSpaceWidth(nscoord &aSpaceWidth)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::GetSpaceWidth mSpaceWidth=<%d>\n", mSpaceWidth));
  aSpaceWidth = mSpaceWidth;
  return NS_OK;
}

struct nsFontFamily
{
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  PLHashTable* mCharSets;
};



#undef DEBUG_DUMP_TREE
#define DEBUG_DUMP_TREE
#ifdef DEBUG_DUMP_TREE

static char* gDumpStyles[3] = { "normal", "italic", "oblique" };

static PRIntn
DumpCharSet(PLHashEntry* he, PRIntn i, void* arg)
{
  printf("        %s\n", (char*) he->key);
#if 0
  nsFontCharSet* charSet = (nsFontCharSet*) he->value;
  for (int sizeIndex = 0; sizeIndex < charSet->mSizesCount; sizeIndex++)
  {
    nsFontGTK* size = &charSet->mSizes[sizeIndex];
    printf("          %d %s\n", size->mSize, size->mName);
  }
#endif

  return HT_ENUMERATE_NEXT;
}

static void
DumpFamily(nsFontFamily* aFamily)
{
#if 0
  for (int styleIndex = 0; styleIndex < 3; styleIndex++) {
    nsFontStyle* style = aFamily->mStyles[styleIndex];
    if (style) {
      printf("  style: %s\n", gDumpStyles[styleIndex]);
      for (int weightIndex = 0; weightIndex < 8; weightIndex++) {
        nsFontWeight* weight = style->mWeights[weightIndex];
        if (weight) {
          printf("    weight: %d\n", (weightIndex + 1) * 100);
          for (int stretchIndex = 0; stretchIndex < 9; stretchIndex++) {
            nsFontStretch* stretch = weight->mStretches[stretchIndex];
            if (stretch) {
              printf("      stretch: %d\n", stretchIndex + 1);
              PL_HashTableEnumerateEntries(stretch->mCharSets, DumpCharSet,
                nsnull);
            }
          }
        }
      }
    }
  }
#endif
}

static PRIntn
DumpFamilyEnum(PLHashEntry* he, PRIntn i, void* arg)
{
  char buf[256];
  ((nsString*) he->key)->ToCString(buf, sizeof(buf));
  printf("family: %s\n", buf);
  nsFontFamily* family = (nsFontFamily*) he->value;
  DumpFamily(family);

  return HT_ENUMERATE_NEXT;
}

static void
DumpTree(void)
{
  PL_HashTableEnumerateEntries(gFamilies, DumpFamilyEnum, nsnull);
}

#endif /* DEBUG_DUMP_TREE */

static nsFontFamily* GetFontNames(char* aPattern)
{
  nsFontFamily* family = nsnull;
  int         MAX_FONTDETAIL = 60;
  FontDetails fDetails[MAX_FONTDETAIL];
  int         fontcount;

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontEnumeratorPh::GetFontNames aPattern=<%s>\n", aPattern));

    fontcount = PfQueryFonts('A', PHFONT_ALL_FONTS, fDetails, MAX_FONTDETAIL);
    if (fontcount >= MAX_FONTDETAIL)
    {
      NS_WARNING("nsFontMetricsPh::GetFontNames ERROR - Font Array size should be increased!\n");
      printf("nsFontMetricsPh::GetFontNames ERROR - Font Array size should be increased: I got %d and %d is the max\n", fontcount, MAX_FONTDETAIL);
    }

    if (fontcount)
    {
      int index;
      for(index=0; index < PR_MIN(fontcount,MAX_FONTDETAIL); index++)
      {
        PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontEnumeratorPh::GetFontNames Adding font <%s>\n", fDetails[index].desc));

        nsAutoString familyName2( (const PRUnichar *) fDetails[index].desc, strlen(fDetails[index].desc) );
        family = (nsFontFamily*) PL_HashTableLookup(gFamilies, (nsString*) &familyName2);
        if (!family)
        {
          family = new nsFontFamily;
          if (!family)
          {
            continue;
          }
          nsString* copy = new nsString((const PRUnichar *) fDetails[index].desc, strlen( fDetails[index].desc) );
          if (!copy) {
           delete family;
           continue;
          }
          PL_HashTableAdd(gFamilies, copy, family);
    }

      }
    }

#ifdef DEBUG_DUMP_TREE
  DumpTree();
#endif

}


// The Font Enumerator

nsFontEnumeratorPh::nsFontEnumeratorPh()
{
  NS_INIT_REFCNT();
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontEnumeratorPh::nsFontEnumeratorPh this=<%p>\n", this));
}

NS_IMPL_ISUPPORTS(nsFontEnumeratorPh, NS_GET_IID(nsIFontEnumerator));

static int gInitializedFontEnumerator = 0;
static PLHashNumber HashKey(const void* aString)
{
  const nsString* key = (const nsString*) aString;
  return (PLHashNumber)
    nsCRT::HashCode(key->GetUnicode());
}

static PRIntn
CompareKeys(const void* aStr1, const void* aStr2)
{
  return nsCRT::strcmp(((const nsString*) aStr1)->GetUnicode(),
    ((const nsString*) aStr2)->GetUnicode()) == 0;
}

static int
InitializeFontEnumerator(void)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontEnumeratorPh::InitializeFontEnumerator\n"));

  gInitializedFontEnumerator = 1;

  if (!gGotAllFontNames)
  {
    gGotAllFontNames = 1;

#if 0
    gFamilies = PL_NewHashTable(0, HashKey, CompareKeys, NULL, NULL, NULL);
    gFamilyNames = PL_NewHashTable(0, HashKey, CompareKeys, NULL, NULL, NULL);
    GetFontNames(NULL);
#endif
  }

  return 1;
}

typedef struct EnumerateFamilyInfo
{
  PRUnichar** mArray;
  int         mIndex;
} EnumerateFamilyInfo;

static PRIntn
EnumerateFamily(PLHashEntry* he, PRIntn i, void* arg)
{
  EnumerateFamilyInfo* info = (EnumerateFamilyInfo*) arg;
  PRUnichar** array = info->mArray;
  int j = info->mIndex;
  PRUnichar* str = ((nsString*) he->key)->ToNewUnicode();

  //PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontEnumeratorPh::EnumerateFamily str=<%s>\n",str ));

  if (!str) {
    for (j = j - 1; j >= 0; j--) {
      nsMemory::Free(array[j]);
    }
    info->mIndex = 0;
    return HT_ENUMERATE_STOP;
  }
  array[j] = str;
  info->mIndex++;

  return HT_ENUMERATE_NEXT;
}

static int
CompareFontNames(const void* aArg1, const void* aArg2, void* aClosure)
{
  const PRUnichar* str1 = *((const PRUnichar**) aArg1);
  const PRUnichar* str2 = *((const PRUnichar**) aArg2);

//  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontEnumeratorPh::CompareFontNames str1=<%s> str2=<%s>\n",str1,str2));

  // XXX add nsICollation stuff

  return nsCRT::strcmp(str1, str2);
}

NS_IMETHODIMP
nsFontEnumeratorPh::EnumerateAllFonts(PRUint32* aCount, PRUnichar*** aResult)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontEnumeratorPh::EnumerateAllFonts  this=<%p> gInitializedFontEnumerator=<%d>\n", this, gInitializedFontEnumerator));

  if (aCount) {
    *aCount = 0;
  }
  else {
    return NS_ERROR_NULL_POINTER;
  }
  if (aResult) {
    *aResult = nsnull;
  }
  else {
    return NS_ERROR_NULL_POINTER;
  }

  if (!gInitializedFontEnumerator) {
    if (!InitializeFontEnumerator()) {
      return NS_ERROR_FAILURE;
    }
  }

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontEnumeratorPh::EnumerateAllFonts after InitializeFontEnumerator gFamilies=<%p>", gFamilies));

  if (gFamilies)
  {
    PRUnichar** array = (PRUnichar**) nsMemory::Alloc(gFamilies->nentries * sizeof(PRUnichar*));
    if (!array)
    {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    EnumerateFamilyInfo info = { array, 0 };
    PL_HashTableEnumerateEntries(gFamilies, EnumerateFamily, &info);
    if (!info.mIndex)
    {
      nsMemory::Free(array);
      return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_QuickSort(array, gFamilies->nentries, sizeof(PRUnichar*),
    CompareFontNames, nsnull);

    *aCount = gFamilies->nentries;
    *aResult = array;

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontEnumeratorPh::EnumerateAllFonts exiting count=<%d>", *aCount));

    return NS_OK;
  }
  else
  {
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP
nsFontEnumeratorPh::EnumerateFonts(const char* aLangGroup,
  const char* aGeneric, PRUint32* aCount, PRUnichar*** aResult)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontEnumeratorPh::EnumerateFonts  this=<%p>\n", this));

  if ((!aLangGroup) || (!aGeneric)) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aCount) {
    *aCount = 0;
  }
  else {
    return NS_ERROR_NULL_POINTER;
  }
  if (aResult) {
    *aResult = nsnull;
  }
  else {
    return NS_ERROR_NULL_POINTER;
  }

  if ((!strcmp(aLangGroup, "x-unicode")) ||
      (!strcmp(aLangGroup, "x-user-def"))) {
    return EnumerateAllFonts(aCount, aResult);
  }

  if (!gInitializedFontEnumerator) {
    if (!InitializeFontEnumerator()) {
      return NS_ERROR_FAILURE;
    }
  }

  // XXX still need to implement aLangGroup and aGeneric
  return EnumerateAllFonts(aCount, aResult);
}
