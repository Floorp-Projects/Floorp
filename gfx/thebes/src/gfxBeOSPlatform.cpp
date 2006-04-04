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

#include "nsIAtom.h"

#include "gfxImageSurface.h"
#include "gfxBeOSSurface.h"

#include <fontconfig/fontconfig.h>

gfxBeOSPlatform::gfxBeOSPlatform()
{
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

// this is in nsFontConfigUtils.h
extern void NS_AddLangGroup (FcPattern *aPattern, nsIAtom *aLangGroup);

nsresult
gfxBeOSPlatform::GetFontList(const nsACString& aLangGroup,
                            const nsACString& aGenericFamily,
                            nsStringArray& aListOfFonts)
{
    FcPattern *pat = NULL;
    FcObjectSet *os = NULL;
    FcFontSet *fs = NULL;
    nsresult rv = NS_ERROR_FAILURE;

    aListOfFonts.Clear();

    PRInt32 serif = 0, sansSerif = 0, monospace = 0, nGenerics;

    pat = FcPatternCreate();
    if (!pat)
        goto end;

    os = FcObjectSetBuild(FC_FAMILY, FC_FOUNDRY, 0);
    if (!os)
        goto end;

    // take the pattern and add the lang group to it
    if (!aLangGroup.IsEmpty()) {
        nsCOMPtr<nsIAtom> langAtom = do_GetAtom(aLangGroup);
        //XXX fix me //NS_AddLangGroup(pat, langAtom);
    }

    fs = FcFontList(0, pat, os);
    if (!fs)
        goto end;

    if (fs->nfont == 0) {
        rv = NS_OK;
        goto end;
    }

    // Fontconfig supports 3 generic fonts, "serif", "sans-serif", and
    // "monospace", slightly different from CSS's 5.
    if (aGenericFamily.IsEmpty())
        serif = sansSerif = monospace = 1;
    else if (aGenericFamily.EqualsLiteral("serif"))
        serif = 1;
    else if (aGenericFamily.EqualsLiteral("sans-serif"))
        sansSerif = 1;
    else if (aGenericFamily.EqualsLiteral("monospace"))
        monospace = 1;
    else if (aGenericFamily.EqualsLiteral("cursive") || aGenericFamily.EqualsLiteral("fantasy"))
        serif = sansSerif = 1;
    else
        NS_NOTREACHED("unexpected CSS generic font family");
    nGenerics = serif + sansSerif + monospace;

    if (serif)
        aListOfFonts.AppendString(NS_LITERAL_STRING("serif"));
    if (sansSerif)
        aListOfFonts.AppendString(NS_LITERAL_STRING("sans-serif"));
    if (monospace)
        aListOfFonts.AppendString(NS_LITERAL_STRING("monospace"));

    for (int i = 0; i < fs->nfont; i++) {
        char *family;

        // if there's no family name, skip this match
        if (FcPatternGetString (fs->fonts[i], FC_FAMILY, 0,
                                (FcChar8 **) &family) != FcResultMatch)
        {
            continue;
        }

        aListOfFonts.AppendString(NS_ConvertASCIItoUTF16(nsDependentCString(family)));
    }

    aListOfFonts.Sort();
    rv = NS_OK;

  end:
    if (NS_FAILED(rv))
        aListOfFonts.Clear();

    if (pat)
        FcPatternDestroy(pat);
    if (os)
        FcObjectSetDestroy(os);
    if (fs)
        FcFontSetDestroy(fs);

    return rv;
}
