/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

// ToDo: Unicode, encoding.
//       Revision by someone who *really* understands OS/2 fonts.

#include "nsGfxDefs.h"
#include "nsDeviceContextOS2.h"
#include "nsFontMetricsOS2.h"
#include "nsString.h"
#include "nsFont.h"

// font handle
nsFontHandleOS2::nsFontHandleOS2()
{
   memset( &fattrs, 0, sizeof fattrs);
   fattrs.usRecordLength = sizeof fattrs;
   charbox.cx = charbox.cy = 0;
}

void nsFontHandleOS2::SelectIntoPS( HPS hps, long lcid)
{
   if( !GpiSetCharBox( hps, &charbox))
      PMERROR("GpiSetCharBox");
   if( !GpiSetCharSet( hps, lcid))
      PMERROR("GpiSetCharSet");
}

nsFontMetricsOS2::nsFontMetricsOS2()
{
   // members are zeroed by new operator (hmm)
   NS_INIT_REFCNT();
}
  
nsFontMetricsOS2::~nsFontMetricsOS2()
{
   Destroy();

   delete mFont;
   delete mFontHandle;
}

NS_IMPL_ISUPPORTS( nsFontMetricsOS2, nsIFontMetrics::GetIID())

nsresult nsFontMetricsOS2::Init( const nsFont &aFont,  nsIAtom* aLangGroup, nsIDeviceContext *aContext)
{
   mFont = new nsFont( aFont);
   mLangGroup = aLangGroup;
   //   mIndexOfSubstituteFont = -1;
   mContext = (nsDeviceContextOS2 *) aContext;
   nsresult rv = RealizeFont();
   return rv;
}

nsresult nsFontMetricsOS2::Destroy()
{
   return NS_OK;
}

// Map CSS font names to something we can understand locally.
// Some conversions are a bit dodgy (fantasy) but there's little option, I
// guess.
// Surely this should be in XP code somewhere?
static void MapGenericFamilyToFont( const nsString &aGenericFamily, 
                                    nsIDeviceContext *aDC,
                                    nsString &aFontFace)
{
   // the CSS generic names (conversions from Nav for now)
   // XXX this  need to check availability with the dc
   PRBool aliased;
   nsAutoString  timesRoman;         timesRoman.AssignWithConversion("Tms Rmn");
   nsAutoString  helv;               helv.AssignWithConversion("Helv");
   nsAutoString  script;             script.AssignWithConversion("Script");
   nsAutoString  arial;              arial.AssignWithConversion("Arial");
   nsAutoString  courier;            courier.AssignWithConversion("Courier");

   if( aGenericFamily.EqualsIgnoreCase( "serif"))
      aDC->GetLocalFontName( timesRoman, aFontFace, aliased);
   else if( aGenericFamily.EqualsIgnoreCase( "sans-serif"))
      aDC->GetLocalFontName( helv, aFontFace, aliased);
   else if( aGenericFamily.EqualsIgnoreCase( "Helv")) // !!
      aDC->GetLocalFontName( script, aFontFace, aliased);
   else if( aGenericFamily.EqualsIgnoreCase( "fantasy")) // !!
      aDC->GetLocalFontName( arial, aFontFace, aliased);
   else if( aGenericFamily.EqualsIgnoreCase( "monospace"))
      aDC->GetLocalFontName( courier, aFontFace, aliased);
   else
      aFontFace.Truncate();
}

struct FontEnumData
{
   FontEnumData( nsIDeviceContext* aContext, char* aFaceName)
    : mContext( aContext), mFaceName( aFaceName)
   {}

   nsIDeviceContext *mContext;
   char             *mFaceName;
};

// callback for each of the faces in the nsFont (use the first we can match)
static PRBool FontEnumCallback( const nsString& aFamily, PRBool aGeneric, void *aData)
{
   FontEnumData *data = (FontEnumData*)aData;
   PRBool        rc = PR_TRUE;

   if( aGeneric)
   {
      nsAutoString realFace;
      MapGenericFamilyToFont( aFamily, data->mContext, realFace);
      realFace.ToCString( data->mFaceName, FACESIZE);
      rc = PR_FALSE;  // stop
   }
   else
   {
      nsAutoString realFace;
      PRBool       aliased;
      data->mContext->GetLocalFontName( aFamily, realFace, aliased);
      if( aliased || (NS_OK == data->mContext->CheckFontExistence( realFace)))
      {
         realFace.ToCString(data->mFaceName, FACESIZE);
         rc = PR_FALSE;  // stop
      }
   }

   return PR_TRUE;
}

// Current strategy wrt. image/outline fonts:
//   If image face requested is available in the point size requested,
//   use it.  If not, use the corresponding outline font.  The reason
//   why we deal with this here instead of letting GpiCreateLogFont() do
//   something sensible is because it doesn't do the right thing with
//   bold/italic effects: if we ask for bold Tms Rmn in 35 pt, we don't
//   get Times New Roman Bold in 35 pt, but Times New Roman 35pt with a
//   fake bold effect courtesy of gpi, which looks ugly.
//
// Yes, it would be easier & quite plausable to ignore image fonts altogether
//   and just use outlines, but I reckon image fonts look better at lower
//   point sizes.
//
// Candidate for butchery!

// Utility; delete [] when done.
static PFONTMETRICS getMetrics( long &lFonts, PCSZ facename, HPS hps)
{
   LONG lWant = 0;
   lFonts = GpiQueryFonts( hps, QF_PUBLIC | QF_PRIVATE,
                           facename, &lWant, 0, 0);
   PFONTMETRICS pMetrics = new FONTMETRICS [ lFonts];

   GpiQueryFonts( hps, QF_PUBLIC | QF_PRIVATE, facename, &lFonts,
                  sizeof( FONTMETRICS), pMetrics);

   return pMetrics;
}

nsresult nsFontMetricsOS2::RealizeFont()
{
   nsFontHandleOS2 *fh = new nsFontHandleOS2;
   if (!fh)
     return NS_ERROR_OUT_OF_MEMORY;

   // 1) Find family name
   char szFamily[ FACESIZE] = "";
   FontEnumData data( mContext, szFamily);
   mFont->EnumerateFamilies( FontEnumCallback, &data); 

   // sanity check - no way of telling whether we want a fixed or prop font..
   if( !szFamily[0])
      strcpy( szFamily, "System Proportional");

   // 2) Get a representative PS for doing font queries into
   HPS hps;

   if (NULL != mContext->mDC){
     hps = mContext->mPS;
   } else {
     hps = WinGetPS((HWND)mContext->mWidget);
   }

   // 3) Work out what our options are wrt. image/outline, prefer image.
   BOOL bOutline = FALSE, bImage = FALSE;
   long lFonts = 0; int i;
   PFONTMETRICS pMetrics = getMetrics( lFonts, szFamily, hps);

   for( i = 0; i < lFonts && !(bImage && bOutline); i++)
      if( pMetrics[ i].fsDefn & FM_DEFN_OUTLINE) bOutline = TRUE;
      else                                       bImage = TRUE;
   delete [] pMetrics;

   if( !bImage) fh->fattrs.fsFontUse = FATTR_FONTUSE_OUTLINE |
                                       FATTR_FONTUSE_TRANSFORMABLE;

   // 4) Try to munge the face for italic & bold effects (could do better)
   BOOL bBold = mFont->weight > NS_FONT_WEIGHT_NORMAL;
   BOOL bItalic = !!(mFont->style & NS_FONT_STYLE_ITALIC);
   FACENAMEDESC fnd = { sizeof( FACENAMEDESC),
                        bBold ? FWEIGHT_BOLD : FWEIGHT_DONT_CARE,
                        FWIDTH_DONT_CARE,
                        0,
                        bItalic ? FTYPE_ITALIC : 0 };
   
   ULONG rc = GpiQueryFaceString( hps, szFamily, &fnd,
                                  FACESIZE, fh->fattrs.szFacename);
   if( rc == GPI_ERROR)
   {  // no real font, fake it
      strcpy( fh->fattrs.szFacename, szFamily);
      if( bBold) fh->fattrs.fsSelection |= FATTR_SEL_BOLD;
      if( bItalic) fh->fattrs.fsSelection |= FATTR_SEL_ITALIC;
   }

   // 5) Add misc effects
   if( mFont->decorations & NS_FONT_DECORATION_UNDERLINE)
      fh->fattrs.fsSelection |= FATTR_SEL_UNDERSCORE;
   if( mFont->decorations & NS_FONT_DECORATION_LINE_THROUGH)
      fh->fattrs.fsSelection |= FATTR_SEL_STRIKEOUT;

   // 6) Encoding
   // There doesn't seem to be any encoding stuff yet, so guess.
   // (XXX unicode hack; use same codepage as converter!)
   fh->fattrs.usCodePage = gModuleData.ulCodepage;
           
   // 7) Find the point size for the font, and set up the charbox too
   float app2dev, app2twip, twip2dev;
   mContext->GetAppUnitsToDevUnits( app2dev);
   mContext->GetDevUnitsToTwips( app2twip);
   mContext->GetTwipsToDevUnits( twip2dev);

   // !! Windows wants to mply up here.  I don't think I do.  If fonts
   // !! ever begin to look `squished', try enabling the following code
#if 0
   float scale;
   mContext->GetCanonicalPixelScale(scale);
   app2twip *= app2dev * scale;
#else
   app2twip *= app2dev;
#endif

   // Note: are you confused by the block above, and thinking that app2twip
   //       must be 1?  Well, there's *no* guarantee that app units are
   //       twips, despite whatever nscoord.h says!
   int points = NSTwipsToFloorIntPoints( nscoord( mFont->size * app2twip));
   fh->charbox.cx = MAKEFIXED( points * 20 * twip2dev, 0);
   fh->charbox.cy = fh->charbox.cx;

   // 8) If we're using an image font, check it's available in the size
   //    required, substituting an outline if necessary.
   if( bImage)
   {
      HDC    hdc = GpiQueryDevice( hps);
      long   res[ 2];
      DevQueryCaps( hdc, CAPS_HORIZONTAL_FONT_RES, 2, res);
      pMetrics = getMetrics( lFonts, szFamily, hps);

      for( i = 0; i < lFonts; i++)
         if( !stricmp( szFamily, pMetrics[ i].szFamilyname) &&
             pMetrics[ i].sNominalPointSize / 10 == points  &&
             pMetrics[ i].sXDeviceRes == res[0]             &&
             pMetrics[ i].sYDeviceRes == res[1]) break;

      if( i == lFonts)
      {
         // Couldn't find an appropriate font, need to use an outline.
         // If there was an outline originally, fine. If not...
         if( !bOutline)
         {
            // Can't have the requested font in requested size; fake.
            if( !stricmp( szFamily, "Helv"))
               strcpy( szFamily, "Helvetica");
            else if( !stricmp( szFamily, "Courier"))
               strcpy( szFamily, "Courier New");
            else
               strcpy( szFamily, "Times New Roman"); // hmm
            fh->fattrs.fsSelection &= ~(FATTR_SEL_BOLD | FATTR_SEL_ITALIC);
            rc = GpiQueryFaceString( hps, szFamily, &fnd,
                                     FACESIZE, fh->fattrs.szFacename);
            if( rc == GPI_ERROR)
            {
               strcpy( fh->fattrs.szFacename, szFamily);
               if( bBold) fh->fattrs.fsSelection |= FATTR_SEL_BOLD;
               if( bItalic) fh->fattrs.fsSelection |= FATTR_SEL_ITALIC;
            }
         }
         fh->fattrs.fsFontUse = FATTR_FONTUSE_OUTLINE |
                                FATTR_FONTUSE_TRANSFORMABLE;
      }
      else
      {
         // image face found fine, set required size in fattrs.
         fh->fattrs.lMaxBaselineExt = pMetrics[ i].lMaxBaselineExt;
         fh->fattrs.lAveCharWidth = pMetrics[ i].lAveCharWidth;
      }
      delete [] pMetrics;
   }

   // 9) Record font handle & record various font metrics to cache
   mFontHandle = fh;
   if( GPI_ERROR == GpiCreateLogFont( hps, 0, 1, &fh->fattrs))
      PMERROR( "GpiCreateLogFont");
   fh->SelectIntoPS( hps, 1);

   FONTMETRICS fm;
   GpiQueryFontMetrics( hps, sizeof fm, &fm);

   float dev2app;
   mContext->GetDevUnitsToAppUnits( dev2app);

   // PM includes the internal leading in the max ascender.  So the max
   // ascender we tell raptor about should be lMaxAscent - lInternalLeading.
   //
   // This is probably all moot 'cos lInternalLeading is usually zero.
   // More so 'cos layout doesn't look at the leading we give it.
   //
   // So let's leave it as zero to avoid confusion & change it if necessary.

   mHeight             = NSToCoordRound( fm.lMaxBaselineExt * dev2app);
   mMaxAscent          = NSToCoordRound( fm.lMaxAscender * dev2app);
   mMaxDescent         = NSToCoordRound( fm.lMaxDescender * dev2app);
   mMaxAdvance         = NSToCoordRound( fm.lMaxCharInc * dev2app);

   mMaxHeight          = NSToCoordRound( fm.lMaxBaselineExt * dev2app);
   mEmHeight           = NSToCoordRound( fm.lEmHeight * dev2app);
   // XXXXOS2TODO Not sure about these two - hs
   mEmAscent           = NSToCoordRound((fm.lMaxAscender - fm.lInternalLeading) * dev2app);
   mEmDescent          = NSToCoordRound( fm.lMaxDescender * dev2app);

   mLeading            = NSToCoordRound( fm.lInternalLeading * dev2app);
   mXHeight            = NSToCoordRound( fm.lXHeight * dev2app);

   mSuperscriptYOffset = NSToCoordRound( fm.lSuperscriptYOffset * dev2app);
   mSubscriptYOffset   = NSToCoordRound( fm.lSubscriptYOffset * dev2app); // !! check this
   mStrikeoutPosition  = NSToCoordRound( fm.lStrikeoutPosition * dev2app);
   mStrikeoutSize      = NSToCoordRound( fm.lStrikeoutSize * dev2app);
   mUnderlinePosition  = NSToCoordRound( -fm.lUnderscorePosition * dev2app);
   mUnderlineSize      = NSToCoordRound( fm.lUnderscoreSize * dev2app);

   // OS/2 field (needs to be kept in sync with mMaxAscent)
   mDevMaxAscent       = fm.lMaxAscender;

   // 10) Clean up
   GpiSetCharSet( hps, LCID_DEFAULT);
   if( !GpiDeleteSetId( hps, 1))
      PMERROR( "GpiDeleteSetID (FM)");
   if (NULL == mContext->mDC)
      WinReleasePS(hps);

   return NS_OK;
}

nscoord nsFontMetricsOS2::GetSpaceWidth( nsIRenderingContext *aRContext)
{
   if( !mSpaceWidth)
   {
      char buf[1];
      buf[0] = ' ';
      aRContext->GetWidth( buf, 1, mSpaceWidth);
   }

   return mSpaceWidth;
}

// Other metrics
NS_IMETHODIMP nsFontMetricsOS2::GetXHeight( nscoord &aResult)
{
   aResult = mXHeight;
   return NS_OK;
}

NS_IMETHODIMP nsFontMetricsOS2::GetSuperscriptOffset(nscoord& aResult)
{
   aResult = mSuperscriptYOffset;
   return NS_OK;
}

NS_IMETHODIMP nsFontMetricsOS2::GetSubscriptOffset(nscoord& aResult)
{
   aResult = mSubscriptYOffset;
   return NS_OK;
}

NS_IMETHODIMP nsFontMetricsOS2::GetStrikeout(nscoord& aOffset, nscoord& aSize)
{
   aOffset = mStrikeoutPosition;
   aSize = mStrikeoutSize;
   return NS_OK;
}

NS_IMETHODIMP nsFontMetricsOS2::GetUnderline(nscoord& aOffset, nscoord& aSize)
{
   aOffset = mUnderlinePosition;
   aSize = mUnderlineSize;
   return NS_OK;
}

NS_IMETHODIMP nsFontMetricsOS2::GetHeight( nscoord &aHeight)
{
   aHeight = mHeight;
   return NS_OK;
}

NS_IMETHODIMP nsFontMetricsOS2::GetLeading( nscoord &aLeading)
{
   aLeading = mLeading;
   return NS_OK;
}

NS_IMETHODIMP nsFontMetricsOS2::GetMaxAscent( nscoord &aAscent)
{
   aAscent = mMaxAscent;
   return NS_OK;
}

NS_IMETHODIMP nsFontMetricsOS2::GetMaxDescent( nscoord &aDescent)
{
   aDescent = mMaxDescent;
   return NS_OK;
}

NS_IMETHODIMP nsFontMetricsOS2::GetMaxAdvance( nscoord &aAdvance)
{
  aAdvance = mMaxAdvance;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsOS2::GetFont( const nsFont *&aFont)
{
   aFont = mFont;
   return NS_OK;
}

NS_IMETHODIMP nsFontMetricsOS2::GetFontHandle( nsFontHandle &aHandle)
{
   aHandle = mFontHandle;
   return NS_OK;
}

NS_IMETHODIMP nsFontMetricsOS2::GetLangGroup(nsIAtom** aLangGroup)
{
  if (!aLangGroup) {
    return NS_ERROR_NULL_POINTER;
  }

  *aLangGroup = mLangGroup;
  NS_IF_ADDREF(*aLangGroup);

  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsOS2::GetNormalLineHeight(nscoord &aHeight)
{
  aHeight = mEmHeight + mLeading;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsOS2::GetEmHeight(nscoord &aHeight)
{
  aHeight = mEmHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsOS2::GetEmAscent(nscoord &aAscent)
{
  aAscent = mEmAscent;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsOS2::GetEmDescent(nscoord &aDescent)
{
  aDescent = mEmDescent;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsOS2::GetMaxHeight(nscoord &aHeight)
{
  aHeight = mMaxHeight;
  return NS_OK;
}
