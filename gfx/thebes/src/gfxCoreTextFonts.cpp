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
 * Portions created by the Initial Developer are Copyright (C) 2006-2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   John Daggett <jdaggett@mozilla.com>
 *   Jonathan Kew <jfkthame@gmail.com>
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
#include "nsBidiUtils.h"

#include "gfxTypes.h"

#include "nsPromiseFlatString.h"

#include "gfxContext.h"
#include "gfxPlatform.h"
#include "gfxPlatformMac.h"
#include "gfxCoreTextFonts.h"

#include "gfxFontTest.h"
#include "gfxFontUtils.h"

#include "cairo-quartz.h"

#include "gfxQuartzSurface.h"
#include "gfxQuartzFontCache.h"
#include "gfxUserFontSet.h"

#include "nsUnicodeRange.h"

// Uncomment this to dump all text runs created to the log (if enabled)
//#define DUMP_TEXT_RUNS

#ifdef DUMP_TEXT_RUNS
static PRLogModuleInfo *gCoreTextTextRunLog = PR_NewLogModule("coreTextTextRun");
#endif

#define ROUND(x) (floor((x) + 0.5))


// standard font descriptors that we construct the first time they're needed
CTFontDescriptorRef gfxCoreTextFont::sDefaultFeaturesDescriptor = NULL;
CTFontDescriptorRef gfxCoreTextFont::sDisableLigaturesDescriptor = NULL;


gfxCoreTextFont::gfxCoreTextFont(MacOSFontEntry *aFontEntry,
                                 const gfxFontStyle *aFontStyle,
                                 PRBool aNeedsBold)
    : gfxFont(aFontEntry, aFontStyle),
      mFontStyle(aFontStyle),
      mHasMetrics(PR_FALSE),
      mAdjustedSize(0.0f)
{
    mATSFont = aFontEntry->GetFontRef();
    mCTFont = NULL;

    // determine whether synthetic bolding is needed
    PRInt8 baseWeight, weightDistance;
    mFontStyle->ComputeWeightAndOffset(&baseWeight, &weightDistance);
    PRUint16 targetWeight = (baseWeight * 100) + (weightDistance * 100);

    // synthetic bolding occurs when font itself is not a bold-face and either the absolute weight
    // is at least 600 or the relative weight (e.g. 402) implies a darker face than the ones available.
    // note: this means that (1) lighter styles *never* synthetic bold and (2) synthetic bolding always occurs
    // at the first bolder step beyond available faces, no matter how light the boldest face
    if (!aFontEntry->IsBold()
        && ((weightDistance == 0 && targetWeight >= 600) || (weightDistance > 0 && aNeedsBold)))
    {
        mSyntheticBoldOffset = 1;  // devunit offset when double-striking text to fake boldness
    }

    // InitMetrics will create the mCTFont (possibly taking account of sizeAdjust)
    InitMetrics();
    if (!mIsValid) {
        return;
    }

    // Set up the default attribute dictionary that we will need each time we create a CFAttributedString
    mAttributesDict =
        CFDictionaryCreate(kCFAllocatorDefault,
                           (const void**) &kCTFontAttributeName,
                           (const void**) &mCTFont,
                           1, // count of attributes
                           &kCFTypeDictionaryKeyCallBacks,
                           &kCFTypeDictionaryValueCallBacks);

    // Remaining initialization is largely based on CommonInit() in the gfxAtsuiFont code
    CGFontRef cgFont = ::CGFontCreateWithPlatformFont(&mATSFont);
    mFontFace = cairo_quartz_font_face_create_for_cgfont(cgFont);
    ::CGFontRelease(cgFont);

    cairo_matrix_t sizeMatrix, ctm;
    cairo_matrix_init_identity(&ctm);
    cairo_matrix_init_scale(&sizeMatrix, mAdjustedSize, mAdjustedSize);

    // synthetic oblique by skewing via the font matrix
    PRBool needsOblique =
        (mFontEntry != NULL) &&
        (!mFontEntry->IsItalic() && (mFontStyle->style & (FONT_STYLE_ITALIC | FONT_STYLE_OBLIQUE)));

    if (needsOblique) {
        double skewfactor = (needsOblique ? Fix2X(kATSItalicQDSkew) : 0);

        cairo_matrix_t style;
        cairo_matrix_init(&style,
                          1,                //xx
                          0,                //yx
                          -1 * skewfactor,   //xy
                          1,                //yy
                          0,                //x0
                          0);               //y0
        cairo_matrix_multiply(&sizeMatrix, &sizeMatrix, &style);
    }

    cairo_font_options_t *fontOptions = cairo_font_options_create();

    // turn off font anti-aliasing based on user pref setting
    if (mAdjustedSize <= (float) gfxPlatformMac::GetPlatform()->GetAntiAliasingThreshold()) {
        cairo_font_options_set_antialias(fontOptions, CAIRO_ANTIALIAS_NONE);
        //printf("font: %s, size: %f, disabling anti-aliasing\n", NS_ConvertUTF16toUTF8(GetName()).get(), mAdjustedSize);
    }

    mScaledFont = cairo_scaled_font_create(mFontFace, &sizeMatrix, &ctm, fontOptions);
    cairo_font_options_destroy(fontOptions);

    cairo_status_t cairoerr = cairo_scaled_font_status(mScaledFont);
    if (cairoerr != CAIRO_STATUS_SUCCESS) {
        mIsValid = PR_FALSE;

#ifdef DEBUG
        char warnBuf[1024];
        sprintf(warnBuf, "Failed to create scaled font: %s status: %d", NS_ConvertUTF16toUTF8(GetName()).get(), cairoerr);
        NS_WARNING(warnBuf);
#endif
    }
}

static double
RoundToNearestMultiple(double aValue, double aFraction)
{
  return floor(aValue/aFraction + 0.5)*aFraction;
}

PRBool
gfxCoreTextFont::SetupCairoFont(gfxContext *aContext)
{
    cairo_scaled_font_t *scaledFont = CairoScaledFont();
    if (cairo_scaled_font_status(scaledFont) != CAIRO_STATUS_SUCCESS) {
        // Don't cairo_set_scaled_font as that would propagate the error to
        // the cairo_t, precluding any further drawing.
        return PR_FALSE;
    }
    cairo_set_scaled_font(aContext->GetCairo(), scaledFont);
    return PR_TRUE;
}

float
gfxCoreTextFont::GetCharWidth(PRUnichar aUniChar, PRUint32 *aGlyphID)
{
    UniChar c = aUniChar;
    CGGlyph glyph;
    if (CTFontGetGlyphsForCharacters(mCTFont, &c, &glyph, 1)) {
        CGSize advance;
        CTFontGetAdvancesForGlyphs(mCTFont,
                                   kCTFontHorizontalOrientation,
                                   &glyph,
                                   &advance,
                                   1);
        if (aGlyphID != nsnull)
            *aGlyphID = glyph;
        return advance.width;
    }

    // couldn't get glyph for the char
    if (aGlyphID != nsnull)
        *aGlyphID = 0;
    return 0;
}

float
gfxCoreTextFont::GetCharHeight(PRUnichar aUniChar)
{
    UniChar c = aUniChar;
    CGGlyph glyph;
    if (CTFontGetGlyphsForCharacters(mCTFont, &c, &glyph, 1)) {
        CGRect boundingRect;
        CTFontGetBoundingRectsForGlyphs(mCTFont,
                                        kCTFontHorizontalOrientation,
                                        &glyph,
                                        &boundingRect,
                                        1);
        return boundingRect.size.height;
    }

    // couldn't get glyph for the char
    return 0;
}

gfxCoreTextFont::~gfxCoreTextFont()
{
    cairo_scaled_font_destroy(mScaledFont);
    cairo_font_face_destroy(mFontFace);

    CFRelease(mAttributesDict);
    CFRelease(mCTFont);
}

MacOSFontEntry*
gfxCoreTextFont::GetFontEntry()
{
    return static_cast<MacOSFontEntry*>(mFontEntry.get());
}

PRBool
gfxCoreTextFont::TestCharacterMap(PRUint32 aCh)
{
    return mIsValid && GetFontEntry()->TestCharacterMap(aCh);
}

void
gfxCoreTextFont::InitMetrics()
{
    if (mHasMetrics)
        return;

    gfxFloat size =
        PR_MAX(((mAdjustedSize != 0.0f) ? mAdjustedSize : GetStyle()->size), 1.0f);

    if (mCTFont != NULL) {
        CFRelease(mCTFont);
        mCTFont = NULL;
    }

    ATSFontMetrics atsMetrics;
    OSStatus err;

    err = ATSFontGetHorizontalMetrics(mATSFont, kATSOptionFlagsDefault,
                                      &atsMetrics);
    if (err != noErr) {
        mIsValid = PR_FALSE;

#ifdef DEBUG
        char warnBuf[1024];
        sprintf(warnBuf, "Bad font metrics for: %s err: %8.8x",
                NS_ConvertUTF16toUTF8(GetName()).get(), PRUint32(err));
        NS_WARNING(warnBuf);
#endif
        return;
    }

    // prefer to get xHeight from ATS metrics (unhinted) rather than Core Text (hinted),
    // see bug 429605.
    if (atsMetrics.xHeight > 0) {
        mMetrics.xHeight = atsMetrics.xHeight * size;
    } else {
        mCTFont = CTFontCreateWithPlatformFont(mATSFont, size, NULL, GetDefaultFeaturesDescriptor());
        mMetrics.xHeight = CTFontGetXHeight(mCTFont);
    }

    if (mAdjustedSize == 0.0f) {
        if (mMetrics.xHeight != 0.0f && GetStyle()->sizeAdjust != 0.0f) {
            gfxFloat aspect = mMetrics.xHeight / size;
            mAdjustedSize = GetStyle()->GetAdjustedSize(aspect);

            // the recursive call to InitMetrics will re-create mCTFont, with adjusted size,
            // and then continue to set up the rest of the metrics fields
            InitMetrics();
            return;
        }
        mAdjustedSize = size;
    }

    // create the CTFontRef if we didn't already do so above
    if (mCTFont == NULL)
        mCTFont = CTFontCreateWithPlatformFont(mATSFont, size, NULL, GetDefaultFeaturesDescriptor());

    mMetrics.superscriptOffset = mMetrics.xHeight;
    mMetrics.subscriptOffset = mMetrics.xHeight;
    mMetrics.underlineSize = CTFontGetUnderlineThickness(mCTFont);
    mMetrics.underlineOffset = CTFontGetUnderlinePosition(mCTFont);
    mMetrics.strikeoutSize = mMetrics.underlineSize;
    mMetrics.strikeoutOffset = mMetrics.xHeight / 2;

    mMetrics.externalLeading = CTFontGetLeading(mCTFont);
    mMetrics.emHeight = size;
//    mMetrics.maxAscent = CTFontGetAscent(mCTFont);
//    mMetrics.maxDescent = CTFontGetDescent(mCTFont);
    // using the ATS metrics rather than CT gives us results more consistent with the ATSUI path
    mMetrics.maxAscent =
      NS_ceil(RoundToNearestMultiple(atsMetrics.ascent * size, 1/1024.0));
    mMetrics.maxDescent =
      NS_ceil(-RoundToNearestMultiple(atsMetrics.descent * size, 1/1024.0));

    mMetrics.maxHeight = mMetrics.maxAscent + mMetrics.maxDescent;
    if (mMetrics.maxHeight - mMetrics.emHeight > 0.0)
        mMetrics.internalLeading = mMetrics.maxHeight - mMetrics.emHeight;
    else
        mMetrics.internalLeading = 0.0;

    mMetrics.maxAdvance = atsMetrics.maxAdvanceWidth * size + mSyntheticBoldOffset;

    mMetrics.emAscent = mMetrics.maxAscent * mMetrics.emHeight / mMetrics.maxHeight;
    mMetrics.emDescent = mMetrics.emHeight - mMetrics.emAscent;

    PRUint32 glyphID;
    float xWidth = GetCharWidth('x', &glyphID);
    if (atsMetrics.avgAdvanceWidth != 0.0)
        mMetrics.aveCharWidth = PR_MIN(atsMetrics.avgAdvanceWidth * size, xWidth);
    else if (glyphID != 0)
        mMetrics.aveCharWidth = xWidth;
    else
        mMetrics.aveCharWidth = mMetrics.maxAdvance;
    mMetrics.aveCharWidth += mSyntheticBoldOffset;

    if (GetFontEntry()->IsFixedPitch()) {
        // Some Quartz fonts are fixed pitch, but there's some glyph with a bigger
        // advance than the average character width... this forces
        // those fonts to be recognized like fixed pitch fonts by layout.
        mMetrics.maxAdvance = mMetrics.aveCharWidth;
    }

    mMetrics.spaceWidth = GetCharWidth(' ', &glyphID);
    mSpaceGlyph = glyphID;

    mMetrics.zeroOrAveCharWidth = GetCharWidth('0', &glyphID);
    if (glyphID == 0)
        mMetrics.zeroOrAveCharWidth = mMetrics.aveCharWidth;

    mHasMetrics = PR_TRUE;

    SanitizeMetrics(&mMetrics, GetFontEntry()->mIsBadUnderlineFont);

#if 0
    fprintf (stderr, "Font: %p (%s) size: %f\n", this,
             NS_ConvertUTF16toUTF8(GetName()).get(), mStyle.size);
//    fprintf (stderr, "    fbounds.origin.x %f y %f size.width %f height %f\n", fbounds.origin.x, fbounds.origin.y, fbounds.size.width, fbounds.size.height);
    fprintf (stderr, "    emHeight: %f emAscent: %f emDescent: %f\n", mMetrics.emHeight, mMetrics.emAscent, mMetrics.emDescent);
    fprintf (stderr, "    maxAscent: %f maxDescent: %f maxAdvance: %f\n", mMetrics.maxAscent, mMetrics.maxDescent, mMetrics.maxAdvance);
    fprintf (stderr, "    internalLeading: %f externalLeading: %f\n", mMetrics.internalLeading, mMetrics.externalLeading);
    fprintf (stderr, "    spaceWidth: %f aveCharWidth: %f xHeight: %f\n", mMetrics.spaceWidth, mMetrics.aveCharWidth, mMetrics.xHeight);
    fprintf (stderr, "    uOff: %f uSize: %f stOff: %f stSize: %f suOff: %f suSize: %f\n", mMetrics.underlineOffset, mMetrics.underlineSize, mMetrics.strikeoutOffset, mMetrics.strikeoutSize, mMetrics.superscriptOffset, mMetrics.subscriptOffset);
#endif
}

// Construct the font attribute descriptor that we'll apply by default when creating a CTFontRef.
// This will turn off line-edge swashes by default, because we don't know the actual line breaks
// when doing glyph shaping.
void
gfxCoreTextFont::CreateDefaultFeaturesDescriptor()
{
    if (sDefaultFeaturesDescriptor != NULL)
        return;

    SInt16 val = kSmartSwashType;
    CFNumberRef swashesType =
        CFNumberCreate(kCFAllocatorDefault,
                       kCFNumberSInt16Type,
                       &val);
    val = kLineInitialSwashesOffSelector;
    CFNumberRef lineInitialsOffSelector =
        CFNumberCreate(kCFAllocatorDefault,
                       kCFNumberSInt16Type,
                       &val);

    CFTypeRef keys[]   = { kCTFontFeatureTypeIdentifierKey,
                           kCTFontFeatureSelectorIdentifierKey };
    CFTypeRef values[] = { swashesType,
                           lineInitialsOffSelector };
    CFDictionaryRef featureSettings[2];
    featureSettings[0] =
        CFDictionaryCreate(kCFAllocatorDefault,
                           (const void **) keys,
                           (const void **) values,
                           NS_ARRAY_LENGTH(keys),
                           &kCFTypeDictionaryKeyCallBacks,
                           &kCFTypeDictionaryValueCallBacks);
    CFRelease(lineInitialsOffSelector);

    val = kLineFinalSwashesOffSelector;
    CFNumberRef lineFinalsOffSelector =
        CFNumberCreate(kCFAllocatorDefault,
                       kCFNumberSInt16Type,
                       &val);
    values[1] = lineFinalsOffSelector;
    featureSettings[1] =
        CFDictionaryCreate(kCFAllocatorDefault,
                           (const void **) keys,
                           (const void **) values,
                           NS_ARRAY_LENGTH(keys),
                           &kCFTypeDictionaryKeyCallBacks,
                           &kCFTypeDictionaryValueCallBacks);
    CFRelease(lineFinalsOffSelector);
    CFRelease(swashesType);

    CFArrayRef featuresArray =
        CFArrayCreate(kCFAllocatorDefault,
                      (const void **) featureSettings,
                      NS_ARRAY_LENGTH(featureSettings),
                      &kCFTypeArrayCallBacks);
    CFRelease(featureSettings[0]);
    CFRelease(featureSettings[1]);

    const CFTypeRef attrKeys[]   = { kCTFontFeatureSettingsAttribute };
    const CFTypeRef attrValues[] = { featuresArray };
    CFDictionaryRef attributesDict =
        CFDictionaryCreate(kCFAllocatorDefault,
                           (const void **) attrKeys,
                           (const void **) attrValues,
                           NS_ARRAY_LENGTH(attrKeys),
                           &kCFTypeDictionaryKeyCallBacks,
                           &kCFTypeDictionaryValueCallBacks);
    CFRelease(featuresArray);

    sDefaultFeaturesDescriptor =
        CTFontDescriptorCreateWithAttributes(attributesDict);
    CFRelease(attributesDict);
}

// Create a copy of a CTFontRef, with the Common Ligatures feature disabled [static]
CTFontRef
gfxCoreTextFont::CreateCopyWithDisabledLigatures(CTFontRef aFontRef)
{
    if (sDisableLigaturesDescriptor == NULL) {
        // initialize cached descriptor to turn off the Common Ligatures feature
        SInt16 val = kLigaturesType;
        CFNumberRef ligaturesType =
            CFNumberCreate(kCFAllocatorDefault,
                           kCFNumberSInt16Type,
                           &val);
        val = kCommonLigaturesOffSelector;
        CFNumberRef commonLigaturesOffSelector =
            CFNumberCreate(kCFAllocatorDefault,
                           kCFNumberSInt16Type,
                           &val);

        const CFTypeRef keys[]   = { kCTFontFeatureTypeIdentifierKey,
                                     kCTFontFeatureSelectorIdentifierKey };
        const CFTypeRef values[] = { ligaturesType,
                                     commonLigaturesOffSelector };
        CFDictionaryRef featureSettingDict =
            CFDictionaryCreate(kCFAllocatorDefault,
                               (const void **) keys,
                               (const void **) values,
                               NS_ARRAY_LENGTH(keys),
                               &kCFTypeDictionaryKeyCallBacks,
                               &kCFTypeDictionaryValueCallBacks);
        CFRelease(ligaturesType);
        CFRelease(commonLigaturesOffSelector);

        CFArrayRef featuresArray =
            CFArrayCreate(kCFAllocatorDefault,
                          (const void **) &featureSettingDict,
                          1,
                          &kCFTypeArrayCallBacks);
        CFRelease(featureSettingDict);

        CFDictionaryRef attributesDict =
            CFDictionaryCreate(kCFAllocatorDefault,
                               (const void **) &kCTFontFeatureSettingsAttribute,
                               (const void **) &featuresArray,
                               1, // count of keys & values
                               &kCFTypeDictionaryKeyCallBacks,
                               &kCFTypeDictionaryValueCallBacks);
        CFRelease(featuresArray);

        sDisableLigaturesDescriptor =
            CTFontDescriptorCreateWithAttributes(attributesDict);
        CFRelease(attributesDict);
    }
    
    aFontRef =
        CTFontCreateCopyWithAttributes(aFontRef,
                                       0.0,
                                       NULL,
                                       sDisableLigaturesDescriptor);
    return aFontRef;
}

void
gfxCoreTextFont::Shutdown() // [static]
{
    if (sDisableLigaturesDescriptor != NULL) {
        CFRelease(sDisableLigaturesDescriptor);
        sDisableLigaturesDescriptor = NULL;
    }        
    if (sDefaultFeaturesDescriptor != NULL) {
        CFRelease(sDefaultFeaturesDescriptor);
        sDefaultFeaturesDescriptor = NULL;
    }
}


/**
 * Look up the font in the gfxFont cache. If we don't find it, create one.
 * In either case, add a ref and return it ---
 * except for OOM in which case we do nothing and return null.
 */

static already_AddRefed<gfxCoreTextFont>
GetOrMakeCTFont(MacOSFontEntry *aFontEntry, const gfxFontStyle *aStyle, PRBool aNeedsBold)
{
    // the font entry name is the psname, not the family name
    nsRefPtr<gfxFont> font = gfxFontCache::GetCache()->Lookup(aFontEntry->Name(), aStyle);
    if (!font) {
        gfxCoreTextFont *newFont = new gfxCoreTextFont(aFontEntry, aStyle, aNeedsBold);
        if (!newFont)
            return nsnull;
        if (!newFont->Valid()) {
            delete newFont;
            return nsnull;
        }
        font = newFont;
        gfxFontCache::GetCache()->AddNew(font);
    }
    gfxFont *f = nsnull;
    font.swap(f);
    return static_cast<gfxCoreTextFont *>(f);
}


gfxCoreTextFontGroup::gfxCoreTextFontGroup(const nsAString& families,
                                           const gfxFontStyle *aStyle,
                                           gfxUserFontSet *aUserFontSet)
    : gfxFontGroup(families, aStyle, aUserFontSet)
{
    ForEachFont(FindCTFont, this);

    if (mFonts.Length() == 0) {
        // XXX this will generate a list of the lang groups for which we have no
        // default fonts for on the mac; we should fix this!
        // Known:
        // ja x-beng x-devanagari x-tamil x-geor x-ethi x-gujr x-mlym x-armn
        // x-orya x-telu x-knda x-sinh

        //fprintf (stderr, "gfxCoreTextFontGroup: %s [%s] -> %d fonts found\n", NS_ConvertUTF16toUTF8(families).get(), aStyle->langGroup.get(), mFonts.Length());

        // If we get here, we most likely didn't have a default font for
        // a specific langGroup.  Let's just pick the default OSX
        // user font.

        PRBool needsBold;
        MacOSFontEntry *defaultFont =
            gfxQuartzFontCache::SharedFontCache()->GetDefaultFont(aStyle, needsBold);
        NS_ASSERTION(defaultFont, "invalid default font returned by GetDefaultFont");

        nsRefPtr<gfxCoreTextFont> font = GetOrMakeCTFont(defaultFont, aStyle, needsBold);

        if (font) {
            mFonts.AppendElement(font);
        }
    }

    mPageLang = gfxPlatform::GetFontPrefLangFor(mStyle.langGroup.get());

    if (!mStyle.systemFont) {
        for (PRUint32 i = 0; i < mFonts.Length(); ++i) {
            gfxCoreTextFont* font = static_cast<gfxCoreTextFont*>(mFonts[i].get());
            if (font->GetFontEntry()->mIsBadUnderlineFont) {
                gfxFloat first = mFonts[0]->GetMetrics().underlineOffset;
                gfxFloat bad = font->GetMetrics().underlineOffset;
                mUnderlineOffset = PR_MIN(first, bad);
                break;
            }
        }
    }
}

PRBool
gfxCoreTextFontGroup::FindCTFont(const nsAString& aName,
                                 const nsACString& aGenericName,
                                 void *closure)
{
    gfxCoreTextFontGroup *fontGroup = (gfxCoreTextFontGroup*) closure;
    const gfxFontStyle *fontStyle = fontGroup->GetStyle();


    PRBool needsBold;
    MacOSFontEntry *fe = nsnull;

    // first, look up in the user font set
    gfxUserFontSet *fs = fontGroup->GetUserFontSet();
    gfxFontEntry *gfe;
    if (fs && (gfe = fs->FindFontEntry(aName, *fontStyle, needsBold))) {
        // assume for now platform font if not SVG
        fe = static_cast<MacOSFontEntry*> (gfe);
    }

    // nothing in the user font set ==> check system fonts
    if (!fe) {
        gfxQuartzFontCache *fc = gfxQuartzFontCache::SharedFontCache();
        fe = fc->FindFontForFamily(aName, fontStyle, needsBold);
    }

    if (fe && !fontGroup->HasFont(fe->GetFontRef())) {
        nsRefPtr<gfxCoreTextFont> font = GetOrMakeCTFont(fe, fontStyle, needsBold);
        if (font) {
            fontGroup->mFonts.AppendElement(font);
        }
    }

    return PR_TRUE;
}

gfxFontGroup *
gfxCoreTextFontGroup::Copy(const gfxFontStyle *aStyle)
{
    return new gfxCoreTextFontGroup(mFamilies, aStyle, mUserFontSet);
}

#define UNICODE_LRO 0x202d
#define UNICODE_RLO 0x202e
#define UNICODE_PDF 0x202c

inline void
AppendDirectionalIndicatorStart(PRUint32 aFlags, nsAString& aString)
{
    static const PRUnichar overrides[2] = { UNICODE_LRO, UNICODE_RLO };
    aString.Append(overrides[(aFlags & gfxTextRunFactory::TEXT_IS_RTL) != 0]);    
    aString.Append(' ');
}

inline void
AppendDirectionalIndicatorEnd(PRBool aNeedDirection, nsAString& aString)
{
    // append a space (always, for consistent treatment of last char,
    // and a direction control if required (we skip this for 8-bit text,
    // which is known to be unidirectional LTR, unless the direction was
    // forced RTL via overrides)
    aString.Append(' ');
    if (!aNeedDirection)
        return;

    aString.Append('.');
    aString.Append(UNICODE_PDF);
}

gfxTextRun *
gfxCoreTextFontGroup::MakeTextRun(const PRUint8 *aString, PRUint32 aLength,
                                  const Parameters *aParams, PRUint32 aFlags)
{
    NS_ASSERTION(aLength > 0, "should use MakeEmptyTextRun for zero-length text");
    NS_ASSERTION(aFlags & TEXT_IS_8BIT, "should be marked 8bit");
    gfxTextRun *textRun = gfxTextRun::Create(aParams, aString, aLength, this, aFlags);
    if (!textRun)
        return nsnull;

    nsDependentCSubstring cString(reinterpret_cast<const char*>(aString),
                                  reinterpret_cast<const char*>(aString) + aLength);

    nsAutoString utf16;
    PRBool wrapBidi = (aFlags & TEXT_IS_RTL) != 0;
    if (wrapBidi)
        AppendDirectionalIndicatorStart(aFlags, utf16);
    PRUint32 startOffset = utf16.Length();
    AppendASCIItoUTF16(cString, utf16);
    AppendDirectionalIndicatorEnd(wrapBidi, utf16);

    InitTextRun(textRun, utf16.get(), utf16.Length(), startOffset, aLength);

    textRun->FetchGlyphExtents(aParams->mContext);

    return textRun;
}

gfxTextRun *
gfxCoreTextFontGroup::MakeTextRun(const PRUnichar *aString, PRUint32 aLength,
                                  const Parameters *aParams, PRUint32 aFlags)
{
    NS_ASSERTION(aLength > 0, "should use MakeEmptyTextRun for zero-length text");
    gfxTextRun *textRun = gfxTextRun::Create(aParams, aString, aLength, this, aFlags);
    if (!textRun)
        return nsnull;

    textRun->RecordSurrogates(aString);
    gfxPlatformMac::SetupClusterBoundaries(textRun, aString);

    nsAutoString utf16;
    AppendDirectionalIndicatorStart(aFlags, utf16);
    PRUint32 startOffset = utf16.Length();
    utf16.Append(aString, aLength);
    // Ensure that none of the whitespace in the run is considered "trailing"
    // by CoreText's bidi algorithm
    AppendDirectionalIndicatorEnd(PR_TRUE, utf16);

    InitTextRun(textRun, utf16.get(), utf16.Length(), startOffset, aLength);

    textRun->FetchGlyphExtents(aParams->mContext);

    return textRun;
}

#define SMALL_GLYPH_RUN 128 // preallocated size of our auto arrays for per-glyph data;
                            // some testing indicates that 90%+ of glyph runs will fit
                            // without requiring a separate allocation

void
gfxCoreTextFontGroup::InitTextRun(gfxTextRun *aTextRun,
                                  const PRUnichar *aString,
                                  PRUint32 aTotalLength,
                                  PRUint32 aLayoutStart,
                                  PRUint32 aLayoutLength)
{
    PRBool disableLigatures = (aTextRun->GetFlags() & TEXT_DISABLE_OPTIONAL_LIGATURES) != 0;

    gfxCoreTextFont *mainFont = static_cast<gfxCoreTextFont*>(mFonts[0].get());

#ifdef DUMP_TEXT_RUNS
    NS_ConvertUTF16toUTF8 str(aString, aTotalLength);
    NS_ConvertUTF16toUTF8 families(mFamilies);
    PR_LOG(gCoreTextTextRunLog, PR_LOG_DEBUG,\
           ("MakeTextRun %p fontgroup %p (%s) lang: %s len %d TEXTRUN \"%s\" ENDTEXTRUN\n",
            aTextRun, this, families.get(), mStyle.langGroup.get(), aTotalLength, str.get()));
#endif

    // This is awfully verbose, but the idea is simply to create a CFAttributedString
    // with our text and style info, then we can use CoreText to do layout with it
    CFStringRef stringObj =
        CFStringCreateWithCharactersNoCopy(kCFAllocatorDefault,
                                           aString,
                                           aTotalLength,
                                           kCFAllocatorNull);

    CFDictionaryRef attrObj;
    if (disableLigatures) {
        // For letterspacing (or maybe other situations) we need to make a copy of the CTFont
        // with the ligature feature disabled
        CTFontRef mainCTFont = mainFont->GetCTFont();
        mainCTFont = gfxCoreTextFont::CreateCopyWithDisabledLigatures(mainCTFont);

        // Set up the initial font, for the (common) case of a monostyled textRun
        attrObj =
            CFDictionaryCreate(kCFAllocatorDefault,
                               (const void**) &kCTFontAttributeName,
                               (const void**) &mainCTFont,
                               1, // count of attributes
                               &kCFTypeDictionaryKeyCallBacks,
                               &kCFTypeDictionaryValueCallBacks);
        // Having created the dict, we're finished with our modified copy of the CTFont
        CFRelease(mainCTFont);
    } else {
        attrObj = mainFont->GetAttributesDictionary();
        CFRetain(attrObj);
    }

    // Now we can create an attributed string
    CFAttributedStringRef attrStringObj =
        CFAttributedStringCreate(kCFAllocatorDefault, stringObj, attrObj);
    CFRelease(stringObj);
    CFRelease(attrObj);

    // Note that the attributed string is currently not mutable;
    // if we need to change attributes of a range, we'll first make a mutable copy
    CFMutableAttributedStringRef mutableStringObj = NULL;

    PRUint32 runStart = aLayoutStart;
    nsAutoTArray<gfxTextRange,3> fontRanges;
    ComputeRanges(fontRanges, aString, runStart, runStart + aLayoutLength);
    PRUint32 numRanges = fontRanges.Length();

    nsAutoTArray<PRPackedBool,SMALL_GLYPH_RUN> unmatchedArray;
    PRPackedBool *unmatched = NULL;

    for (PRUint32 r = 0; r < numRanges; r++) {
        const gfxTextRange& range = fontRanges[r];
        UniCharCount matchedLength = range.Length();
        gfxCoreTextFont *matchedFont =
            static_cast<gfxCoreTextFont*> (range.font ? range.font.get() : nsnull);

#ifdef DUMP_TEXT_RUNS
        PR_LOG(gCoreTextTextRunLog, PR_LOG_DEBUG,
               ("InitTextRun %p fontgroup %p font %p match %s (%d-%d)", aTextRun, this, matchedFont,
                   (matchedFont ? NS_ConvertUTF16toUTF8(matchedFont->GetUniqueName()).get() : "<null>"),
                   runStart, matchedLength));
#endif

        if (matchedFont) {
            // apply the appropriate font to the run, if it's not the primary font that was already set
            if (matchedFont != mainFont) {
                CTFontRef matchedCTFont = matchedFont->GetCTFont();
                if (disableLigatures)
                    matchedCTFont = gfxCoreTextFont::CreateCopyWithDisabledLigatures(matchedCTFont);
                // if necessary, make a mutable copy of the string
                if (!mutableStringObj) {
                    mutableStringObj =
                        CFAttributedStringCreateMutableCopy(kCFAllocatorDefault,
                                                            0,
                                                            attrStringObj);
                    CFRelease(attrStringObj);
                }
                CFAttributedStringSetAttribute(mutableStringObj,
                                               CFRangeMake(runStart, matchedLength),
                                               kCTFontAttributeName,
                                               matchedCTFont);
                if (disableLigatures)
                    CFRelease(matchedCTFont);
            }

            aTextRun->AddGlyphRun(matchedFont, runStart-aLayoutStart, matchedLength);

        } else {
            // no font available, so record missing glyph info instead
            if (unmatched == NULL) {
                if (unmatchedArray.SetLength(aTotalLength)) {
                    unmatched = unmatchedArray.Elements();
                    memset(unmatched, PR_FALSE, aTotalLength*sizeof(PRPackedBool));
                }
            }

            // create the glyph run before calling SetMissing Glyph
            aTextRun->AddGlyphRun(mainFont, runStart-aLayoutStart, matchedLength);

            for (PRUint32 index = runStart; index < runStart + matchedLength; index++) {
                // Record the char code so we can draw a box with the Unicode value; also need to
                // replace char(s) in the string with SPACE because CoreText crashes on missing chars :(
                if (!mutableStringObj) {
                    mutableStringObj =
                        CFAttributedStringCreateMutableCopy(kCFAllocatorDefault,
                                                            0,
                                                            attrStringObj);
                    CFRelease(attrStringObj);
                }

                if (NS_IS_HIGH_SURROGATE(aString[index]) &&
                    index + 1 < aTotalLength &&
                    NS_IS_LOW_SURROGATE(aString[index+1])) {
                    aTextRun->SetMissingGlyph(index-aLayoutStart,
                                              SURROGATE_TO_UCS4(aString[index],
                                                                aString[index+1]));
                    CFAttributedStringReplaceString(mutableStringObj,
                                                    CFRangeMake(index, 2),
                                                    CFSTR("  "));
                    index++;
                } else {
                    aTextRun->SetMissingGlyph(index-aLayoutStart, aString[index]);
                    CFAttributedStringReplaceString(mutableStringObj,
                                                    CFRangeMake(index, 1),
                                                    CFSTR(" "));
                }
            }

            // We have to remember the indices of unmatched chars to avoid overwriting
            // their glyph (actually char code) data with the space glyph later,
            // while we're retrieving actual glyph data from CoreText runs.
            if (unmatched)
                memset(unmatched + runStart, PR_TRUE, matchedLength);
        }

        runStart += matchedLength;
    }

    // Create the CoreText line from our string, then we're done with it
    CTLineRef line;
    if (mutableStringObj) {
        line = CTLineCreateWithAttributedString(mutableStringObj);
        CFRelease(mutableStringObj);
    } else {
        line = CTLineCreateWithAttributedString(attrStringObj);
        CFRelease(attrStringObj);
    }

    // and finally retrieve the glyph data and store into the gfxTextRun
    CFArrayRef glyphRuns = CTLineGetGlyphRuns(line);
    PRUint32 numRuns = CFArrayGetCount(glyphRuns);

    // Iterate through the glyph runs. Note that they may extend into the wrapper
    // area, so we have to be careful not to include the extra glyphs from there
    for (PRUint32 runIndex = 0; runIndex < numRuns; runIndex++) {
        CTRunRef aCTRun = (CTRunRef)CFArrayGetValueAtIndex(glyphRuns, runIndex);
        if (SetGlyphsFromRun(aTextRun, aCTRun, unmatched, aLayoutStart, aLayoutLength) != NS_OK)
            break;
    } // end loop over each CTRun in the CTLine

    CFRelease(line);

    // It's possible for CoreText to omit glyph runs if it decides they contain
    // only invisibles (e.g., U+FEFF, see reftest 474417-1). In this case, we
    // need to eliminate them from the glyph run array to avoid drawing "partial
    // ligatures" with the wrong font.
    aTextRun->SanitizeGlyphRuns();

    // Is this actually necessary? Without it, gfxTextRun::CopyGlyphDataFrom may assert
    // "Glyphruns not coalesced", but does that matter?
    aTextRun->SortGlyphRuns();
}

nsresult
gfxCoreTextFontGroup::SetGlyphsFromRun(gfxTextRun *aTextRun,
                                       CTRunRef aCTRun,
                                       const PRPackedBool *aUnmatched,
                                       PRInt32 aLayoutStart,
                                       PRInt32 aLayoutLength)
{
    // The textRun has been bidi-wrapped (if necessary);
    // aLayoutStart and aLayoutLength define the range of characters
    // within the textRun that are "real" data we need to handle.
    // aCTRun is a glyph run from the CoreText layout process.

    PRBool isLTR = !aTextRun->IsRightToLeft();
    PRInt32 direction = isLTR ? 1 : -1;

    PRInt32 numGlyphs = CTRunGetGlyphCount(aCTRun);
    if (numGlyphs == 0)
        return NS_OK;

    // skip the run if it is entirely outside the real text range
    CFRange stringRange = CTRunGetStringRange(aCTRun);
    if (stringRange.location >= aLayoutStart + aLayoutLength ||
        stringRange.location + stringRange.length <= aLayoutStart)
        return NS_OK;

    // retrieve the laid-out glyph data from the CTRun
    nsAutoArrayPtr<CGGlyph> glyphsArray;
    nsAutoArrayPtr<CGPoint> positionsArray;
    nsAutoArrayPtr<CFIndex> glyphToCharArray;
    const CGGlyph* glyphs = NULL;
    const CGPoint* positions = NULL;
    const CFIndex* glyphToChar = NULL;

    // Testing indicates that CTRunGetGlyphsPtr (almost?) always succeeds,
    // and so allocating a new array and copying data with CTRunGetGlyphs
    // will be extremely rare.
    // If this were not the case, we could use an nsAutoTArray<> to
    // try and avoid the heap allocation for small runs.
    // It's possible that some future change to CoreText will mean that
    // CTRunGetGlyphsPtr fails more often; if this happens, nsAutoTArray<>
    // may become an attractive option.
    glyphs = CTRunGetGlyphsPtr(aCTRun);
    if (glyphs == NULL) {
        glyphsArray = new CGGlyph[numGlyphs];
        if (!glyphsArray)
            return NS_ERROR_OUT_OF_MEMORY;
        CTRunGetGlyphs(aCTRun, CFRangeMake(0, 0), glyphsArray.get());
        glyphs = glyphsArray.get();
    }

    positions = CTRunGetPositionsPtr(aCTRun);
    if (positions == NULL) {
        positionsArray = new CGPoint[numGlyphs];
        if (!positionsArray)
            return NS_ERROR_OUT_OF_MEMORY;
        CTRunGetPositions(aCTRun, CFRangeMake(0, 0), positionsArray.get());
        positions = positionsArray.get();
    }

    // String indices from CoreText are relative to the line, not the current run
    glyphToChar = CTRunGetStringIndicesPtr(aCTRun);
    if (glyphToChar == NULL) {
        glyphToCharArray = new CFIndex[numGlyphs];
        if (!glyphToCharArray)
            return NS_ERROR_OUT_OF_MEMORY;
        CTRunGetStringIndices(aCTRun, CFRangeMake(0, 0), glyphToCharArray.get());
        glyphToChar = glyphToCharArray.get();
    }

    double runWidth = CTRunGetTypographicBounds(aCTRun, CFRangeMake(0, 0), NULL, NULL, NULL);

    nsAutoTArray<gfxTextRun::DetailedGlyph,1> detailedGlyphs;
    gfxTextRun::CompressedGlyph g;
    const PRUint32 appUnitsPerDevUnit = aTextRun->GetAppUnitsPerDevUnit();

    // CoreText gives us the glyphindex-to-charindex mapping, which relates each glyph
    // to a source text character; we also need the charindex-to-glyphindex mapping to
    // find the glyph for a given char. Note that some chars may not map to any glyph
    // (ligature continuations), and some may map to several glyphs (eg Indic split vowels).
    // We set the glyph index to NO_GLYPH for chars that have no associated glyph, and we
    // record the last glyph index for cases where the char maps to several glyphs,
    // so that our clumping will include all the glyph fragments for the character.
    // The charToGlyph array is indexed by char position within the stringRange of the run.
    
    static const PRInt32 NO_GLYPH = -1;
    nsAutoTArray<PRInt32,SMALL_GLYPH_RUN> charToGlyphArray;
    if (!charToGlyphArray.SetLength(stringRange.length))
        return NS_ERROR_OUT_OF_MEMORY;
    PRInt32 *charToGlyph = charToGlyphArray.Elements();
    for (PRInt32 offset = 0; offset < stringRange.length; ++offset)
        charToGlyph[offset] = NO_GLYPH;
    for (PRInt32 g = 0; g < numGlyphs; ++g)
        // charToGlyph array is indexed by char position within the run, not the whole line
        charToGlyph[glyphToChar[g]-stringRange.location] = g;

    // Find character and glyph clumps that correspond, allowing for ligatures,
    // indic reordering, split glyphs, etc.
    //
    // The idea is that we'll find a character sequence starting at the first char of stringRange,
    // and extend it until it includes the character associated with the first glyph;
    // we also extend it as long as there are "holes" in the range of glyphs. So we
    // will eventually have a contiguous sequence of characters, starting at the beginning
    // of the range, that map to a contiguous sequence of glyphs, starting at the beginning
    // of the glyph array. That's a clump; then we update the starting positions and repeat.
    //
    // NB: In the case of RTL layouts, we iterate over the stringRange in reverse.
    //
    // This may find characters that fall outside the range aLayoutStart:aLayoutLength,
    // so we won't necessarily use everything we find here.

    PRInt32 glyphStart = 0; // looking for a clump that starts at this glyph index
    PRInt32 charStart = isLTR ?
        0 : stringRange.length-1; // and this char index (relative to stringRange.location)

    while (glyphStart < numGlyphs) { // keep finding groups until all glyphs are accounted for

        // We will iterate over the characters, need to ensure we have found the first glyph
        PRInt32 charEnd = charStart;
        PRInt32 glyphEnd = glyphStart;
        PRBool firstGlyphFound = PR_FALSE;

        do {
            // at the top of this loop, charEnd points to (NOT beyond) the current end of the clump;
            // we'll advance it after using it to read the corresponding glyph index.
            // glyphEnd is the limit of the (initially empty) glyph range we've found

            // check whether we've found the char for the first glyph in the clump;
            // can't break until we have seen this
            if (charEnd == glyphToChar[glyphStart] - stringRange.location)
                firstGlyphFound = PR_TRUE;

            // find glyph offset corresponding to current char
            PRInt32 curGlyph = charToGlyph[charEnd];
            charEnd += direction;
            if (curGlyph == NO_GLYPH)
                continue; // if the char has no glyph, just continue

            glyphEnd = PR_MAX(glyphEnd, curGlyph + 1); // update extent of glyph range

        } while (!firstGlyphFound); // found a starting glyph, it's a valid clump

        // We also need to check if there are any following glyphs that have to be
        // included because they are associated with a char that we've already passed;
        // otherwise we'll never find the expected startGlyph next time around the loop
        if (isLTR) {
            while (glyphEnd < numGlyphs &&
                   glyphToChar[glyphEnd] < charEnd + stringRange.location)
                glyphEnd++;
        } else {
            while (glyphEnd < numGlyphs &&
                   glyphToChar[glyphEnd] > charEnd + stringRange.location)
                glyphEnd++;
        }

        NS_ASSERTION(glyphStart < glyphEnd, "character/glyph clump contains no glyphs!");
        NS_ASSERTION(charStart != charEnd, "character/glyph contains no characters!");

        // Now charStart..charEnd is a ligature clump, corresponding to glyphStart..glyphEnd;
        // also collect any following chars with no glyph assigned (trailing parts of ligature).
        // Set baseCharIndex to the char we'll actually attach the glyphs to (1st of ligature),
        // and endCharIndex to the limit (position beyond the last char).
        PRInt32 baseCharIndex, endCharIndex;
        if (isLTR) {
            while (charEnd < stringRange.length && charToGlyph[charEnd] == NO_GLYPH)
                charEnd++;
            baseCharIndex = charStart + stringRange.location;
            endCharIndex = charEnd + stringRange.location;
        } else {
            while (charEnd >= 0 && charToGlyph[charEnd] == NO_GLYPH)
                charEnd--;
            baseCharIndex = charEnd + stringRange.location + 1;
            endCharIndex = charStart + stringRange.location + 1;
        }

        // Then we check if the clump falls outside our real string range; if so, just go to the next.
        if (baseCharIndex >= aLayoutStart + aLayoutLength || endCharIndex <= aLayoutStart) {
            glyphStart = glyphEnd;
            charStart = charEnd;
            continue;
        }

        // charIndex might be < aLayoutStart if we had a leading combining mark, for example,
        // that got ligated with the space that was prefixed to the string
        baseCharIndex = PR_MAX(baseCharIndex, aLayoutStart);
        endCharIndex = PR_MIN(endCharIndex, aLayoutStart + aLayoutLength);

        // for missing glyphs, we already recorded the info in the textRun
        if (aUnmatched && aUnmatched[baseCharIndex]) {
            glyphStart = glyphEnd;
            charStart = charEnd;
            continue;
        }

        // Now we're ready to set the glyph info in the textRun; measure the glyph width
        // of the first (perhaps only) glyph, to see if it is "Simple"
        double toNextGlyph;
        if (glyphStart < numGlyphs-1)
            toNextGlyph = positions[glyphStart+1].x - positions[glyphStart].x;
        else
            toNextGlyph = positions[0].x + runWidth - positions[glyphStart].x;
        PRInt32 advance = PRInt32(toNextGlyph * appUnitsPerDevUnit);

        // Check if it's a simple one-to-one mapping
        PRInt32 glyphsInClump = glyphEnd - glyphStart;
        if (glyphsInClump == 1 &&
            gfxTextRun::CompressedGlyph::IsSimpleGlyphID(glyphs[glyphStart]) &&
            gfxTextRun::CompressedGlyph::IsSimpleAdvance(advance) &&
            aTextRun->IsClusterStart(baseCharIndex - aLayoutStart) &&
            positions[glyphStart].y == 0.0)
        {
            aTextRun->SetSimpleGlyph(baseCharIndex - aLayoutStart,
                                     g.SetSimpleGlyph(advance, glyphs[glyphStart]));
        } else {
            // collect all glyphs in a list to be assigned to the first char;
            // there must be at least one in the clump, and we already measured its advance,
            // hence the placement of the loop-exit test and the measurement of the next glyph
            while (1) {
                gfxTextRun::DetailedGlyph *details = detailedGlyphs.AppendElement();
                details->mGlyphID = glyphs[glyphStart];
                details->mXOffset = 0;
                details->mYOffset = -positions[glyphStart].y * appUnitsPerDevUnit;
                details->mAdvance = advance;
                if (++glyphStart >= glyphEnd)
                   break;
                if (glyphStart < numGlyphs-1)
                    toNextGlyph = positions[glyphStart+1].x - positions[glyphStart].x;
                else
                    toNextGlyph = positions[0].x + runWidth - positions[glyphStart].x;
                advance = PRInt32(toNextGlyph * appUnitsPerDevUnit);
            }

            gfxTextRun::CompressedGlyph g;
            g.SetComplex(aTextRun->IsClusterStart(baseCharIndex - aLayoutStart),
                         PR_TRUE, detailedGlyphs.Length());
            aTextRun->SetGlyphs(baseCharIndex - aLayoutStart, g, detailedGlyphs.Elements());

            detailedGlyphs.Clear();
        }

        // the rest of the chars in the group are ligature continuations, no associated glyphs
        while (++baseCharIndex != endCharIndex &&
            (baseCharIndex - aLayoutStart) < aLayoutLength) {
            g.SetComplex(glyphsInClump > 1 ?
                             PR_FALSE : aTextRun->IsClusterStart(baseCharIndex - aLayoutStart),
                         PR_FALSE, 0);
            aTextRun->SetGlyphs(baseCharIndex - aLayoutStart, g, nsnull);
        }

        glyphStart = glyphEnd;
        charStart = charEnd;
    }

    return NS_OK;
}

PRBool
gfxCoreTextFontGroup::HasFont(ATSFontRef aFontRef)
{
    for (PRUint32 i = 0; i < mFonts.Length(); ++i) {
        if (aFontRef == static_cast<gfxCoreTextFont *>(mFonts.ElementAt(i).get())->GetATSFont())
            return PR_TRUE;
    }
    return PR_FALSE;
}

struct PrefFontCallbackData {
    PrefFontCallbackData(nsTArray<nsRefPtr<MacOSFamilyEntry> >& aFamiliesArray)
        : mPrefFamilies(aFamiliesArray)
    {}

    nsTArray<nsRefPtr<MacOSFamilyEntry> >& mPrefFamilies;

    static PRBool AddFontFamilyEntry(eFontPrefLang aLang, const nsAString& aName, void *aClosure)
    {
        PrefFontCallbackData *prefFontData = (PrefFontCallbackData*) aClosure;

        MacOSFamilyEntry *family = gfxQuartzFontCache::SharedFontCache()->FindFamily(aName);
        if (family) {
            prefFontData->mPrefFamilies.AppendElement(family);
        }
        return PR_TRUE;
    }
};


already_AddRefed<gfxFont>
gfxCoreTextFontGroup::WhichPrefFontSupportsChar(PRUint32 aCh)
{
    gfxFont *font;

    // FindCharUnicodeRange only supports BMP character points and there are no non-BMP fonts in prefs
    if (aCh > 0xFFFF)
        return nsnull;

    // get the pref font list if it hasn't been set up already
    PRUint32 unicodeRange = FindCharUnicodeRange(aCh);
    eFontPrefLang charLang = gfxPlatformMac::GetFontPrefLangFor(unicodeRange);

    // if the last pref font was the first family in the pref list, no need to recheck through a list of families
    if (mLastPrefFont && charLang == mLastPrefLang &&
        mLastPrefFirstFont && mLastPrefFont->TestCharacterMap(aCh)) {
        font = mLastPrefFont;
        NS_ADDREF(font);
        return font;
    }

    // based on char lang and page lang, set up list of pref lang fonts to check
    eFontPrefLang prefLangs[kMaxLenPrefLangList];
    PRUint32 i, numLangs = 0;

    gfxPlatformMac *macPlatform = gfxPlatformMac::GetPlatform();
    macPlatform->GetLangPrefs(prefLangs, numLangs, charLang, mPageLang);

    for (i = 0; i < numLangs; i++) {
        nsAutoTArray<nsRefPtr<MacOSFamilyEntry>, 5> families;
        eFontPrefLang currentLang = prefLangs[i];

        gfxQuartzFontCache *fc = gfxQuartzFontCache::SharedFontCache();

        // get the pref families for a single pref lang
        if (!fc->GetPrefFontFamilyEntries(currentLang, &families)) {
            eFontPrefLang prefLangsToSearch[1] = { currentLang };
            PrefFontCallbackData prefFontData(families);
            gfxPlatform::ForEachPrefFont(prefLangsToSearch, 1, PrefFontCallbackData::AddFontFamilyEntry,
                                           &prefFontData);
            fc->SetPrefFontFamilyEntries(currentLang, families);
        }

        // find the first pref font that includes the character
        PRUint32  i, numPrefs;
        numPrefs = families.Length();
        for (i = 0; i < numPrefs; i++) {
            // look up the appropriate face
            MacOSFamilyEntry *family = families[i];
            if (!family) continue;

            // if a pref font is used, it's likely to be used again in the same text run.
            // the style doesn't change so the face lookup can be cached rather than calling
            // GetOrMakeCTFont repeatedly.  speeds up FindFontForChar lookup times for subsequent
            // pref font lookups
            if (family == mLastPrefFamily && mLastPrefFont->TestCharacterMap(aCh)) {
                font = mLastPrefFont;
                NS_ADDREF(font);
                return font;
            }

            PRBool needsBold;
            MacOSFontEntry *fe = family->FindFont(&mStyle, needsBold);
            // if ch in cmap, create and return a gfxFont
            if (fe && fe->TestCharacterMap(aCh)) {
                nsRefPtr<gfxCoreTextFont> prefFont = GetOrMakeCTFont(fe, &mStyle, needsBold);
                if (!prefFont) continue;
                mLastPrefFamily = family;
                mLastPrefFont = prefFont;
                mLastPrefLang = charLang;
                mLastPrefFirstFont = (i == 0);
                nsRefPtr<gfxFont> font2 = (gfxFont*) prefFont;
                return font2.forget();
            }

        }
    }

    return nsnull;
}

already_AddRefed<gfxFont>
gfxCoreTextFontGroup::WhichSystemFontSupportsChar(PRUint32 aCh)
{
    MacOSFontEntry *fe;

    fe = gfxQuartzFontCache::SharedFontCache()->FindFontForChar(aCh, GetFontAt(0));
    if (fe) {
        nsRefPtr<gfxCoreTextFont> ctFont = GetOrMakeCTFont(fe, &mStyle, PR_FALSE); // ignore bolder considerations in system fallback case...
        nsRefPtr<gfxFont> font = (gfxFont*) ctFont;
        return font.forget();
    }

    return nsnull;
}

void
gfxCoreTextFontGroup::UpdateFontList()
{
    // if user font set is set, check to see if font list needs updating
    if (mUserFontSet && mCurrGeneration != GetGeneration()) {
        // xxx - can probably improve this to detect when all fonts were found, so no need to update list
        mFonts.Clear();
        ForEachFont(FindCTFont, this);
        mCurrGeneration = GetGeneration();
    }
}
