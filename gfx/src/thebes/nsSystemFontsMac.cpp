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

#include <Carbon/Carbon.h>

#include "nsSystemFontsMac.h"

nsSystemFontsMac::nsSystemFontsMac()
{
}

// helper function to get the system font for a specific script
#define FONTNAME_MAX_UNICHRS sizeof(fontName255) * 2
nsresult 
GetSystemFontForScript(ThemeFontID aFontID, ScriptCode aScriptCode,
                       nsAFlatString& aFontName, SInt16& aFontSize,
                       Style& aFontStyle)
{
    Str255 fontName255;
    ::GetThemeFont(aFontID, aScriptCode, fontName255, &aFontSize, &aFontStyle);
    if (fontName255[0] == 255) {
        NS_WARNING("Too long fong name (> 254 chrs)");
        return NS_ERROR_FAILURE;
    }
    fontName255[fontName255[0]+1] = 0;
        
    OSStatus err;

    // the theme font could contains font name in different encoding. 
    // we need to covert them to unicode according to the font's text encoding.
    TECObjectRef converter = 0;
    TextEncoding unicodeEncoding = 
        ::CreateTextEncoding(kTextEncodingUnicodeDefault, 
                             kTextEncodingDefaultVariant,
                             kTextEncodingDefaultFormat);
                                                              
    FMFontFamily fontFamily;
    TextEncoding fontEncoding = 0;
    fontFamily = ::FMGetFontFamilyFromName(fontName255);

    err = ::FMGetFontFamilyTextEncoding(fontFamily, &fontEncoding);
    if (err != noErr) {
        NS_WARNING("Could not get the encoding for the font.");
        return NS_ERROR_FAILURE;
    }

    err = ::TECCreateConverter(&converter, fontEncoding, unicodeEncoding);
    if (err != noErr) {
        NS_WARNING("Could not create the converter.");
        return NS_ERROR_FAILURE;
    }

    PRUnichar unicodeFontName[FONTNAME_MAX_UNICHRS + 1];
    ByteCount actualInputLength, actualOutputLength;
    err = ::TECConvertText(converter, &fontName255[1], fontName255[0], 
                           &actualInputLength, 
                           (TextPtr)unicodeFontName,
                           FONTNAME_MAX_UNICHRS * sizeof(PRUnichar),
                           &actualOutputLength);

    if (err != noErr) {
        NS_WARNING("Could not convert the font name.");
        return NS_ERROR_FAILURE;
    }

    ::TECDisposeConverter(converter);

    unicodeFontName[actualOutputLength / sizeof(PRUnichar)] = PRUnichar('\0');
    aFontName = nsDependentString(unicodeFontName);
    return NS_OK;
}

nsresult
nsSystemFontsMac::GetSystemFont(nsSystemFontID aID, nsString *aFontName,
                                gfxFontStyle *aFontStyle) const
{
    nsresult rv;

    // hack for now
    if (aID == eSystemFont_Window ||
        aID == eSystemFont_Document)
    {
        aFontStyle->style       = FONT_STYLE_NORMAL;
        aFontStyle->weight      = FONT_WEIGHT_NORMAL;
        aFontStyle->stretch     = NS_FONT_STRETCH_NORMAL;

        aFontName->AssignLiteral("sans-serif");
        aFontStyle->size = 14;
        aFontStyle->systemFont = PR_TRUE;

        return NS_OK;
    }

    ThemeFontID fontID = kThemeViewsFont;
    switch (aID) {
        // css2
        case eSystemFont_Caption:       fontID = kThemeSystemFont;         break;
        case eSystemFont_Icon:          fontID = kThemeViewsFont;          break;
        case eSystemFont_Menu:          fontID = kThemeSystemFont;         break;
        case eSystemFont_MessageBox:    fontID = kThemeSmallSystemFont;    break;
        case eSystemFont_SmallCaption:  fontID = kThemeSmallEmphasizedSystemFont;  break;
        case eSystemFont_StatusBar:     fontID = kThemeSmallSystemFont;    break;
        // css3
        //case eSystemFont_Window:      = 'sans-serif'
        //case eSystemFont_Document:    = 'sans-serif'
        case eSystemFont_Workspace:     fontID = kThemeViewsFont;          break;
        case eSystemFont_Desktop:       fontID = kThemeViewsFont;          break;
        case eSystemFont_Info:          fontID = kThemeViewsFont;          break;
        case eSystemFont_Dialog:        fontID = kThemeSystemFont;         break;
        case eSystemFont_Button:        fontID = kThemeSmallSystemFont;    break;
        case eSystemFont_PullDownMenu:  fontID = kThemeMenuItemFont;       break;
        case eSystemFont_List:          fontID = kThemeSmallSystemFont;    break;
        case eSystemFont_Field:         fontID = kThemeSmallSystemFont;    break;
        // moz
        case eSystemFont_Tooltips:      fontID = kThemeSmallSystemFont;    break;
        case eSystemFont_Widget:        fontID = kThemeSmallSystemFont;    break;
        default:
            // should never hit this
            return NS_ERROR_FAILURE;
    }

    nsAutoString fontName;
    SInt16 fontSize;
    Style fontStyle;

    ScriptCode sysScript = GetScriptManagerVariable(smSysScript);
    rv = GetSystemFontForScript(fontID, smRoman, fontName, fontSize, fontStyle);
    if (NS_FAILED(rv))
        fontName = NS_LITERAL_STRING("Lucida Grande");

    if (sysScript != smRoman) {
        SInt16 localFontSize;
        Style localFontStyle;
        nsAutoString localSysFontName;
        rv = GetSystemFontForScript(fontID, sysScript,
                                    localSysFontName,
                                    localFontSize, localFontStyle);
        if (NS_SUCCEEDED(rv) && !fontName.Equals(localSysFontName)) {
            fontName += NS_LITERAL_STRING(",") + localSysFontName;
            fontSize = localFontSize;
            fontStyle = localFontStyle;
        }
    }

    *aFontName = fontName; 
    aFontStyle->size = gfxFloat(fontSize);

    if (fontStyle & bold)
        aFontStyle->weight = FONT_WEIGHT_BOLD;
    if (fontStyle & italic)
        aFontStyle->style = FONT_STYLE_ITALIC;
    // FIXME: Set aFontStyle->stretch correctly!

    aFontStyle->systemFont = PR_TRUE;

    return NS_OK;
}
