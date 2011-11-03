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
 * The Original Code is Android port code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
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

#ifndef GFX_PLATFORM_ANDROID_H
#define GFX_PLATFORM_ANDROID_H

#include "gfxFT2Fonts.h"
#include "gfxPlatform.h"
#include "gfxUserFontSet.h"
#include "nsTArray.h"

namespace mozilla {
    namespace dom {
        class FontListEntry;
    };
};
using mozilla::dom::FontListEntry;

typedef struct FT_LibraryRec_ *FT_Library;

class THEBES_API gfxAndroidPlatform : public gfxPlatform {
public:
    gfxAndroidPlatform();
    virtual ~gfxAndroidPlatform();

    static gfxAndroidPlatform *GetPlatform() {
        return (gfxAndroidPlatform*) gfxPlatform::GetPlatform();
    }

    virtual already_AddRefed<gfxASurface>
    CreateOffscreenSurface(const gfxIntSize& size,
                           gfxASurface::gfxContentType contentType);

    virtual gfxImageFormat GetOffscreenFormat() { return gfxASurface::ImageFormatRGB16_565; }
    
    mozilla::RefPtr<mozilla::gfx::ScaledFont>
      GetScaledFontForFont(gfxFont *aFont);

    // to support IPC font list (sharing between chrome and content)
    void GetFontList(InfallibleTArray<FontListEntry>* retValue);

    // platform implementations of font functions
    virtual bool IsFontFormatSupported(nsIURI *aFontURI, PRUint32 aFormatFlags);
    virtual gfxPlatformFontList* CreatePlatformFontList();
    virtual gfxFontEntry* MakePlatformFont(const gfxProxyFontEntry *aProxyEntry,
                                           const PRUint8 *aFontData, PRUint32 aLength);

    virtual nsresult GetFontList(nsIAtom *aLangGroup,
                                 const nsACString& aGenericFamily,
                                 nsTArray<nsString>& aListOfFonts);

    virtual nsresult UpdateFontList();

    virtual nsresult ResolveFontName(const nsAString& aFontName,
                                     FontResolverCallback aCallback,
                                     void *aClosure, bool& aAborted);

    virtual nsresult GetStandardFamilyName(const nsAString& aFontName,
                                           nsAString& aFamilyName);

    virtual gfxFontGroup *CreateFontGroup(const nsAString &aFamilies,
                                          const gfxFontStyle *aStyle,
                                          gfxUserFontSet* aUserFontSet);

    FT_Library GetFTLibrary();
};

#endif /* GFX_PLATFORM_ANDROID_H */

