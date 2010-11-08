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
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.com>
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

#include "gfxDWriteFontList.h"
#include "gfxDWriteFonts.h"
#include "nsUnicharUtils.h"
#include "nsILocaleService.h"

#include "gfxGDIFontList.h"

#include "nsIWindowsRegKey.h"

// font info loader constants
static const PRUint32 kDelayBeforeLoadingFonts = 8 * 1000; // 8secs
static const PRUint32 kIntervalBetweenLoadingFonts = 150; // 150ms

static __inline void
BuildKeyNameFromFontName(nsAString &aName)
{
    if (aName.Length() >= LF_FACESIZE)
        aName.Truncate(LF_FACESIZE - 1);
    ToLowerCase(aName);
}

////////////////////////////////////////////////////////////////////////////////
// gfxDWriteFontFamily

gfxDWriteFontFamily::~gfxDWriteFontFamily()
{
}

void
gfxDWriteFontFamily::FindStyleVariations()
{
    HRESULT hr;
    if (mHasStyles) {
        return;
    }
    mHasStyles = PR_TRUE;

    for (UINT32 i = 0; i < mDWFamily->GetFontCount(); i++) {
        nsRefPtr<IDWriteFont> font;
        hr = mDWFamily->GetFont(i, getter_AddRefs(font));
        if (FAILED(hr)) {
            // This should never happen.
            NS_WARNING("Failed to get existing font from family.");
            continue;
        }

        if (font->GetSimulations() & DWRITE_FONT_SIMULATIONS_OBLIQUE) {
            // We don't want these.
            continue;
        }

        nsRefPtr<IDWriteLocalizedStrings> names;
        hr = font->GetFaceNames(getter_AddRefs(names));
        if (FAILED(hr)) {
            continue;
        }
        
        BOOL exists;
        nsAutoTArray<WCHAR,32> faceName;
        UINT32 englishIdx = 0;
        hr = names->FindLocaleName(L"en-us", &englishIdx, &exists);
        if (FAILED(hr)) {
            continue;
        }

        if (!exists) {
            // No english found, use whatever is first in the list.
            englishIdx = 0;
        }
        UINT32 length;
        hr = names->GetStringLength(englishIdx, &length);
        if (FAILED(hr)) {
            continue;
        }
        if (!faceName.SetLength(length + 1)) {
            // Eeep - running out of memory. Unlikely to end well.
            continue;
        }

        hr = names->GetString(englishIdx, faceName.Elements(), length + 1);
        if (FAILED(hr)) {
            continue;
        }

        nsString fullID(mName);
        fullID.Append(faceName.Elements());

        /**
         * Faces do not have a localized name so we just put the en-us name in
         * here.
         */
        gfxDWriteFontEntry *fe = 
            new gfxDWriteFontEntry(fullID, font);
        fe->SetFamily(this);

        mAvailableFonts.AppendElement(fe);
    }
    if (!mAvailableFonts.Length()) {
        NS_WARNING("Family with no font faces in it.");
    }

    if (mIsBadUnderlineFamily) {
        SetBadUnderlineFonts();
    }
}

void
gfxDWriteFontFamily::LocalizedName(nsAString &aLocalizedName)
{
    aLocalizedName.AssignLiteral("Unknown Font");
    HRESULT hr;
    nsresult rv;
    nsCOMPtr<nsILocaleService> ls = do_GetService(NS_LOCALESERVICE_CONTRACTID,
                                                  &rv);
    nsCOMPtr<nsILocale> locale;
    rv = ls->GetApplicationLocale(getter_AddRefs(locale));
    nsString localeName;
    if (NS_SUCCEEDED(rv)) {
        rv = locale->GetCategory(NS_LITERAL_STRING(NSILOCALE_MESSAGE), 
                                 localeName);
    }
    if (NS_FAILED(rv)) {
        localeName.AssignLiteral("en-us");
    }

    nsRefPtr<IDWriteLocalizedStrings> names;

    hr = mDWFamily->GetFamilyNames(getter_AddRefs(names));
    if (FAILED(hr)) {
        return;
    }
    UINT32 idx = 0;
    BOOL exists;
    hr = names->FindLocaleName(localeName.BeginReading(),
                               &idx,
                               &exists);
    if (FAILED(hr)) {
        return;
    }
    if (!exists) {
        // Use english is localized is not found.
        hr = names->FindLocaleName(L"en-us", &idx, &exists);
        if (FAILED(hr)) {
            return;
        }
        if (!exists) {
            // Use 0 index if english is not found.
            idx = 0;
        }
    }
    nsAutoTArray<WCHAR, 32> famName;
    UINT32 length;
    
    hr = names->GetStringLength(idx, &length);
    if (FAILED(hr)) {
        return;
    }
    
    if (!famName.SetLength(length + 1)) {
        // Eeep - running out of memory. Unlikely to end well.
        return;
    }

    hr = names->GetString(idx, famName.Elements(), length + 1);
    if (FAILED(hr)) {
        return;
    }

    aLocalizedName = nsDependentString(famName.Elements());
}

////////////////////////////////////////////////////////////////////////////////
// gfxDWriteFontEntry

gfxDWriteFontEntry::~gfxDWriteFontEntry()
{
}

PRBool
gfxDWriteFontEntry::IsSymbolFont()
{
    if (mFont) {
        return mFont->IsSymbolFont();
    } else {
        return PR_FALSE;
    }
}

nsresult
gfxDWriteFontEntry::GetFontTable(PRUint32 aTableTag,
                                 nsTArray<PRUint8> &aBuffer)
{
    nsRefPtr<IDWriteFontFace> fontFace;
    HRESULT hr;
    nsresult rv;
    rv = CreateFontFace(getter_AddRefs(fontFace));

    if (NS_FAILED(rv)) {
        return rv;
    }

    PRUint8 *tableData;
    PRUint32 len;
    void *tableContext = NULL;
    BOOL exists;
    hr = fontFace->TryGetFontTable(NS_SWAP32(aTableTag),
                                   (const void**)&tableData,
                                   &len,
                                   &tableContext,
                                   &exists);

    if (FAILED(hr) || !exists) {
        return NS_ERROR_FAILURE;
    }
    if (!aBuffer.SetLength(len)) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    memcpy(aBuffer.Elements(), tableData, len);
    if (tableContext) {
        fontFace->ReleaseFontTable(&tableContext);
    }
    return NS_OK;
}

nsresult
gfxDWriteFontEntry::ReadCMAP()
{
    nsRefPtr<IDWriteFontFace> fontFace;
    HRESULT hr;
    nsresult rv;
    rv = CreateFontFace(getter_AddRefs(fontFace));

    if (NS_FAILED(rv)) {
        return rv;
    }

    PRUint8 *tableData;
    PRUint32 len;
    void *tableContext = NULL;
    BOOL exists;
    hr = fontFace->TryGetFontTable(DWRITE_MAKE_OPENTYPE_TAG('c', 'm', 'a', 'p'),
                                   (const void**)&tableData,
                                   &len,
                                   &tableContext,
                                   &exists);
    if (FAILED(hr)) {
        return NS_ERROR_FAILURE;
    }

    PRPackedBool isSymbol = fontFace->IsSymbolFont();
    PRPackedBool isUnicode = PR_TRUE;
    if (exists) {
        rv = gfxFontUtils::ReadCMAP(tableData,
                                    len,
                                    mCharacterMap,
                                    mUVSOffset,
                                    isUnicode,
                                    isSymbol);
    }
    fontFace->ReleaseFontTable(tableContext);

    mCmapInitialized = PR_TRUE;
    mHasCmapTable = NS_SUCCEEDED(rv);
    return rv;
}

gfxFont *
gfxDWriteFontEntry::CreateFontInstance(const gfxFontStyle* aFontStyle,
                                       PRBool aNeedsBold)
{
    return new gfxDWriteFont(this, aFontStyle, aNeedsBold);
}

nsresult
gfxDWriteFontEntry::CreateFontFace(IDWriteFontFace **aFontFace,
                                   DWRITE_FONT_SIMULATIONS aSimulations)
{
    HRESULT hr;
    if (mFont) {
        hr = mFont->CreateFontFace(aFontFace);
    } else if (mFontFile) {
        IDWriteFontFile *fontFile = mFontFile.get();
        hr = gfxWindowsPlatform::GetPlatform()->GetDWriteFactory()->
            CreateFontFace(mFaceType,
                           1,
                           &fontFile,
                           0,
                           aSimulations,
                           aFontFace);
    }
    if (FAILED(hr)) {
        return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// gfxDWriteFontList

gfxDWriteFontList::gfxDWriteFontList()
{
    mFontSubstitutes.Init();
}

gfxFontEntry *
gfxDWriteFontList::GetDefaultFont(const gfxFontStyle *aStyle,
                                  PRBool &aNeedsBold)
{
    NONCLIENTMETRICSW ncm;
    ncm.cbSize = sizeof(ncm);
    BOOL status = ::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, 
                                          sizeof(ncm), &ncm, 0);
    if (status) {
        nsAutoString resolvedName;
        if (ResolveFontName(nsDependentString(ncm.lfMessageFont.lfFaceName),
                            resolvedName)) {
            return FindFontForFamily(resolvedName, aStyle, aNeedsBold);
        }
    }

    return nsnull;
}

gfxFontEntry *
gfxDWriteFontList::LookupLocalFont(const gfxProxyFontEntry *aProxyEntry,
                                   const nsAString& aFullname)
{
    PRBool found;
    gfxFontEntry *lookup;

    // initialize name lookup tables if needed
    if (!mFaceNamesInitialized) {
        InitFaceNameLists();
    }

    // lookup in name lookup tables, return null if not found
    if (!(lookup = mPostscriptNames.GetWeak(aFullname, &found)) &&
        !(lookup = mFullnames.GetWeak(aFullname, &found))) 
    {
        return nsnull;
    }
    gfxFontEntry *fe = 
        new gfxDWriteFontEntry(lookup->Name(),
                               static_cast<gfxDWriteFontEntry*>(lookup)->mFont,
                               aProxyEntry->Weight(),
                               aProxyEntry->Stretch(),
                               aProxyEntry->IsItalic());

    return fe;
}

gfxFontEntry *
gfxDWriteFontList::MakePlatformFont(const gfxProxyFontEntry *aProxyEntry,
                                    const PRUint8 *aFontData,
                                    PRUint32 aLength)
{
    nsresult rv;
    nsAutoString uniqueName;
    rv = gfxFontUtils::MakeUniqueUserFontName(uniqueName);
    if (NS_FAILED(rv)) {
        NS_Free((void*)aFontData);
        return nsnull;
    }

    nsTArray<PRUint8> newFontData;

    rv = gfxFontUtils::RenameFont(uniqueName, aFontData, aLength, &newFontData);
    NS_Free((void*)aFontData);

    if (NS_FAILED(rv)) {
        return nsnull;
    }
    
    DWORD numFonts = 0;

    nsRefPtr<IDWriteFontFile> fontFile;
    HRESULT hr;

    /**
     * We pass in a pointer to a structure containing a pointer to the array
     * containing the font data and a unique identifier. DWrite will
     * internally copy what is at that pointer, and pass that to
     * CreateStreamFromKey. The array will be empty when the function 
     * succesfully returns since it swaps out the data.
     */
    ffReferenceKey key;
    key.mArray = &newFontData;
    nsCOMPtr<nsIUUIDGenerator> uuidgen =
      do_GetService("@mozilla.org/uuid-generator;1");
    if (!uuidgen) {
        return nsnull;
    }

    rv = uuidgen->GenerateUUIDInPlace(&key.mGUID);

    if (NS_FAILED(rv)) {
        return nsnull;
    }

    hr = gfxWindowsPlatform::GetPlatform()->GetDWriteFactory()->
        CreateCustomFontFileReference(&key,
                                      sizeof(key),
                                      gfxDWriteFontFileLoader::Instance(),
                                      getter_AddRefs(fontFile));

    if (FAILED(hr)) {
        NS_WARNING("Failed to create custom font file reference.");
        return nsnull;
    }

    BOOL isSupported;
    DWRITE_FONT_FILE_TYPE fileType;
    UINT32 numFaces;

    PRUint16 w = (aProxyEntry->mWeight == 0 ? 400 : aProxyEntry->mWeight);
    gfxDWriteFontEntry *entry = 
        new gfxDWriteFontEntry(uniqueName, 
                               fontFile,
                               aProxyEntry->Weight(),
                               aProxyEntry->Stretch(),
                               aProxyEntry->IsItalic());

    fontFile->Analyze(&isSupported, &fileType, &entry->mFaceType, &numFaces);
    if (!isSupported || numFaces > 1) {
        // We don't know how to deal with 0 faces either.
        delete entry;
        return nsnull;
    }

    return entry;
}

nsresult
gfxDWriteFontList::InitFontList()
{
    HRESULT hr;
    gfxFontCache *fc = gfxFontCache::GetCache();
    if (fc) {
        fc->AgeAllGenerations();
    }

    gfxPlatformFontList::InitFontList();

    mFontSubstitutes.Clear();
    mNonExistingFonts.Clear();

    nsRefPtr<IDWriteFontCollection> systemFonts;
    hr = gfxWindowsPlatform::GetPlatform()->GetDWriteFactory()->
        GetSystemFontCollection(getter_AddRefs(systemFonts));
    NS_ASSERTION(SUCCEEDED(hr), "GetSystemFontCollection failed!");

    if (FAILED(hr)) {
        return NS_ERROR_FAILURE;
    }

    for (UINT32 i = 0; i < systemFonts->GetFontFamilyCount(); i++) {
        nsRefPtr<IDWriteFontFamily> family;
        systemFonts->GetFontFamily(i, getter_AddRefs(family));

        nsRefPtr<IDWriteLocalizedStrings> names;
        hr = family->GetFamilyNames(getter_AddRefs(names));
        if (FAILED(hr)) {
            continue;
        }

        UINT32 englishIdx = 0;

        BOOL exists;
        hr = names->FindLocaleName(L"en-us", &englishIdx, &exists);
        if (FAILED(hr)) {
            continue;
        }
        if (!exists) {
            // Use 0 index if english is not found.
            englishIdx = 0;
        }

        nsAutoTArray<WCHAR, 32> famName;
        UINT32 length;
        
        hr = names->GetStringLength(englishIdx, &length);
        if (FAILED(hr)) {
            continue;
        }
        
        if (!famName.SetLength(length + 1)) {
            // Eeep - running out of memory. Unlikely to end well.
            continue;
        }

        hr = names->GetString(englishIdx, famName.Elements(), length + 1);
        if (FAILED(hr)) {
            continue;
        }

        nsAutoString name(famName.Elements());
        BuildKeyNameFromFontName(name);

        if (!mFontFamilies.GetWeak(name)) {
            nsRefPtr<gfxFontFamily> fam = 
                new gfxDWriteFontFamily(nsDependentString(famName.Elements()),
                                        family);
            if (mBadUnderlineFamilyNames.Contains(name)) {
                fam->SetBadUnderlineFamily();
            }
            mFontFamilies.Put(name, fam);
        }
    }

    GetFontSubstitutes();

    StartLoader(kDelayBeforeLoadingFonts, kIntervalBetweenLoadingFonts);

    return NS_OK;
}

static void
RemoveCharsetFromFontSubstitute(nsAString &aName)
{
    PRInt32 comma = aName.FindChar(PRUnichar(','));
    if (comma >= 0)
        aName.Truncate(comma);
}

nsresult
gfxDWriteFontList::GetFontSubstitutes()
{
    // Create the list of FontSubstitutes
    nsCOMPtr<nsIWindowsRegKey> regKey = 
        do_CreateInstance("@mozilla.org/windows-registry-key;1");
    if (!regKey) {
        return NS_ERROR_FAILURE;
    }
    NS_NAMED_LITERAL_STRING(
        kFontSubstitutesKey,
        "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\FontSubstitutes");

    nsresult rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_LOCAL_MACHINE,
                               kFontSubstitutesKey,
                               nsIWindowsRegKey::ACCESS_READ);
    if (NS_FAILED(rv)) {
        return rv;
    }

    PRUint32 count;
    rv = regKey->GetValueCount(&count);
    if (NS_FAILED(rv) || count == 0)
        return rv;
    for (PRUint32 i = 0; i < count; i++) {
        nsAutoString substituteName;
        rv = regKey->GetValueName(i, substituteName);
        if (NS_FAILED(rv) || substituteName.IsEmpty() ||
            substituteName.CharAt(1) == PRUnichar('@')) {
            continue;
        }
        PRUint32 valueType;
        rv = regKey->GetValueType(substituteName, &valueType);
        if (NS_FAILED(rv) || valueType != nsIWindowsRegKey::TYPE_STRING) {
            continue;
        }
        nsAutoString actualFontName;
        rv = regKey->ReadStringValue(substituteName, actualFontName);
        if (NS_FAILED(rv)) {
            continue;
        }

        RemoveCharsetFromFontSubstitute(substituteName);
        BuildKeyNameFromFontName(substituteName);
        RemoveCharsetFromFontSubstitute(actualFontName);
        BuildKeyNameFromFontName(actualFontName);
        gfxFontFamily *ff;
        if (!actualFontName.IsEmpty() && 
            (ff = mFontFamilies.GetWeak(actualFontName))) {
            mFontSubstitutes.Put(substituteName, ff);
        } else {
            mNonExistingFonts.AppendElement(substituteName);
        }
    }
    return NS_OK;
}

PRBool
gfxDWriteFontList::GetStandardFamilyName(const nsAString& aFontName,
                                         nsAString& aFamilyName)
{
    gfxFontFamily *family = FindFamily(aFontName);
    if (family) {
        family->LocalizedName(aFamilyName);
        return PR_TRUE;
    }

    return PR_FALSE;
}

PRBool 
gfxDWriteFontList::ResolveFontName(const nsAString& aFontName,
                                   nsAString& aResolvedFontName)
{
    nsAutoString keyName(aFontName);
    BuildKeyNameFromFontName(keyName);

    nsRefPtr<gfxFontFamily> ff;
    if (mFontSubstitutes.Get(keyName, &ff)) {
        aResolvedFontName = ff->Name();
        return PR_TRUE;
    }

    if (mNonExistingFonts.Contains(keyName)) {
        return PR_FALSE;
    }

    return gfxPlatformFontList::ResolveFontName(aFontName, aResolvedFontName);
}
