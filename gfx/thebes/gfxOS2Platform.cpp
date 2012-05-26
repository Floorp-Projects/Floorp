/* vim: set sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxOS2Platform.h"
#include "gfxOS2Surface.h"
#include "gfxImageSurface.h"
#include "gfxOS2Fonts.h"
#include "nsTArray.h"

#include "gfxFontconfigUtils.h"
//#include <fontconfig/fontconfig.h>

/**********************************************************************
 * class gfxOS2Platform
 **********************************************************************/
gfxFontconfigUtils *gfxOS2Platform::sFontconfigUtils = nsnull;

gfxOS2Platform::gfxOS2Platform()
{
#ifdef DEBUG_thebes
    printf("gfxOS2Platform::gfxOS2Platform()\n");
#endif
    // this seems to be reasonably early in the process and only once,
    // so it's a good place to initialize OS/2 cairo stuff
    cairo_os2_init();
#ifdef DEBUG_thebes
    printf("  cairo_os2_init() was called\n");
#endif
    if (!sFontconfigUtils) {
        sFontconfigUtils = gfxFontconfigUtils::GetFontconfigUtils();
    }
}

gfxOS2Platform::~gfxOS2Platform()
{
#ifdef DEBUG_thebes
    printf("gfxOS2Platform::~gfxOS2Platform()\n");
#endif
    gfxFontconfigUtils::Shutdown();
    sFontconfigUtils = nsnull;

    // clean up OS/2 cairo stuff
    cairo_os2_fini();
#ifdef DEBUG_thebes
    printf("  cairo_os2_fini() was called\n");
#endif
}

already_AddRefed<gfxASurface>
gfxOS2Platform::CreateOffscreenSurface(const gfxIntSize& aSize,
                                       gfxASurface::gfxContentType contentType)
{
#ifdef DEBUG_thebes_2
    printf("gfxOS2Platform::CreateOffscreenSurface(%d/%d, %d)\n",
           aSize.width, aSize.height, aImageFormat);
#endif
    gfxASurface *newSurface = nsnull;

    // we only ever seem to get aImageFormat=0 or ImageFormatARGB32 but
    // I don't really know if we need to differ between ARGB32 and RGB24 here
    if (contentType == gfxASurface::CONTENT_COLOR_ALPHA ||
        contentType == gfxASurface::CONTENT_COLOR)
    {
        newSurface = new gfxOS2Surface(aSize, OptimalFormatForContent(contentType));
    } else if (contentType == gfxASurface::CONTENT_ALPHA) {
        newSurface = new gfxImageSurface(aSize, OptimalFormatForContent(contentType));
    } else {
        return nsnull;
    }

    NS_IF_ADDREF(newSurface);
    return newSurface;
}

nsresult
gfxOS2Platform::GetFontList(nsIAtom *aLangGroup,
                            const nsACString& aGenericFamily,
                            nsTArray<nsString>& aListOfFonts)
{
#ifdef DEBUG_thebes
    const char *langgroup = "(null)";
    if (aLangGroup) {
        aLangGroup->GetUTF8String(&langgroup);
    }
    char *family = ToNewCString(aGenericFamily);
    printf("gfxOS2Platform::GetFontList(%s, %s, ..)\n",
           langgroup, family);
    free(family);
#endif
    return sFontconfigUtils->GetFontList(aLangGroup, aGenericFamily,
                                         aListOfFonts);
}

nsresult gfxOS2Platform::UpdateFontList()
{
#ifdef DEBUG_thebes
    printf("gfxOS2Platform::UpdateFontList()\n");
#endif
    mCodepointsWithNoFonts.reset();

    nsresult rv = sFontconfigUtils->UpdateFontList();

    // initialize ranges of characters for which system-wide font search should be skipped
    mCodepointsWithNoFonts.SetRange(0,0x1f);     // C0 controls
    mCodepointsWithNoFonts.SetRange(0x7f,0x9f);  // C1 controls
    return rv;
}

nsresult
gfxOS2Platform::ResolveFontName(const nsAString& aFontName,
                                FontResolverCallback aCallback,
                                void *aClosure, bool& aAborted)
{
#ifdef DEBUG_thebes
    char *fontname = ToNewCString(aFontName);
    printf("gfxOS2Platform::ResolveFontName(%s, ...)\n", fontname);
    free(fontname);
#endif
    return sFontconfigUtils->ResolveFontName(aFontName, aCallback, aClosure,
                                             aAborted);
}

nsresult
gfxOS2Platform::GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName)
{
    return sFontconfigUtils->GetStandardFamilyName(aFontName, aFamilyName);
}

gfxFontGroup *
gfxOS2Platform::CreateFontGroup(const nsAString &aFamilies,
                const gfxFontStyle *aStyle,
                gfxUserFontSet *aUserFontSet)
{
    return new gfxOS2FontGroup(aFamilies, aStyle, aUserFontSet);
}

already_AddRefed<gfxOS2Font>
gfxOS2Platform::FindFontForChar(PRUint32 aCh, gfxOS2Font *aFont)
{
#ifdef DEBUG_thebes
    printf("gfxOS2Platform::FindFontForChar(%d, ...)\n", aCh);
#endif

    // is codepoint with no matching font? return null immediately
    if (mCodepointsWithNoFonts.test(aCh)) {
        return nsnull;
    }

    // the following is not very clever but it's a quick fix to search all fonts
    // (one should instead cache the charmaps as done on Mac and Win)

    // just continue to append all fonts known to the system
    nsTArray<nsString> fontList;
    nsCAutoString generic;
    nsresult rv = GetFontList(aFont->GetStyle()->language, generic, fontList);
    if (NS_SUCCEEDED(rv)) {
        // start at 3 to skip over the generic entries
        for (PRUint32 i = 3; i < fontList.Length(); i++) {
#ifdef DEBUG_thebes
            printf("searching in entry i=%d (%s)\n",
                   i, NS_LossyConvertUTF16toASCII(fontList[i]).get());
#endif
            nsRefPtr<gfxOS2Font> font =
                gfxOS2Font::GetOrMakeFont(fontList[i], aFont->GetStyle());
            if (!font)
                continue;
            FT_Face face = cairo_ft_scaled_font_lock_face(font->CairoScaledFont());
            if (!face || !face->charmap) {
                if (face)
                    cairo_ft_scaled_font_unlock_face(font->CairoScaledFont());
                continue;
            }

            FT_UInt gid = FT_Get_Char_Index(face, aCh); // find the glyph id
            if (gid != 0) {
                // this is the font
                cairo_ft_scaled_font_unlock_face(font->CairoScaledFont());
                return font.forget();
            }
        }
    }

    // no match found, so add to the set of non-matching codepoints
    mCodepointsWithNoFonts.set(aCh);
    return nsnull;
}
