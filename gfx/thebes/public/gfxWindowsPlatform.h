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
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@pavlov.net>
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

#ifndef GFX_WINDOWS_PLATFORM_H
#define GFX_WINDOWS_PLATFORM_H

#include "gfxWindowsSurface.h"
#include "gfxWindowsFonts.h"
#include "gfxPlatform.h"

#include "nsVoidArray.h"
#include "nsDataHashtable.h"

#include <windows.h>

class THEBES_API gfxWindowsPlatform : public gfxPlatform {
public:
    gfxWindowsPlatform();
    virtual ~gfxWindowsPlatform();
    static gfxWindowsPlatform *GetPlatform() {
        return (gfxWindowsPlatform*) gfxPlatform::GetPlatform();
    }

    already_AddRefed<gfxASurface> CreateOffscreenSurface(const gfxIntSize& size,
                                                         gfxASurface::gfxImageFormat imageFormat);

    nsresult GetFontList(const nsACString& aLangGroup,
                         const nsACString& aGenericFamily,
                         nsStringArray& aListOfFonts);

    nsresult UpdateFontList();

    nsresult ResolveFontName(const nsAString& aFontName,
                             FontResolverCallback aCallback,
                             void *aClosure, PRBool& aAborted);

    /* local methods */
    void FindOtherFonts(const PRUnichar *aString, PRUint32 aLength, const char *aLangGroup, const char *aGeneric, nsString& array);

    WeightTable *GetFontWeightTable(const nsAString& aName);
    void PutFontWeightTable(const nsAString& aName, WeightTable *aWeightTable);

private:
    void Init();

    static int CALLBACK FontEnumProc(const ENUMLOGFONTEXW *lpelfe,
                                     const NEWTEXTMETRICEXW *metrics,
                                     DWORD fontType, LPARAM data);

    static int CALLBACK FontResolveProc(const ENUMLOGFONTEXW *lpelfe,
                                        const NEWTEXTMETRICEXW *metrics,
                                        DWORD fontType, LPARAM data);

    static PLDHashOperator PR_CALLBACK HashEnumFunc(nsStringHashKey::KeyType aKey,
                                                    nsRefPtr<FontEntry>& aData,
                                                    void* userArg);

    static PLDHashOperator PR_CALLBACK FindFontForChar(nsStringHashKey::KeyType aKey,
                                                       nsRefPtr<FontEntry>& aFontEntry,
                                                       void* userArg);

    nsDataHashtable<nsStringHashKey, nsRefPtr<FontEntry> > mFonts;
    nsDataHashtable<nsStringHashKey, nsRefPtr<WeightTable> > mFontWeights;
    nsDataHashtable<nsStringHashKey, nsRefPtr<FontEntry> > mFontAliases;
    nsDataHashtable<nsStringHashKey, nsRefPtr<FontEntry> > mFontSubstitutes;
    nsStringArray mNonExistingFonts;
};

#endif /* GFX_WINDOWS_PLATFORM_H */
