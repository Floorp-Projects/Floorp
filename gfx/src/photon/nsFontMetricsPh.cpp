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

#include "nsQuickSort.h"
#include "nsIServiceManager.h"
#include "nsFontMetricsPh.h"
#include "nsPhGfxLog.h"
#include "nsHashtable.h"
#include "nsIPref.h"
#include "nsReadableUtils.h"

#include <errno.h>
#include <string.h>

// XXX many of these statics need to be freed at shutdown time
static nsHashtable* gFontMetricsCache = nsnull;
static nsCString **gFontNames = nsnull;
static FontDetails *gFontDetails = nsnull;
static int gnFonts = 0;
static nsIPref* gPref = nsnull;

#undef USER_DEFINED
#define USER_DEFINED "x-user-def"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

nsFontMetricsPh::nsFontMetricsPh()
{
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
	mAveCharWidth = 0;
}

static nsresult InitGlobals()
{
  nsServiceManager::GetService(kPrefCID, NS_GET_IID(nsIPref), (nsISupports**) &gPref);
  if (!gPref) return NS_ERROR_FAILURE;

	gFontMetricsCache = new nsHashtable();
	return NS_OK;
}

#if 0
static PRBool FreeFontMetricsCache(nsHashKey* aKey, void* aData, void* aClosure)
{
	FontQueryInfo * node = (FontQueryInfo*) aData;
	
	/* Use free() rather since we use calloc() in ::Init to alloc the node.  */
	if (node)
		free (node);
	
	return PR_TRUE;
}
static void FreeGlobals()
{
	if (gFontMetricsCache)
	{
		gFontMetricsCache->Reset(FreeFontMetricsCache, nsnull);
		delete gFontMetricsCache;
		gFontMetricsCache = nsnull;
	}
}
#endif

nsFontMetricsPh :: ~nsFontMetricsPh( )
{
	if( nsnull != mFont )
	  {
		  delete mFont;
		  mFont = nsnull;
	  }
	if (mFontHandle)
	   free (mFontHandle);
  if (mDeviceContext) {
    // Notify our device context that owns us so that it can update its font cache
    mDeviceContext->FontMetricsDeleted(this);
    mDeviceContext = nsnull;
  }
}

NS_IMPL_ISUPPORTS1( nsFontMetricsPh, nsIFontMetrics )

NS_IMETHODIMP nsFontMetricsPh::Init ( const nsFont& aFont, nsIAtom* aLangGroup, nsIDeviceContext* aContext )
{
	NS_ASSERTION(!(nsnull == aContext), "attempt to init fontmetrics with null device context");
	
	nsAutoString  firstFace;
	char          *str = nsnull;
	nsresult      result;
	PhRect_t      extent;

	if( !gFontMetricsCache ) {
		nsresult res = InitGlobals( );
		if( NS_FAILED(res) ) return res;
		}
	
	mFont = new nsFont(aFont);
	mLangGroup = aLangGroup;
	
	mDeviceContext = aContext;
	
	result = aContext->FirstExistingFont(aFont, firstFace);

	str = ToNewCString(firstFace);

#ifdef DEBUG_Adrian
printf( "\n\n\t\t\tIn nsFontMetricsPh::Init str=%s\n", str );
#endif

	if( !str || !str[0] )
	{
		if( str ) free (str);
		str = strdup("serif");
	}
	
	const char *cstring;
	aLangGroup->GetUTF8String( &cstring );
	
	char prop[256];
	sprintf( prop, "font.name.%s.%s", str, cstring );

	char *font_default = NULL;
	gPref->CopyCharPref( prop, &font_default );
	if( font_default )
		{
		free (str);
		/* font_default was allocated. in CopyCharPref. */
		str = font_default;
		}

	float app2dev;
	app2dev = mDeviceContext->AppUnitsToDevUnits();

	PRInt32 sizePoints;
	if( mFont->systemFont == PR_TRUE )
		sizePoints = NSToIntRound( app2dev * mFont->size * 0.68 );
	else sizePoints = NSToIntRound( app2dev * mFont->size * 0.74 );
	
	char NSFullFontName[MAX_FONT_TAG];

	unsigned int uiFlags = 0L;

	if(aFont.weight > NS_FONT_WEIGHT_NORMAL)
	   uiFlags |= PF_STYLE_BOLD;

	if(aFont.style & (NS_FONT_STYLE_ITALIC|NS_FONT_STYLE_OBLIQUE) )
	   uiFlags |= PF_STYLE_ITALIC;

	if(aFont.style & NS_FONT_STYLE_ANTIALIAS)
		uiFlags |= PF_STYLE_ANTIALIAS;

	if( PfGenerateFontName( (char *)str, uiFlags, sizePoints, (char *)NSFullFontName ) == NULL )
	  {
#ifdef DEBUG_Adrian
printf( "!!!!!!!!!!!! PfGenerateFontName failed\n" );
#endif
		  PfGenerateFontName( "TextFont", uiFlags, sizePoints, (char *)NSFullFontName );
	  }

	/* Once the Photon Font String is built get the attributes */
	FontQueryInfo *node;

	nsCStringKey key((char *)(NSFullFontName));
	node = (FontQueryInfo *) gFontMetricsCache->Get(&key);

#ifdef DEBUG_Adrian
printf( "\t\t\tThe generated font name is NSFullFontName=%s\n", NSFullFontName );
if( node ) printf( "\t\t\t( cached ) The real font is desc=%s\n", node->desc );
#endif

	if( !node )
	  {
		node = (FontQueryInfo *)calloc(sizeof(FontQueryInfo), 1);
		PfQueryFont(NSFullFontName, node);

#ifdef DEBUG_Adrian
printf( "\t\t\t(not cached ) The real font is desc=%s\n", node->desc );
printf( "\tCall PfLoadMetrics for NSFullFontName=%s\n", NSFullFontName );
#endif

		gFontMetricsCache->Put(&key, node);

		PfLoadMetrics( NSFullFontName );
	  }

	float dev2app;
	double height;
	nscoord onePixel;

	dev2app = mDeviceContext->DevUnitsToAppUnits();
	onePixel = NSToCoordRound(1 * dev2app);
	height = node->descender - node->ascender;
	PfExtent( &extent, NULL, NSFullFontName, 0L, 0L, " ", 1, PF_SIMPLE_METRICS, NULL );
	mSpaceWidth = NSToCoordRound((extent.lr.x - extent.ul.x + 1) * dev2app);

	mLeading = NSToCoordRound(0);
	mEmHeight = NSToCoordRound(height * dev2app);
	mEmAscent = NSToCoordRound(node->ascender * dev2app * -1.0);
	mEmDescent = NSToCoordRound(node->descender * dev2app);
	mHeight = mMaxHeight = NSToCoordRound(height * dev2app);
	mAscent = mMaxAscent = NSToCoordRound(node->ascender * dev2app * -1.0);
	mDescent = mMaxDescent = NSToCoordRound(node->descender * dev2app);
	mMaxAdvance = NSToCoordRound(node->width * dev2app);
	mAveCharWidth = mSpaceWidth;

	mXHeight = NSToCoordRound((float)node->ascender * dev2app * 0.56f * -1.0); // 56% of ascent, best guess for non-true type
	mSuperscriptOffset = mXHeight;     // XXX temporary code!
	mSubscriptOffset = mXHeight;     // XXX temporary code!

	mStrikeoutSize = onePixel; // XXX this is a guess
	mStrikeoutOffset = NSToCoordRound(mXHeight / 2.0f); // 50% of xHeight
	mUnderlineSize = onePixel; // XXX this is a guess
	mUnderlineOffset = -NSToCoordRound((float)node->descender * dev2app * 0.30f); // 30% of descent

	if (mFontHandle)
	   free (mFontHandle);
	mFontHandle = strdup(NSFullFontName);

	free (str);
	return NS_OK;
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

void nsFontMetricsPh::RealizeFont()
{
#ifdef DEBUG_Adrian
printf( "In RealizeFont\n" );
#endif
}

struct nsFontFamily
{
	NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW
	   PLHashTable* mCharSets;
};

// The Font Enumerator
nsFontEnumeratorPh::nsFontEnumeratorPh()
{
}

NS_IMPL_ISUPPORTS1(nsFontEnumeratorPh, nsIFontEnumerator)

typedef struct EnumerateFamilyInfo
{
	PRUnichar** mArray;
	int         mIndex;
}
EnumerateFamilyInfo;

#if 0
static PRIntn EnumerateFamily( PLHashEntry* he, PRIntn i, void* arg )
{
	EnumerateFamilyInfo* info = (EnumerateFamilyInfo*) arg;
	PRUnichar** array = info->mArray;
	int j = info->mIndex;
	PRUnichar* str = ToNewUnicode(*NS_STATIC_CAST(const nsString*, he->key));

	if (!str)
	  {
		  for (j = j - 1; j >= 0; j--)
			{
				nsMemory::Free(array[j]);
			}
		  info->mIndex = 0;
		  return HT_ENUMERATE_STOP;
	  }
	array[j] = str;
	info->mIndex++;

	return HT_ENUMERATE_NEXT;
}
#endif

NS_IMETHODIMP nsFontEnumeratorPh::EnumerateFonts( const char* aLangGroup, const char* aGeneric, PRUint32* aCount, PRUnichar*** aResult )
{

	NS_ENSURE_ARG_POINTER(aResult);
	*aResult = nsnull;
	NS_ENSURE_ARG_POINTER(aCount);
	*aCount = 0;

	// aLangGroup=null or ""  means any (i.e., don't care)
	// aGeneric=null or ""  means any (i.e, don't care)
	const char* generic = nsnull;
	if (aGeneric && *aGeneric)
		generic = aGeneric;

	int i;
	if(!gFontDetails)
	  {
		  gnFonts = PfQueryFonts('a', PHFONT_DONT_SHOW_LEGACY, NULL, 0);
		  if(gnFonts>0)
			{
				gFontDetails = new FontDetails[gnFonts];
				if(gFontDetails)
				  {
					gFontNames = (nsCString**) nsMemory::Alloc(gnFonts * sizeof(nsCString*));
					PfQueryFonts('a', PHFONT_DONT_SHOW_LEGACY, gFontDetails, gnFonts);

					int total = 0;
					for(i=0;i<gnFonts;i++) {
						gFontNames[total++] = new nsCString(gFontDetails[i].desc);
						}
					gnFonts = total;
				  }

			}
	  }

	if( gnFonts > 0 )
	  {
		  PRUnichar** array = (PRUnichar**) nsMemory::Alloc(gnFonts * sizeof(PRUnichar*));
		  if(!array)
			 return NS_ERROR_OUT_OF_MEMORY;

		  int nCount = 0;
		  for(i=0;i<gnFonts;i++)
			{
				if(!generic) /* select all fonts */
				  {
					  if (gFontDetails[i].flags & PHFONT_INFO_PROP) /* always use proportional fonts */
					  	array[nCount++] = ToNewUnicode(*gFontNames[i]);
				  }
				else if(stricmp(generic, "monospace") == 0)
				  {
					  if(gFontDetails[i].flags & PHFONT_INFO_FIXED)
						 array[nCount++] = ToNewUnicode(*gFontNames[i]);
				  }
				/* other possible vallues for generic are: serif, sans-serif, cursive, fantasy */
				else
				  {
					  if (gFontDetails[i].flags & PHFONT_INFO_PROP) /* always use proportional fonts */
						 array[nCount++] = ToNewUnicode(*gFontNames[i]);
				  }
			}
		  *aCount = nCount;
		  *aResult = array;
		  return NS_OK;
	  }

	return NS_OK;
}
