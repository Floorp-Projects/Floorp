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
 * The Original Code is Mozilla Japan code.
 *
 * The Initial Developer of the Original Code is Mozilla Japan.
 * Portions created by the Initial Developer are Copyright (C) 2007
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

#ifndef GFX_FONTCONFIG_UTILS_H
#define GFX_FONTCONFIG_UTILS_H

#include "gfxPlatform.h"

#include "nsTArray.h"
#include "nsDataHashtable.h"

class gfxFontNameList : public nsTArray<nsString>
{
public:
    THEBES_INLINE_DECL_REFCOUNTING(gfxFontList)
    PRBool Exists(nsAString& aName);
};

class gfxFontconfigUtils {
public:
    gfxFontconfigUtils();

    static gfxFontconfigUtils* GetFontconfigUtils() {
        static gfxFontconfigUtils* sUtils = nsnull;
        if (!sUtils)
            sUtils = new gfxFontconfigUtils();
        return sUtils;
    }

    nsresult GetFontList(const nsACString& aLangGroup,
                         const nsACString& aGenericFamily,
                         nsStringArray& aListOfFonts);

    nsresult UpdateFontList();

    nsresult ResolveFontName(const nsAString& aFontName,
                             gfxPlatform::FontResolverCallback aCallback,
                             void *aClosure, PRBool& aAborted);

protected:
    PRInt32 IsExistingFont(const nsACString& aFontName);
    nsresult GetResolvedFonts(const nsACString& aName,
                              gfxFontNameList* aResult);

    nsresult GetFontListInternal(nsCStringArray& aListOfFonts,
                                 const nsACString *aLangGroup = nsnull);
    nsresult UpdateFontListInternal(PRBool aForce = PR_FALSE);

    nsCStringArray mFonts;
    nsCStringArray mNonExistingFonts;
    nsCStringArray mAliasForSingleFont;
    nsCStringArray mAliasForMultiFonts;

    nsDataHashtable<nsCStringHashKey, nsRefPtr<gfxFontNameList> > mAliasTable;
};

#endif /* GFX_FONTCONFIG_UTILS_H */
