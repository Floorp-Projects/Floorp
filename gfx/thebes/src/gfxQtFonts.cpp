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
#include <QFontMetricsF>
#include "cairo-ft.h"
#include <freetype/tttables.h>
#include "nsStyleConsts.h"

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
    // check for WarpSans and as we cannot display that (yet), replace
    // it with Workplace Sans
    int pos = 0;
    if ((pos = mFamilies.Find("WarpSans", PR_FALSE, 0, -1)) > -1) {
        mFamilies.Replace(pos, 8, NS_LITERAL_STRING("Workplace Sans"));
    }

    nsStringArray familyArray;
    ForEachFont(FontCallback, &familyArray);

    // To be able to easily search for glyphs in other fonts, append a few good
    // replacement candidates to the list. The best ones are the Unicode fonts that
    // are set up, and if the user was so clever to set up the User Defined fonts,
    // then these are probable candidates, too.
    nsString fontString;
    gfxPlatform::GetPlatform()->GetPrefFonts("x-unicode", fontString, PR_FALSE);
    ForEachFont(fontString, NS_LITERAL_CSTRING("x-unicode"), FontCallback, &familyArray);
    gfxPlatform::GetPlatform()->GetPrefFonts("x-user-def", fontString, PR_FALSE);
    ForEachFont(fontString, NS_LITERAL_CSTRING("x-user-def"), FontCallback, &familyArray);

    // Should append some default font if there are no available fonts.
    // Let's use Helv which should be available on any OS/2 system; if
    // it's not there, Fontconfig replaces it with something else...
    if (familyArray.Count() == 0) {
        familyArray.AppendString(NS_LITERAL_STRING("Helv"));
    }

    for (int i = 0; i < familyArray.Count(); i++) {
        nsRefPtr<gfxQtFont> font = GetOrMakeFont(*familyArray[i], &mStyle);
        if (font) {
            mFonts.AppendElement(font);
        }
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
        mFontFace(nsnull),
        mHasSpaceGlyph(PR_FALSE),
        mSpaceGlyph(0),
        mHasMetrics(PR_FALSE),
        mAdjustedSize(0)
{
    QFont::Style style = QFont::StyleNormal;
    // add style to pattern
    switch (GetStyle()->style) {
    case FONT_STYLE_ITALIC:
        style = QFont::StyleItalic;
        break;
    case FONT_STYLE_OBLIQUE:
        style = QFont::StyleOblique;
        break;
    case FONT_STYLE_NORMAL:
    default:
        style = QFont::StyleNormal;
    }

    PRInt16 weight = QFont::Normal;
    switch (GetStyle()->weight) {
    case NS_STYLE_FONT_WEIGHT_NORMAL:
        weight = QFont::Normal;
    case NS_STYLE_FONT_WEIGHT_BOLD:
        weight = QFont::Bold;
    case NS_STYLE_FONT_WEIGHT_BOLDER:
        weight = QFont::Black;
    case NS_STYLE_FONT_WEIGHT_LIGHTER:
        weight = QFont::Light;
    default:
        weight = QFont::Normal;
    }
    mQFont = new QFont();
    mQFont->setFamily(QString(NS_ConvertUTF16toUTF8(mName).get()));
    mQFont->setWeight(weight);
    mQFont->setStyle(style);
    mQFont->setPixelSize (mAdjustedSize ? mAdjustedSize : GetStyle()->size);
}

gfxQtFont::~gfxQtFont()
{
    delete mQFont;
    cairo_scaled_font_destroy(mCairoFont);
}

// rounding and truncation functions for a Freetype floating point number 
// (FT26Dot6) stored in a 32bit integer with high 26 bits for the integer
// part and low 6 bits for the fractional part. 
#define MOZ_FT_ROUND(x) (((x) + 32) & ~63) // 63 = 2^6 - 1
#define MOZ_FT_TRUNC(x) ((x) >> 6)
#define CONVERT_DESIGN_UNITS_TO_PIXELS(v, s) \
        MOZ_FT_TRUNC(MOZ_FT_ROUND(FT_MulFix((v) , (s))))

const gfxFont::Metrics&
gfxQtFont::GetMetrics()
{
    if (mHasMetrics)
        return mMetrics;


    mMetrics.emHeight = GetStyle()->size;

    FT_UInt gid; // glyph ID
    QFontMetrics fontMetrics( *mQFont );
    FT_Face face = mQFont->freetypeFace();

    if (!face) {
        // Abort here already, otherwise we crash in the following
        // this can happen if the font-size requested is zero.
        // The metrics will be incomplete, but then we don't care.
        return mMetrics;
    }

    double emUnit = 1.0 * face->units_per_EM;
    double yScale = face->size->metrics.y_ppem / emUnit;

    // properties of space
    gid = FT_Get_Char_Index(face, ' ');
    // Load glyph into glyph slot. Use load_default here to get results in
    // 26.6 fractional pixel format which is what is used for all other
    // characters in gfxOS2FontGroup::CreateGlyphRunsFT.
    FT_Load_Glyph(face, gid, FT_LOAD_DEFAULT);
    // face->glyph->metrics.width doesn't work for spaces, use advance.x instead
    mMetrics.spaceWidth = fontMetrics.width( QChar(' ') );
    // save the space glyph
    mSpaceGlyph = gid;

    mMetrics.xHeight = fontMetrics.xHeight();
    mMetrics.aveCharWidth = fontMetrics.averageCharWidth();

    // compute an adjusted size if we need to
    if (mAdjustedSize == 0 && GetStyle()->sizeAdjust != 0) {
        gfxFloat aspect = mMetrics.xHeight / GetStyle()->size;
        mAdjustedSize = GetStyle()->GetAdjustedSize(aspect);
        mMetrics.emHeight = mAdjustedSize;
    }

    if (face) {
        mMetrics.maxAdvance = face->size->metrics.max_advance / 64.0; // 26.6
        float val;
        TT_OS2 *os2 = (TT_OS2 *) FT_Get_Sfnt_Table(face, ft_sfnt_os2);
        if (os2 && os2->ySuperscriptYOffset) {
            val = CONVERT_DESIGN_UNITS_TO_PIXELS(os2->ySuperscriptYOffset,
                                                 face->size->metrics.y_scale);
            mMetrics.superscriptOffset = PR_MAX(1, val);
        } else {
            mMetrics.superscriptOffset = mMetrics.xHeight;
        }

        if (os2 && os2->ySubscriptYOffset) {
            val = CONVERT_DESIGN_UNITS_TO_PIXELS(os2->ySubscriptYOffset,
                                                 face->size->metrics.y_scale);
            // some fonts have the incorrect sign..
            val = (val < 0) ? -val : val;
            mMetrics.subscriptOffset = PR_MAX(1, val);
        } else {
            mMetrics.subscriptOffset = mMetrics.xHeight;
        }
    } else
    {
        mMetrics.superscriptOffset = mMetrics.xHeight;
        mMetrics.subscriptOffset = mMetrics.xHeight;
    }

    mMetrics.strikeoutOffset = fontMetrics.strikeOutPos();
    mMetrics.strikeoutSize = fontMetrics.lineWidth();
    mMetrics.aveCharWidth = fontMetrics.averageCharWidth();

    // seems that underlineOffset really has to be negative
    mMetrics.underlineOffset = -fontMetrics.underlinePos();
    mMetrics.underlineSize = fontMetrics.lineWidth();

    // descents are negative in FT but Thebes wants them positive
    mMetrics.emAscent        = face->ascender * yScale;
    mMetrics.emDescent       = -face->descender * yScale;
    mMetrics.maxHeight       = face->height * yScale;
    mMetrics.maxAscent       = fontMetrics.ascent();
    mMetrics.maxDescent = fontMetrics.descent();
    mMetrics.maxAdvance = fontMetrics.maxWidth();
    // leading are not available directly (only for WinFNTs)
    double lineHeight = mMetrics.maxAscent + mMetrics.maxDescent;
    if (lineHeight > mMetrics.emHeight) {
        mMetrics.internalLeading = lineHeight - mMetrics.emHeight;
    } else {
        mMetrics.internalLeading = 0;
    }
    mMetrics.externalLeading = 0; // normal value for OS/2 fonts, too

    SanitizeMetrics(&mMetrics, PR_FALSE);

#ifdef DEBUG_thebes_1
    printf("gfxOS2Font[%#x]::GetMetrics():\n"
           "  %s (%s)\n"
           "  emHeight=%f == %f=gfxFont::style.size == %f=adjSz\n"
           "  maxHeight=%f  xHeight=%f\n"
           "  aveCharWidth=%f==xWidth  spaceWidth=%f\n"
           "  supOff=%f SubOff=%f   strOff=%f strSz=%f\n"
           "  undOff=%f undSz=%f    intLead=%f extLead=%f\n"
           "  emAsc=%f emDesc=%f maxH=%f\n"
           "  maxAsc=%f maxDes=%f maxAdv=%f\n",
           (unsigned)this,
           NS_LossyConvertUTF16toASCII(mName).get(),
           os2 && os2->version != 0xFFFF ? "has OS/2 table" : "no OS/2 table!",
           mMetrics.emHeight, GetStyle()->size, mAdjustedSize,
           mMetrics.maxHeight, mMetrics.xHeight,
           mMetrics.aveCharWidth, mMetrics.spaceWidth,
           mMetrics.superscriptOffset, mMetrics.subscriptOffset,
           mMetrics.strikeoutOffset, mMetrics.strikeoutSize,
           mMetrics.underlineOffset, mMetrics.underlineSize,
           mMetrics.internalLeading, mMetrics.externalLeading,
           mMetrics.emAscent, mMetrics.emDescent, mMetrics.maxHeight,
           mMetrics.maxAscent, mMetrics.maxDescent, mMetrics.maxAdvance
          );
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

static const PRInt8 nFcWeight = 2; // 10; // length of weight list
static const int fcWeight[] = {
    //FC_WEIGHT_THIN,
    //FC_WEIGHT_EXTRALIGHT, // == FC_WEIGHT_ULTRALIGHT
    //FC_WEIGHT_LIGHT,
    //FC_WEIGHT_BOOK,
    FC_WEIGHT_REGULAR, // == FC_WEIGHT_NORMAL
    //FC_WEIGHT_MEDIUM,
    //FC_WEIGHT_DEMIBOLD, // == FC_WEIGHT_SEMIBOLD
    FC_WEIGHT_BOLD,
    //FC_WEIGHT_EXTRABOLD, // == FC_WEIGHT_ULTRABOLD
    //FC_WEIGHT_BLACK // == FC_WEIGHT_HEAVY
};


cairo_font_face_t *gfxQtFont::CairoFontFace(QFont *aFont)
{
#ifdef DEBUG_thebes_2
    printf("gfxOS2Font[%#x]::CairoFontFace()\n", (unsigned)this);
#endif
    if (!mFontFace) {
#ifdef DEBUG_thebes
        printf("gfxOS2Font[%#x]::CairoFontFace(): create it for %s, %f\n",
               (unsigned)this, NS_LossyConvertUTF16toASCII(mName).get(), GetStyle()->size);
#endif
        if (aFont) {
            FT_Face ftFace = aFont->freetypeFace();
            mFontFace = cairo_ft_font_face_create_for_ft_face( ftFace, 0 );
            if (mFontFace)
                return mFontFace;
        }

        FcPattern *fcPattern = FcPatternCreate();

        // add (family) name to pattern
        // (the conversion should work, font names don't contain high bit chars)
        FcPatternAddString(fcPattern, FC_FAMILY,
                           (FcChar8 *)NS_LossyConvertUTF16toASCII(mName).get());


        // adjust font weight using the offset
        // The requirements outlined in gfxFont.h are difficult to meet without
        // having a table of available font weights, so we map the gfxFont
        // weight to possible FontConfig weights.
        PRInt8 weight, offset;
        GetStyle()->ComputeWeightAndOffset(&weight, &offset);
        // gfxFont weight   FC weight
        //    400              80
        //    700             200
        PRInt16 fcW = 40 * weight - 80; // match gfxFont weight to base FC weight
        // find the correct weight in the list
        PRInt8 i = 0;
        while (i < nFcWeight && fcWeight[i] < fcW) {
            i++;
        }
        // add the offset, but observe the available number of weights
        i += offset;
        if (i < 0) {
            i = 0;
        } else if (i >= nFcWeight) {
            i = nFcWeight - 1;
        }
        fcW = fcWeight[i];

        // add weight to pattern
        FcPatternAddInteger(fcPattern, FC_WEIGHT, fcW);

        PRUint8 fcProperty;
        // add style to pattern
        switch (GetStyle()->style) {
        case FONT_STYLE_ITALIC:
            fcProperty = FC_SLANT_ITALIC;
            break;
        case FONT_STYLE_OBLIQUE:
            fcProperty = FC_SLANT_OBLIQUE;
            break;
        case FONT_STYLE_NORMAL:
        default:
            fcProperty = FC_SLANT_ROMAN;
        }
        FcPatternAddInteger(fcPattern, FC_SLANT, fcProperty);

        // add the size we want
        FcPatternAddDouble(fcPattern, FC_PIXEL_SIZE,
                           mAdjustedSize ? mAdjustedSize : GetStyle()->size);

        // finally find a matching font
        FcResult fcRes;
        FcPattern *fcMatch = FcFontMatch(NULL, fcPattern, &fcRes);
        FcPatternDestroy(fcPattern);
        if (mName == NS_LITERAL_STRING("Workplace Sans") && fcW >= FC_WEIGHT_DEMIBOLD) {
            // if we are dealing with Workplace Sans and want a bold font, we
            // need to artificially embolden it (no bold counterpart yet)
            FcPatternAddBool(fcMatch, FC_EMBOLDEN, FcTrue);
        }
        // and ask cairo to return a font face for this
        mFontFace = cairo_ft_font_face_create_for_pattern(fcMatch);
        FcPatternDestroy(fcMatch);
    }

    NS_ASSERTION(mFontFace, "Failed to make font face");
    return mFontFace;
}

cairo_scaled_font_t*
gfxQtFont::CreateScaledFont(cairo_t *aCR, cairo_matrix_t *aCTM, QFont &aQFont)
{
    double size = mAdjustedSize ? mAdjustedSize : GetStyle()->size;
    cairo_matrix_t fontMatrix;
    cairo_matrix_init_scale(&fontMatrix, size, size);
    cairo_font_options_t *fontOptions = cairo_font_options_create();

    cairo_scaled_font_t* scaledFont = 
                cairo_scaled_font_create( CairoFontFace(&aQFont),
//                cairo_scaled_font_create( CairoFontFace(), // This makes fonts working almost perfect
                                          &fontMatrix,
                                          aCTM,
                                          fontOptions);

    cairo_font_options_destroy(fontOptions);

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
