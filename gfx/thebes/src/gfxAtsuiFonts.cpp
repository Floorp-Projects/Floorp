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
#include "gfxTypes.h"

#include "nsPromiseFlatString.h"

#include "gfxContext.h"
#include "gfxAtsuiFonts.h"

#include "cairo-atsui.h"

THEBES_IMPL_REFCOUNTING(gfxAtsuiFont)

gfxAtsuiFont::gfxAtsuiFont(ATSUFontID fontID,
                           gfxAtsuiFontGroup *fontGroup)
    : mATSUFontID(fontID), mFontGroup(fontGroup)
{
    ATSFontRef fontRef = FMGetATSFontRefFromFont(fontID);

    mFontStyle = mFontGroup->GetStyle();

    ATSFontMetrics atsMetrics;
    ATSFontGetHorizontalMetrics(fontRef, kATSOptionFlagsDefault,
                                &atsMetrics);

    gfxFloat size = mFontStyle->size;

    mMetrics.emHeight = size;

    mMetrics.maxAscent = atsMetrics.ascent * size;
    mMetrics.maxDescent = - (atsMetrics.descent * size);
    mMetrics.height = mMetrics.maxAscent + mMetrics.maxDescent;

    mMetrics.internalLeading = atsMetrics.leading * size;
    mMetrics.externalLeading = 0.0;

    mMetrics.maxHeight = mMetrics.height;
    mMetrics.emAscent = mMetrics.maxAscent * mMetrics.emHeight / mMetrics.height;
    mMetrics.emDescent = mMetrics.emHeight - mMetrics.emAscent;

    mMetrics.maxAdvance = atsMetrics.maxAdvanceWidth * size;
    mMetrics.xHeight = atsMetrics.xHeight * size;

    if (atsMetrics.avgAdvanceWidth != 0.0)
        mMetrics.aveCharWidth = atsMetrics.avgAdvanceWidth * size;
    else
        mMetrics.aveCharWidth = mMetrics.maxAdvance;

    mMetrics.underlineOffset = atsMetrics.underlinePosition * size;
    mMetrics.underlineSize = atsMetrics.underlineThickness * size;

    mMetrics.subscriptOffset = mMetrics.xHeight;
    mMetrics.superscriptOffset = mMetrics.xHeight;

    mMetrics.strikeoutOffset = mMetrics.xHeight / 2.0;
    mMetrics.strikeoutSize = mMetrics.underlineSize;

    mMetrics.spaceWidth = mMetrics.aveCharWidth;

#if 0
    fprintf (stderr, "Font: %p size: %f", this, size);
    fprintf (stderr, "    emHeight: %f emAscent: %f emDescent: %f\n", mMetrics.emHeight, mMetrics.emAscent, mMetrics.emDescent);
    fprintf (stderr, "    maxAscent: %f maxDescent: %f maxAdvance: %f\n", mMetrics.maxAscent, mMetrics.maxDescent, mMetrics.maxAdvance);
    fprintf (stderr, "    height: %f internalLeading: %f externalLeading: %f\n", mMetrics.height, mMetrics.externalLeading, mMetrics.internalLeading);
    fprintf (stderr, "    spaceWidth: %f aveCharWidth: %f xHeight: %f\n", mMetrics.spaceWidth, mMetrics.aveCharWidth, mMetrics.xHeight);
    fprintf (stderr, "    uOff: %f uSize: %f stOff: %f stSize: %f suOff: %f suSize: %f\n", mMetrics.underlineOffset, mMetrics.underlineSize, mMetrics.strikeoutOffset, mMetrics.strikeoutSize, mMetrics.superscriptOffset, mMetrics.subscriptOffset);
#endif

    mFontFace = cairo_atsui_font_face_create_for_atsu_font_id(mATSUFontID);

    cairo_matrix_t sizeMatrix, ctm;
    cairo_matrix_init_identity(&ctm);
    cairo_matrix_init_scale(&sizeMatrix, size, size);

    cairo_font_options_t *fontOptions = cairo_font_options_create();
    mScaledFont = cairo_scaled_font_create(mFontFace, &sizeMatrix, &ctm, fontOptions);
    cairo_font_options_destroy(fontOptions);
}

gfxAtsuiFont::~gfxAtsuiFont()
{
    cairo_scaled_font_destroy(mScaledFont);
    cairo_font_face_destroy(mFontFace);
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

    // Create the fallback structure
    ATSUCreateFontFallbacks(&mFallbacks);

#define NUM_STATIC_FIDS 16
    ATSUFontID static_fids[NUM_STATIC_FIDS];
    ATSUFontID *fids;
    if (mFonts.Length() > NUM_STATIC_FIDS)
        fids = (ATSUFontID *) PR_Malloc(sizeof(ATSUFontID) * mFonts.Length());
    else
        fids = static_fids;

    for (unsigned int i = 0; i < mFonts.Length(); i++)
        fids[i] = ((gfxAtsuiFont*)(mFonts[i]))->GetATSUFontID();

    ATSUSetObjFontFallbacks(mFallbacks,
                            mFonts.Length(),
                            fids,
                            kATSUSequentialFallbacksPreferred /* kATSUSequentialFallbacksExclusive? */);

    if (fids != static_fids)
        PR_Free(fids);
}

/*
 * This function will /really/ want to just keep a hash lookup table
 * based on name and style attributes that we compute; doing stuff with
 * Pascal strings and whatnot is /so/ 1980's.
 */

/*
06:19 < AngryLuke> vlad, which method in NSFontManager does what you want?
06:21 < vlad_> ideally fontWithFamily:traits:weight:size:, but I really need to 
               just iterate through all the fonts in a particular family
06:21 < vlad_> and examine their traits
06:22 < AngryLuke> vlad, NSFontManager is using the private FontObject APIs for 
                   this it seems
06:22 < vlad_> AngryLuke: fun
06:22 < vlad_> AngryLuke: so what's the "right" way of doing this?
06:23 < AngryLuke> vlad, crying ;) I ended up just using the FontManager, 
                   despite the fact it is deprecated. But that won't work for 
                   Unicode fonts with no FOND
06:23 < AngryLuke> ATS has no functions for this
06:23 < AngryLuke> and NSFontManager is the only other way that is public.
*/

PRBool
gfxAtsuiFontGroup::FindATSUFont(const nsAString& aName,
                                const nsAString& aGenericName,
                                void *closure)
{
    OSStatus status;
    gfxAtsuiFontGroup *fontGroup = (gfxAtsuiFontGroup*) closure;
    const gfxFontStyle *fontStyle = fontGroup->GetStyle();

    PRInt16 baseWeight, offsetWeight;
    gfxFontStyle::ComputeWeightAndOffset(fontStyle->weight, &baseWeight, &offsetWeight);
    baseWeight += offsetWeight;
    baseWeight = PR_MIN(9, PR_MAX(0, baseWeight));

    Boolean isBold = (baseWeight >= 7) ? TRUE : FALSE;
    Boolean isItalic = ((fontStyle->style & FONT_STYLE_ITALIC) != 0) ? TRUE : FALSE;

#if 0

    Str255 pascalName;
    CopyCStringToPascalString(nsPromiseFlatCString(NS_ConvertUTF16toUTF8(aName)).get(),
                              pascalName);

    FMFontFamily fmFamily = FMGetFontFamilyFromName(pascalName);
    if (fmFamily == kInvalidFontFamily)
        return PR_TRUE;

    FMFontFamilyInstanceIterator famFontIterator;
    status = FMCreateFontFamilyInstanceIterator(fmFamily, &famFontIterator);
    if (status != noErr) {
        fprintf(stderr, "FMCreateFontFamilyInstanceIterator returned error %d\n", (int) status);
        return PR_TRUE;
    }

    FMFont famFont;
    FMFontStyle famFontStyle;
    FMFontSize famFontSize;

    while ((status = FMGetNextFontFamilyInstance(famFontIterator,
                                                 &famFont,
                                                 &famFontStyle,
                                                 &famFontSize)) == noErr)
    {
        
    }

    if (status != kFMIterationCompleted) {
        fprintf (stderr, "FMGetNextFontFamilyInstance returned error %d\n", (int) status);
        return PR_TRUE;
    }

    FMDisposeFontFamilyInstanceIterator(&familyFontIterator);
#else
    ATSUFontID fontID;

    status = ATSUFindFontFromName(NS_ConvertUTF16toUTF8(aName).get(), aName.Length(),
                                  /* nsPromiseFlatString(aName).get(),
                                     aName.Length() * 2,*/
                                  kFontFamilyName,
                                  kFontNoPlatformCode /* kFontUnicodePlatform */,
                                  kFontNoScriptCode,
                                  kFontNoLanguageCode,
                                  &fontID);

    //fprintf (stderr, "FindATSUFont: %s -> %d (status: %d)\n", NS_ConvertUTF16toUTF8(aName).get(), (int) fontID, (int) status);

    if (fontID != kATSUInvalidFontID)
        fontGroup->mFonts.AppendElement(new gfxAtsuiFont(fontID, fontGroup));
#endif

    return PR_TRUE;
}

gfxAtsuiFontGroup::~gfxAtsuiFontGroup()
{
    ATSUDisposeFontFallbacks(mFallbacks);
}

gfxTextRun*
gfxAtsuiFontGroup::MakeTextRun(const nsAString& aString)
{
    return new gfxAtsuiTextRun(aString, this);
}

/**
 ** gfxAtsuiTextRun
 **/

THEBES_IMPL_REFCOUNTING(gfxAtsuiTextRun)

gfxAtsuiTextRun::gfxAtsuiTextRun(const nsAString& aString, gfxAtsuiFontGroup *aGroup)
    : mString(aString), mGroup(aGroup)
{
    OSStatus status;
    const gfxFontStyle *fontStyle = mGroup->GetStyle();

    PRInt16 baseWeight, offsetWeight;
    gfxFontStyle::ComputeWeightAndOffset(fontStyle->weight, &baseWeight, &offsetWeight);
    baseWeight += offsetWeight;
    if (baseWeight < 0) baseWeight = 0;
    else if (baseWeight > 9) baseWeight = 9;

    // we can't do kerning until layout draws what it measures, instead of splitting things up
    ATSUAttributeTag styleTags[] = {
        kATSUFontTag,
        kATSUSizeTag,
        kATSUKerningInhibitFactorTag,
        kATSUQDBoldfaceTag,
        kATSUQDItalicTag,
    };

    ByteCount styleArgSizes[] = {
        sizeof(ATSUFontID),
        sizeof(Fixed),
        sizeof(Fract),
        sizeof(Boolean),
        sizeof(Boolean)
    };

    //fprintf (stderr, "string: '%s', size: %f\n", NS_ConvertUTF16toUTF8(aString).get(), aGroup->GetStyle()->size);

    // fSize is in points (72dpi)
    Fixed fSize = FloatToFixed(aGroup->GetStyle()->size);
    ATSUFontID fid = ((gfxAtsuiFont*)(mGroup->GetFontList()[0]))->GetATSUFontID();
    Fract inhibitKerningFactor = FloatToFract(1.0);
    /* Why am I even setting these? the fid font will end up being used no matter what;
     * need smarter font selection earlier on...
     */
    Boolean isBold = (baseWeight >= 7) ? TRUE : FALSE;
    Boolean isItalic = ((fontStyle->style & FONT_STYLE_ITALIC) != 0) ? TRUE : FALSE;


    //fprintf (stderr, " bold: %d italic: %d\n", isBold, isItalic);

    ATSUAttributeValuePtr styleArgs[] = {
        &fid,
        &fSize,
        &inhibitKerningFactor,
        &isBold,
        &isItalic
    };

    status = ATSUCreateStyle(&mATSUStyle);
    status = ATSUSetAttributes(mATSUStyle,
                               sizeof(styleTags)/sizeof(ATSUAttributeTag),
                               styleTags,
                               styleArgSizes,
                               styleArgs);
    if (status != noErr)
        fprintf (stderr, "ATUSetAttributes gave error: %d\n", (int) status);

    UniCharCount runLengths = kATSUToTextEnd;

    status = ATSUCreateTextLayoutWithTextPtr
        (nsPromiseFlatString(mString).get(),
         kATSUFromTextBeginning,
         kATSUToTextEnd,
         mString.Length(),
         1,
         &runLengths,
         &mATSUStyle,
         &mATSULayout);

    // Set up our font fallbacks
    ATSUAttributeTag lineTags[] = { kATSULineFontFallbacksTag };
    ByteCount lineArgSizes[] = { sizeof(ATSUFontFallbacks) };
    ATSUAttributeValuePtr lineArgs[] = { mGroup->GetATSUFontFallbacks() };
    status = ATSUSetLineControls(mATSULayout,
                                 0,
                                 sizeof(lineTags) / sizeof(ATSUAttributeTag),
                                 lineTags,
                                 lineArgSizes,
                                 lineArgs);
    if (status != noErr)
        fprintf(stderr, "ATSUSetLineControls gave error: %d\n", (int) status);

    ATSUSetTransientFontMatching(mATSULayout, true);
}

gfxAtsuiTextRun::~gfxAtsuiTextRun()
{
    ATSUDisposeTextLayout(mATSULayout);
    ATSUDisposeStyle(mATSUStyle);
}

void
gfxAtsuiTextRun::DrawString(gfxContext *aContext, gfxPoint pt)
{
    cairo_t *cr = aContext->GetCairo();
    cairo_set_font_face(cr, ((gfxAtsuiFont*)(mGroup->GetFontList()[0]))->CairoFontFace());
    cairo_set_font_size(cr, mGroup->GetStyle()->size);

    ItemCount cnt;
    ATSLayoutRecord *layoutRecords = nsnull;
    OSStatus status = ATSUDirectGetLayoutDataArrayPtrFromTextLayout
        (mATSULayout,
         kATSUFromTextBeginning,
         kATSUDirectDataLayoutRecordATSLayoutRecordCurrent,
         (void**)&layoutRecords,
         &cnt);

    if (status != noErr) {
        fprintf(stderr, "ATSUDirectGetLayoutDataArrayPtrFromTextLayout failed, %d\n", noErr);
        return;
    }

    cairo_glyph_t *cglyphs = (cairo_glyph_t *) PR_Malloc (cnt * sizeof(cairo_glyph_t));
    if (!cglyphs)
        return;

    //fprintf (stderr, "String: '%s'\n", NS_ConvertUTF16toUTF8(mString).get());

    PRUint32 cgindex = 0;
    for (PRUint32 i = 0; i < cnt; i++) {
        //fprintf(stderr, "[%d 0x%04x %f] ", i, layoutRecords[i].glyphID, FixedToFloat(layoutRecords[i].realPos));
        if (!(layoutRecords[i].flags & kATSGlyphInfoIsWhiteSpace)) {
            cglyphs[cgindex].index = layoutRecords[i].glyphID;
            cglyphs[cgindex].x = pt.x + FixedToFloat(layoutRecords[i].realPos);
            cglyphs[cgindex].y = pt.y;
            cgindex++;
        }
    }
    //fprintf (stderr, "\n");

    ATSUDirectReleaseLayoutDataArrayPtr(NULL,
                                        kATSUDirectDataLayoutRecordATSLayoutRecordCurrent,
                                        (void**)&layoutRecords);

    cairo_show_glyphs(cr, cglyphs, cgindex);

    PR_Free(cglyphs);
}

gfxFloat
gfxAtsuiTextRun::MeasureString(gfxContext *aContext)
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
    if (status != noErr)
        fprintf(stderr, "ATSUGetGlyphBounds returned error %d!\n", (int) status);

    float f = FixedToFloat(PR_MAX(trap.upperRight.x, trap.lowerRight.x)) - FixedToFloat(PR_MIN(trap.upperLeft.x, trap.lowerLeft.x));

    //fprintf (stderr, "measured: %f\n", f);
    return f;
}
