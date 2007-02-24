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
 *  John Fairhurst, <john_fairhurst@iname.com> } original developers of
 *  Henry Sobotka <sobotka@axess.com>          } code taken from 
 *  IBM Corp.                                  } nsDeviceContextOS2
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

#ifndef _NS_SYSTEMFONTSOS2_H_
#define _NS_SYSTEMFONTSOS2_H_

#include <gfxFont.h>
#define INCL_WINSHELLDATA
#define INCL_DOSNLS
#define INCL_DOSERRORS
#include <os2.h>

class nsSystemFontsOS2
{
public:
    nsSystemFontsOS2();
    nsresult GetSystemFont(nsSystemFontID aID, nsString* aFontName,
                           gfxFontStyle *aFontStyle) const;

private:
    nsresult GetSystemFontInfo(nsSystemFontID aID, nsString* aFontName,
                               gfxFontStyle *aFontStyle) const;
};

#endif /* _NS_SYSTEMFONTSOS2_H_ */
