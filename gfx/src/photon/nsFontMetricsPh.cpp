/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsFontMetricsPh.h"
#include "nsPhGfxLog.h"

static NS_DEFINE_IID(kIFontMetricsIID, NS_IFONT_METRICS_IID);

nsFontMetricsPh :: nsFontMetricsPh()
{
  /* stolen from GTK version */
  NS_INIT_REFCNT();

  mDeviceContext = nsnull;
  mFont = nsnull;
	  
  mHeight = 0;
  mAscent = 0;
  mDescent = 0;
  mLeading = 0;
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

NS_IMPL_ISUPPORTS(nsFontMetricsPh, kIFontMetricsIID)

NS_IMETHODIMP
nsFontMetricsPh :: Init(const nsFont& aFont, nsIDeviceContext *aContext)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::Init with nsFont\n"));

  NS_ASSERTION(!(nsnull == aContext), "attempt to init fontmetrics with null device context");

  nsAutoString  firstFace;
  char          *str = nsnull;
  nsresult      result;  	
  nsresult      ret_code = NS_ERROR_FAILURE;
  int           MAX_FONTDETAIL = 30;
  FontDetails   fDetails[MAX_FONTDETAIL];
  int           fontcount;
  int           index;

//  str = firstFace.ToNewCString();
//  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::Init with nsFont: aFont = <%s>\n", str));
//  delete [] str; 

  result = aContext->FirstExistingFont(aFont, firstFace);

  str = firstFace.ToNewCString();
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::Init with nsFont: after FirstExistingFont, firstFace = <%s> %d\n", str, result));
//  delete [] str;

  if (NS_OK != result)
  {
    aFont.GetFirstFamily(firstFace);

    char *str = nsnull;
    str = firstFace.ToNewCString();
    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::Init with nsFont: after GetFirstFamily = <%s>\n", str));
    delete [] str;
  }

//    str = aFont.name.ToNewCString();
    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::Init with nsFont: aFont.name = <%s>\n", str));
    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::Init with nsFont: aFont.weight = <%d>\n", aFont.weight));
    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::Init with nsFont: aFont.style = <%d>\n", aFont.style));
    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::Init with nsFont: aFont.size = <%d>\n", aFont.size));
//    delete [] str;

    /* Now match the Face name requested with a face Photon has */
    fontcount = PfQueryFonts('a', PHFONT_ALL_FONTS, fDetails, MAX_FONTDETAIL);
    if (fontcount >= MAX_FONTDETAIL)
    {
	  printf("nsFontMetricsPh::Init Font Array should be increased!\n");
    }

    if (fontcount)
    {
 	  for(index=0; index < fontcount; index++)
 	  {
        PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsFontMetricsPh::Init comparing <%s> with <%s>\n", str, fDetails[index].desc));
	    if (strncmp(str, fDetails[index].desc, strlen(str)) == 0)
	    {
          ret_code = NS_OK;
		  break;	  
	    }
      }
	}


    mFont = new nsFont(aFont);
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
    char NSFullFontName[64];
    	
	NSFontSuffix[0] = nsnull;
	
    if (ret_code == NS_OK)
      sprintf(NSFontName, "%s%d",fDetails[index].stem, sizePoints);
	else
    {
      PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::Init with nsFont: Unknown font using helv\n"));
      sprintf(NSFontName, "helv%d",sizePoints);		/* default font! should be a pref.*/
    }
		
//    if (aFont.weight >= NS_FONT_WEIGHT_BOLD)  /* this is more correct! */
    if (aFont.weight > NS_FONT_WEIGHT_NORMAL)
    {
      PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::Init with nsFont: set font BOLD\n"));
	  strcat(NSFontSuffix,"b");
    }
	
    if (aFont.style & NS_FONT_STYLE_ITALIC)
   {
      PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::Init with nsFont: set font ITALIC\n"));
	  strcat(NSFontSuffix,"i");
   }	

   sprintf(NSFullFontName, "%s%s", NSFontName, NSFontSuffix);
   
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

    /* For Scalable fonts the PfQueryFont does not return the font size! */
    /* so I have to append it */
    if (fontInfo.size == 0)
    {
      memset(NSFontName, NULL, sizeof(NSFontName));
      strncpy(NSFontName,fontInfo.font, (strlen(fontInfo.font)-strlen(NSFontSuffix)));

//PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::Init with nsFont: name1 <%s>\n",NSFontName));

      mFontHandle = nsString(NSFontName);

      char buffer[5];
      sprintf(buffer, "%d", sizePoints);

//PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::Init with nsFont: size buffer <%s> suffix=<%s>\n",buffer, NSFontSuffix));

      mFontHandle.Append(buffer);

	  mFontHandle.Append(NSFontSuffix);

      char *str=mFontHandle.ToNewCString();
      PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::Init with nsFont: Final Name <%s>\n",str));
	  delete [] str;
	}
    else
	{
      mFontHandle = nsString(fontInfo.font);
	}
	/* These are in pixels and need to be converted! */
    float height;
	height = fontInfo.descender - fontInfo.ascender + 1.0;
    mHeight                = nscoord(height * f);
    mMaxAscent = mAscent   = nscoord(fontInfo.ascender * f * -1.0); // HACK
    mMaxDescent = mDescent = nscoord(fontInfo.descender * f);
    mMaxAdvance            = nscoord(fontInfo.width * f);  /* max width */

 /****** stolen from GTK *******/
    // 56% of ascent, best guess for non-true type
    mXHeight = NSToCoordRound((float) fontInfo.ascender * f * 0.56f * -1.0);

    mUnderlineOffset = -NSToIntRound(MAX (1, floor (0.1 * height + 0.5)) * f);
    mUnderlineSize = NSToIntRound(MAX(1, floor (0.05 * height + 0.5)) * f);
    mStrikeoutOffset = NSToIntRound((mAscent + 1) / 2.5);
    mStrikeoutSize = mUnderlineSize;
    mSuperscriptOffset = mXHeight;
    mSubscriptOffset = mXHeight;
    mLeading = 0;

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
nsFontMetricsPh :: GetLeading(nscoord &aLeading)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::GetLeading mLeading=<%d>- Not Implemented\n", mLeading));
  aLeading = mLeading;
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
//  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::GetFont\n"));
  aFont = mFont;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPh::GetFontHandle(nsFontHandle &aHandle)
{
//  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsFontMetricsPh::GetFontHandle\n"));

  aHandle = (nsFontHandle) &mFontHandle;
  return NS_OK;
}
