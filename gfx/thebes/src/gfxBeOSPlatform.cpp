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
 * The Original Code is BeOS code in Thebes.
 *
 * The Initial Developer of the Original Code is
 * Christian Biesinger <cbiesinger@web.de>.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Masayuki Nakano <masayuki@d-toybox.com>
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

#include "gfxBeOSPlatform.h"
#include "gfxFontconfigUtils.h"

#include "gfxImageSurface.h"
#include "gfxBeOSSurface.h"

gfxFontconfigUtils *gfxPlatformGtk::sFontconfigUtils = nsnull;

gfxBeOSPlatform::gfxBeOSPlatform()
{
    if (!sFontconfigUtils)
        sFontconfigUtils = gfxFontconfigUtils::GetFontconfigUtils();
}

already_AddRefed<gfxASurface>
gfxBeOSPlatform::CreateOffscreenSurface (PRUint32 width,
                                         PRUint32 height,
                                         gfxASurface::gfxImageFormat imageFormat)
{
    gfxASurface *newSurface = nsnull;

    if (imageFormat == gfxASurface::ImageFormatA1 ||
        imageFormat == gfxASurface::ImageFormatA8) {
        newSurface = new gfxImageSurface(imageFormat, width, height);
    } else {
        newSurface = new gfxBeOSSurface(width, height,
                                        imageFormat == gfxASurface::ImageFormatARGB32 ? B_RGBA32 : B_RGB32);
    }

    NS_ADDREF(newSurface);
    return newSurface;
}

nsresult
gfxBeOSPlatform::GetFontList(const nsACString& aLangGroup,
                             const nsACString& aGenericFamily,
                             nsStringArray& aListOfFonts)
{
    return sFontconfigUtils->GetFontList(aLangGroup, aGenericFamily,
                                         aListOfFonts);
}

nsresult
gfxBeOSPlatform::UpdateFontList()
{
    return sFontconfigUtils->UpdateFontList();
}

nsresult
gfxBeOSPlatform::ResolveFontName(const nsAString& aFontName,
                                FontResolverCallback aCallback,
                                void *aClosure,
                                PRBool& aAborted)
{
    return sFontconfigUtils->ResolveFontName(aFontName, aCallback,
                                             aClosure, aAborted);
}
