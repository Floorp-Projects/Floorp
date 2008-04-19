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
 *   Vladimir Vukicevic <vladimir@mozilla.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   Behdad Esfahbod <behdad@gnome.org>
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *   Karl Tomlinson <karlt+@karlt.net>, Mozilla Corporation
 *
 * based on nsFontMetricsPango.cpp by
 *   Christopher Blizzard <blizzard@mozilla.org>
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
        // XXX If there are no fonts, we should use dummy family.
        // Pango will resolve from this.
        // behdad: yep, looks good.
        // printf("%s(%s)\n", NS_ConvertUTF16toUTF8(families).get(),
        //                    aStyle->langGroup.get());
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

#if defined(ENABLE_FAST_PATH_8BIT)
PRBool
gfxQtFontGroup::CanTakeFastPath(PRUint32 aFlags)
{
    // Can take fast path only if OPTIMIZE_SPEED is set and IS_RTL isn't.
    // We need to always use Pango for RTL text, in case glyph mirroring is
    // required.
    PRBool speed = aFlags & gfxTextRunFactory::TEXT_OPTIMIZE_SPEED;
    PRBool isRTL = aFlags & gfxTextRunFactory::TEXT_IS_RTL;
    return speed && !isRTL
           //&& PANGO_IS_FC_FONT(GetFontAt(0)->GetPangoFont())
           ;
}
#endif

#if defined(ENABLE_FAST_PATH_8BIT) || defined(ENABLE_FAST_PATH_ALWAYS)
#define UTF8_COMPUTE(Char, Mask, Len)					      \
  if (Char < 128)							      \
    {									      \
      Len = 1;								      \
      Mask = 0x7f;							      \
    }									      \
  else if ((Char & 0xe0) == 0xc0)					      \
    {									      \
      Len = 2;								      \
      Mask = 0x1f;							      \
    }									      \
  else if ((Char & 0xf0) == 0xe0)					      \
    {									      \
      Len = 3;								      \
      Mask = 0x0f;							      \
    }									      \
  else if ((Char & 0xf8) == 0xf0)					      \
    {									      \
      Len = 4;								      \
      Mask = 0x07;							      \
    }									      \
  else if ((Char & 0xfc) == 0xf8)					      \
    {									      \
      Len = 5;								      \
      Mask = 0x03;							      \
    }									      \
  else if ((Char & 0xfe) == 0xfc)					      \
    {									      \
      Len = 6;								      \
      Mask = 0x01;							      \
    }									      \
  else									      \
    Len = -1;

#define UTF8_GET(Result, Chars, Count, Mask, Len)			      \
  (Result) = (Chars)[0] & (Mask);					      \
  for ((Count) = 1; (Count) < (Len); ++(Count))				      \
    {									      \
      if (((Chars)[(Count)] & 0xc0) != 0x80)				      \
	{								      \
	  (Result) = -1;						      \
	  break;							      \
	}								      \
      (Result) <<= 6;							      \
      (Result) |= ((Chars)[(Count)] & 0x3f);				      \
    }

static const char utf8_skip_data[256] = {
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
  3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,6,6,1,1
};

const char * const g_utf8_skip = utf8_skip_data;
#define g_utf8_next_char(p) (char *)((p) + g_utf8_skip[*(const unsigned char *)(p)])

int
g_utf8_get_char (const char *p)
{
  int i, mask = 0, len;
  int result;
  unsigned char c = (unsigned char) *p;

  UTF8_COMPUTE (c, mask, len);
  if (len == -1)
    return (int)-1;
  UTF8_GET (result, p, i, mask, len);
  return result;
}

nsresult
gfxQtFontGroup::CreateGlyphRunsFast(gfxTextRun *aTextRun,
                                    const char *aUTF8, PRUint32 aUTF8Length)
{
    const char *p = aUTF8;
    gfxQtFont *font = GetFontAt(0);
    const QFont& qFont = font->GetQFont();
    FT_Face face = qFont.freetypeFace();
    PRUint32 utf16Offset = 0;
    gfxTextRun::CompressedGlyph g;
    const PRUint32 appUnitsPerDevUnit = aTextRun->GetAppUnitsPerDevUnit();

    aTextRun->AddGlyphRun(font, 0);

    while (p < aUTF8 + aUTF8Length) {
        // glib-2.12.9: "If p does not point to a valid UTF-8 encoded
        // character, results are undefined." so it is not easy to assert that
        // aUTF8 in fact points to UTF8 data but asserting
        // g_unichar_validate(ch) may be mildly useful.
        int ch = g_utf8_get_char(p);
        p = g_utf8_next_char(p);

        if (ch == 0) {
            // treat this null byte as a missing glyph. Pango
            // doesn't create glyphs for these, not even missing-glyphs.
            aTextRun->SetMissingGlyph(utf16Offset, 0);
        } else {
            NS_ASSERTION(!IsInvalidChar(ch), "Invalid char detected");
            int glyph = 124;
            glyph = FT_Get_Char_Index (face, ch);

            if (!glyph)                  // character not in font,
                return NS_ERROR_FAILURE; // fallback to CreateGlyphRunsItemizing

            FT_Load_Glyph(face, glyph, FT_LOAD_DEFAULT);
            PRInt32 advance = face->glyph->advance.x;
            advance = (advance >> 6) * appUnitsPerDevUnit;

            if (advance >= 0 &&
                gfxTextRun::CompressedGlyph::IsSimpleAdvance(advance) &&
                gfxTextRun::CompressedGlyph::IsSimpleGlyphID(glyph)) {
                aTextRun->SetSimpleGlyph(utf16Offset,
                                         g.SetSimpleGlyph(advance, glyph));
            } else {
                gfxTextRun::DetailedGlyph details;
                details.mGlyphID = glyph;
                NS_ASSERTION(details.mGlyphID == glyph,
                             "Seriously weird glyph ID detected!");
                details.mAdvance = advance;
                details.mXOffset = 0;
                details.mYOffset = 0;
                g.SetComplex(aTextRun->IsClusterStart(utf16Offset), PR_TRUE, 1);
                aTextRun->SetGlyphs(utf16Offset, g, &details);
            }

            NS_ASSERTION(!IS_SURROGATE(ch), "Surrogates shouldn't appear in UTF8");
            if (ch >= 0x10000) {
                // This character is a surrogate pair in UTF16
                ++utf16Offset;
            }
        }

        ++utf16Offset;
    }
    return NS_OK;
}
#endif


void
gfxQtFontGroup::InitTextRun(gfxTextRun *aTextRun, const char *aUTF8Text,
                             PRUint32 aUTF8Length, PRUint32 aUTF8HeaderLength,
                             PRBool aTake8BitPath)
{
#if defined(ENABLE_FAST_PATH_ALWAYS)
    CreateGlyphRunsFast(aTextRun, aUTF8Text + aUTF8HeaderLength, aUTF8Length - aUTF8HeaderLength);
#else
#if defined(ENABLE_FAST_PATH_8BIT)
    if (aTake8BitPath && CanTakeFastPath(aTextRun->GetFlags())) {
        qDebug("QTFONT NOT_IMPLEMENTED!!!! Func:%s::%d, Text:'%s'\n", __PRETTY_FUNCTION__, __LINE__, aUTF8Text);
//        nsresult rv = CreateGlyphRunsFast(aTextRun, aUTF8Text + aUTF8HeaderLength, aUTF8Length - aUTF8HeaderLength);
        if (NS_SUCCEEDED(rv))
            return;
    }
#endif
      qDebug("QTFONT NOT_IMPLEMENTED!!!! Func:%s::%d, Text:'%s'\n", __PRETTY_FUNCTION__, __LINE__, aUTF8Text);
//    CreateGlyphRunsItemizing(aTextRun, aUTF8Text, aUTF8Length, aUTF8HeaderLength);
#endif
}

/**
 * We use this to append an LTR or RTL Override character to the start of the
 * string. This forces Pango to honour our direction even if there are neutral characters
 * in the string.
 */
static PRInt32 AppendDirectionalIndicatorUTF8(PRBool aIsRTL, nsACString& aString)
{
    static const PRUnichar overrides[2][2] =
      { { 0x202d, 0 }, { 0x202e, 0 }}; // LRO, RLO
    AppendUTF16toUTF8(overrides[aIsRTL], aString);
    return 3; // both overrides map to 3 bytes in UTF8
}

gfxTextRun *
gfxQtFontGroup::MakeTextRun(const PRUint8 *aString, PRUint32 aLength,
                               const Parameters *aParams, PRUint32 aFlags)
{
    NS_ASSERTION(aFlags & TEXT_IS_8BIT, "8bit should have been set");
    gfxTextRun *run = gfxTextRun::Create(aParams, aString, aLength, this, aFlags);
    if (!run)
        return nsnull;

    PRBool isRTL = run->IsRightToLeft();
    if ((aFlags & TEXT_IS_ASCII) && !isRTL) {
        // We don't need to send an override character here, the characters must be all LTR
        const char *utf8Chars = reinterpret_cast<const char*>(aString);
        InitTextRun(run, utf8Chars, aLength, 0, PR_TRUE);
    } else {
        // this is really gross...
        const char *chars = reinterpret_cast<const char*>(aString);
        NS_ConvertASCIItoUTF16 unicodeString(chars, aLength);
        nsCAutoString utf8;
        PRInt32 headerLen = AppendDirectionalIndicatorUTF8(isRTL, utf8);
        AppendUTF16toUTF8(unicodeString, utf8);
        InitTextRun(run, utf8.get(), utf8.Length(), headerLen, PR_TRUE);
    }
    run->FetchGlyphExtents(aParams->mContext);
    return run;
}

gfxTextRun *
gfxQtFontGroup::MakeTextRun(const PRUnichar *aString, PRUint32 aLength,
                               const Parameters *aParams, PRUint32 aFlags)
{
    gfxTextRun *run = gfxTextRun::Create(aParams, aString, aLength, this, aFlags);
    if (!run)
        return nsnull;

    run->RecordSurrogates(aString);

    nsCAutoString utf8;
    PRInt32 headerLen = AppendDirectionalIndicatorUTF8(run->IsRightToLeft(), utf8);
    AppendUTF16toUTF8(Substring(aString, aString + aLength), utf8);
    PRBool is8Bit = PR_FALSE;

#if defined(ENABLE_FAST_PATH_8BIT)
    if (CanTakeFastPath(aFlags)) {
        PRUint32 allBits = 0;
        PRUint32 i;
        for (i = 0; i < aLength; ++i) {
            allBits |= aString[i];
        }
        is8Bit = (allBits & 0xFF00) == 0;
    }
#endif
    InitTextRun(run, utf8.get(), utf8.Length(), headerLen, is8Bit);
    run->FetchGlyphExtents(aParams->mContext);
    return run;
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
    mQFont->setPixelSize((int) GetStyle()->size);
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
    mMetrics.underlineOffset = fontMetrics.underlinePos();
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
