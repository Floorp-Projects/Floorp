/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
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
 * The Initial Developer of the Original Code is Christopher Blizzard
 * <blizzard@mozilla.org>.  Portions created by the Initial Developer
 * are Copyright (C) 2002 the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com> 
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Brian Stell <bstell@ix.netcom.com>
 *   Morten Nilsen <morten@nilsen.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsISupportsUtils.h"
#include "nsIServiceManagerUtils.h"
#include "nsIPref.h"
#include "nsFontMetricsXft.h"
#include "prenv.h"
#include "prprf.h"
#include "nsQuickSort.h"
#include "nsFont.h"
#include "nsIDeviceContext.h"
#include "nsRenderingContextGTK.h"
#include "nsDeviceContextGTK.h"

#include <gdk/gdkx.h>
#include <freetype/tttables.h>

// Class nsFontXft is an actual instance of a font.  The
// |nsFontMetricsXft| class is made up of a collection of these little
// fonts, really.

class nsFontXft {
public:
    nsFontXft(FcPattern *aPattern, FcPattern *aFontName);
    ~nsFontXft();

    XftFont   *GetXftFont (void);
    gint       GetWidth8  (const char *aString, PRUint32 aLength);
    gint       GetWidth16 (const PRUnichar *aString, PRUint32 aLength);

#ifdef MOZ_MATHML
    void       GetBoundingMetrics8  (const char *aString, PRUint32 aLength,
                                     nsBoundingMetrics &aBoundingMetrics);
    void       GetBoundingMetrics16 (const PRUnichar *aString,
                                     PRUint32 aLength,
                                     nsBoundingMetrics &aBoundingMetrics);
#endif /* MOZ_MATHML */

    PRInt16    GetMaxAscent(void);
    PRInt16    GetMaxDescent(void);

    // a reference to the font loaded and information about it.  
    XftFont   *mXftFont;
    FcPattern *mPattern;
    FcPattern *mFontName;
    FcCharSet *mCharset;
};

struct MozXftLangGroup {
    const char    *mozLangGroup;
    FcChar32       character;
    const FcChar8 *XftLang;
};

static const MozXftLangGroup MozXftLangGroups[] = {
    { "x-western",      0x0041, (const FcChar8 *)"en" },
    { "x-central-euro", 0x0100, (const FcChar8 *)"pl" },
    { "x-cyrillic",     0x0411, (const FcChar8 *)"ru" },
    { "x-baltic",       0x0104, (const FcChar8 *)"lv" },
    { "x-unicode",      0x0000,                  0    },
    { "x-user-def",     0x0000,	                 0    },
};

#define NUM_XFT_LANG_GROUPS (sizeof (MozXftLangGroups) / \
			                 sizeof (MozXftLangGroups[0]))

static int      CalculateSlant   (PRUint8  aStyle);
static int      CalculateWeight  (PRUint16 aWeight);
static void     AddLangGroup     (FcPattern *aPattern, nsIAtom *aLangGroup);
static void     AddFFRE          (FcPattern *aPattern, nsCString *aFamily,
                                  PRBool aWeak);
static void     FFREToFamily     (nsACString &aFFREName, nsACString &oFamily);
static int      FFRECountHyphens (nsACString &aFFREName);
static int      CompareFontNames (const void* aArg1, const void* aArg2,
                                  void* aClosure);
static PRBool   IsASCIIFontName  (const nsString& aName);
static nsresult EnumFontsXft     (nsIAtom* aLangGroup, const char* aGeneric,
                                  PRUint32* aCount, PRUnichar*** aResult);

static const MozXftLangGroup* FindFCLangGroup (nsACString &aLangGroup);

#ifdef MOZ_WIDGET_GTK2
static void GdkRegionSetXftClip(GdkRegion *aGdkRegion, XftDraw *aDraw);
#endif

// This is the scaling factor that we keep fonts limited to against
// the display size.  If a pixel size is requested that is more than
// this factor larger than the height of the display, it's clamped to
// that value instead of the requested size.
#define FONT_MAX_FONT_SCALE 2

#define FONT_SPEC_BUFFER_SIZE 3000
#define FONT_STRING_BUFFER_SIZE 3000
#define UCS2_NOMAPPING 0xFFFD
#define UCS2_SPACE 0x0020

static XftCharFontSpec  gFontSpecBuffer[FONT_SPEC_BUFFER_SIZE];

#undef DEBUG_XFT_MEMORY
#ifdef DEBUG_XFT_MEMORY

static int gNumInstances;

extern "C" {
extern void XftMemReport(void);
extern void FcMemReport(void);
}
#endif

static nsresult
EnumFontsXft(nsIAtom* aLangGroup, const char* aGeneric,
             PRUint32* aCount, PRUnichar*** aResult);

nsFontMetricsXft::nsFontMetricsXft()
{
    NS_INIT_ISUPPORTS();
}

nsFontMetricsXft::~nsFontMetricsXft()
{
    delete mFont;

    if (mDeviceContext)
        mDeviceContext->FontMetricsDeleted(this);

    if (mPattern)
        FcPatternDestroy(mPattern);

    for (PRInt32 i= mLoadedFonts.Count() - 1; i >= 0; --i) {
        nsFontXft *font = (nsFontXft *)mLoadedFonts.ElementAt(i);
        delete font;
    }

    if (mMiniFont)
        XftFontClose(GDK_DISPLAY(), mMiniFont);

#ifdef DEBUG_XFT_MEMORY
    if (--gNumInstances == 0) {
        XftMemReport();
        FcMemReport();
    }
#endif
}

NS_IMPL_ISUPPORTS1(nsFontMetricsXft, nsIFontMetrics)

// nsIFontMetrics impl

NS_IMETHODIMP
nsFontMetricsXft::Init(const nsFont& aFont, nsIAtom* aLangGroup,
                       nsIDeviceContext *aContext)
{
    mFont = new nsFont(aFont);
    mLangGroup = aLangGroup;

    // Hang onto the device context
    mDeviceContext = aContext;

    float app2dev;
    mDeviceContext->GetAppUnitsToDevUnits(app2dev);

    mPixelSize = NSToIntRound(app2dev * mFont->size);
    // Make sure to clamp the pixel size to something reasonable so we
    // don't make the X server blow up.
    mPixelSize = PR_MIN(gdk_screen_height() * FONT_MAX_FONT_SCALE, mPixelSize);

    // enumerate over the font names passed in
    mFont->EnumerateFamilies(nsFontMetricsXft::EnumFontCallback, this);

    nsCOMPtr<nsIPref> prefService;
    prefService = do_GetService(NS_PREF_CONTRACTID);
    if (!prefService)
        return NS_ERROR_FAILURE;
        
    nsXPIDLCString value;

    // Set up the default font name if it's not set
    if (!mGenericFont) {
        prefService->CopyCharPref("font.default", getter_Copies(value));

        if (value.get())
            mDefaultFont = value.get();
        else
            mDefaultFont = "serif";
        
        mGenericFont = &mDefaultFont;
    }

    // set up the minimum sizes for fonts
    if (mLangGroup) {
        nsCAutoString name("font.min-size.");

        if (mGenericFont->Equals("monospace"))
            name.Append("fixed");
        else
            name.Append("variable");

        name.Append(char('.'));

        const PRUnichar* langGroup = nsnull;
        mLangGroup->GetUnicode(&langGroup);

        name.AppendWithConversion(langGroup);

        PRInt32 minimum = 0;
        nsresult res;
        res = prefService->GetIntPref(name.get(), &minimum);
        if (NS_FAILED(res))
            prefService->GetDefaultIntPref(name.get(), &minimum);
        
        if (minimum < 0)
            minimum = 0;
        if (mPixelSize < minimum)
            mPixelSize = minimum;
    }

    if (NS_FAILED(RealizeFont()))
        return NS_ERROR_FAILURE;

    return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsXft::Destroy()
{
    mDeviceContext = nsnull;

    return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsXft::GetFont(const nsFont *&aFont)
{
    aFont = mFont;
    return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsXft::GetLangGroup(nsIAtom** aLangGroup)
{
    *aLangGroup = mLangGroup;
    NS_IF_ADDREF(*aLangGroup);

    return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsXft::GetFontHandle(nsFontHandle &aHandle)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIFontMetricsXft impl

nsresult
nsFontMetricsXft::GetWidth(const char* aString, PRUint32 aLength,
                           nscoord& aWidth,
                           nsRenderingContextGTK *aContext)
{
    gint rawWidth;
    rawWidth = mWesternFont->GetWidth8(aString, aLength);

    float f;
    mDeviceContext->GetDevUnitsToAppUnits(f);
    aWidth = NSToCoordRound(rawWidth * f);

    return NS_OK;
}

nsresult
nsFontMetricsXft::GetWidth(const PRUnichar* aString, PRUint32 aLength,
                           nscoord& aWidth, PRInt32 *aFontID,
                           nsRenderingContextGTK *aContext)
{
    if (!aLength) {
        aWidth = 0;
        return NS_OK;
    }

    gint rawWidth = RawGetWidth(aString, aLength);

    float f;
    mDeviceContext->GetDevUnitsToAppUnits(f);
    aWidth = NSToCoordRound(rawWidth * f);

    if (aFontID)
        *aFontID = 0;

    return NS_OK;
}

nsresult
nsFontMetricsXft::GetTextDimensions(const PRUnichar* aString,
                                    PRUint32 aLength,
                                    nsTextDimensions& aDimensions, 
                                    PRInt32* aFontID,
                                    nsRenderingContextGTK *aContext)
{
    aDimensions.Clear();

    if (!aLength)
        return NS_OK;

    // Before we do anything make sure the minifont has been loaded in
    // case we stumble across any unknown characters.  If this fails,
    // we will jsut substitute space chars for any unknown characters
    // below.
    if (!mMiniFont)
        SetupMiniFont();

    // Walk the list of loaded fonts, looking for a font that supports
    // this character, using this well tested algorithm.
    nsFontXft* prevFont = nsnull;
    gint rawWidth = 0, rawAscent = 0, rawDescent = 0;
    PRUint32 start = 0;
    PRUint32 i;

    for (i = 0; i < aLength; ++i) {
        PRUnichar c = aString[i];
        nsFontXft *currFont = nsnull;
        nsFontXft *font = nsnull;

        // Walk the list of fonts, looking for a font that supports
        // this character.
        PRInt32 end = mLoadedFonts.Count();
        for (PRInt32 j = 0; j < end; ++j) {
            font = (nsFontXft *)mLoadedFonts.ElementAt(j);
            if (FcCharSetHasChar(font->mCharset, c)) {
                currFont = font;
                goto FoundFont; // for speed -- avoid "if" statement
            }
        }

        // If for some reason the mini-font isn't found, just measure
        // a space character instead.
        if (!mMiniFont) {
            c = UCS2_SPACE;
            currFont = FindFont(c);
            goto FoundFont;
        }

        // If we got here it means that we couldn't find a font for
        // this character so we need to measure the unknown glyph box,
        // after measuring any left over text.
        if (prevFont) {
            rawWidth += prevFont->GetWidth16(&aString[start], i - start);
            nscoord tmpMaxAscent = prevFont->GetMaxAscent();
            nscoord tmpMaxDescent = prevFont->GetMaxDescent();
            if (rawAscent < tmpMaxAscent)
                rawAscent = tmpMaxAscent;
            if (rawDescent < tmpMaxDescent)
                rawDescent = tmpMaxDescent;
            
            // Unset prevFont so that we will automatically start
            // measuring from the next character in the string.  Note
            // that we don't reset start here.  We don't have to
            // because the next time that a font is found, start will
            // be set to i since prefFont is NULL. (See below in the
            // next if/else.)
            prevFont = nsnull;
        }

        // Measure the size of the unknown glyph char and jump
        // back to the beginning of the loop. (straight out of
        // pango_xft_font_get_glyph_extents for consistency)
        rawWidth += mMiniFontWidth * 2 + mMiniFontPadding * 5;

        if (rawAscent < mMiniFont->ascent)
            rawAscent = mMiniFont->ascent;
        if (rawDescent < mMiniFont->descent)
            rawDescent = mMiniFont->descent;

        // Jump to the top of the loop
        continue;


    FoundFont:

        if (prevFont) {
            if (currFont != prevFont) {
                rawWidth += prevFont->GetWidth16(&aString[start], i - start);
                nscoord tmpMaxAscent = prevFont->GetMaxAscent();
                nscoord tmpMaxDescent = prevFont->GetMaxDescent();
                if (rawAscent < tmpMaxAscent)
                    rawAscent = tmpMaxAscent;
                if (rawDescent < tmpMaxDescent)
                    rawDescent = tmpMaxDescent;
                prevFont = currFont;
                start = i;
            }
        }
        else {
            prevFont = currFont;
            start = i;
        }
    }

    if (prevFont) {
        rawWidth += prevFont->GetWidth16(&aString[start], i - start);
        nscoord tmpMaxAscent = prevFont->GetMaxAscent();
        nscoord tmpMaxDescent = prevFont->GetMaxDescent();
        if (rawAscent < tmpMaxAscent)
            rawAscent = tmpMaxAscent;
        if (rawDescent < tmpMaxDescent)
            rawDescent = tmpMaxDescent;
    }

    float P2T;
    mDeviceContext->GetDevUnitsToAppUnits(P2T);

    aDimensions.width = NSToCoordRound(rawWidth * P2T);
    aDimensions.ascent = NSToCoordRound(rawAscent * P2T);
    aDimensions.descent = NSToCoordRound(rawDescent * P2T);

    if (nsnull != aFontID)
        *aFontID = 0;

    return NS_OK;
}

nsresult
nsFontMetricsXft::GetTextDimensions(const char*         aString,
                                    PRInt32             aLength,
                                    PRInt32             aAvailWidth,
                                    PRInt32*            aBreaks,
                                    PRInt32             aNumBreaks,
                                    nsTextDimensions&   aDimensions,
                                    PRInt32&            aNumCharsFit,
                                    nsTextDimensions&   aLastWordDimensions,
                                    PRInt32*            aFontID,
                                    nsRenderingContextGTK *aContext)
{
    NS_NOTREACHED("GetTextDimensions");
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsFontMetricsXft::GetTextDimensions(const PRUnichar*    aString,
                                    PRInt32             aLength,
                                    PRInt32             aAvailWidth,
                                    PRInt32*            aBreaks,
                                    PRInt32             aNumBreaks,
                                    nsTextDimensions&   aDimensions,
                                    PRInt32&            aNumCharsFit,
                                    nsTextDimensions&   aLastWordDimensions,
                                    PRInt32*            aFontID,
                                    nsRenderingContextGTK *aContext)
{
    NS_NOTREACHED("GetTextDimensions");
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsFontMetricsXft::DrawString(const char *aString, PRUint32 aLength,
                             nscoord aX, nscoord aY,
                             const nscoord* aSpacing,
                             nsRenderingContextGTK *aContext,
                             nsDrawingSurfaceGTK *aSurface)
{
    // set up our colors and clip regions
    XftDraw *draw;
    XftColor color;
    PrepareToDraw(aContext, aSurface, &draw, color);

    // use the fast path if there's no spacing
    if (!aSpacing) {
        // translate these into native coords
        nscoord x = aX;
        nscoord y = aY;
        aContext->GetTranMatrix()->TransformCoord(&x, &y);

        // casting away const here for aString - should be safe
        XftDrawString8(draw, &color, mWesternFont->GetXftFont(),
                       x, y, (FcChar8 *)aString, aLength);
        
        return NS_OK;
    }

    // if we got this far, we need to use spacing - much more
    // complicated

    // set up our spec buffer
    PRBool useLocalSpecBuffer = PR_FALSE;
    XftCharFontSpec *specBuffer = gFontSpecBuffer;

    // see if we have to allocate our own spec buffer
    if (aLength > FONT_SPEC_BUFFER_SIZE) {
        useLocalSpecBuffer = PR_TRUE;
        specBuffer = new XftCharFontSpec[aLength];

        if (!specBuffer)
            return NS_ERROR_FAILURE;
    }

    // our X offset when drawing
    nscoord xOffset = 0;

    // walk down the string, positioning everything in the right
    // location
    for (PRUint32 i = 0; i < aLength; ++i) {
        nscoord x = aX + xOffset;
        nscoord y = aY;
        char c = aString[i];

        // convert this into device coordinates
        aContext->GetTranMatrix()->TransformCoord(&x, &y);

        specBuffer[i].font = mWesternFont->GetXftFont();
        specBuffer[i].ucs4 = c;
        specBuffer[i].x = x;
        specBuffer[i].y = y;

        // bump up our spacing
        xOffset += *aSpacing++;
    }

    // go forth and blit!
    XftDrawCharFontSpec(draw, &color, specBuffer, aLength);

    // only free memory if we didn't use the static buffer
    if (useLocalSpecBuffer)
        delete [] specBuffer;

    return NS_OK;
}

nsresult
nsFontMetricsXft::DrawString(const PRUnichar* aString, PRUint32 aLength,
                             nscoord aX, nscoord aY,
                             PRInt32 aFontID,
                             const nscoord* aSpacing,
                             nsRenderingContextGTK *aContext,
                             nsDrawingSurfaceGTK *aSurface)
{
    // set up our colors and clip regions
    XftDraw *draw;
    XftColor color;
    PrepareToDraw(aContext, aSurface, &draw, color);

    // We use XftDrawCharFontSpec to do our drawing here so that even
    // though we might have many different fonts at many different
    // locations due to spacing, we can do it all in one shot.  This
    // is much faster from a protocol and code standpoint.

    // If we've been asked to draw more than the buffer size, allocate
    // a temporary buffer.  This avoids allocations for every draw.
    PRBool useLocalSpecBuffer = PR_FALSE;
    XftCharFontSpec *specBuffer = gFontSpecBuffer;

    // The spec buffer length might change if there's a character that
    // we can't render in the middle of the string.  Because of this,
    // we keep track of the size of the buffer seperately.
    PRUint32 specBufferLen = 0;

    if (aLength > FONT_SPEC_BUFFER_SIZE) {
        useLocalSpecBuffer = PR_TRUE;
        specBuffer = new XftCharFontSpec[aLength];

        if (!specBuffer)
            return NS_ERROR_FAILURE;
    }

    float P2T;
    mDeviceContext->GetDevUnitsToAppUnits(P2T);

    // Set up the mini font, if it hasn't already been done before.
    // If it fails, we'll just substitute the space character below.

    // When we walk the list of fonts this is the offset from the
    // original x coordinate that we need to render.
    nscoord xOffset = 0;

    for (PRUint32 i = 0; i < aLength; ++i) {
        PRUnichar c = aString[i];
        nsFontXft *currFont = nsnull;
        nsFontXft *font = nsnull;

        // position in X is the location offset in the string plus
        // whatever offset is required for the spacing argument
        nscoord x = aX + xOffset;
        nscoord y = aY;

        // convert this into device coordinates
        aContext->GetTranMatrix()->TransformCoord(&x, &y);

        for (PRInt32 j = 0, end = mLoadedFonts.Count(); j < end; ++j) {
            font = (nsFontXft *)mLoadedFonts.ElementAt(j);
            if (FcCharSetHasChar(font->mCharset, c)) {
                currFont = font;
                goto FoundFont; // for speed -- avoid "if" statement
            }
        }

        // If we got this far it means that we weren't able to find a
        // font that supports this character.  If the minifont hasn't
        // been loaded (it should have been at the top of this
        // function) just draw a space instead.
        if (!mMiniFont) {
            c = UCS2_SPACE;
            currFont = FindFont(c);
            goto FoundFont;
        }

        // Draw the unknown glyph, advance our x offset and jump to
        // the top of the loop.

        // Note that we offset the height of this drawing so that it's
        // drawn on the average baseline of the font, not the actual
        // baseline since a lot of characters actually go below the
        // baseline and if the unknown glyph box is on the baseline it
        // looks higher than all the other fonts in a string.
        DrawUnknownGlyph(c, x, y + mMiniFontYOffset, &color, draw);

        if (aSpacing) {
            xOffset += *aSpacing++;
        }
        else {
            xOffset += NSToCoordRound((mMiniFontWidth * 2 +
                                       mMiniFontPadding * 5) * P2T);
        }

        // jump to the top of the loop
        continue;

    FoundFont:
        // use current found font to render this character
        specBuffer[specBufferLen].font = currFont->GetXftFont();
        specBuffer[specBufferLen].ucs4 = c;
        specBuffer[specBufferLen].x = x;
        specBuffer[specBufferLen].y = y;
        ++specBufferLen;

        if (aSpacing)
            xOffset += *aSpacing++;
        else
            xOffset += NSToCoordRound(currFont->GetWidth16(&c, 1) * P2T);
    }

    // Go forth and blit!
    XftDrawCharFontSpec(draw, &color, specBuffer, specBufferLen);

    // only free memory if we didn't use the static buffer
    if (useLocalSpecBuffer)
        delete [] specBuffer;

    return NS_OK;
}

#ifdef MOZ_MATHML

nsresult
nsFontMetricsXft::GetBoundingMetrics(const char *aString, PRUint32 aLength,
                                     nsBoundingMetrics &aBoundingMetrics,
                                     nsRenderingContextGTK *aContext)
{
    NS_NOTREACHED("GetBoundingMetrics");
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsFontMetricsXft::GetBoundingMetrics(const PRUnichar *aString,
                                     PRUint32 aLength,
                                     nsBoundingMetrics &aBoundingMetrics,
                                     PRInt32 *aFontID,
                                     nsRenderingContextGTK *aContext)
{
    NS_NOTREACHED("GetBoundingMetrics");
    return NS_ERROR_NOT_IMPLEMENTED;
}

#endif /* MOZ_MATHML */

GdkFont*
nsFontMetricsXft::GetCurrentGDKFont(void)
{
    return nsnull;
}

PRUint32
nsFontMetricsXft::GetHints(void)
{
    return 0;
}

nsresult
nsFontMetricsXft::RealizeFont(void)
{
    // make sure that the western font is loaded since we need that to
    // get the font metrics
    mWesternFont = FindFont('a');
    if (!mWesternFont)
        return NS_ERROR_FAILURE;

    return CacheFontMetrics();
}

nsresult
nsFontMetricsXft::CacheFontMetrics(void)
{
    // Get our scale factor
    float f;
    float val;
    mDeviceContext->GetDevUnitsToAppUnits(f);
    
    // Get our font face
    FT_Face face;
    TT_OS2 *os2;
    XftFont *xftFont = mWesternFont->GetXftFont();

    face = XftLockFace(xftFont);
    os2 = (TT_OS2 *) FT_Get_Sfnt_Table(face, ft_sfnt_os2);

    // mEmHeight (size in pixels of EM height)
    int size;
    if (FcPatternGetInteger(mWesternFont->mPattern, FC_PIXEL_SIZE, 0, &size) !=
        FcResultMatch) {
        size = 12;
    }
    mEmHeight = PR_MAX(1, nscoord(size * f));

    // mMaxAscent
    mMaxAscent = nscoord(xftFont->ascent * f);

    // mMaxDescent
    mMaxDescent = nscoord(xftFont->descent * f);

    nscoord lineHeight = mMaxAscent + mMaxDescent;

    // mLeading (needs ascent and descent and EM height) 
    if (lineHeight > mEmHeight)
        mLeading = lineHeight - mEmHeight;
    else
        mLeading = 0;

    // mMaxHeight (needs ascent and descent)
    mMaxHeight = lineHeight;

    // mEmAscent (needs maxascent, EM height, ascent and descent)
    mEmAscent = nscoord(mMaxAscent * mEmHeight / lineHeight);

    // mEmDescent (needs EM height and EM ascent
    mEmDescent = mEmHeight - mEmAscent;

    // mMaxAdvance
    mMaxAdvance = nscoord(xftFont->max_advance_width * f);

    // mSpaceWidth (width of a space)
    gint rawWidth;
    PRUnichar unispace(' ');
    rawWidth = RawGetWidth(&unispace, 1);
    mSpaceWidth = NSToCoordRound(rawWidth * f);

    // mAveCharWidth (width of an 'average' char)
    PRUnichar xUnichar('x');
    rawWidth = RawGetWidth(&xUnichar, 1);
    mAveCharWidth = NSToCoordRound(rawWidth * f);

    // mXHeight (height of an 'x' character)
    if (FcCharSetHasChar(mWesternFont->mCharset, xUnichar)) {
        XGlyphInfo extents;
        XftTextExtents16(GDK_DISPLAY(), xftFont, &xUnichar, 1, &extents);
        mXHeight = extents.height;
    }
    else {
        // 56% of ascent, best guess for non-true type or asian fonts
        mXHeight = nscoord(((float)mMaxAscent) * 0.56);
    }
    mXHeight = nscoord(mXHeight * f);

    // mUnderlineOffset (offset for underlines)
    val = face->underline_position >> 16;
    if (val) {
        mUnderlineOffset = NSToIntRound(val * f);
    }
    else {
        mUnderlineOffset =
            -NSToIntRound(PR_MAX(1, floor(0.1 * xftFont->height + 0.5)) * f);
    }

    // mUnderlineSize (thickness of an underline)
    val = face->underline_thickness >> 16;
    if (val) {
        mUnderlineSize = nscoord(PR_MAX(f, NSToIntRound(val * f)));
    }
    else {
        mUnderlineSize =
            NSToIntRound(PR_MAX(1, floor(0.05 * xftFont->height + 0.5)) * f);
    }

    // mSuperscriptOffset
    if (os2 && os2->ySuperscriptYOffset) {
        val = os2->ySuperscriptYOffset >> 16;
        mSuperscriptOffset = nscoord(PR_MAX(f, NSToIntRound(val * f)));
    }
    else {
        mSuperscriptOffset = mXHeight;
    }

    // mSubscriptOffset
    if (os2 && os2->ySubscriptYOffset) {
        val = os2->ySubscriptYOffset >> 16;
        mSubscriptOffset = nscoord(PR_MAX(f, NSToIntRound(val * f)));
    }
    else {
        mSubscriptOffset = mXHeight;
    }

    // mStrikeoutOffset
    mStrikeoutOffset = NSToCoordRound(mXHeight / 2.0);

    // mStrikeoutSize
    mStrikeoutSize = mUnderlineSize;

    XftUnlockFace(xftFont);

    return NS_OK;
}

nsFontXft *
nsFontMetricsXft::FindFont(PRUnichar aChar)
{

    // If mPattern is null, set up the base bits of it so we can
    // match.  If we need to match later we don't have to set it up
    // again.
    if (!mPattern) {
        SetupFCPattern();
        // did we fail to set it up?
        if (!mPattern)
            return nsnull;
    }

    // go forth and find us some fonts
    if (!mMatched)
        DoMatch();

    // Now that we have the fonts loaded and ready to run, return the
    // font in our loaded list that supports the character
    for (PRInt32 i = 0, end = mLoadedFonts.Count(); i < end; ++i) {
        nsFontXft *font = (nsFontXft *)mLoadedFonts.ElementAt(i);
        if (FcCharSetHasChar(font->mCharset, aChar))
            return font;
    }

    // If we got this far, none of the fonts support this character.
    // Return nothing.
    return nsnull;
}

void
nsFontMetricsXft::SetupFCPattern(void)
{
    mPattern = FcPatternCreate();
    if (!mPattern)
        return;

    // XXX need to add user defined family

    // Add CSS names - walk the list of fonts, adding the generic as
    // the last font
    for (int i=0; i < mFontList.Count(); ++i) {
        // if this was a generic name, break out of the loop since we
        // don't want to add it to the pattern yet
        if (mFontIsGeneric[i])
            break;

        nsCString *familyName = mFontList.CStringAt(i);
        AddFFRE(mPattern, familyName, PR_FALSE);
    }

    // Add the language group.  Note that we do this before adding any
    // generics.  That's because the language is more important than
    // any generic font.
    AddLangGroup (mPattern, mLangGroup);

    // If there's a generic add a pref for the generic if there's one
    // set.
    if (mGenericFont) {
        nsCString name;
        name += "font.name.";
        name += mGenericFont->get();
        name += ".";

        nsString langGroup;
        mLangGroup->ToString(langGroup);

        name.AppendWithConversion(langGroup);

        nsCOMPtr<nsIPref> pref;
        pref = do_GetService(NS_PREF_CONTRACTID);
        if (pref) {
            nsresult rv;
            nsXPIDLCString value;
            rv = pref->GetCharPref(name.get(), getter_Copies(value));

            // we ignore prefs that have three hypens since they are X
            // style prefs.
            if (FFRECountHyphens(value) < 3) {
                nsCString tmpstr;
                tmpstr.Append(value);
                AddFFRE(mPattern, &tmpstr, PR_FALSE);
            }
        }
    }

    // Add the generic if there is one.
    if (mGenericFont)
        AddFFRE(mPattern, mGenericFont, PR_FALSE);

    // add the pixel size
    FcPatternAddInteger(mPattern, FC_PIXEL_SIZE, mPixelSize);

    // Add the slant type
    FcPatternAddInteger(mPattern, FC_SLANT,
                        CalculateSlant(mFont->style));

    // Add the weight
    FcPatternAddInteger(mPattern, FC_WEIGHT,
                        CalculateWeight(mFont->weight));

    // Set up the default substitutions for this font
    FcConfigSubstitute(0, mPattern, FcMatchPattern);
    XftDefaultSubstitute(GDK_DISPLAY(), DefaultScreen(GDK_DISPLAY()),
                         mPattern);
}

void
nsFontMetricsXft::DoMatch(void)
{
    FcFontSet *set = nsnull;
    // now that we have a pattern we can match
    FcCharSet *charset;
    FcResult   result;

    set = FcFontSort(0, mPattern, FcTrue, &charset, &result);

    // we don't need the charset since we're going to break it up into
    // specific fonts anyway
    if (charset)
        FcCharSetDestroy(charset);

    // did we not match anything?
    if (!set) {
        goto loser;
    }

    // Create a list of new font objects based on the fonts returned
    // as part of the query
    for (int i=0; i < set->nfont; ++i) {
        nsFontXft *font = new nsFontXft(mPattern, set->fonts[i]);
        if (!font)
            goto loser;

        // append this font to our list of loaded fonts
        mLoadedFonts.AppendElement((void *)font);
    }

    // we're done with the set now
    FcFontSetDestroy(set);
    set = nsnull;

    // Done matching!
    mMatched = PR_TRUE;
    return;

    // if we got this far, something went terribly wrong
 loser:
    NS_WARNING("nsFontMetricsXft::DoMatch failed to match anything");

    if (set)
        FcFontSetDestroy(set);

    for (PRInt32 i = mLoadedFonts.Count() - 1; i >= 0; --i) {
        nsFontXft *font = (nsFontXft *)mLoadedFonts.ElementAt(i);
        mLoadedFonts.RemoveElementAt(i);
        delete font;
    }
}

gint
nsFontMetricsXft::RawGetWidth(const PRUnichar* aString, PRUint32 aLength)
{
    // Walk the list of loaded fonts, looking for a font that supports
    // this character, using this well tested algorithm.
    nsFontXft* prevFont = nsnull;
    gint rawWidth = 0;
    PRUint32 start = 0;
    PRUint32 i;

    // Before we do anything make sure the minifont has been loaded in
    // case we stumble across any unknown characters.  If this fails,
    // we will jsut substitute space chars for any unknown characters
    // below.
    if (!mMiniFont)
        SetupMiniFont();

    for (i = 0; i < aLength; ++i) {
        PRUnichar c = aString[i];
        nsFontXft *currFont = nsnull;
        nsFontXft *font = nsnull;

        for (PRInt32 j = 0, end = mLoadedFonts.Count(); j < end; ++j) {
            font = (nsFontXft *)mLoadedFonts.ElementAt(j);
            if (FcCharSetHasChar(font->mCharset, c)) {
                currFont = font;
                goto FoundFont; // for speed -- avoid "if" statement
            }
        }

        // If for some reason the mini-font isn't found, just measure
        // a space character instead.
        if (!mMiniFont) {
            c = UCS2_SPACE;
            currFont = FindFont(c);
            goto FoundFont;
        }

        // If we got here it means that we couldn't find a font for
        // this character so we need to measure the unknown glyph box,
        // after measuring any left over text.
        if (prevFont) {
            rawWidth += prevFont->GetWidth16(&aString[start], i - start);
            // Unset prevFont so that we will automatically start
            // measuring from the next character in the string.  Note
            // that we don't reset start here.  We don't have to
            // because the next time that a font is found, start will
            // be set to i since prefFont is NULL. (See below in the
            // next if/else.)
            prevFont = nsnull;
        }

        // Measure the size of the unknown glyph char and jump
        // back to the beginning of the loop. (straight out of
        // pango_xft_font_get_glyph_extents for consistency)
        rawWidth += mMiniFontWidth * 2 + mMiniFontPadding * 5;

        // Jump to the top of the loop
        continue;

    FoundFont:
        // XXX avoid this test by duplicating code -- erik
        if (prevFont) {
            if (currFont != prevFont) {
                rawWidth += prevFont->GetWidth16(&aString[start], i - start);
                prevFont = currFont;
                start = i;
            }
        }
        else {
            prevFont = currFont;
            start = i;
        }
    }

    if (prevFont) {
        rawWidth += prevFont->GetWidth16(&aString[start], i - start);
    }

    return rawWidth;
}

nsresult
nsFontMetricsXft::SetupMiniFont(void)
{
    FcPattern *pattern = nsnull;
    XftFont *font = nsnull;
    XftFont *xftFont = mWesternFont->GetXftFont();
    FcPattern *pat = nsnull;
    nsresult rv = NS_ERROR_FAILURE;

    pattern = FcPatternCreate();
    if (!pattern)
        return NS_ERROR_FAILURE;

    FcPatternAddString(pattern, FC_FAMILY, (FcChar8 *)"monospace");

    FcPatternAddInteger(pattern, FC_PIXEL_SIZE, 
                        (int)(0.5 * mPixelSize));

    FcPatternAddInteger(pattern, FC_WEIGHT,
                        CalculateWeight(mFont->weight));

    FcConfigSubstitute(0, pattern, FcMatchPattern);
    XftDefaultSubstitute(GDK_DISPLAY(), DefaultScreen(GDK_DISPLAY()),
                         pattern);

    FcFontSet *set = nsnull;
    FcResult res;
    
    set = FcFontSort(0, pattern, FcTrue, NULL, &res);
    if (!set)
        goto end;

    pat = FcFontRenderPrepare(0, pattern, set->fonts[0]);
    if (!pat)
        goto end;

    font = XftFontOpenPattern(GDK_DISPLAY(), pat);
    if (!font)
        goto end;

    // the font owns the pattern now
    pat = nsnull;

    mMiniFont = font;

    // now that the font has been loaded, measure the fonts to find
    // the bounds
    for (int i=0; i < 16; ++i) {
        // create a little mini-string that contains what we want to
        // measure.
        char c = i < 10 ? '0' + i : 'A' + i - 10;
        char str[2];
        str[0] = c;
        str[1] = '\0';

        XGlyphInfo extents;
        XftTextExtents8(GDK_DISPLAY(), mMiniFont,
                        (FcChar8 *)str, 1, &extents);

        mMiniFontWidth = PR_MAX (mMiniFontWidth, extents.width);
        mMiniFontHeight = PR_MAX (mMiniFontHeight, extents.height);
    }

    mMiniFontPadding = PR_MAX(mMiniFontHeight / 10, 1);

    mMiniFontYOffset = ((xftFont->ascent + xftFont->descent) -
                        (mMiniFontHeight * 2 + mMiniFontPadding * 5)) / 2;

    rv = NS_OK;

 end:
    if (pat)
        FcPatternDestroy(pat);
    if (pattern)
        FcPatternDestroy(pattern);
    if (set)
        FcFontSetSortDestroy(set);

    if (NS_FAILED(rv)) {
        if (font)
            XftFontClose(GDK_DISPLAY(), font);
    }

    return rv;
}

nsresult
nsFontMetricsXft::DrawUnknownGlyph(PRUnichar  aChar,
                                   nscoord    aX,
                                   nscoord    aY,
                                   XftColor  *aColor,
                                   XftDraw   *aDraw)
{
    nsresult rv;

    if (!mMiniFont) {
        rv = SetupMiniFont();
        if (NS_FAILED(rv))
            return rv;
    }

    int width,height;

    // Two characters + padding around the fonts.  From left to right
    // it would be one padding for the glyph box, one for the padding
    // in between the box and the letter, one in between the left and
    // right letter, one right of the right most letter and one for
    // the rightmost part of the box.  This pattern is throughout the
    // rest of this function.
    width = mMiniFontWidth * 2 + mMiniFontPadding * 5;
    height = mMiniFontHeight * 2 + mMiniFontPadding * 5;

    // Draw an outline of a box.  We do with this with four calls
    // since with one call it would fill in the box.
    
    // horizontal lines
    XftDrawRect(aDraw, aColor,
                aX, aY - height,
                width, mMiniFontPadding);
    XftDrawRect(aDraw, aColor,
                aX, aY - mMiniFontPadding,
                width, mMiniFontPadding);

    // vertical lines
    XftDrawRect(aDraw, aColor,
                aX,
                aY - height + mMiniFontPadding,
                mMiniFontPadding, height - mMiniFontPadding * 2);
    XftDrawRect(aDraw, aColor,
                aX + width - mMiniFontPadding,
                aY - height + mMiniFontPadding,
                mMiniFontPadding, height - mMiniFontPadding * 2);

    // now draw the characters
    char buf[5];
    PR_snprintf (buf, sizeof(buf), "%04X", aChar);

    // Draw the four characters
    for (int i=0; i < 4; ++i) {
        XftDrawString8(aDraw, aColor, mMiniFont,
                       aX + mMiniFontPadding * 2,
                       aY - mMiniFontHeight - mMiniFontPadding * 3,
                       (FcChar8 *)&buf[0], 1);
        XftDrawString8(aDraw, aColor, mMiniFont,
                       aX + mMiniFontWidth + mMiniFontPadding * 3,
                       aY - mMiniFontHeight - mMiniFontPadding * 3,
                       (FcChar8 *)&buf[1], 1);
        XftDrawString8(aDraw, aColor, mMiniFont,
                       aX + mMiniFontPadding * 2,
                       aY - mMiniFontPadding * 2,
                       (FcChar8 *)&buf[2], 1);
        XftDrawString8(aDraw, aColor, mMiniFont,
                       aX + mMiniFontWidth + mMiniFontPadding * 3,
                       aY - mMiniFontPadding * 2,
                       (FcChar8 *)&buf[3], 1);
    }
    return NS_OK;
}

void
nsFontMetricsXft::PrepareToDraw(nsRenderingContextGTK *aContext,
                                nsDrawingSurfaceGTK *aSurface,
                                XftDraw **aDraw, XftColor &aColor)
{
    // Set our color
    nscolor rccolor;

    aContext->GetColor(rccolor);

    aColor.pixel = gdk_rgb_xpixel_from_rgb(NS_TO_GDK_RGB(rccolor));
    aColor.color.red = (NS_GET_R(rccolor) << 8) | NS_GET_R(rccolor);
    aColor.color.green = (NS_GET_G(rccolor) << 8) | NS_GET_G(rccolor);
    aColor.color.blue = (NS_GET_B(rccolor) << 8) | NS_GET_B(rccolor);
    aColor.color.alpha = 0xffff;

    *aDraw = aSurface->GetXftDraw();

    nsCOMPtr<nsIRegion> lastRegion;
    nsCOMPtr<nsIRegion> clipRegion;

    aSurface->GetLastXftClip(getter_AddRefs(lastRegion));
    aContext->GetClipRegion(getter_AddRefs(clipRegion));

    // avoid setting the clip, if possible
    if (!lastRegion || !clipRegion || !lastRegion->IsEqual(*clipRegion)) {
        aSurface->SetLastXftClip(clipRegion);

        GdkRegion *rgn = nsnull;
        clipRegion->GetNativeRegion((void *&)rgn);

#ifdef MOZ_WIDGET_GTK
        GdkRegionPrivate  *priv = (GdkRegionPrivate *)rgn;
        XftDrawSetClip(*aDraw, priv->xregion);
#endif

#ifdef MOZ_WIDGET_GTK2
        GdkRegionSetXftClip(rgn, *aDraw);
#endif
    }
}

/* static */
nsresult
nsFontMetricsXft::FamilyExists(nsIDeviceContext *aDevice,
                               const nsString &aName)
{
    if (!IsASCIIFontName(aName))
        return NS_ERROR_FAILURE;

    NS_ConvertUCS2toUTF8 name(aName);

    FcFontSet *set = nsnull;
    FcObjectSet *os = nsnull;

    FcPattern *pat = FcPatternCreate();
    if (!pat)
        return NS_ERROR_FAILURE;

    nsresult rv = NS_ERROR_FAILURE;
    
    // Build a list of familes and walk the list looking to see if we
    // have it.
    os = FcObjectSetBuild(FC_FAMILY, 0);
    if (!os)
        goto end;

    set = FcFontList(0, pat, os);

    if (!set || !set->nfont)
        goto end;

    for (int i = 0; i < set->nfont; ++i) {
        const char *tmpname = NULL;
        if (FcPatternGetString(set->fonts[i], FC_FAMILY, 0,
                               (FcChar8 **)&tmpname) != FcResultMatch) {
            continue;
        }

        // do they match?
        if (!Compare(nsDependentCString(tmpname), name,
                     nsCaseInsensitiveCStringComparator())) {
            rv = NS_OK;
            break;
        }
    }

 end:
    if (set)
        FcFontSetDestroy(set);
    if (os)
        FcObjectSetDestroy(os);

    FcPatternDestroy(pat);

    return rv;
}

/* static */
PRBool
nsFontMetricsXft::EnumFontCallback(const nsString &aFamily, PRBool aIsGeneric,
                                   void *aData)
{
    // make sure it's an ascii name, if not then return and continue
    // enumerating
    if (!IsASCIIFontName(aFamily))
        return PR_TRUE;

    nsCAutoString name;
    name.AssignWithConversion(aFamily.get());
    ToLowerCase(name);
    nsFontMetricsXft *metrics = (nsFontMetricsXft *)aData;
    metrics->mFontList.AppendCString(name);
    metrics->mFontIsGeneric.AppendElement((void *)aIsGeneric);
    if (aIsGeneric) {
        metrics->mGenericFont = 
            metrics->mFontList.CStringAt(metrics->mFontList.Count() - 1);
        return PR_FALSE; // stop processing
    }

    return PR_TRUE; // keep processing
}

// nsFontEnumeratorXft class

nsFontEnumeratorXft::nsFontEnumeratorXft()
{
    NS_INIT_ISUPPORTS();
}

NS_IMPL_ISUPPORTS1(nsFontEnumeratorXft, nsIFontEnumerator)

NS_IMETHODIMP
nsFontEnumeratorXft::EnumerateAllFonts(PRUint32 *aCount, PRUnichar ***aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = nsnull;
    NS_ENSURE_ARG_POINTER(aCount);
    *aCount = 0;

    return EnumFontsXft(nsnull, nsnull, aCount, aResult);

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsFontEnumeratorXft::EnumerateFonts(const char *aLangGroup,
                                    const char *aGeneric,
                                    PRUint32 *aCount, PRUnichar ***aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = nsnull;
    NS_ENSURE_ARG_POINTER(aCount);
    *aCount = 0;
    NS_ENSURE_ARG_POINTER(aGeneric);
    NS_ENSURE_ARG_POINTER(aLangGroup);

    nsCOMPtr<nsIAtom> langGroup = getter_AddRefs(NS_NewAtom(aLangGroup));

    return EnumFontsXft(langGroup, aGeneric, aCount, aResult);
}

NS_IMETHODIMP
nsFontEnumeratorXft::HaveFontFor(const char *aLangGroup, PRBool *aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = PR_FALSE;
    NS_ENSURE_ARG_POINTER(aLangGroup);

    *aResult = PR_TRUE; // always return true for now.
    // Finish me - ftang
    return NS_OK;
}

NS_IMETHODIMP
nsFontEnumeratorXft::UpdateFontList(PRBool *_retval)
{
    *_retval = PR_FALSE; // always return false for now
    return NS_OK;
}

// class nsFontXft

nsFontXft::nsFontXft(FcPattern *aPattern, FcPattern *aFontName)
{
    // save our pattern - we own it now
    mPattern = aPattern;
    mFontName = aFontName;
    // don't destroy it out from under us
    FcPatternReference(aPattern);
    FcPatternReference(mFontName);

    mXftFont = nsnull;

    // set up our charset
    mCharset = nsnull;
    FcCharSet *charset = nsnull;

    // this references an internal charset, we need to copy it
    FcPatternGetCharSet(aFontName, FC_CHARSET, 0, &charset);
    if (charset)
        mCharset = FcCharSetCopy(charset);
}

nsFontXft::~nsFontXft()
{
    if (mXftFont)
        XftFontClose(GDK_DISPLAY(), mXftFont);
    if (mCharset)
        FcCharSetDestroy(mCharset);
    if (mPattern)
        FcPatternDestroy(mPattern);
    if (mFontName)
        FcPatternDestroy(mFontName);
}

XftFont *
nsFontXft::GetXftFont(void)
{
    if (!mXftFont) {
        FcPattern *pat = FcFontRenderPrepare(0, mPattern, mFontName);
        if (!pat)
            return nsnull;

        mXftFont = XftFontOpenPattern(GDK_DISPLAY(), pat);
        if (!mXftFont)
            FcPatternDestroy(pat);
    }

    return mXftFont;
}

gint
nsFontXft::GetWidth8(const char *aString, PRUint32 aLength)
{
    XGlyphInfo glyphInfo;
    if (!mXftFont)
        GetXftFont();

    // casting away const for aString but it  should be safe
    XftTextExtents8(GDK_DISPLAY(), mXftFont, (FcChar8 *)aString,
                    aLength, &glyphInfo);

    return glyphInfo.xOff;
}

gint
nsFontXft::GetWidth16(const PRUnichar *aString, PRUint32 aLength)
{
    XGlyphInfo glyphInfo;
    if (!mXftFont)
        GetXftFont();

    // casting away const here on aString - should be safe
    XftTextExtents16(GDK_DISPLAY(), mXftFont, (FcChar16 *)aString,
                     aLength, &glyphInfo);

    return glyphInfo.xOff;
}

#ifdef MOZ_MATHML
void
nsFontXft::GetBoundingMetrics8 (const char *aString, PRUint32 aLength,
                                nsBoundingMetrics &aBoundingMetrics)
{
    NS_NOTREACHED("GetBoundingMetrics8");
}

void
nsFontXft::GetBoundingMetrics16 (const PRUnichar *aString,
                                 PRUint32 aLength,
                                 nsBoundingMetrics &aBoundingMetrics)
{
    NS_NOTREACHED("GetBoundingMetrics16");
}
#endif /* MOZ_MATHML */

PRInt16
nsFontXft::GetMaxAscent(void)
{
    if (!mXftFont)
        GetXftFont();

    return mXftFont->ascent;
}

PRInt16
nsFontXft::GetMaxDescent(void)
{
    if (!mXftFont)
        GetXftFont();

    return mXftFont->descent;
}

// Static functions

/* static */
int
CalculateSlant(PRUint8 aStyle)
{
    int fcSlant;

    switch(aStyle) {
    case NS_FONT_STYLE_ITALIC:
        fcSlant = FC_SLANT_ITALIC;
        break;
    case NS_FONT_STYLE_OBLIQUE:
        fcSlant = FC_SLANT_OBLIQUE;
        break;
    default:
        fcSlant = FC_SLANT_ROMAN;
        break;
    }

    return fcSlant;
}

/* static */
int
CalculateWeight (PRUint16 aWeight)
{
    /*
     * weights come in two parts crammed into one
     * integer -- the "base" weight is weight / 100,
     * the rest of the value is the "offset" from that
     * weight -- the number of steps to move to adjust
     * the weight in the list of supported font weights,
     * this value can be negative or positive.
     */
    PRInt32 baseWeight = (aWeight + 50) / 100;
    PRInt32 offset = aWeight - baseWeight * 100;

    /* clip weights to range 0 to 9 */
    if (baseWeight < 0)
        baseWeight = 0;
    if (baseWeight > 9)
        baseWeight = 9;

    /* Map from weight value to fcWeights index */
    static int fcWeightLookup[10] = {
        0, 0, 0, 0, 1, 1, 2, 3, 3, 4,
    };

    PRInt32 fcWeight = fcWeightLookup[baseWeight];

    /*
     * adjust by the offset value, make sure we stay inside the 
     * fcWeights table
     */
    fcWeight += offset;

    if (fcWeight < 0)
        fcWeight = 0;
    if (fcWeight > 4)
        fcWeight = 4;

    /* Map to final FC_WEIGHT value */
    static int fcWeights[5] = {
        FC_WEIGHT_LIGHT,      /* 0 */
        FC_WEIGHT_MEDIUM,     /* 1 */
        FC_WEIGHT_DEMIBOLD,	  /* 2 */
        FC_WEIGHT_BOLD,       /* 3 */
        FC_WEIGHT_BLACK,      /* 4 */
    };

    return fcWeights[fcWeight];

}

/* static */
void
AddLangGroup(FcPattern *aPattern, nsIAtom *aLangGroup)
{
    // Find the FC lang group for this lang group
    const PRUnichar *name;
    aLangGroup->GetUnicode(&name);
    nsCAutoString cname;
    cname.AssignWithConversion(nsDependentString(name));

    // see if the lang group needs to be translated from mozilla's
    // internal mapping into fontconfig's
    const struct MozXftLangGroup *langGroup;
    langGroup = FindFCLangGroup(cname);

    // if there's no lang group, just use the lang group as it was
    // passed to us
    //
    // we're casting away the const here for the strings - should be
    // safe.
    if (!langGroup)
        FcPatternAddString(aPattern, FC_LANG, (FcChar8 *)cname.get());
    else if (langGroup->XftLang) 
        FcPatternAddString(aPattern, FC_LANG, (FcChar8 *)langGroup->XftLang);
}

/* static */
void
AddFFRE(FcPattern *aPattern, nsCString *aFamily, PRBool aWeak)
{
    nsCAutoString family;
    FFREToFamily(*aFamily, family);

    FcValue v;
    v.type = FcTypeString;
    // casting away the const here, should be safe
    v.u.s = (FcChar8 *)family.get();

    if (aWeak)
        FcPatternAddWeak(aPattern, FC_FAMILY, v, FcTrue);
    else
        FcPatternAdd(aPattern, FC_FAMILY, v, FcTrue);
}

/* static */
void
FFREToFamily(nsACString &aFFREName, nsACString &oFamily)
{
  if (FFRECountHyphens(aFFREName) == 3) {
      PRInt32 familyHyphen = aFFREName.FindChar('-') + 1;
      PRInt32 registryHyphen = aFFREName.FindChar('-',familyHyphen);
      oFamily.Append(Substring(aFFREName, familyHyphen,
                               registryHyphen-familyHyphen));
  }
  else {
      oFamily.Append(aFFREName);
  }
}

/* static */
int
FFRECountHyphens (nsACString &aFFREName)
{
    int h = 0;
    PRInt32 hyphen = 0;
    while ((hyphen = aFFREName.FindChar('-', hyphen)) >= 0) {
        ++h;
        ++hyphen;
    }
    return h;
}

/* static */
int
CompareFontNames (const void* aArg1, const void* aArg2, void* aClosure)
{
    const PRUnichar* str1 = *((const PRUnichar**) aArg1);
    const PRUnichar* str2 = *((const PRUnichar**) aArg2);

    return nsCRT::strcmp(str1, str2);
}

PRBool
IsASCIIFontName(const nsString& aName)
{
    PRUint32 len = aName.Length();
    const PRUnichar* str = aName.get();
    for (PRUint32 i = 0; i < len; i++) {
        /*
         * X font names are printable ASCII, ignore others (for now)
         */
        if ((str[i] < 0x20) || (str[i] > 0x7E)) {
            return PR_FALSE;
        }
    }
  
    return PR_TRUE;
}

/* static */
nsresult
EnumFontsXft(nsIAtom* aLangGroup, const char* aGeneric,
             PRUint32* aCount, PRUnichar*** aResult)
{
    FcPattern   *pat = NULL;
    FcObjectSet *os  = NULL;
    FcFontSet   *fs  = NULL;
    nsresult     rv  = NS_ERROR_FAILURE;

    PRUnichar **array = NULL;
    PRUint32    narray;

    *aCount = 0;
    *aResult = nsnull;

    pat = FcPatternCreate();
    if (!pat)
        goto end;

    os = FcObjectSetBuild(FC_FAMILY, FC_FOUNDRY, 0);
    if (!os)
        goto end;

    // take the pattern and add the lang group to it
    AddLangGroup(pat, aLangGroup);

    // get the font list
    fs = FcFontList(0, pat, os);

    if (!fs)
        goto end;

    if (!fs->nfont) {
        rv = NS_OK;
        goto end;
    }

    array = NS_STATIC_CAST(PRUnichar **,
                           nsMemory::Alloc(fs->nfont * sizeof(PRUnichar *)));
    if (!array)
        goto end;

    narray = 0;

    for (int i=0; i < fs->nfont; ++i) {
        char *family;
        PRUnichar *name;

        // if there's no family, just move to the next iteration
        if (FcPatternGetString (fs->fonts[i], FC_FAMILY, 0,
                                (FcChar8 **) &family) != FcResultMatch) {
            continue;
        }

        name = NS_STATIC_CAST(PRUnichar *,
                              nsMemory::Alloc ((strlen (family) + 1)
                                               * sizeof (PRUnichar)));

        if (!name) {
            for (PRUint32 j = 0; j < narray; ++j)
                nsMemory::Free (array[j]);
            nsMemory::Free (array);
            goto end;
        }

        PRUnichar *r = name;
        for (char *f = family; *f; ++f)
            *r++ = *f;
        *r = '\0';

        array[narray++] = name;
    }

    NS_QuickSort(array, narray, sizeof (PRUnichar*),
                 CompareFontNames, nsnull);

    *aCount = narray;
    if (narray)
        *aResult = array;
    else
        nsMemory::Free(array);

    rv = NS_OK;

 end:
    if (pat)
        FcPatternDestroy(pat);
    if (os)
        FcObjectSetDestroy(os);
    if (fs)
        FcFontSetDestroy(fs);

    return rv;
}

/* static */
const MozXftLangGroup*
FindFCLangGroup (nsACString &aLangGroup)
{
    for (unsigned int i=0; i < NUM_XFT_LANG_GROUPS; ++i) {
        if (aLangGroup.Equals(MozXftLangGroups[i].mozLangGroup,
                              nsCaseInsensitiveCStringComparator())) {
            return &MozXftLangGroups[i];
        }
    }

    return nsnull;
}

#ifdef MOZ_WIDGET_GTK2
/* static */
void
GdkRegionSetXftClip(GdkRegion *aGdkRegion, XftDraw *aDraw)
{
    GdkRectangle  *rects   = nsnull;
    int            n_rects = 0;

    gdk_region_get_rectangles(aGdkRegion, &rects, &n_rects);

    XRectangle *xrects = g_new(XRectangle, n_rects);

    for (int i=0; i < n_rects; ++i) {
        xrects[i].x = CLAMP(rects[i].x, G_MINSHORT, G_MAXSHORT);
        xrects[i].y = CLAMP(rects[i].y, G_MINSHORT, G_MAXSHORT);
        xrects[i].width = CLAMP(rects[i].width, G_MINSHORT, G_MAXSHORT);
        xrects[i].height = CLAMP(rects[i].height, G_MINSHORT, G_MAXSHORT);
    }

    XftDrawSetClipRectangles(aDraw, 0, 0, xrects, n_rects);

    g_free(rects);
}
#endif /* MOZ_WIDGET_GTK2 */
