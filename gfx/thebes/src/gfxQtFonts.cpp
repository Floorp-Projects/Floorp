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
 * The Original Code is Mozilla Foundation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "gfxPlatformQt.h"
#include "gfxTypes.h"
#include "gfxQtFonts.h"
#include "qdebug.h"
#include "qrect.h"
#include <locale.h>
#include <QFont>
#include <QFontMetrics>
#include "cairo-ft.h"

/**
 * gfxQtFontGroup
 */

static int
FFRECountHyphens (const nsAString &aFFREName)
{
    int h = 0;
    PRInt32 hyphen = 0;
    while ((hyphen = aFFREName.FindChar('-', hyphen)) >= 0) {
        ++h;
        ++hyphen;
    }
    return h;
}

PRBool
gfxQtFontGroup::FontCallback (const nsAString& fontName,
                                 const nsACString& genericName,
                                 void *closure)
{
    nsStringArray *sa = static_cast<nsStringArray*>(closure);

    // We ignore prefs that have three hypens since they are X style prefs.
    if (genericName.Length() && FFRECountHyphens(fontName) >= 3)
        return PR_TRUE;

    if (sa->IndexOf(fontName) < 0) {
        sa->AppendString(fontName);
    }

    return PR_TRUE;
}

/**
 * Look up the font in the gfxFont cache. If we don't find it, create one.
 * In either case, add a ref, append it to the aFonts array, and return it ---
 * except for OOM in which case we do nothing and return null.
 */
static already_AddRefed<gfxQtFont>
GetOrMakeFont(const nsAString& aName, const gfxFontStyle *aStyle)
{
    nsRefPtr<gfxFont> font = gfxFontCache::GetCache()->Lookup(aName, aStyle);
    if (!font) {
        font = new gfxQtFont(aName, aStyle);
        if (!font)
            return nsnull;
        gfxFontCache::GetCache()->AddNew(font);
    }
    gfxFont *f = nsnull;
    font.swap(f);
    return static_cast<gfxQtFont *>(f);
}


gfxQtFontGroup::gfxQtFontGroup (const nsAString& families,
                                const gfxFontStyle *aStyle)
    : gfxFontGroup(families, aStyle)
{
    nsStringArray familyArray;

    // Leave non-existing fonts in the list so that fontconfig can get the
    // best match.
    ForEachFontInternal(families, aStyle->langGroup, PR_TRUE, PR_FALSE,
                        FontCallback, &familyArray);

    // Construct a string suitable for fontconfig
    nsAutoString fcFamilies;
    if (familyArray.Count()) {
        int i = 0;
        while (1) {
            fcFamilies.Append(*familyArray[i]);
            ++i;
            if (i >= familyArray.Count())
                break;
            fcFamilies.Append(NS_LITERAL_STRING(","));
        }
    }
    else {
        fcFamilies.Append(NS_LITERAL_STRING("sans-serif"));
    }

    nsRefPtr<gfxQtFont> font = GetOrMakeFont(fcFamilies, &mStyle);
    if (font) {
        mFonts.AppendElement(font);
    }
}

gfxQtFontGroup::~gfxQtFontGroup()
{
}

gfxFontGroup *
gfxQtFontGroup::Copy(const gfxFontStyle *aStyle)
{
     return new gfxQtFontGroup(mFamilies, aStyle);
}

/**
 * We use this to append an LTR or RTL Override character to the start of the
 * string. This forces Pango to honour our direction even if there are neutral
 * characters in the string.
 */
static PRInt32 AppendDirectionalIndicatorUTF8(PRBool aIsRTL, nsACString& aString)
{
    static const PRUnichar overrides[2][2] = { { 0x202d, 0 }, { 0x202e, 0 }}; // LRO, RLO
    AppendUTF16toUTF8(overrides[aIsRTL], aString);
    return 3; // both overrides map to 3 bytes in UTF8
}

gfxTextRun *gfxQtFontGroup::MakeTextRun(const PRUnichar* aString, PRUint32 aLength,
                                         const Parameters* aParams, PRUint32 aFlags)
{
    gfxTextRun *textRun = gfxTextRun::Create(aParams, aString, aLength, this, aFlags);
    if (!textRun)
        return nsnull;

    mEnableKerning = !(aFlags & gfxTextRunFactory::TEXT_OPTIMIZE_SPEED);

    textRun->RecordSurrogates(aString);

    nsCAutoString utf8;
    PRInt32 headerLen = AppendDirectionalIndicatorUTF8(textRun->IsRightToLeft(), utf8);
    AppendUTF16toUTF8(Substring(aString, aString + aLength), utf8);

    InitTextRun(textRun, (PRUint8 *)utf8.get(), utf8.Length(), headerLen);

    textRun->FetchGlyphExtents(aParams->mContext);

    return textRun;
}

gfxTextRun *gfxQtFontGroup::MakeTextRun(const PRUint8* aString, PRUint32 aLength,
                                         const Parameters* aParams, PRUint32 aFlags)
{
    NS_ASSERTION(aFlags & TEXT_IS_8BIT, "8bit should have been set");
    gfxTextRun *textRun = gfxTextRun::Create(aParams, aString, aLength, this, aFlags);
    if (!textRun)
        return nsnull;

    mEnableKerning = !(aFlags & gfxTextRunFactory::TEXT_OPTIMIZE_SPEED);

    const char *chars = reinterpret_cast<const char *>(aString);
    PRBool isRTL = textRun->IsRightToLeft();
    if ((aFlags & TEXT_IS_ASCII) && !isRTL) {
        // We don't need to send an override character here, the characters must be all
        // LTR
        InitTextRun(textRun, (PRUint8 *)chars, aLength, 0);
    } else {
        // Although chars in not necessarily ASCII (as it may point to the low
        // bytes of any UCS-2 characters < 256), NS_ConvertASCIItoUTF16 seems
        // to DTRT.
        NS_ConvertASCIItoUTF16 unicodeString(chars, aLength);
        nsCAutoString utf8;
        PRInt32 headerLen = AppendDirectionalIndicatorUTF8(isRTL, utf8);
        AppendUTF16toUTF8(unicodeString, utf8);
        InitTextRun(textRun, (PRUint8 *)utf8.get(), utf8.Length(), headerLen);
    }

    textRun->FetchGlyphExtents(aParams->mContext);

    return textRun;
}

void gfxQtFontGroup::InitTextRun(gfxTextRun *aTextRun, const PRUint8 *aUTF8Text,
                                  PRUint32 aUTF8Length,
                                  PRUint32 aUTF8HeaderLength)
{
    CreateGlyphRunsFT(aTextRun, aUTF8Text + aUTF8HeaderLength,
                      aUTF8Length - aUTF8HeaderLength);
}

// Helper function to return the leading UTF-8 character in a char pointer
// as 32bit number. Also sets the length of the current character (i.e. the
// offset to the next one) in the second argument
PRUint32 getUTF8CharAndNext(const PRUint8 *aString, PRUint8 *aLength)
{
    *aLength = 1;
    if (aString[0] < 0x80) { // normal 7bit ASCII char
        return aString[0];
    }
    if ((aString[0] >> 5) == 6) { // two leading ones -> two bytes
        *aLength = 2;
        return ((aString[0] & 0x1F) << 6) + (aString[1] & 0x3F);
    }
    if ((aString[0] >> 4) == 14) { // three leading ones -> three bytes
        *aLength = 3;
        return ((aString[0] & 0x0F) << 12) + ((aString[1] & 0x3F) << 6) +
               (aString[2] & 0x3F);
    }
    if ((aString[0] >> 4) == 15) { // four leading ones -> four bytes
        *aLength = 4;
        return ((aString[0] & 0x07) << 18) + ((aString[1] & 0x3F) << 12) +
               ((aString[2] & 0x3F) <<  6) + (aString[3] & 0x3F);
    }
    return aString[0];
}

void gfxQtFontGroup::CreateGlyphRunsFT(gfxTextRun *aTextRun, const PRUint8 *aUTF8,
                                        PRUint32 aUTF8Length)
{

    PRUint32 fontlistLast = FontListLength()-1;
    gfxQtFont *font0 = GetFontAt(0);
    const PRUint8 *p = aUTF8;
    PRUint32 utf16Offset = 0;
    gfxTextRun::CompressedGlyph g;
    const PRUint32 appUnitsPerDevUnit = aTextRun->GetAppUnitsPerDevUnit();

    aTextRun->AddGlyphRun(font0, 0);
    // a textRun likely has the same font for most of the characters, so we can
    // lock it before the loop for efficiency
    FT_Face face0 =  font0->GetQFont().freetypeFace();
    while (p < aUTF8 + aUTF8Length) {
        PRBool glyphFound = PR_FALSE;
        // convert UTF-8 character and step to the next one in line
        PRUint8 chLen;
        PRUint32 ch = getUTF8CharAndNext(p, &chLen);
        p += chLen; // move to next char

        if (ch == 0) {
            // treat this null byte as a missing glyph, don't create a glyph for it
            aTextRun->SetMissingGlyph(utf16Offset, 0);
        } else {
            // Try to get a glyph from all fonts available to us.
            // Once we found it in one of the fonts we quit the loop early.
            // If we don't find the glyph, we set the missing glyph symbol after
            // trying the last font.
            for (PRUint32 i = 0; i <= fontlistLast; i++) {
                gfxQtFont *font = font0;
                FT_Face face = face0;
                if (i > 0) {
                    font = GetFontAt(i);
                    face = font->GetQFont().freetypeFace();
                }
                // select the current font into the text run
                aTextRun->AddGlyphRun(font, utf16Offset);

                NS_ASSERTION(!IsInvalidChar(ch), "Invalid char detected");
                FT_UInt gid = FT_Get_Char_Index(face, ch); // find the glyph id
                PRInt32 advance = 0;
                if (gid == font->GetSpaceGlyph()) {
                    advance = (int)(font->GetMetrics().spaceWidth * appUnitsPerDevUnit);
                } else if (gid == 0) {
                    advance = -1; // trigger the missing glyphs case below
                } else {
                    // find next character and its glyph -- in case they exist
                    // and exist in the current font face -- to compute kerning
                    PRUint32 chNext = 0;
                    FT_UInt gidNext = 0;
                    FT_Pos lsbDeltaNext = 0;
                    if (mEnableKerning && FT_HAS_KERNING(face) && p < aUTF8 + aUTF8Length) {
                        chNext = getUTF8CharAndNext(p, &chLen);
                        if (chNext) {
                            gidNext = FT_Get_Char_Index(face, chNext);
                            if (gidNext && gidNext != font->GetSpaceGlyph()) {
                                FT_Load_Glyph(face, gidNext, FT_LOAD_DEFAULT);
                                lsbDeltaNext = face->glyph->lsb_delta;
                            }
                        }
                    }

                    // now load the current glyph
                    FT_Load_Glyph(face, gid, FT_LOAD_DEFAULT); // load glyph into the slot
                    advance = face->glyph->advance.x;

                    // now add kerning to the current glyph's advance
                    if (chNext && gidNext) {
                        FT_Vector kerning;
                        FT_Get_Kerning(face, gid, gidNext, FT_KERNING_DEFAULT, &kerning);
                        advance += kerning.x;
                        if (face->glyph->rsb_delta - lsbDeltaNext >= 32) {
                            advance -= 64;
                        } else if (face->glyph->rsb_delta - lsbDeltaNext < -32) {
                            advance += 64;
                        }
                    }

                    // now apply unit conversion and scaling
                    advance = (advance >> 6) * appUnitsPerDevUnit;
                }

                if (advance >= 0 &&
                    gfxTextRun::CompressedGlyph::IsSimpleAdvance(advance) &&
                    gfxTextRun::CompressedGlyph::IsSimpleGlyphID(gid))
                {
                    aTextRun->SetSimpleGlyph(utf16Offset,
                                             g.SetSimpleGlyph(advance, gid));
                    glyphFound = PR_TRUE;
                } else if (gid == 0) {
                    // gid = 0 only happens when the glyph is missing from the font
                    if (i == fontlistLast) {
                        // set the missing glyph only when it's missing from the very
                        // last font
                        aTextRun->SetMissingGlyph(utf16Offset, ch);
                    }
                    glyphFound = PR_FALSE;
                } else {
                    gfxTextRun::DetailedGlyph details;
                    details.mGlyphID = gid;
                    NS_ASSERTION(details.mGlyphID == gid, "Seriously weird glyph ID detected!");
                    details.mAdvance = advance;
                    details.mXOffset = 0;
                    details.mYOffset = 0;
                    g.SetComplex(aTextRun->IsClusterStart(utf16Offset), PR_TRUE, 1);
                    aTextRun->SetGlyphs(utf16Offset, g, &details);
                    glyphFound = PR_TRUE;
                }

                if (glyphFound) {
                    break;
                }
            }
        } // for all fonts

        NS_ASSERTION(!IS_SURROGATE(ch), "Surrogates shouldn't appear in UTF8");
        if (ch >= 0x10000) {
            // This character is a surrogate pair in UTF16
            ++utf16Offset;
        }

        ++utf16Offset;
    }
}

/**
 * gfxQtFont
 */
gfxQtFont::gfxQtFont(const nsAString &aName,
                     const gfxFontStyle *aFontStyle)
    : gfxFont(aName, aFontStyle),
      mQFont(nsnull),
      mCairoFont(nsnull),
      mHasSpaceGlyph(PR_FALSE),
      mSpaceGlyph(0),
      mHasMetrics(PR_FALSE), 
      mAdjustedSize(0)
{
    mQFont = new QFont();
    mQFont->setFamily(QString( NS_ConvertUTF16toUTF8(mName).get() ) );
    mQFont->setPixelSize(GetStyle()->size);
    int weight = GetStyle()->weight/10;
    if( weight > 99 )
    {
        // Max Weight for QFont is 99
        weight = 99;
    }
    mQFont->setWeight(weight);
    mQFont->setItalic(bool( GetStyle()->style == FONT_STYLE_ITALIC ));
}

gfxQtFont::~gfxQtFont()
{
    delete mQFont;
    cairo_scaled_font_destroy(mCairoFont);
}

const gfxFont::Metrics&
gfxQtFont::GetMetrics()
{
     if (mHasMetrics)
        return mMetrics;

    QFontMetrics fontMetrics( *mQFont );

    mMetrics.maxAscent = fontMetrics.ascent();
    mMetrics.maxDescent = fontMetrics.descent();
    mMetrics.aveCharWidth = fontMetrics.averageCharWidth();
    mMetrics.underlineOffset = -fontMetrics.underlinePos();
    mMetrics.underlineSize = fontMetrics.lineWidth();
    mMetrics.strikeoutOffset = fontMetrics.strikeOutPos(); 
    mMetrics.strikeoutSize = fontMetrics.lineWidth();
    mMetrics.maxAdvance = fontMetrics.maxWidth();

    mMetrics.emHeight = mAdjustedSize ? mAdjustedSize : GetStyle()->size;

    gfxFloat lineHeight = mMetrics.maxAscent + mMetrics.maxDescent;
    if (lineHeight > mMetrics.emHeight)
    {
        mMetrics.externalLeading = lineHeight - mMetrics.emHeight;
    }
    else
    {
        mMetrics.externalLeading = 0;
    }
    mMetrics.internalLeading = 0;

    mMetrics.maxHeight = lineHeight;

    mMetrics.emAscent = lineHeight > 0.0 ?
        mMetrics.maxAscent * mMetrics.emHeight / lineHeight : 0.0;
    mMetrics.emDescent = mMetrics.emHeight - mMetrics.emAscent;

    mMetrics.spaceWidth = fontMetrics.width( QChar(' ') );
    mMetrics.xHeight = fontMetrics.xHeight();


    mMetrics.superscriptOffset = mMetrics.xHeight;
    mMetrics.subscriptOffset = mMetrics.xHeight;

#if 0

    fprintf (stderr, "Font: %s\n", NS_ConvertUTF16toUTF8(mName).get());
    fprintf (stderr, "    emHeight: %f emAscent: %f emDescent: %f\n", mMetrics.emHeight, mMetrics.emAscent, mMetrics.emDescent);
    fprintf (stderr, "    maxAscent: %f maxDescent: %f\n", mMetrics.maxAscent, mMetrics.maxDescent);
    fprintf (stderr, "    internalLeading: %f externalLeading: %f\n", mMetrics.externalLeading, mMetrics.internalLeading);
    fprintf (stderr, "    spaceWidth: %f aveCharWidth: %f xHeight: %f\n", mMetrics.spaceWidth, mMetrics.aveCharWidth, mMetrics.xHeight);
    fprintf (stderr, "    uOff: %f uSize: %f stOff: %f stSize: %f suOff: %f suSize: %f\n", mMetrics.underlineOffset, mMetrics.underlineSize, mMetrics.strikeoutOffset, mMetrics.strikeoutSize, mMetrics.superscriptOffset, mMetrics.subscriptOffset);
#endif

    mHasMetrics = PR_TRUE;
    return mMetrics;
}


nsString
gfxQtFont::GetUniqueName()
{
    return mName;
}

PRUint32 gfxQtFont::GetSpaceGlyph ()
{
    NS_ASSERTION (GetStyle ()->size != 0,
    "forgot to short-circuit a text run with zero-sized font?");

    if(!mHasSpaceGlyph)
    {
        FT_UInt gid = 0; // glyph ID
        FT_Face face = mQFont->freetypeFace();
        gid = FT_Get_Char_Index(face, ' ');
        mSpaceGlyph = gid;
        mHasSpaceGlyph = PR_TRUE;
    }
    return mSpaceGlyph;
}


cairo_scaled_font_t*
gfxQtFont::CreateScaledFont(cairo_t *aCR, cairo_matrix_t *aCTM, QFont &aQFont)
{
    FT_Face ftFace = aQFont.freetypeFace();

    double size = mAdjustedSize ? mAdjustedSize : GetStyle()->size;
    cairo_matrix_t fontMatrix;
    cairo_matrix_init_scale(&fontMatrix, size, size);
    cairo_font_options_t *fontOptions = cairo_font_options_create();

    cairo_font_face_t *cairoFontFace = 
                cairo_ft_font_face_create_for_ft_face( ftFace, 0 );

    cairo_scaled_font_t* scaledFont = 
                cairo_scaled_font_create( cairoFontFace, 
                                          &fontMatrix,
                                          aCTM,
                                          fontOptions);

    cairo_font_options_destroy(fontOptions);
    cairo_font_face_destroy(cairoFontFace);

    return scaledFont;
}

PRBool
gfxQtFont::SetupCairoFont(gfxContext *aContext)
{

    cairo_t *cr = aContext->GetCairo();
    cairo_matrix_t currentCTM;
    cairo_get_matrix(cr, &currentCTM);

    if (mCairoFont) {
        // Need to validate that its CTM is OK
        cairo_matrix_t fontCTM;
        cairo_scaled_font_get_ctm(mCairoFont, &fontCTM);
        if (fontCTM.xx != currentCTM.xx || fontCTM.yy != currentCTM.yy ||
            fontCTM.xy != currentCTM.xy || fontCTM.yx != currentCTM.yx) {
            // Just recreate it from scratch, simplest way
            cairo_scaled_font_destroy(mCairoFont);
            mCairoFont = nsnull;
        }
    }
    if (!mCairoFont) {
        mCairoFont = CreateScaledFont(cr, &currentCTM, *mQFont);
        return PR_FALSE;
    }
    if (cairo_scaled_font_status(mCairoFont) != CAIRO_STATUS_SUCCESS) {
        // Don't cairo_set_scaled_font as that would propagate the error to
        // the cairo_t, precluding any further drawing.
        return PR_FALSE;
    }
    cairo_set_scaled_font(cr, mCairoFont);
    return PR_TRUE;
}
