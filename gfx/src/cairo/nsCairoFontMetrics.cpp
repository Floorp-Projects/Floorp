/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Initial Developer of the Original Code is mozilla.org.
 * Portions created by the Initial Developer are
 * Copyright (C) 2004 the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Stuart Parmenter <pavlov@netscape.com>
 *    Joe Hewitt <hewitt@netscape.com>
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
 */

#include "nsCairoFontMetrics.h"    
#include "nsFont.h"

#include "nsDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsString.h"

#define FONT_FILE "Vera.ttf"
#define FONT_SIZE 10
/*
#define FONT_FILE "verdana.ttf"
#define FONT_SIZE 12
*/

#define FT_FLOOR(X)	((X & -64) >> 6)
#define FT_CEIL(X)	(((X + 63) & -64) >> 6)

static FT_Library ftlib = nsnull;

NS_IMPL_ISUPPORTS1(nsCairoFontMetrics, nsIFontMetrics)

nsCairoFontMetrics::nsCairoFontMetrics() :
    mFont(nsnull),
    mMaxAscent(0),
    mMaxDescent(0),
    mMaxAdvance(0),
    mUnderlineOffset(0),
    mUnderlineHeight(0)
{
    NS_INIT_ISUPPORTS();
    if (!ftlib) {
        FT_Init_FreeType(&ftlib);
    }
}

nsCairoFontMetrics::~nsCairoFontMetrics()
{
    FT_Done_Face(mFace);
    delete mFont;
}

static char *
GetFontPath()
{
    return strdup("/tmp/fonts/" FONT_FILE);

    nsCOMPtr<nsIFile> aFile;
    NS_GetSpecialDirectory(NS_XPCOM_CURRENT_PROCESS_DIR, getter_AddRefs(aFile));
    aFile->Append(NS_LITERAL_STRING(FONT_FILE));
    nsAutoString pathBuf;
    aFile->GetPath(pathBuf);
    NS_LossyConvertUCS2toASCII pathCBuf(pathBuf);

    return strdup(pathCBuf.get());
}

NS_IMETHODIMP
nsCairoFontMetrics::Init(const nsFont& aFont, nsIAtom* aLangGroup,
                         nsIDeviceContext *aContext)
{
    mFont = new nsFont(aFont);
    mLangGroup = aLangGroup;

    mDeviceContext = aContext;
    mDev2App = 1.0;

    char *fontPath = GetFontPath();
    FT_Error ferr = FT_New_Face(ftlib, fontPath, 0, &mFace);
    if (ferr != 0) {
        fprintf (stderr, "FT Error: %d\n", ferr);
        return NS_ERROR_FAILURE;
    }
    free(fontPath);

    FT_Set_Char_Size(mFace, 0, FONT_SIZE << 6, 72, 72);

    mMaxAscent  = mFace->bbox.yMax;
    mMaxDescent = mFace->bbox.yMin;
    mMaxAdvance = mFace->max_advance_width;

    mUnderlineOffset = mFace->underline_position;
    mUnderlineHeight = mFace->underline_thickness;

    return NS_OK;
}

NS_IMETHODIMP
nsCairoFontMetrics::Destroy()
{
    delete mFont;
    mFont = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsCairoFontMetrics::GetXHeight(nscoord& aResult)
{
    aResult = NSToCoordRound(14 * mDev2App);
    return NS_OK;
}

NS_IMETHODIMP
nsCairoFontMetrics::GetSuperscriptOffset(nscoord& aResult)
{
    aResult = 0;
    return NS_OK;
}

NS_IMETHODIMP
nsCairoFontMetrics::GetSubscriptOffset(nscoord& aResult)
{
    aResult = 0;
    return NS_OK;
}

NS_IMETHODIMP
nsCairoFontMetrics::GetStrikeout(nscoord& aOffset, nscoord& aSize)
{
    aOffset = 0;
    aSize = NSToCoordRound(1 * mDev2App);
    return NS_OK;
}

NS_IMETHODIMP
nsCairoFontMetrics::GetUnderline(nscoord& aOffset, nscoord& aSize)
{
    const FT_Fixed scale = mFace->size->metrics.y_scale;

    aOffset = FT_FLOOR(FT_MulFix(mUnderlineOffset, scale));
    aOffset = NSToCoordRound(aOffset * mDev2App);

    aSize = FT_CEIL(FT_MulFix(mUnderlineHeight, scale));
    if (aSize > 1)
        aSize = 1;
    aSize = NSToCoordRound(aSize * mDev2App);

    return NS_OK;
}

NS_IMETHODIMP
nsCairoFontMetrics::GetHeight(nscoord &aHeight)
{
    aHeight = NSToCoordRound((mFace->size->metrics.height >> 6) * mDev2App);
    return NS_OK;
}

NS_IMETHODIMP
nsCairoFontMetrics::GetInternalLeading(nscoord &aLeading)
{
    // aLeading = 0 * mDev2App;
    aLeading = 0;
    return NS_OK;
}

NS_IMETHODIMP
nsCairoFontMetrics::GetExternalLeading(nscoord &aLeading)
{
    // aLeading = 0 * mDev2App;
    aLeading = 0;
    return NS_OK;
}

NS_IMETHODIMP
nsCairoFontMetrics::GetEmHeight(nscoord &aHeight)
{
    /* ascent + descent */
#if FONT_SIZE == 10
    const nscoord emHeight = 10;
#else
    /* XXX this is for 12px verdana ... */
    const nscoord emHeight = 14;
#endif

    aHeight = NSToCoordRound(emHeight * mDev2App);
    return NS_OK;
}

NS_IMETHODIMP
nsCairoFontMetrics::GetEmAscent(nscoord &aAscent)
{
    /* units above the base line */
#if FONT_SIZE == 10
    const nscoord emAscent = 10;
#else
    /* XXX this is for 12px verdana ... */
    const nscoord emAscent = 12;
#endif
    aAscent = NSToCoordRound(emAscent * mDev2App);
    return NS_OK;
}

NS_IMETHODIMP
nsCairoFontMetrics::GetEmDescent(nscoord &aDescent)
{
    /* units below the base line */
#if FONT_SIZE == 10
    const nscoord emDescent = 0;
#else
    /* XXX this is for 12px verdana ... */
    const nscoord emDescent = 2;
#endif
    aDescent = NSToCoordRound(emDescent * mDev2App);
    return NS_OK;
}

NS_IMETHODIMP
nsCairoFontMetrics::GetMaxHeight(nscoord &aHeight)
{
    /* ascent + descent */
    aHeight = FT_CEIL(FT_MulFix(mMaxAscent, mFace->size->metrics.y_scale))
              - FT_CEIL(FT_MulFix(mMaxDescent, mFace->size->metrics.y_scale))
              + 1;

    aHeight = NSToCoordRound(aHeight * mDev2App);
    return NS_OK;
}

NS_IMETHODIMP
nsCairoFontMetrics::GetMaxAscent(nscoord &aAscent)
{
    /* units above the base line */
    aAscent = FT_CEIL(FT_MulFix(mMaxAscent, mFace->size->metrics.y_scale));
    aAscent = NSToCoordRound(aAscent * mDev2App);
    return NS_OK;
}

NS_IMETHODIMP
nsCairoFontMetrics::GetMaxDescent(nscoord &aDescent)
{
    /* units below the base line */
    aDescent = -FT_CEIL(FT_MulFix(mMaxDescent, mFace->size->metrics.y_scale));
    aDescent = NSToCoordRound(aDescent * mDev2App);
    return NS_OK;
}

NS_IMETHODIMP
nsCairoFontMetrics::GetMaxAdvance(nscoord &aAdvance)
{
    aAdvance = FT_CEIL(FT_MulFix(mMaxAdvance, mFace->size->metrics.x_scale));
    aAdvance = NSToCoordRound(aAdvance * mDev2App);
    return NS_OK;
}

NS_IMETHODIMP
nsCairoFontMetrics::GetFont(const nsFont *&aFont)
{
    aFont = mFont;
    return NS_OK;
}

NS_IMETHODIMP
nsCairoFontMetrics::GetLangGroup(nsIAtom** aLangGroup)
{
    *aLangGroup = mLangGroup;
    NS_IF_ADDREF(*aLangGroup);
    return NS_OK;
}

NS_IMETHODIMP
nsCairoFontMetrics::GetFontHandle(nsFontHandle &aHandle)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCairoFontMetrics::GetAveCharWidth(nscoord& aAveCharWidth)
{
    aAveCharWidth = NSToCoordRound(7 * mDev2App);
    return NS_OK;
}

NS_IMETHODIMP
nsCairoFontMetrics::GetSpaceWidth(nscoord& aSpaceCharWidth)
{
    FT_Load_Char(mFace, ' ', FT_LOAD_DEFAULT | FT_LOAD_NO_AUTOHINT);

    aSpaceCharWidth = NSToCoordRound((mFace->glyph->advance.x >> 6) * mDev2App);

    return NS_OK;
}

nscoord
nsCairoFontMetrics::MeasureString(const char *aString, PRUint32 aLength)
{
    nscoord width = 0;
    PRUint32 i;
    int error;

    FT_UInt glyph_index;
    FT_UInt previous = 0;

    for (i=0; i < aLength; ++i) {
        glyph_index = FT_Get_Char_Index(mFace, aString[i]);
/*
        if (previous && glyph_index) {
            FT_Vector delta;
            FT_Get_Kerning(mFace, previous, glyph_index,
                           FT_KERNING_DEFAULT, &delta);
            width += delta.x >> 6;
        }
*/
        error = FT_Load_Glyph(mFace, glyph_index, FT_LOAD_DEFAULT | FT_LOAD_NO_AUTOHINT);
        if (error)
            continue;

        width += mFace->glyph->advance.x;
        previous = glyph_index;
    }

    return NSToCoordRound((width >> 6) * mDev2App);
}

nscoord
nsCairoFontMetrics::MeasureString(const PRUnichar *aString, PRUint32 aLength)
{
    nscoord width = 0;
    PRUint32 i;
    int error;

    FT_UInt glyph_index;
    FT_UInt previous = 0;

    for (i=0; i < aLength; ++i) {
        glyph_index = FT_Get_Char_Index(mFace, aString[i]);
/*
        if (previous && glyph_index) {
            FT_Vector delta;
            FT_Get_Kerning(mFace, previous, glyph_index,
                           FT_KERNING_DEFAULT, &delta);
            width += delta.x >> 6;
        }
*/
        error = FT_Load_Glyph(mFace, glyph_index, FT_LOAD_DEFAULT | FT_LOAD_NO_AUTOHINT);
        if (error)
            continue;

        width += mFace->glyph->advance.x;
        previous = glyph_index;
    }

    return NSToCoordRound((width >> 6) * mDev2App);
}

NS_IMETHODIMP
nsCairoFontMetrics::GetLeading(nscoord& aLeading)
{
    aLeading = 0;
    return NS_OK;
}

NS_IMETHODIMP
nsCairoFontMetrics::GetNormalLineHeight(nscoord& aLineHeight)
{
    aLineHeight = 10;
    return NS_OK;
}
