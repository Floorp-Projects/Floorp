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
 * The Original Code is OS/2 system font code in Thebes.
 *
 * The Initial Developer of the Original Code is
 * Peter Weilbacher <mozilla@Weilbacher.org>.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Developers of code taken from nsDeviceContextOS2:
 *    John Fairhurst, <john_fairhurst@iname.com>
 *    Henry Sobotka <sobotka@axess.com>
 *    IBM Corp.
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

#include "nsIDeviceContext.h"
#include "nsUnitConversion.h"
#include "nsSystemFontsOS2.h"
#include <stdlib.h>

/************************
 *   Helper functions   *
 ************************/
static BOOL bIsDBCS;
static BOOL bIsDBCSSet = FALSE;

/* Helper function to determine if we are running on DBCS */
BOOL IsDBCS()
{
    if (!bIsDBCSSet) {
        // the following lines of code determine whether the system is a DBCS country
        APIRET rc;
        COUNTRYCODE ctrycodeInfo = {0};
        CHAR        achDBCSInfo[12] = {0};                  // DBCS environmental vector
        ctrycodeInfo.country  = 0;                          // current country
        ctrycodeInfo.codepage = 0;                          // current codepage

        rc = DosQueryDBCSEnv(sizeof(achDBCSInfo), &ctrycodeInfo, achDBCSInfo);
        if (rc == NO_ERROR) {
            // Non-DBCS countries will have four bytes in the first four bytes of the
            // DBCS environmental vector
            if (achDBCSInfo[0] != 0 || achDBCSInfo[1] != 0 ||
                achDBCSInfo[2] != 0 || achDBCSInfo[3] != 0)
            {
                bIsDBCS = TRUE;
            } else {
                bIsDBCS = FALSE;
            }
        } else {
            bIsDBCS = FALSE;
        } /* endif */
        bIsDBCSSet = TRUE;
    } /* endif */
    return bIsDBCS;
}

/* Helper function to query font from INI file */
void QueryFontFromINI(char* fontType, char* fontName, ULONG ulLength)
{
    ULONG ulMaxNameL = ulLength;

    /* We had to switch to using PrfQueryProfileData because */
    /* some users have binary font data in their INI files */
    BOOL rc = PrfQueryProfileData(HINI_USER, "PM_SystemFonts", fontType,
                                  fontName, &ulMaxNameL);
    /* If there was no entry in the INI, default to something */
    if (rc == FALSE) {
        /* Different values for DBCS vs. SBCS */
        /* WarpSans is only available on Warp 4, we exclude Warp 3 now */
        if (!IsDBCS()) {
            strcpy(fontName, "9.WarpSans");
        } else {
            strcpy(fontName, "9.WarpSans Combined");
        }
    } else {
        /* null terminate fontname */
        fontName[ulMaxNameL] = '\0';
    }
}

/************************/

nsSystemFontsOS2::nsSystemFontsOS2()
{
#ifdef DEBUG_thebes
    printf("nsSystemFontsOS2::nsSystemFontsOS2()\n");
#endif
}

/*
 * Query the font used for various CSS properties (aID) from the system.
 * For OS/2, only very few fonts are defined in the system, so most of the IDs
 * resolve to the same system font.
 * The font queried will give back a string like
 *    9.WarpSans Bold
 *    12.Times New Roman Bold Italic
 *    10.Times New Roman.Strikeout.Underline
 *    20.Bitstream Vera Sans Mono Obli
 * (always restricted to 32 chars, at least before the second dot)
 * We use the value before the dot as the font size (in pt, and convert it to
 * px using the screen resolution) and then try to use the rest of the string
 * to determine the font style from it.
 */
nsresult nsSystemFontsOS2::GetSystemFont(nsSystemFontID aID, nsString* aFontName,
                                         gfxFontStyle *aFontStyle) const
{
#ifdef DEBUG_thebes
    printf("nsSystemFontsOS2::GetSystemFont: ");
#endif
    char szFontNameSize[MAXNAMEL];

    switch (aID)
    {
    case eSystemFont_Icon:
        QueryFontFromINI("IconText", szFontNameSize, MAXNAMEL);
#ifdef DEBUG_thebes
        printf("IconText ");
#endif
        break;

    case eSystemFont_Menu:
        QueryFontFromINI("Menus", szFontNameSize, MAXNAMEL);
#ifdef DEBUG_thebes
        printf("Menus ");
#endif
        break;

    case eSystemFont_Caption:
    case eSystemFont_MessageBox:
    case eSystemFont_SmallCaption:
    case eSystemFont_StatusBar:
    case eSystemFont_Tooltips:
    case eSystemFont_Widget:

    case eSystemFont_Window:      // css3
    case eSystemFont_Document:
    case eSystemFont_Workspace:
    case eSystemFont_Desktop:
    case eSystemFont_Info:
    case eSystemFont_Dialog:
    case eSystemFont_Button:
    case eSystemFont_PullDownMenu:
    case eSystemFont_List:
    case eSystemFont_Field:
        QueryFontFromINI("WindowText", szFontNameSize, MAXNAMEL);
#ifdef DEBUG_thebes
        printf("WindowText ");
#endif
        break;

    default:
        NS_WARNING("None of the listed font types, using WarpSans");
        if (!IsDBCS()) {
            strcpy(szFontNameSize, "9.WarpSans");
        } else {
            strcpy(szFontNameSize, "9.WarpSans Combined");
        }
    } // switch
#ifdef DEBUG_thebes
    printf(" (%s)\n", szFontNameSize);
#endif

    char *szFacename = strchr(szFontNameSize, '.');
    if (!szFacename || (*(szFacename++) == '\0'))
        return NS_ERROR_FAILURE;

    // local DPI for size will be taken into account below
    aFontStyle->size = atof(szFontNameSize);

    // determine DPI resolution of screen device to compare compute
    // font size in pixels
    HPS ps = WinGetScreenPS(HWND_DESKTOP);
    HDC dc = GpiQueryDevice(ps);
    // effective vertical resolution in DPI
    LONG vertScreenRes = 120; // assume 120 dpi as default
    DevQueryCaps(dc, CAPS_VERTICAL_FONT_RES, 1, &vertScreenRes);
    WinReleasePS(ps);

    // now scale to make pixels from points (1 pt = 1/72in)
    aFontStyle->size *= vertScreenRes / 72.0;

    NS_ConvertUTF8toUTF16 fontFace(szFacename);
    int pos = 0;

    // this is a system font in any case
    aFontStyle->systemFont = PR_TRUE;

    // bold fonts should have " Bold" in their names, at least we hope that they
    // do, otherwise it's bad luck
    NS_NAMED_LITERAL_CSTRING(spcBold, " Bold");
    if ((pos = fontFace.Find(spcBold.get(), PR_FALSE, 0, -1)) > -1) {
        aFontStyle->weight = FONT_WEIGHT_BOLD;
        // strip the attribute, now that we have set it in the gfxFontStyle
        fontFace.Cut(pos, spcBold.Length());
    } else {
        aFontStyle->weight = FONT_WEIGHT_NORMAL;
    }

    // similar hopes for italic and oblique fonts...
    NS_NAMED_LITERAL_CSTRING(spcItalic, " Italic");
    NS_NAMED_LITERAL_CSTRING(spcOblique, " Oblique");
    NS_NAMED_LITERAL_CSTRING(spcObli, " Obli");
    if ((pos = fontFace.Find(spcItalic.get(), PR_FALSE, 0, -1)) > -1) {
        aFontStyle->style = FONT_STYLE_ITALIC;
        fontFace.Cut(pos, spcItalic.Length());
    } else if ((pos = fontFace.Find(spcOblique.get(), PR_FALSE, 0, -1)) > -1) {
        // oblique fonts are rare on OS/2 and not specially supported by
        // the GPI system, but at least we are trying...
        aFontStyle->style = FONT_STYLE_OBLIQUE;
        fontFace.Cut(pos, spcOblique.Length());
    } else if ((pos = fontFace.Find(spcObli.get(), PR_FALSE, 0, -1)) > -1) {
        // especially oblique often gets cut by the 32 char limit to "Obli",
        // so search for that, too (anything shorter would be ambiguous)
        aFontStyle->style = FONT_STYLE_OBLIQUE;
        // In this case, assume that this is the last property in the line
        // and cut off everything else, too
        // This is needed in case it was really Obliq or Obliqu...
        fontFace.Cut(pos, fontFace.Length());
    } else {
        aFontStyle->style = FONT_STYLE_NORMAL;
    }

    // just throw away any modifiers that are separated by dots (which are either
    // .Strikeout, .Underline, or .Outline, none of which have a corresponding
    // gfxFont property)
    if ((pos = fontFace.Find(".", PR_FALSE, 0, -1)) > -1) {
        fontFace.Cut(pos, fontFace.Length());
    }

#ifdef DEBUG_thebes
    printf("  after=%s\n", NS_LossyConvertUTF16toASCII(fontFace).get());
    printf("  style: %s %s %s\n",
           (aFontStyle->weight == FONT_WEIGHT_BOLD) ? "BOLD" : "",
           (aFontStyle->style == FONT_STYLE_ITALIC) ? "ITALIC" : "",
           (aFontStyle->style == FONT_STYLE_OBLIQUE) ? "OBLIQUE" : "");
#endif
    NS_NAMED_LITERAL_STRING(quote, "\""); // seems like we need quotes around the font name
    *aFontName = quote + fontFace + quote;

    return NS_OK;
}
