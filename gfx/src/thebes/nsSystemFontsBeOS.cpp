/* vim: set sw=2 sts=2 et cin: */
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
 * The Original Code is BeOS system font code in Thebes.
 *
 * The Initial Developer of the Original Code is
 * Christian Biesinger <cbiesinger@web.de>.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Parts of this file used to live in nsDeviceContextBeos.cpp. The Original
 *   Author of that code is Netscape Communications Corporation.
 *
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include <Font.h>
#include <Menu.h>

#include "nsIDeviceContext.h"
#include "nsSystemFontsBeOS.h"

#define DEFAULT_PIXEL_FONT_SIZE 16.0f

nsSystemFontsBeOS::nsSystemFontsBeOS()
  : mDefaultFontName(NS_LITERAL_STRING("sans-serif"))
  , mMenuFontName(NS_LITERAL_STRING("sans-serif"))
  , mCaptionFontName(NS_LITERAL_STRING("sans-serif"))
  , mDefaultFontStyle(FONT_STYLE_NORMAL, FONT_WEIGHT_NORMAL,
                 DEFAULT_PIXEL_FONT_SIZE, NS_LITERAL_CSTRING(""),
                 0.0f, PR_TRUE, PR_FALSE)
  , mMenuFontStyle(FONT_STYLE_NORMAL, FONT_WEIGHT_NORMAL,
               DEFAULT_PIXEL_FONT_SIZE, NS_LITERAL_CSTRING(""),
               0.0f, PR_TRUE, PR_FALSE)
  , mCaptionFontStyle(FONT_STYLE_NORMAL, FONT_WEIGHT_NORMAL,
               DEFAULT_PIXEL_FONT_SIZE, NS_LITERAL_CSTRING(""),
               0.0f, PR_TRUE, PR_FALSE)
{
  menu_info info;
  get_menu_info(&info);
  BFont menuFont;
  menuFont.SetFamilyAndStyle(info.f_family, info.f_style);
  menuFont.SetSize(info.font_size);

  GetSystemFontInfo(be_plain_font, &mDefaultFontName, &mDefaultFontStyle);
  GetSystemFontInfo(be_bold_font, &mCaptionFontName, &mCaptionFontStyle);
  GetSystemFontInfo(&menuFont, &mMenuFontName, &mMenuFontStyle);
}

nsresult nsSystemFontsBeOS::GetSystemFont(nsSystemFontID aID,
                                          nsString *aFontName,
                                          gfxFontStyle *aFontStyle) const
{
  switch (aID) {
    case eSystemFont_PullDownMenu: 
    case eSystemFont_Menu:
      *aFontName = mMenuFontName;
      *aFontStyle = mMenuFontStyle;
      return NS_OK;
    case eSystemFont_Caption:             // css2 bold  
      *aFontName = mCaptionFontName;
      *aFontStyle = mCaptionFontStyle;
      return NS_OK;
    case eSystemFont_List:   
    case eSystemFont_Field:
    case eSystemFont_Icon : 
    case eSystemFont_MessageBox : 
    case eSystemFont_SmallCaption : 
    case eSystemFont_StatusBar : 
    case eSystemFont_Window:              // css3 
    case eSystemFont_Document: 
    case eSystemFont_Workspace: 
    case eSystemFont_Desktop: 
    case eSystemFont_Info: 
    case eSystemFont_Dialog: 
    case eSystemFont_Button: 
    case eSystemFont_Tooltips:            // moz 
    case eSystemFont_Widget: 
    default:
      *aFontName = mDefaultFontName;
      *aFontStyle = mDefaultFontStyle;
      return NS_OK;
  }
  NS_NOTREACHED("huh?");
}

nsresult 
nsSystemFontsBeOS::GetSystemFontInfo(const BFont *theFont, nsString *aFontName,
                                     gfxFontStyle *aFontStyle) const 
{ 
 
  aFontStyle->style       = FONT_STYLE_NORMAL; 
  aFontStyle->weight      = FONT_WEIGHT_NORMAL; 
  aFontStyle->decorations = FONT_DECORATION_NONE; 
  
  font_family family; 
  theFont->GetFamilyAndStyle(&family, NULL);

  uint16 face = theFont->Face();
  CopyUTF8toUTF16(family, *aFontName);
  aFontStyle->size = theFont->Size();

  if (face & B_ITALIC_FACE)
    aFontStyle->style = FONT_STYLE_ITALIC;

  if (face & B_BOLD_FACE)
    aFontStyle->weight = FONT_WEIGHT_BOLD;

  if (face & B_UNDERSCORE_FACE)
    aFontStyle->decorations |= FONT_DECORATION_UNDERLINE;

  if (face & B_STRIKEOUT_FACE)
    aFontStyle->decorations |= FONT_DECORATION_STRIKEOUT;

  aFontStyle->systemFont = PR_TRUE;

  return NS_OK;
}

