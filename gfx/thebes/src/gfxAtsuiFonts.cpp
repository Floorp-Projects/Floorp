/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
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

#include "prtypes.h"
#include "prmem.h"
#include "nsString.h"

#include "gfxTypes.h"

#include "nsPromiseFlatString.h"

#include "gfxContext.h"
#include "gfxAtsuiFonts.h"

#include "gfxFontTest.h"

#include "cairo-atsui.h"

#include "gfxQuartzSurface.h"
#include "gfxQuartzFontCache.h"

#define ROUND(x) (floor((x) + 0.5))

/* We might still need this for fast-pathing, but we'll see */
#if 0
OSStatus ATSUGetStyleGroup(ATSUStyle style, void **styleGroup);
OSStatus ATSUDisposeStyleGroup(void *styleGroup);
OSStatus ATSUConvertCharToGlyphs(void *styleGroup,
                                 PRunichar *buffer
                                 unsigned int bufferLength,
                                 void *glyphVector);
OSStatus ATSInitializeGlyphVector(int size, void *glyphVectorPtr);
OSStatus ATSClearGlyphVector(void *glyphVectorPtr);
#endif

gfxAtsuiFont::gfxAtsuiFont(ATSUFontID fontID,
                           const gfxFontStyle *fontStyle)
    : gfxFont(EmptyString(), fontStyle),
      mFontStyle(fontStyle), mATSUFontID(fontID), mATSUStyle(nsnull),
      mAdjustedSize(0)
{
    ATSFontRef fontRef = FMGetATSFontRefFromFont(fontID);

    InitMetrics(fontID, fontRef);

    mFontFace = cairo_atsui_font_face_create_for_atsu_font_id(mATSUFontID);

    cairo_matrix_t sizeMatrix, ctm;
    cairo_matrix_init_identity(&ctm);
    cairo_matrix_init_scale(&sizeMatrix, mAdjustedSize, mAdjustedSize);

    cairo_font_options_t *fontOptions = cairo_font_options_create();
    mScaledFont = cairo_scaled_font_create(mFontFace, &sizeMatrix, &ctm, fontOptions);
    cairo_font_options_destroy(fontOptions);
}

void
gfxAtsuiFont::InitMetrics(ATSUFontID aFontID, ATSFontRef aFontRef)
{
    /* Create the ATSUStyle */

    ATSUAttributeTag styleTags[] = {
        kATSUFontTag,
        kATSUSizeTag,
        kATSUFontMatrixTag,
        kATSUKerningInhibitFactorTag
    };

    ByteCount styleArgSizes[] = {
        sizeof(ATSUFontID),
        sizeof(Fixed),
        sizeof(CGAffineTransform),
        sizeof(Fract)
    };

    gfxFloat size =
        PR_MAX(((mAdjustedSize != 0) ? mAdjustedSize : mStyle->size), 1.0f);

    //fprintf (stderr, "string: '%s', size: %f\n", NS_ConvertUTF16toUTF8(aString).get(), size);

    // fSize is in points (72dpi)
    Fixed fSize = FloatToFixed(size);
    ATSUFontID fid = aFontID;
    // make the font render right-side up
    CGAffineTransform transform = CGAffineTransformMakeScale(1, -1);
    // we can't do kerning until layout draws what it measures, instead of splitting things up
    Fract inhibitKerningFactor = FloatToFract(1.0);

    ATSUAttributeValuePtr styleArgs[] = {
        &fid,
        &fSize,
        &transform,
        &inhibitKerningFactor
    };

    if (mATSUStyle)
        ATSUDisposeStyle(mATSUStyle);

    ATSUCreateStyle(&mATSUStyle);
    ATSUSetAttributes(mATSUStyle,
                      sizeof(styleTags)/sizeof(ATSUAttributeTag),
                      styleTags,
                      styleArgSizes,
                      styleArgs);

    /* Now pull out the metrics */

    ATSFontMetrics atsMetrics;
    ATSFontGetHorizontalMetrics(aFontRef, kATSOptionFlagsDefault,
                                &atsMetrics);

    if (atsMetrics.xHeight)
        mMetrics.xHeight = atsMetrics.xHeight * size;
    else
        mMetrics.xHeight = GetCharHeight('x');

    if (mAdjustedSize == 0) {
        if (mStyle->sizeAdjust != 0) {
            gfxFloat aspect = mMetrics.xHeight / size;
            mAdjustedSize =
                PR_MAX(ROUND(size * (mStyle->sizeAdjust / aspect)), 1.0f);
            InitMetrics(aFontID, aFontRef);
            return;
        }
        mAdjustedSize = size;
    }

    mMetrics.emHeight = size;

    mMetrics.maxAscent = atsMetrics.ascent * size;
    mMetrics.maxDescent = - (atsMetrics.descent * size);

    mMetrics.maxHeight = mMetrics.maxAscent + mMetrics.maxDescent;

    if (mMetrics.maxHeight - mMetrics.emHeight > 0)
        mMetrics.internalLeading = mMetrics.maxHeight - mMetrics.emHeight; 
    else
        mMetrics.internalLeading = 0.0;
    mMetrics.externalLeading = atsMetrics.leading * size;

    mMetrics.emAscent = mMetrics.maxAscent * mMetrics.emHeight / mMetrics.maxHeight;
    mMetrics.emDescent = mMetrics.emHeight - mMetrics.emAscent;

    mMetrics.maxAdvance = atsMetrics.maxAdvanceWidth * size;

    if (atsMetrics.avgAdvanceWidth != 0.0)
        mMetrics.aveCharWidth =
            PR_MIN(atsMetrics.avgAdvanceWidth * size, GetCharWidth('x'));
    else
        mMetrics.aveCharWidth = GetCharWidth('x');

    mMetrics.underlineOffset = atsMetrics.underlinePosition * size;
    // ATSUI sometimes returns 0 for underline thickness, see bug 361576.
    mMetrics.underlineSize = PR_MAX(1.0f, atsMetrics.underlineThickness * size);

    mMetrics.subscriptOffset = mMetrics.xHeight;
    mMetrics.superscriptOffset = mMetrics.xHeight;

    mMetrics.strikeoutOffset = mMetrics.xHeight / 2.0;
    mMetrics.strikeoutSize = mMetrics.underlineSize;

    mMetrics.spaceWidth = GetCharWidth(' ');

#if 0
    fprintf (stderr, "Font: %p size: %f", this, size);
    fprintf (stderr, "    emHeight: %f emAscent: %f emDescent: %f\n", mMetrics.emHeight, mMetrics.emAscent, mMetrics.emDescent);
    fprintf (stderr, "    maxAscent: %f maxDescent: %f maxAdvance: %f\n", mMetrics.maxAscent, mMetrics.maxDescent, mMetrics.maxAdvance);
    fprintf (stderr, "    internalLeading: %f externalLeading: %f\n", mMetrics.externalLeading, mMetrics.internalLeading);
    fprintf (stderr, "    spaceWidth: %f aveCharWidth: %f xHeight: %f\n", mMetrics.spaceWidth, mMetrics.aveCharWidth, mMetrics.xHeight);
    fprintf (stderr, "    uOff: %f uSize: %f stOff: %f stSize: %f suOff: %f suSize: %f\n", mMetrics.underlineOffset, mMetrics.underlineSize, mMetrics.strikeoutOffset, mMetrics.strikeoutSize, mMetrics.superscriptOffset, mMetrics.subscriptOffset);
#endif
}

nsString
gfxAtsuiFont::GetUniqueName()
{
    return gfxQuartzFontCache::SharedFontCache()->GetPostscriptNameForFontID(mATSUFontID);
}

float
gfxAtsuiFont::GetCharWidth(PRUnichar c)
{
    // this sucks.  There is a faster way to go from a char -> glyphs, but it
    // requires using oodles of apple private interfaces.  If we start caching
    // gfxAtsuiFonts, then it might make sense to do that.
    ATSUTextLayout layout;

    UniCharCount one = 1;
    ATSUCreateTextLayoutWithTextPtr(&c, 0, 1, 1, 1, &one, &mATSUStyle, &layout);

    ATSTrapezoid trap;
    ItemCount numBounds;
    ATSUGetGlyphBounds(layout, FloatToFixed(0.0), FloatToFixed(0.0),
                       0, 1, kATSUseFractionalOrigins, 1, &trap, &numBounds);

    float f =
        FixedToFloat(PR_MAX(trap.upperRight.x, trap.lowerRight.x)) -
        FixedToFloat(PR_MIN(trap.upperLeft.x, trap.lowerLeft.x));

    ATSUDisposeTextLayout(layout);

    return f;
}

float
gfxAtsuiFont::GetCharHeight(PRUnichar c)
{
    // this sucks.  There is a faster way to go from a char -> glyphs, but it
    // requires using oodles of apple private interfaces.  If we start caching
    // gfxAtsuiFonts, then it might make sense to do that.
    ATSUTextLayout layout;

    UniCharCount one = 1;
    ATSUCreateTextLayoutWithTextPtr(&c, 0, 1, 1, 1, &one, &mATSUStyle, &layout);

    Rect rect;
    ATSUMeasureTextImage(layout, 0, 1, 0, 0, &rect);

    ATSUDisposeTextLayout(layout);

    return rect.bottom - rect.top;
}

gfxAtsuiFont::~gfxAtsuiFont()
{
    cairo_scaled_font_destroy(mScaledFont);
    cairo_font_face_destroy(mFontFace);

    ATSUDisposeStyle(mATSUStyle);
}

const gfxFont::Metrics&
gfxAtsuiFont::GetMetrics()
{
    return mMetrics;
}

gfxAtsuiFontGroup::gfxAtsuiFontGroup(const nsAString& families,
                                     const gfxFontStyle *aStyle)
    : gfxFontGroup(families, aStyle)
{
    ForEachFont(FindATSUFont, this);

    if (mFonts.Length() == 0) {
        // XXX this will generate a list of the lang groups for which we have no
        // default fonts for on the mac; we should fix this!
        // Known:
        // ja x-beng x-devanagari x-tamil x-geor x-ethi x-gujr x-mlym x-armn

        //fprintf (stderr, "gfxAtsuiFontGroup: %s [%s] -> %d fonts found\n", NS_ConvertUTF16toUTF8(families).get(), aStyle->langGroup.get(), mFonts.Length());

        // If we get here, we most likely didn't have a default font for
        // a specific langGroup.  Let's just pick the default OSX
        // user font.
        ATSUFontID fontID = gfxQuartzFontCache::SharedFontCache()->GetDefaultATSUFontID (aStyle);
        mFonts.AppendElement(new gfxAtsuiFont(fontID, aStyle));
    }

    // Create the fallback structure
    ATSUCreateFontFallbacks(&mFallbacks);

#define NUM_STATIC_FIDS 16
    ATSUFontID static_fids[NUM_STATIC_FIDS];
    ATSUFontID *fids;
    if (mFonts.Length() > NUM_STATIC_FIDS)
        fids = (ATSUFontID *) PR_Malloc(sizeof(ATSUFontID) * mFonts.Length());
    else
        fids = static_fids;

    for (unsigned int i = 0; i < mFonts.Length(); i++) {
        gfxAtsuiFont* atsuiFont = NS_STATIC_CAST(gfxAtsuiFont*, NS_STATIC_CAST(gfxFont*, mFonts[i]));
        fids[i] = atsuiFont->GetATSUFontID();
    }
    ATSUSetObjFontFallbacks(mFallbacks,
                            mFonts.Length(),
                            fids,
                            kATSUSequentialFallbacksPreferred /* kATSUSequentialFallbacksExclusive? */);

    if (fids != static_fids)
        PR_Free(fids);
}

PRBool
gfxAtsuiFontGroup::FindATSUFont(const nsAString& aName,
                                const nsACString& aGenericName,
                                void *closure)
{
    gfxAtsuiFontGroup *fontGroup = (gfxAtsuiFontGroup*) closure;
    const gfxFontStyle *fontStyle = fontGroup->GetStyle();

    gfxQuartzFontCache *fc = gfxQuartzFontCache::SharedFontCache();
    ATSUFontID fontID = fc->FindATSUFontIDForFamilyAndStyle (aName, fontStyle);

    if (fontID != kATSUInvalidFontID) {
        //printf ("FindATSUFont! %s %d -> %d\n", NS_ConvertUTF16toUTF8(aName).get(), fontStyle->weight, (int)fontID);
        fontGroup->mFonts.AppendElement(new gfxAtsuiFont(fontID, fontStyle));
    }

    return PR_TRUE;
}

gfxAtsuiFontGroup::~gfxAtsuiFontGroup()
{
    ATSUDisposeFontFallbacks(mFallbacks);
}

class gfxWrapperTextRun : public gfxTextRun {
public:
    gfxWrapperTextRun(gfxAtsuiFontGroup *aGroup,
                      const PRUint8* aString, PRUint32 aLength,
                      gfxTextRunFactory::Parameters* aParams)
        : gfxTextRun(aParams),
          mContext(aParams->mContext),
          mInner(NS_ConvertASCIItoUTF16(nsDependentCSubstring(reinterpret_cast<const char*>(aString),
                                        reinterpret_cast<const char*>(aString + aLength))),
                 aGroup),
          mLength(aLength)
    {
        mInner.SetRightToLeft(IsRightToLeft());
    }
    gfxWrapperTextRun(gfxAtsuiFontGroup *aGroup,
                      const PRUnichar* aString, PRUint32 aLength,
                      gfxTextRunFactory::Parameters* aParams)
        : gfxTextRun(aParams), mContext(aParams->mContext),
          mInner(nsDependentSubstring(aString, aString + aLength), aGroup),
          mLength(aLength)
    {
        mInner.SetRightToLeft(IsRightToLeft());
    }
    ~gfxWrapperTextRun() {}

    virtual void GetCharFlags(PRUint32 aStart, PRUint32 aLength,
                              PRUint8* aFlags)
    { NS_ERROR("NOT IMPLEMENTED"); }
    virtual PRUint8 GetCharFlags(PRUint32 aOffset)
    { NS_ERROR("NOT IMPLEMENTED"); return 0; }
    virtual PRUint32 GetLength()
    { NS_ERROR("NOT IMPLEMENTED"); return 0; }
    virtual PRBool SetPotentialLineBreaks(PRUint32 aStart, PRUint32 aLength,
                                          PRPackedBool* aBreakBefore)
    { NS_ERROR("NOT IMPLEMENTED"); return PR_FALSE; }
    virtual void DrawToPath(gfxContext *aContext, gfxPoint aPt,
                            PRUint32 aStart, PRUint32 aLength,
                            PropertyProvider* aBreakProvider,
                            gfxFloat* aAdvanceWidth)
    { NS_ERROR("NOT IMPLEMENTED"); }
    virtual Metrics MeasureText(PRUint32 aStart, PRUint32 aLength,
                                PRBool aTightBoundingBox,
                                PropertyProvider* aBreakProvider)
    { NS_ERROR("NOT IMPLEMENTED"); return Metrics(); }
    virtual void SetLineBreaks(PRUint32 aStart, PRUint32 aLength,
                               PRBool aLineBreakBefore, PRBool aLineBreakAfter,
                               TextProvider* aProvider,
                               gfxFloat* aAdvanceWidthDelta)
    { NS_ERROR("NOT IMPLEMENTED"); }
    virtual PRUint32 BreakAndMeasureText(PRUint32 aStart, PRUint32 aMaxLength,
                                         PRBool aLineBreakBefore, gfxFloat aWidth,
                                         PropertyProvider* aProvider,
                                         PRBool aSuppressInitialBreak,
                                         Metrics* aMetrics, PRBool aTightBoundingBox,
                                         PRBool* aUsedHyphenation,
                                         PRUint32* aLastBreak)
    { NS_ERROR("NOT IMPLEMENTED"); return 0; }

    virtual void Draw(gfxContext *aContext, gfxPoint aPt,
                      PRUint32 aStart, PRUint32 aLength,
                      const gfxRect* aDirtyRect,
                      PropertyProvider* aBreakProvider,
                      gfxFloat* aAdvanceWidth);
    virtual gfxFloat GetAdvanceWidth(PRUint32 aStart, PRUint32 aLength,
                                     PropertyProvider* aBreakProvider);

    virtual void SetContext(gfxContext* aContext) { mContext = aContext; }

private:
    gfxContext*     mContext;
    gfxAtsuiTextRun mInner;
    PRUint32        mLength;
    
    void SetupSpacingFromProvider(PropertyProvider* aProvider);
};

#define ROUND(x) floor((x) + 0.5)

void
gfxWrapperTextRun::SetupSpacingFromProvider(PropertyProvider* aProvider)
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
    gfxFloat offset = 0; // in device pixels
    for (i = 0; i < mLength; ++i) {
        NS_ASSERTION(spacing.Elements()[i].mBefore == 0,
                     "Can't handle before-spacing!");
        gfxFloat nextOffset = offset + spacing.Elements()[i].mAfter/mAppUnitsPerDevUnit;
        spaceArray.AppendElement(ROUND(nextOffset) - ROUND(offset));
        offset = nextOffset;
    }
    mInner.SetSpacing(spaceArray);
}

void
gfxWrapperTextRun::Draw(gfxContext *aContext, gfxPoint aPt,
                        PRUint32 aStart, PRUint32 aLength,
                        const gfxRect* aDirtyRect,
                        PropertyProvider* aBreakProvider,
                        gfxFloat* aAdvanceWidth)
{
    NS_ASSERTION(aStart == 0 && aLength == mLength, "Can't handle substrings");
    SetupSpacingFromProvider(aBreakProvider);
    gfxPoint pt(aPt.x/mAppUnitsPerDevUnit, aPt.y/mAppUnitsPerDevUnit);
    return mInner.Draw(mContext, pt);
}

gfxFloat
gfxWrapperTextRun::GetAdvanceWidth(PRUint32 aStart, PRUint32 aLength,
                                   PropertyProvider* aBreakProvider)
{
    NS_ASSERTION(aStart == 0 && aLength == mLength, "Can't handle substrings");
    SetupSpacingFromProvider(aBreakProvider);
    return mInner.Measure(mContext)*mAppUnitsPerDevUnit;
}

gfxTextRun *
gfxAtsuiFontGroup::MakeTextRun(const PRUnichar* aString, PRUint32 aLength,
                               Parameters* aParams)
{
    return new gfxWrapperTextRun(this, aString, aLength, aParams);
}

gfxTextRun *
gfxAtsuiFontGroup::MakeTextRun(const PRUint8* aString, PRUint32 aLength,
                               Parameters* aParams)
{
    return new gfxWrapperTextRun(this, aString, aLength, aParams);
}

gfxAtsuiFont*
gfxAtsuiFontGroup::FindFontFor(ATSUFontID fid)
{
    gfxAtsuiFont *font;

    // In most cases, this will just be 1 -- maybe a
    // small number, so no need for any more complex
    // lookup
    for (int i = 0; i < FontListLength(); i++) {
        font = GetFontAt(i);
        if (font->GetATSUFontID() == fid)
            return font;
    }

    font = new gfxAtsuiFont(fid, GetStyle());
    mFonts.AppendElement(font);

    return font;
}

/**
 ** gfxAtsuiTextRun
 **/

gfxAtsuiTextRun::gfxAtsuiTextRun(const nsAString& aString, gfxAtsuiFontGroup *aGroup)
    : mString(aString), mGroup(aGroup)
{
    OSStatus status;
    gfxAtsuiFont *atsuiFont = mGroup->GetFontAt(0);
    ATSUStyle mainStyle = atsuiFont->GetATSUStyle();

    UniCharCount runLengths = mString.Length();
    status = ATSUCreateTextLayoutWithTextPtr
        (nsPromiseFlatString(mString).get(),
         0,
         mString.Length(),
         mString.Length(),
         1,
         &runLengths,
         &mainStyle,
         &mATSULayout);

    // Set up our layout attributes
    ATSLineLayoutOptions lineLayoutOptions = kATSLineKeepSpacesOutOfMargin | kATSLineHasNoHangers;
    ATSUAttributeTag layoutTags[] = { kATSULineLayoutOptionsTag, kATSULineFontFallbacksTag };
    ByteCount layoutArgSizes[] = { sizeof(ATSLineLayoutOptions), sizeof(ATSUFontFallbacks) };
    ATSUAttributeValuePtr layoutArgs[] = { &lineLayoutOptions, mGroup->GetATSUFontFallbacksPtr() };
    ATSUSetLayoutControls(mATSULayout,
                          sizeof(layoutTags) / sizeof(ATSUAttributeTag),
                          layoutTags,
                          layoutArgSizes,
                          layoutArgs);

    /* Now go through and update the styles for the text, based on font matching. */

    UniCharArrayOffset runStart = 0;
    UniCharCount totalLength = mString.Length();
    UniCharCount runLength = totalLength;

    //fprintf (stderr, "==== Starting font maching [string length: %d]\n", totalLength);
    while (runStart < totalLength) {
        ATSUFontID substituteFontID;
        UniCharArrayOffset changedOffset;
        UniCharCount changedLength;

        OSStatus status = ATSUMatchFontsToText (mATSULayout, runStart, runLength,
                                                &substituteFontID, &changedOffset, &changedLength);
        if (status == noErr) {
            //fprintf (stderr, "ATSUMatchFontsToText returned noErr\n");
            // everything's good, finish up
            break;
        } else if (status == kATSUFontsMatched) {
            //fprintf (stderr, "ATSUMatchFontsToText returned kATSUFontsMatched: FID %d\n", substituteFontID);

            ATSUStyle subStyle;
            ATSUCreateStyle (&subStyle);
            ATSUCopyAttributes (mainStyle, subStyle);

            ATSUAttributeTag fontTags[] = { kATSUFontTag };
            ByteCount fontArgSizes[] = { sizeof(ATSUFontID) };
            ATSUAttributeValuePtr fontArgs[] = { &substituteFontID };

            ATSUSetAttributes (subStyle, 1, fontTags, fontArgSizes, fontArgs);

            ATSUSetRunStyle (mATSULayout, subStyle, changedOffset, changedLength);

            mStylesToDispose.AppendElement(subStyle);
        } else if (status == kATSUFontsNotMatched) {
            //fprintf (stderr, "ATSUMatchFontsToText returned kATSUFontsNotMatched\n");
            /* I need to select the last resort font; how the heck do I do that? */
        }

        //fprintf (stderr, "total length: %d changedOffset: %d changedLength: %d\n",  runLength, changedOffset, changedLength);

        runStart = changedOffset + changedLength;
        runLength = totalLength - runStart;
    }

    //fprintf (stderr, "==== End font matching\n");
}

gfxAtsuiTextRun::~gfxAtsuiTextRun()
{
    if (mATSULayout)
        ATSUDisposeTextLayout(mATSULayout);

    for (PRUint32 i = 0; i < mStylesToDispose.Length(); i++) {
        ATSUStyle s = mStylesToDispose[i];
        ATSUDisposeStyle(s);
    }
}

struct CairoGlyphBuffer {
    CairoGlyphBuffer() {
        size = 128;
        glyphs = staticGlyphBuf;
    }

    ~CairoGlyphBuffer() {
        if (glyphs != staticGlyphBuf)
            PR_Free(glyphs);
    }

    void EnsureSize(PRUint32 numGlyphs) {
        if (size < numGlyphs) {
            if (glyphs != staticGlyphBuf)
                PR_Free (staticGlyphBuf);
            glyphs = (cairo_glyph_t*) PR_Malloc(sizeof(cairo_glyph_t) * numGlyphs);
            size = numGlyphs;
        }
    }

    PRUint32 size;
    cairo_glyph_t *glyphs;
private:
    cairo_glyph_t staticGlyphBuf[128];
};

void
gfxAtsuiTextRun::Draw(gfxContext *aContext, gfxPoint pt)
{
    gfxFloat offsetX, offsetY;
    nsRefPtr<gfxASurface> surf = aContext->CurrentSurface (&offsetX, &offsetY);

    cairo_t *cr = aContext->GetCairo();
    double fontSize = mGroup->GetStyle()->size;

    cairo_save (cr);
    cairo_translate (cr, pt.x, pt.y);

    OSStatus status;

    ByteCount bufferSize = 4096;
    ATSUGlyphInfoArray *glyphInfo = (ATSUGlyphInfoArray*) PR_Malloc (bufferSize);
    status = ATSUGetGlyphInfo (mATSULayout, kATSUFromTextBeginning, kATSUToTextEnd, &bufferSize, glyphInfo);
    if (status == buffersTooSmall) {
        ATSUGetGlyphInfo (mATSULayout, kATSUFromTextBeginning, kATSUToTextEnd, &bufferSize, NULL);
        PR_Free(glyphInfo);
        glyphInfo = (ATSUGlyphInfoArray*) PR_Malloc (bufferSize);

        status = ATSUGetGlyphInfo (mATSULayout, kATSUFromTextBeginning, kATSUToTextEnd, &bufferSize, glyphInfo);
    }

    if (status != noErr) {
        fprintf (stderr, "ATSUGetGlyphInfo returned error %d\n", (int) status);
        PR_Free (glyphInfo);
        return;
    }

    CairoGlyphBuffer cairoGlyphs;

    ItemCount i = 0;
    while (i < glyphInfo->numGlyphs) {
        ATSUStyle runStyle = glyphInfo->glyphs[i].style;

        ItemCount lastRunGlyph;
        for (lastRunGlyph = i+1; lastRunGlyph < glyphInfo->numGlyphs; lastRunGlyph++) {
            if (glyphInfo->glyphs[lastRunGlyph].style != runStyle)
                break;
        }

        ItemCount numGlyphs = lastRunGlyph-i;
        ATSUFontID runFontID;
        ByteCount unused;
        status = ATSUGetAttribute (runStyle, kATSUFontTag, sizeof(ATSUFontID), &runFontID, &unused);
        if (status != noErr || runFontID == kATSUInvalidFontID) {
            i += numGlyphs;
            continue;
        }

        cairoGlyphs.EnsureSize(numGlyphs);
        cairo_glyph_t *runGlyphs = cairoGlyphs.glyphs;

        for (ItemCount k = i; k < lastRunGlyph; k++) {
            runGlyphs->index = glyphInfo->glyphs[k].glyphID;
            runGlyphs->x = glyphInfo->glyphs[k].idealX; /* screenX */
            runGlyphs->y = glyphInfo->glyphs[k].deltaY;

            runGlyphs++;
        }

        gfxAtsuiFont *font = mGroup->FindFontFor(runFontID);

        if (gfxFontTestStore::CurrentStore()) {
            gfxFontTestStore::CurrentStore()->AddItem(font->GetUniqueName(),
                                                      cairoGlyphs.glyphs, numGlyphs);
        }

        cairo_set_scaled_font (cr, font->CairoScaledFont());
        cairo_show_glyphs (cr, cairoGlyphs.glyphs, numGlyphs);

        i += numGlyphs;
    }

    PR_Free (glyphInfo);
    
    cairo_restore (cr);
}

gfxFloat
gfxAtsuiTextRun::Measure(gfxContext *aContext)
{
    OSStatus status;
    ATSTrapezoid trap;
    ItemCount numBounds;

    status = ATSUGetGlyphBounds(mATSULayout,
                                FloatToFixed(0.0),
                                FloatToFixed(0.0),
                                kATSUFromTextBeginning,
                                kATSUToTextEnd,
                                kATSUseFractionalOrigins,
                                1,
                                &trap,
                                &numBounds);
    if (status != noErr) {
        fprintf(stderr, "ATSUGetGlyphBounds returned error %d!\n", (int) status);
        return 0.0;
    }

#if 0
    printf ("Measure: '%s' trap: %f %f %f %f\n",
            NS_ConvertUTF16toUTF8(mString).get(),
            FixedToFloat(trap.upperLeft.x), FixedToFloat(trap.lowerLeft.x),
            FixedToFloat(trap.upperRight.x), FixedToFloat(trap.lowerRight.x));
#endif

    float f =
        FixedToFloat(PR_MAX(trap.upperRight.x, trap.lowerRight.x)) -
        FixedToFloat(PR_MIN(trap.upperLeft.x, trap.lowerLeft.x));

    return f;
}

void
gfxAtsuiTextRun::SetSpacing(const nsTArray<gfxFloat> &spacingArray)
{
    // We can implement this, but there's really a mismatch between
    // the spacings given here and cluster spacings.  So we're
    // going to do nothing until we figure out what the layout API
    // will look like for this.
}

const nsTArray<gfxFloat> *const
gfxAtsuiTextRun::GetSpacing() const
{
    return nsnull;
}

