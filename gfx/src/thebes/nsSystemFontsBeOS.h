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

#ifndef _NS_SYSTEMFONTSBEOS_H_
#define _NS_SYSTEMFONTSBEOS_H_

#include <gfxFont.h>

class BFont;

class nsSystemFontsBeOS
{
public:
    nsSystemFontsBeOS();

    nsresult GetSystemFont(nsSystemFontID anID, nsString *aFontName,
                           gfxFontStyle *aFontStyle) const;

private:
    nsresult GetSystemFontInfo(const BFont *theFont, nsString *aFontName,
                               gfxFontStyle *aFontStyle) const;

    /*
     * The following system font constants exist:
     *
     * css2: http://www.w3.org/TR/REC-CSS2/fonts.html#x27
     * eSystemFont_Caption, eSystemFont_Icon, eSystemFont_Menu,
     * eSystemFont_MessageBox, eSystemFont_SmallCaption,
     * eSystemFont_StatusBar,
     * // css3
     * eSystemFont_Window, eSystemFont_Document,
     * eSystemFont_Workspace, eSystemFont_Desktop,
     * eSystemFont_Info, eSystemFont_Dialog,
     * eSystemFont_Button, eSystemFont_PullDownMenu,
     * eSystemFont_List, eSystemFont_Field,
     * // moz
     * eSystemFont_Tooltips, eSystemFont_Widget
     */
    nsString mDefaultFontName, mMenuFontName, mCaptionFontName;
    gfxFontStyle mDefaultFontStyle, mMenuFontStyle, mCaptionFontStyle;
};

#endif /* _NS_SYSTEMFONTSBEOS_H_ */
