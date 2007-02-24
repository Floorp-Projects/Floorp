/* vim: set sw=4 sts=4 et cin: */
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
 * The Original Code is OS/2 code in Thebes.
 *
 * The Initial Developer of the Original Code is
 * Peter Weilbacher <mozilla@Weilbacher.org>.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  IBM Corp.  (code inherited from nsFontMetricsOS2 and OS2Uni)
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

#include "gfxContext.h"

#include "gfxOS2Platform.h"
#include "gfxOS2Surface.h"
#include "gfxOS2Fonts.h"

#include "nsIServiceManager.h"
#include "nsIPlatformCharset.h"

/**********************************************************************
 * helper macros and functions
 **********************************************************************/

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#define MK_RGB(r,g,b) ((LONG)r*255 * 65536) + ((LONG)g*255 * 256) + ((LONG)b*255)

// get the extent of bit of text and return it in aSizeL->cx and cy
BOOL GetTextExtentPoint32(HPS aPS, const char* aString, int aLength, PSIZEL aSizeL)
{
    BOOL rc = TRUE;
    POINTL ptls[5];

    aSizeL->cx = 0;

    while (aLength > 0 && rc == TRUE) {
        ULONG thislen = min(aLength, 512);
        rc = GpiQueryTextBox(aPS, thislen, (PCH)aString, 5, ptls);
        aSizeL->cx += ptls[TXTBOX_CONCAT].x;
        aLength -= thislen;
        aString += thislen;
    }

    aSizeL->cy = ptls[TXTBOX_TOPLEFT].y - ptls[TXTBOX_BOTTOMLEFT].y;
    return rc;
}

BOOL ExtTextOut(HPS aPS, int aX, int aY, UINT aFuOptions, const RECTL* aLprc,
                const char* aString, unsigned int aLength, const int* aPSpacing)
{
    long rc = GPI_OK;
    POINTL ptl = { aX, aY};

    GpiMove(aPS, &ptl);

    // GpiCharString has a max length of 512 chars at a time...
    while (aLength > 0 && rc == GPI_OK) {
        ULONG ulChunkLen = min(aLength, 512);
        if (aPSpacing) {
            rc = GpiCharStringPos(aPS, nsnull, CHS_VECTOR, ulChunkLen,
                                  (PCH)aString, (PLONG)aPSpacing);
            aPSpacing += ulChunkLen;
        } else {
            rc = GpiCharString(aPS, ulChunkLen, (PCH)aString);
        }
        aLength -= ulChunkLen;
        aString += ulChunkLen;
    }

    if (rc != GPI_ERROR)
        return TRUE;
    else
        return FALSE;
}


/**********************************************************************
 * class gfxOS2Font
 **********************************************************************/

gfxOS2Font::gfxOS2Font(const nsAString &aName, const gfxFontStyle *aFontStyle)
    : gfxFont(aName, aFontStyle), mMetrics(nsnull)
{
#ifdef DEBUG_thebes
    printf("gfxOS2Font::gfxOS2Font(\"%s\", aFontStyle)\n",
           NS_LossyConvertUTF16toASCII(aName).get());
#endif
}

gfxOS2Font::~gfxOS2Font()
{
#ifdef DEBUG_thebes
    printf("gfxOS2Font::~gfxOS2Font()\n");
#endif
    if (mMetrics)
        delete mMetrics;
    mMetrics = nsnull;
}

const gfxFont::Metrics& gfxOS2Font::GetMetrics()
{
    if (!mMetrics)
        ComputeMetrics();

    return *mMetrics;
}

void gfxOS2Font::ComputeMetrics()
{
    if (!mMetrics)
        mMetrics = new gfxFont::Metrics;

    HPS hps = WinGetPS(HWND_DESKTOP); // temporary PS to get font metrics
    FONTMETRICS fm;
    if (GpiQueryFontMetrics(hps, sizeof(fm), &fm)) {
        mMetrics->xHeight           = fm.lXHeight;
        mMetrics->superscriptOffset = fm.lSuperscriptYOffset;
        mMetrics->subscriptOffset   = fm.lSubscriptYOffset;
        mMetrics->strikeoutSize     = fm.lStrikeoutSize;
        mMetrics->strikeoutOffset   = fm.lStrikeoutPosition;
        mMetrics->underlineSize     = fm.lUnderscoreSize;
        mMetrics->underlineOffset   = fm.lUnderscorePosition;

        mMetrics->internalLeading   = fm.lInternalLeading;
        mMetrics->externalLeading   = fm.lExternalLeading;

        mMetrics->emHeight          = fm.lEmHeight;
        mMetrics->emAscent          = fm.lLowerCaseAscent;
        mMetrics->emDescent         = fm.lLowerCaseDescent;
        mMetrics->maxHeight         = fm.lXHeight + fm.lMaxAscender + fm.lMaxDescender;
        mMetrics->maxAscent         = fm.lMaxAscender;
        mMetrics->maxDescent        = fm.lMaxDescender;
        mMetrics->maxAdvance        = fm.lMaxCharInc;

        mMetrics->aveCharWidth      = fm.lAveCharWidth;

        // special handling for space width
        SIZEL space;
        GetTextExtentPoint32(hps, " ", 1, &space);
        mMetrics->spaceWidth = space.cx;
    }
    WinReleasePS(hps);
#ifdef DEBUG_thebes
    printf("gfxOS2Font::ComputeMetrics():\n"
           "  emHeight=%f\n"
           "  maxHeight=%f\n"
           "  aveCharWidth=%f\n"
           "  spaceWidth=%ld/%f\n",
           mMetrics->emHeight,
           mMetrics->maxHeight,
           mMetrics->aveCharWidth,
           fm.lMaxCharInc, mMetrics->spaceWidth);
#endif
}

double gfxOS2Font::GetWidth(HPS aPS, const char* aString, PRUint32 aLength)
{
#ifdef DEBUG_thebes
    printf("gfxOS2Font::GetWidth(ASCII)\n");
#endif
    SIZEL size;
    GetTextExtentPoint32(aPS, aString, aLength, &size);
    return (double)size.cx;
}

double gfxOS2Font::GetWidth(HPS aPS, const PRUnichar* aString, PRUint32 aLength)
{
#ifdef DEBUG_thebes
    printf("gfxOS2Font::GetWidth(WIDE)\n");
#endif
    nsAutoCharBuffer buffer;
    PRInt32 destLength = aLength;
    // XXX dummy convert!
    if (WideCharToMultiByte(0, aString, aLength, buffer, destLength))
        return 0.0; // somehow the conversion did not work

    SIZEL size;
    GetTextExtentPoint32(aPS, buffer.get(), destLength, &size);

    return (double)size.cx;
}


/**********************************************************************
 * class gfxOS2FontGroup
 **********************************************************************/
gfxOS2FontGroup::gfxOS2FontGroup(const nsAString& aFamilies, const gfxFontStyle* aStyle)
    : gfxFontGroup(aFamilies, aStyle)
{
#ifdef DEBUG_thebes
    printf("gfxOS2FontGroup::gfxOS2FontGroup(\"%s\", %#x)\n",
           NS_LossyConvertUTF16toASCII(aFamilies).get(), (unsigned)aStyle);
#endif

    //mFontCache.Init(25);

    ForEachFont(MakeFont, this);

    if (mGenericFamily.IsEmpty())
        FindGenericFontFromStyle(MakeFont, this);

    if (mFonts.Length() == 0) {
        // Should append default GUI font if there are no available fonts.
        // We use WarpSans as in the default case in nsSystemFontsOS2.
        nsAutoString defaultFont(NS_LITERAL_STRING("WarpSans"));
        MakeFont(defaultFont, mGenericFamily, this);
    }
}

gfxOS2FontGroup::~gfxOS2FontGroup()
{
#ifdef DEBUG_thebes
    printf("gfxOS2FontGroup::~gfxOS2FontGroup()\n");
#endif
}

// gfxWrapperTextRun, adapted the code from gfxAtsui/gfxWindowsFonts.cpp
class gfxWrapperTextRun : public gfxTextRun {
public:
    gfxWrapperTextRun(gfxOS2FontGroup *aGroup, const PRUint8* aString, PRUint32 aLength,
                      gfxTextRunFactory::Parameters* aParams)
        : gfxTextRun(aParams), mContext(aParams->mContext),
          mInner(nsDependentCSubstring(reinterpret_cast<const char*>(aString),
                                       reinterpret_cast<const char*>(aString + aLength)),
                 aGroup),
          mLength(aLength)
    {
        mInner.SetRightToLeft(IsRightToLeft());
    }

    gfxWrapperTextRun(gfxOS2FontGroup *aGroup, const PRUnichar* aString, PRUint32 aLength,
                      gfxTextRunFactory::Parameters* aParams)
        : gfxTextRun(aParams), mContext(aParams->mContext),
          mInner(nsDependentSubstring(aString, aString + aLength), aGroup),
          mLength(aLength)
    {
        mInner.SetRightToLeft(IsRightToLeft());
    }

    ~gfxWrapperTextRun() {}

    virtual void GetCharFlags(PRUint32 aStart, PRUint32 aLength, PRUint8* aFlags) { NS_ERROR("NOT IMPLEMENTED"); }
    virtual PRUint8 GetCharFlags(PRUint32 aOffset) { NS_ERROR("NOT IMPLEMENTED"); return 0; }
    virtual PRUint32 GetLength() { NS_ERROR("NOT IMPLEMENTED"); return 0; }
    virtual PRBool SetPotentialLineBreaks(PRUint32 aStart, PRUint32 aLength, PRPackedBool* aBreakBefore) { NS_ERROR("NOT IMPLEMENTED"); return PR_FALSE; }
    virtual void DrawToPath(gfxContext *aContext, gfxPoint aPt, PRUint32 aStart, PRUint32 aLength, PropertyProvider* aBreakProvider, gfxFloat* aAdvanceWidth) { NS_ERROR("NOT IMPLEMENTED"); }
    virtual Metrics MeasureText(PRUint32 aStart, PRUint32 aLength, PRBool aTightBoundingBox, PropertyProvider* aBreakProvider) { NS_ERROR("NOT IMPLEMENTED"); return Metrics(); }
    virtual void SetLineBreaks(PRUint32 aStart, PRUint32 aLength, PRBool aLineBreakBefore, PRBool aLineBreakAfter, TextProvider* aProvider, gfxFloat* aAdvanceWidthDelta) { NS_ERROR("NOT IMPLEMENTED"); }
    virtual PRUint32 BreakAndMeasureText(PRUint32 aStart, PRUint32 aMaxLength, PRBool aLineBreakBefore, gfxFloat aWidth, PropertyProvider* aProvider, PRBool aSuppressInitialBreak, Metrics* aMetrics, PRBool aTightBoundingBox, PRBool* aUsedHyphenation, PRUint32* aLastBreak) { NS_ERROR("NOT IMPLEMENTED"); return 0; }

    virtual void Draw(gfxContext *aContext, gfxPoint aPt, PRUint32 aStart, PRUint32 aLength, const gfxRect* aDirtyRect, PropertyProvider* aBreakProvider, gfxFloat* aAdvanceWidth);
    virtual gfxFloat GetAdvanceWidth(PRUint32 aStart, PRUint32 aLength, PropertyProvider* aBreakProvider);
    virtual void SetContext(gfxContext* aContext) { mContext = aContext; }

private:
    gfxContext* mContext;
    gfxOS2TextRun mInner;
    PRUint32 mLength;

    void SetupSpacingFromProvider(PropertyProvider* aProvider);
};

#define ROUND(x) floor((x) + 0.5)

void gfxWrapperTextRun::SetupSpacingFromProvider(PropertyProvider* aProvider)
{
    if (!(mFlags & gfxTextRunFactory::TEXT_ENABLE_SPACING))
        return;

    NS_ASSERTION(mFlags & gfxTextRunFactory::TEXT_ABSOLUTE_SPACING,
                 "Can't handle relative spacing");

    nsAutoTArray<PropertyProvider::Spacing,200> spacing;
    spacing.AppendElements(mLength);
    aProvider->GetSpacing(0, mLength, spacing.Elements());

    nsTArray<gfxFloat> spaceArray;
    PRUint32 i;
    gfxFloat offset = 0;
    for (i = 0; i < mLength; ++i) {
        NS_ASSERTION(spacing.Elements()[i].mBefore == 0, "Can't handle before-spacing!");
        gfxFloat nextOffset = offset + spacing.Elements()[i].mAfter/mAppUnitsPerDevUnit;
        spaceArray.AppendElement(ROUND(nextOffset) - ROUND(offset));
        offset = nextOffset;
    }
    mInner.SetSpacing(spaceArray);
}

void gfxWrapperTextRun::Draw(gfxContext *aContext,
                             gfxPoint aPt,
                             PRUint32 aStart,
                             PRUint32 aLength,
                             const gfxRect* aDirtyRect,
                             PropertyProvider* aBreakProvider,
                             gfxFloat* aAdvanceWidth)
{
    NS_ASSERTION(aStart == 0 && aLength == mLength, "Can't handle substrings");
    SetupSpacingFromProvider(aBreakProvider);
    gfxPoint pt(aPt.x/mAppUnitsPerDevUnit, aPt.y/mAppUnitsPerDevUnit);
    return mInner.Draw(mContext, pt);
}

gfxFloat gfxWrapperTextRun::GetAdvanceWidth(PRUint32 aStart,
                                            PRUint32 aLength,
                                            PropertyProvider* aBreakProvider)
{
    NS_ASSERTION(aStart == 0 && aLength == mLength, "Can't handle substrings");
    SetupSpacingFromProvider(aBreakProvider);
    return mInner.Measure(mContext)*mAppUnitsPerDevUnit;
}

gfxTextRun *gfxOS2FontGroup::MakeTextRun(const PRUnichar* aString,
                                         PRUint32 aLength,
                                         Parameters* aParams)
{
    return new gfxWrapperTextRun(this, aString, aLength, aParams);
}

gfxTextRun *gfxOS2FontGroup::MakeTextRun(const PRUint8* aString,
                                         PRUint32 aLength,
                                         Parameters* aParams)
{
    aParams->mFlags |= TEXT_IS_8BIT;
    return new gfxWrapperTextRun(this, aString, aLength, aParams);
}

PRBool gfxOS2FontGroup::MakeFont(const nsAString& aName,
                                 const nsACString& aGenericName,
                                 void *closure)
{
#ifdef DEBUG_thebes
    printf("gfxOS2FontGroup::MakeFont(\"%s\", \"%s\", %#x)\n",
           NS_LossyConvertUTF16toASCII(aName).get(),
           nsPromiseFlatCString(aGenericName).get(), (unsigned)closure);
#endif
    if (!aName.IsEmpty()) {
        gfxOS2FontGroup *fg = NS_STATIC_CAST(gfxOS2FontGroup*, closure);

        if (fg->HasFontNamed(aName))
            return PR_TRUE;

        gfxOS2Font *font = new gfxOS2Font(aName, fg->GetStyle());
        fg->AppendFont(font);

        if (!aGenericName.IsEmpty() && fg->GetGenericFamily().IsEmpty())
            fg->mGenericFamily = aGenericName;
    }

    return PR_TRUE;
}


/**********************************************************************
 * class gfxOS2TextRun
 **********************************************************************/

// Helper to easily get the presentation handle from a gfxSurface
inline HPS GetPSFromSurface(gfxASurface *aSurface) {
    if (aSurface->GetType() != gfxASurface::SurfaceTypeOS2) {
        NS_ERROR("gfxOS2TextRun: GetPSFromSurface: Context target is not os2!");
        return nsnull;
    }
    return NS_STATIC_CAST(gfxOS2Surface*, aSurface)->GetPS();
}

// Helper to easily get the size from a gfxSurface
inline gfxIntSize GetSizeFromSurface(gfxASurface *aSurface) {
    gfxIntSize size(0, 0);
    if (aSurface->GetType() != gfxASurface::SurfaceTypeOS2) {
        NS_ERROR("gfxOS2TextRun: GetPSFromSurface: Context target is not os2!");
        return size;
    }
    return NS_STATIC_CAST(gfxOS2Surface*, aSurface)->GetSize();
}

gfxOS2TextRun::gfxOS2TextRun(const nsAString& aString, gfxOS2FontGroup *aFontGroup)
    : mGroup(aFontGroup), mString(aString), mIsASCII(PR_FALSE), mLength(-1.0)
{
#ifdef DEBUG_thebes
    printf("gfxOS2TextRun::gfxOS2TextRun(nsAString \"%s\", %#x)\n",
           NS_LossyConvertUTF16toASCII(aString).get(), (unsigned)aFontGroup);
#endif
}

gfxOS2TextRun::gfxOS2TextRun(const nsACString& aString, gfxOS2FontGroup *aFontGroup)
    : mGroup(aFontGroup), mCString(aString), mIsASCII(PR_TRUE), mLength(-1.0)
{
#ifdef DEBUG_thebes
    printf("gfxOS2TextRun::gfxOS2TextRun(nsACString \"%s\", %#x)\n",
           nsPromiseFlatCString(aString).get(), (unsigned)aFontGroup);
#endif
}

gfxOS2TextRun::~gfxOS2TextRun()
{
#ifdef DEBUG_thebes
    printf("gfxOS2TextRun::~gfxOS2TextRun()\n");
#endif
}

void gfxOS2TextRun::Draw(gfxContext *aContext, gfxPoint aPt)
{
#ifdef DEBUG_thebes
    printf("gfxOS2TextRun::Draw(...): ");
#endif
    const char *string;
    if (mIsASCII) {
        string = mCString.get();
    } else {
        string = NS_LossyConvertUTF16toASCII(mString).get();
    }
#ifdef DEBUG_thebes
    printf("%s\n", string);
#endif

#ifdef USE_CAIRO_FOR_DRAWING
    cairo_t *cr = aContext->GetCairo();

    if (!cr) {
        printf("  no cairo context!\n");
      return;
    }

    cairo_select_font_face(cr, "Helvetica", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12.0);
    cairo_show_text(cr, string);
#else
    double rc = MeasureOrDrawFast(aContext, PR_TRUE, aPt);
    if (rc >= 0) {
        return;
    }
    MeasureOrDrawSlow(aContext, PR_TRUE, aPt);
#endif
}

gfxFloat gfxOS2TextRun::Measure(gfxContext *aContext)
{
#ifdef DEBUG_thebes
    printf("gfxOS2TextRun::Measure(...): ");
#endif
    const char *string;
    if (mIsASCII) {
        string = mCString.get();
    } else {
        string = NS_LossyConvertUTF16toASCII(mString).get();
    }
#ifdef DEBUG_thebes
    printf("%s\n", string);
#endif

    static const gfxPoint kZeroZero(0, 0);

    if (mLength < 0) {
#ifdef USE_CAIRO_FOR_DRAWING
        cairo_t *cr = aContext->GetCairo();
        cairo_text_extents_t extents;

        if (!cr) {
            printf("  no cairo context!\n");
            return -1;
        }

        cairo_select_font_face(cr, "Helvetica", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 12.0);
        cairo_text_extents(cr, string, &extents);
        mLength = extents.width;
#ifdef DEBUG_thebes
        printf("  string=%s\n", string);
        printf("  font\n");
        printf("  size\n");
        printf("  extents\n");
        printf("  mLength=%f\n",mLength);
#endif
#else
        mLength = MeasureOrDrawFast(aContext, PR_FALSE, kZeroZero);
#endif
        if (mLength >= 0) {
            return mLength;
        }
        // need something more sophisticated
        mLength = MeasureOrDrawSlow(aContext, PR_FALSE, kZeroZero);
    }
    return mLength;
}

double
gfxOS2TextRun::MeasureOrDrawFast(gfxContext *aContext, PRBool aDraw, gfxPoint aPt)
{
#ifdef DEBUG_thebes
    printf("gfxOS2TextRun::MeasureOrDrawFast(0x%p, %d, aPt)\n", (void*)aContext, aDraw);
#endif
    double length = 0;
    gfxRGBA color;

//#ifdef WE_HAVE_REALLY_FAST
    if (!aContext->CurrentMatrix().HasNonTranslation()) {
        // need to not call this if the color isn't solid
        // or the pattern isn't a color
        // or the destination has alpha
        if (mSpacing.IsEmpty()) { // XXX remove this once the below handles spacing
            if (aContext->GetColor(color) && color.a == 1) {
                // we can measure with 32bpp surfaces, but we can't draw to them using this API
                nsRefPtr<gfxASurface> currentSurface = aContext->CurrentSurface();
                if (!aDraw || currentSurface->GetContentType() == gfxASurface::CONTENT_COLOR) {
                    // XXX version of Windows' MeasureOrDrawReallyFast(...)
#ifdef DEBUG_thebes
                    printf("  should call MeasureOrDrawReallyFast()\n");
#endif
                } // if !aDraw
            } // if color
        } // if not empty spacing
    } // if !...HasNonTranslation()
//#endif

    if (IsRightToLeft()) { // this function doesn't handle RTL text
        return -1;
    }

    const char *asciiString;
    const PRUnichar *wideString;
    PRUint32 stringLength;

    if (mIsASCII) {
        asciiString = mCString.BeginReading();
        stringLength = mCString.Length();
    } else {
        wideString = mString.BeginReading();
        stringLength = mString.Length();
        // XXX there is no "ScriptIsComplex(...)" API on OS/2,
        // not sure what to do
    }

    nsRefPtr<gfxASurface> surf = aContext->CurrentSurface();
    HPS hps = GetPSFromSurface(surf);
    NS_ASSERTION(hps, "No HPS");

    nsRefPtr<gfxOS2Font> currentFont = mGroup->GetFontAt(0);
    //GpiCreateLogFont(hps, NULL, 1, currentFont->mFAttrs)

    if (aDraw) {
        aContext->GetColor(color);
        LONG lColor = MK_RGB(color.r, color.g, color.b);
        if (lColor == 0x0) {
            // XXX somehow CLR_BLACK is -1 not 0, will palette handling perhaps rectify this?
            lColor = CLR_BLACK;
        }
#ifdef DEBUG_thebes
        printf("  color(RGBA)=%f,%f,%f,%f, 0x%0lx\n", color.r, color.g, color.b, color.a, lColor);
#endif
        GpiSetColor(hps, lColor);
        GpiSetBackColor(hps, CLR_BACKGROUND);
        //GpiSetBackMix(hps, BM_LEAVEALONE); // XXX BM_LEAVEALONE is default

        aContext->UpdateSurfaceClip();

        gfxPoint offset;
        nsRefPtr<gfxASurface> surf = aContext->CurrentSurface(&offset.x, &offset.y);
        gfxIntSize size = GetSizeFromSurface(surf);
        gfxPoint p = aContext->CurrentMatrix().Transform(aPt) + offset;
        p.y = (gfxFloat)size.height - p.y;
//#define SIMPLE_DRAW
#ifdef SIMPLE_DRAW
        POINTL startPos = { (LONG)p.x, (LONG)p.y };
#endif

        // finally output it
        if (mIsASCII) {
#ifdef SIMPLE_DRAW
            GpiCharStringAt(hps, &startPos, stringLength, asciiString);
#else
            ExtTextOut(hps, (int)p.x, (int)p.y, 0, NULL, asciiString, stringLength, NULL);
#endif
#ifdef DEBUG_thebes
            printf("  string=%s\n", asciiString);
#endif
        } else {
            nsAutoCharBuffer destBuffer;
            PRInt32 destLength = stringLength;
            // XXX dummy convert!
            if (WideCharToMultiByte(0, wideString, stringLength, destBuffer, destLength)) {
#ifdef DEBUG_thebes
                printf("conversion didn't work!\n");
#endif
                return 0.0; // conversion didn't work
            }
#ifdef SIMPLE_DRAW
            GpiCharStringAt(hps, &startPos, destLength, destBuffer.get());
#else
            ExtTextOut(hps, (int)p.x, (int)p.y, 0, NULL, destBuffer.get(), destLength, NULL);
#endif
#ifdef DEBUG_thebes
            printf("  string=%s\n", destBuffer.get());
#endif
        }

        // XXX why do we need to mark dirty after Gpi function call?!
        surf->MarkDirty();
    } else { // just measure
        if (mIsASCII) {
            // XXX this gives rubbish!!
            //length = currentFont->GetWidth(hps, asciiString, stringLength);
            // XXX for now just take the average char width as good measure
            length = currentFont->GetMetrics().aveCharWidth * stringLength;
        } else {
            // XXX this gives rubbish!!
            // length = currentFont->GetWidth(hps, wideString, stringLength);
            // XXX for now just take the average char width as good measure
            length = currentFont->GetMetrics().aveCharWidth * stringLength;
        }
#ifdef DEBUG_thebes
        printf("  Measured as %f\n", length);
#endif
    }

    return length;
}

double
gfxOS2TextRun::MeasureOrDrawSlow(gfxContext *aContext, PRBool aDraw, gfxPoint aPt)
{
    printf("gfxOS2TextRun::MeasureOrDrawSlow(.., %d, ..)\n", aDraw);
    // XXX hack for now
    return 12;
}

void gfxOS2TextRun::SetSpacing(const nsTArray<gfxFloat> &aSpacingArray)
{
#ifdef DEBUG_thebes
    printf("gfxOS2TextRun::SetSpacing(...)\n");
#endif
    mSpacing = aSpacingArray;
}

const nsTArray<gfxFloat> *const gfxOS2TextRun::GetSpacing() const
{
#ifdef DEBUG_thebes
    printf("gfxOS2TextRun::GetSpacing()\n");
#endif
    return &mSpacing;
}


/**********************************************************************
 * class gfxOS2Uni, to handle Unicode on OS/2 (copied from gfx/src/os2)
 **********************************************************************/
nsICharsetConverterManager* gfxOS2Uni::gCharsetManager = nsnull;

struct ConverterInfo
{
    PRUint16           mCodePage;
    char              *mConvName;
    nsIUnicodeEncoder *mEncoder;
    nsIUnicodeDecoder *mDecoder;
};

#define eCONVERTER_COUNT  17
ConverterInfo gConverterInfo[eCONVERTER_COUNT] =
{
    { 0,    "",              nsnull,  nsnull },
    { 1252, "windows-1252",  nsnull,  nsnull },
    { 1208, "UTF-8",         nsnull,  nsnull },
    { 1250, "windows-1250",  nsnull,  nsnull },
    { 1251, "windows-1251",  nsnull,  nsnull },
    { 813,  "ISO-8859-7",    nsnull,  nsnull },
    { 1254, "windows-1254",  nsnull,  nsnull },
    { 864,  "IBM864",        nsnull,  nsnull },
    { 1257, "windows-1257",  nsnull,  nsnull },
    { 874,  "windows-874",   nsnull,  nsnull },
    { 932,  "Shift_JIS",     nsnull,  nsnull },
    { 943,  "Shift_JIS",     nsnull,  nsnull },
    { 1381, "GB2312",        nsnull,  nsnull },
    { 1386, "GB2312",        nsnull,  nsnull },
    { 949,  "x-windows-949", nsnull,  nsnull },
    { 950,  "Big5",          nsnull,  nsnull },
    { 1361, "x-johab",       nsnull,  nsnull }
};

nsISupports* gfxOS2Uni::GetUconvObject(int aCodePage, ConverterRequest aReq)
{
    if (gCharsetManager == nsnull) {
        CallGetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &gCharsetManager);
    }

    nsresult rv;
    nsISupports* uco = nsnull;
    for (int i = 0; i < eCONVERTER_COUNT; i++) {
        if (aCodePage == gConverterInfo[i].mCodePage) {
            if (gConverterInfo[i].mEncoder == nsnull) {
                const char* convname;
                nsCAutoString charset;
                if (aCodePage == 0) {
                    nsCOMPtr<nsIPlatformCharset>
                        plat(do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv));
                    if (NS_SUCCEEDED(rv)) {
                        plat->GetCharset(kPlatformCharsetSel_FileName, charset);
                    } else {
                        // default to IBM850 if this should fail
                        charset = "IBM850";
                    }
                    convname = charset.get();
                } else {
                    convname = gConverterInfo[i].mConvName;
                }
                rv = gCharsetManager->GetUnicodeEncoderRaw(convname,
                                                           &gConverterInfo[i].mEncoder);
                gConverterInfo[i].mEncoder->
                    SetOutputErrorBehavior(nsIUnicodeEncoder::kOnError_Replace,
                                           nsnull, '?');
                gCharsetManager->GetUnicodeDecoderRaw(convname,
                                                      &gConverterInfo[i].mDecoder);
                NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to get converter");
            }
            if (aReq == eConv_Encoder) {
                uco = gConverterInfo[i].mEncoder;
            } else {
                uco = gConverterInfo[i].mDecoder;
            }
            break;
        }
    }

    return uco;
}

void gfxOS2Uni::FreeUconvObjects()
{
    for (int i = 0; i < eCONVERTER_COUNT; i++) {
        NS_IF_RELEASE(gConverterInfo[i].mEncoder);
        NS_IF_RELEASE(gConverterInfo[i].mDecoder);
    }
    NS_IF_RELEASE(gCharsetManager);
}

nsresult WideCharToMultiByte(int aCodePage,
                             const PRUnichar* aSrc, PRInt32 aSrcLength,
                             nsAutoCharBuffer& aResult, PRInt32& aResultLength)
{
#ifdef DEBUG_thebes
    printf("WideCharToMultiByte()\n");
#endif
    nsresult rv;
    nsISupports* sup = gfxOS2Uni::GetUconvObject(aCodePage, eConv_Encoder);
    nsCOMPtr<nsIUnicodeEncoder> uco = do_QueryInterface(sup);

    if (!uco || NS_FAILED(uco->GetMaxLength(aSrc, aSrcLength, &aResultLength))) {
        return NS_ERROR_UNEXPECTED;
    }
    if (!aResult.EnsureElemCapacity(aResultLength + 1)) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    char* str = aResult.get();

    rv = uco->Convert(aSrc, &aSrcLength, str, &aResultLength);
    aResult.get()[aResultLength] = '\0';
    return rv;
}

nsresult MultiByteToWideChar(int aCodePage,
                             const char* aSrc, PRInt32 aSrcLength,
                             nsAutoChar16Buffer& aResult, PRInt32& aResultLength)
{
#ifdef DEBUG_thebes
    printf("MultiByteToWideChar()\n");
#endif
    nsresult rv;
    nsISupports* sup = gfxOS2Uni::GetUconvObject(aCodePage, eConv_Decoder);
    nsCOMPtr<nsIUnicodeDecoder> uco = do_QueryInterface(sup);

    if (!uco || NS_FAILED(uco->GetMaxLength(aSrc, aSrcLength, &aResultLength))) {
        return NS_ERROR_UNEXPECTED;
    }
    if (!aResult.EnsureElemCapacity(aResultLength + 1))
        return NS_ERROR_OUT_OF_MEMORY;
    PRUnichar* str = aResult.get();

    rv = uco->Convert(aSrc, &aSrcLength, str, &aResultLength);
    aResult.get()[aResultLength] = '\0';
    return rv;
}
