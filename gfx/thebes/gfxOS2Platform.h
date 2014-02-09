/* vim: set sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_OS2_PLATFORM_H
#define GFX_OS2_PLATFORM_H

#define INCL_GPIBITMAPS
#include <os2.h>

#include "gfxPlatform.h"
#include "gfxOS2Fonts.h"
#include "gfxFontUtils.h"
#include "nsTArray.h"

class gfxFontconfigUtils;

class gfxOS2Platform : public gfxPlatform {

public:
    gfxOS2Platform();
    virtual ~gfxOS2Platform();

    static gfxOS2Platform *GetPlatform() {
        return (gfxOS2Platform*) gfxPlatform::GetPlatform();
    }

    virtual already_AddRefed<gfxASurface>
      CreateOffscreenSurface(const IntSize& size,
                             gfxContentType contentType) MOZ_OVERRIDE;

    nsresult GetFontList(nsIAtom *aLangGroup,
                         const nsACString& aGenericFamily,
                         nsTArray<nsString>& aListOfFonts);
    nsresult UpdateFontList();
    nsresult ResolveFontName(const nsAString& aFontName,
                             FontResolverCallback aCallback,
                             void *aClosure, bool& aAborted);
    nsresult GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName);

    gfxFontGroup *CreateFontGroup(const nsAString &aFamilies,
                                  const gfxFontStyle *aStyle,
                                  gfxUserFontSet *aUserFontSet);

    // Given a string and a font we already have, find the font that
    // supports the most code points and most closely resembles aFont.
    // This simple version involves looking at the fonts on the machine to see
    // which code points they support.
    already_AddRefed<gfxOS2Font> FindFontForChar(uint32_t aCh, gfxOS2Font *aFont);

    // return true if it's already known that we don't have a font for this char
    bool noFontWithChar(uint32_t aCh) {
        return mCodepointsWithNoFonts.test(aCh);
    }

protected:
    static gfxFontconfigUtils *sFontconfigUtils;

private:
    // when font lookup fails for a character, cache it to skip future searches
    gfxSparseBitSet mCodepointsWithNoFonts;
};

#endif /* GFX_OS2_PLATFORM_H */
