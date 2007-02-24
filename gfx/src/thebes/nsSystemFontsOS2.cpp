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

nsresult nsSystemFontsOS2::GetSystemFont(nsSystemFontID aID, nsString* aFontName,
                                         gfxFontStyle *aFontStyle) const
{
    return GetSystemFontInfo(aID, aFontName, aFontStyle);
}

nsresult nsSystemFontsOS2::GetSystemFontInfo(nsSystemFontID aID, nsString* aFontName,
                                             gfxFontStyle *aFontStyle) const
{
#ifdef DEBUG_thebes
    printf("nsSystemFontsOS2::GetSystemFontInfo: ");
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
        NS_WARNING("None of the listed font types, using 9.WarpSans");
#ifdef DEBUG_thebes
        printf("using 9.WarpSans... ");
#endif
        strcpy(szFontNameSize, "9.WarpSans");
    } // switch
#ifdef DEBUG_thebes
    printf(" (%s)\n", szFontNameSize);
#endif

    int pointSize = atoi(szFontNameSize);
    char *szFacename = strchr(szFontNameSize, '.');

    if ((pointSize == 0) || (!szFacename) || (*(szFacename++) == '\0'))
        return NS_ERROR_FAILURE;

    NS_NAMED_LITERAL_STRING(quote, "\""); // seems like we need quotes around the font name
    NS_ConvertUTF8toUTF16 fontFace(szFacename);
    *aFontName = quote + fontFace + quote;

    // As in old gfx/src/os2 set the styles to the defaults
    aFontStyle->style = FONT_STYLE_NORMAL;
    aFontStyle->variant = FONT_VARIANT_NORMAL;
    aFontStyle->weight = FONT_WEIGHT_NORMAL;
    aFontStyle->decorations = FONT_DECORATION_NONE;
    aFontStyle->size = gfxFloat(pointSize);

    aFontStyle->systemFont = PR_TRUE;

    return NS_OK;
}
