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
 * The Original Code is Mozilla Foundation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#ifndef GFX_PLATFORM_MAC_H
#define GFX_PLATFORM_MAC_H

#include "nsTArray.h"
#include "gfxPlatform.h"

#define MAC_OS_X_VERSION_10_4_HEX 0x00001040
#define MAC_OS_X_VERSION_10_5_HEX 0x00001050
#define MAC_OS_X_VERSION_10_6_HEX 0x00001060
#define MAC_OS_X_VERSION_10_7_HEX 0x00001070

#define MAC_OS_X_MAJOR_VERSION_MASK 0xFFFFFFF0U

class gfxTextRun;

class THEBES_API gfxPlatformMac : public gfxPlatform {
public:
    gfxPlatformMac();
    virtual ~gfxPlatformMac();

    static gfxPlatformMac *GetPlatform() {
        return (gfxPlatformMac*) gfxPlatform::GetPlatform();
    }

    already_AddRefed<gfxASurface> CreateOffscreenSurface(const gfxIntSize& size,
                                                         gfxASurface::gfxContentType contentType);
    
    virtual mozilla::RefPtr<mozilla::gfx::DrawTarget>
      CreateOffscreenDrawTarget(const mozilla::gfx::IntSize& aSize, mozilla::gfx::SurfaceFormat aFormat);

    already_AddRefed<gfxASurface> OptimizeImage(gfxImageSurface *aSurface,
                                                gfxASurface::gfxImageFormat format);
    
    mozilla::RefPtr<mozilla::gfx::ScaledFont>
      GetScaledFontForFont(gfxFont *aFont);

    nsresult ResolveFontName(const nsAString& aFontName,
                             FontResolverCallback aCallback,
                             void *aClosure, bool& aAborted);

    nsresult GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName);

    gfxFontGroup *CreateFontGroup(const nsAString &aFamilies,
                                  const gfxFontStyle *aStyle,
                                  gfxUserFontSet *aUserFontSet);

    virtual gfxFontEntry* LookupLocalFont(const gfxProxyFontEntry *aProxyEntry,
                                          const nsAString& aFontName);

    virtual gfxPlatformFontList* CreatePlatformFontList();

    virtual gfxFontEntry* MakePlatformFont(const gfxProxyFontEntry *aProxyEntry,
                                           const PRUint8 *aFontData,
                                           PRUint32 aLength);

    bool IsFontFormatSupported(nsIURI *aFontURI, PRUint32 aFormatFlags);

    nsresult GetFontList(nsIAtom *aLangGroup,
                         const nsACString& aGenericFamily,
                         nsTArray<nsString>& aListOfFonts);
    nsresult UpdateFontList();

    // Returns the OS X version as returned from Gestalt(gestaltSystemVersion, ...)
    // Ex: Mac OS X 10.4.x ==> 0x104x 
    PRInt32 OSXVersion();

    // lower threshold on font anti-aliasing
    PRUint32 GetAntiAliasingThreshold() { return mFontAntiAliasingThreshold; }

private:
    virtual qcms_profile* GetPlatformCMSOutputProfile();
    
    // read in the pref value for the lower threshold on font anti-aliasing
    static PRUint32 ReadAntiAliasingThreshold();    
    
    PRInt32 mOSXVersion;
    PRUint32 mFontAntiAliasingThreshold;
};

#endif /* GFX_PLATFORM_MAC_H */
