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
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <stuart@mozilla.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *   John Daggett <jdaggett@mozilla.com>
 *   Jonathan Kew <jfkthame@gmail.com>
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

#include "gfxFT2FontList.h"
#include "gfxUserFontSet.h"
#include "gfxFontUtils.h"

#include "ft2build.h"
#include FT_FREETYPE_H
#include "gfxFT2Fonts.h"

#include "nsServiceManagerUtils.h"
#include "nsTArray.h"
#include "nsUnicharUtils.h"

#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsISimpleEnumerator.h"

#ifdef XP_WIN
#include "nsIWindowsRegKey.h"
#include <windows.h>
#endif

#define ROUND(x) floor((x) + 0.5)

#ifdef PR_LOGGING
static PRLogModuleInfo *gFontInfoLog = PR_NewLogModule("fontInfoLog");
#endif /* PR_LOGGING */

#ifdef ANDROID
#include "gfxAndroidPlatform.h"
#include <dirent.h>
#include <android/log.h>
#define ALOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Gecko" , ## args)

#endif
#define LOG(args) PR_LOG(gFontInfoLog, PR_LOG_DEBUG, args)
#define LOG_ENABLED() PR_LOG_TEST(gFontInfoLog, PR_LOG_DEBUG)

static __inline void
BuildKeyNameFromFontName(nsAString &aName)
{
#ifdef XP_WIN
    if (aName.Length() >= LF_FACESIZE)
        aName.Truncate(LF_FACESIZE - 1);
#endif
    ToLowerCase(aName);
}

/***************************************************************
 *
 * gfxFT2FontList
 *
 */

// For Mobile, we use gfxFT2Fonts, and we build the font list by directly scanning
// the system's Fonts directory for OpenType and TrueType files.
//
// FontEntry is currently defined in gfxFT2Fonts.h, but will probably be moved here
// as part of the Freetype/Linux font restructuring for Harfbuzz integration.
//
// TODO: investigate startup performance - we might be able to improve by avoiding
// the creation of FT_Faces here, and just reading names directly from the file;
// or even consider caching a mapping from font family name to (list of) filenames,
// so that we don't have to scan all the files before we can do any font lookups.

gfxFT2FontList::gfxFT2FontList()
{
}

void
gfxFT2FontList::AppendFacesFromFontFile(const PRUnichar *aFileName)
{
    AppendFacesFromFontFile(NS_ConvertUTF16toUTF8(aFileName).get());
}

void
gfxFT2FontList::AppendFacesFromFontFile(const char *aFileName)
{
#ifdef XP_WIN
    FT_Library ftLibrary = gfxWindowsPlatform::GetPlatform()->GetFTLibrary();
#elif defined(ANDROID)
    FT_Library ftLibrary = gfxAndroidPlatform::GetPlatform()->GetFTLibrary();
#endif
    FT_Face dummy;
    if (FT_Err_Ok == FT_New_Face(ftLibrary, aFileName, -1, &dummy)) {
        for (FT_Long i = 0; i < dummy->num_faces; i++) {
            FT_Face face;
            if (FT_Err_Ok != FT_New_Face(ftLibrary, aFileName, i, &face))
                continue;

            FontEntry* fe = FontEntry::CreateFontEntryFromFace(face);
            if (fe) {
                NS_ConvertUTF8toUTF16 name(face->family_name);
                BuildKeyNameFromFontName(name);       
                gfxFontFamily *family = mFontFamilies.GetWeak(name);
                if (!family) {
                    family = new gfxFontFamily(name);
                    mFontFamilies.Put(name, family);
                    if (mBadUnderlineFamilyNames.Contains(name))
                        family->SetBadUnderlineFamily();
                }
                family->AddFontEntry(fe);
                family->SetHasStyles(PR_TRUE);
                if (family->IsBadUnderlineFamily())
                    fe->mIsBadUnderlineFont = PR_TRUE;
#ifdef PR_LOGGING
                if (LOG_ENABLED()) {
                    LOG(("(fontinit) added (%s) to family (%s)"
                         " with style: %s weight: %d stretch: %d",
                         NS_ConvertUTF16toUTF8(fe->Name()).get(), 
                         NS_ConvertUTF16toUTF8(family->Name()).get(), 
                         fe->IsItalic() ? "italic" : "normal",
                         fe->Weight(), fe->Stretch()));
                }
#endif
            }
        }
        FT_Done_Face(dummy);
    }
}

void
gfxFT2FontList::FindFonts()
{
#ifdef XP_WIN
    nsTArray<nsString> searchPaths(3);
    nsTArray<nsString> fontPatterns(3);
    fontPatterns.AppendElement(NS_LITERAL_STRING("\\*.ttf"));
    fontPatterns.AppendElement(NS_LITERAL_STRING("\\*.ttc"));
    fontPatterns.AppendElement(NS_LITERAL_STRING("\\*.otf"));
    wchar_t pathBuf[256];
    SHGetSpecialFolderPathW(0, pathBuf, CSIDL_WINDOWS, 0);
    searchPaths.AppendElement(pathBuf);
    SHGetSpecialFolderPathW(0, pathBuf, CSIDL_FONTS, 0);
    searchPaths.AppendElement(pathBuf);
    nsCOMPtr<nsIFile> resDir;
    NS_GetSpecialDirectory(NS_APP_RES_DIR, getter_AddRefs(resDir));
    if (resDir) {
        resDir->Append(NS_LITERAL_STRING("fonts"));
        nsAutoString resPath;
        resDir->GetPath(resPath);
        searchPaths.AppendElement(resPath);
    }
    WIN32_FIND_DATAW results;
    for (PRUint32 i = 0;  i < searchPaths.Length(); i++) {
        const nsString& path(searchPaths[i]);
        for (PRUint32 j = 0; j < fontPatterns.Length(); j++) { 
            nsAutoString pattern(path);
            pattern.Append(fontPatterns[j]);
            HANDLE handle = FindFirstFileExW(pattern.get(),
                                             FindExInfoStandard,
                                             &results,
                                             FindExSearchNameMatch,
                                             NULL,
                                             0);
            PRBool moreFiles = handle != INVALID_HANDLE_VALUE;
            while (moreFiles) {
                nsAutoString filePath(path);
                filePath.AppendLiteral("\\");
                filePath.Append(results.cFileName);
                AppendFacesFromFontFile(static_cast<const PRUnichar*>(filePath.get()));
                moreFiles = FindNextFile(handle, &results);
            }
            if (handle != INVALID_HANDLE_VALUE)
                FindClose(handle);
        }
    }
#elif defined(ANDROID)
    gfxFontCache *fc = gfxFontCache::GetCache();
    if (fc)
        fc->AgeAllGenerations();
    mPrefFonts.Clear();
    mCodepointsWithNoFonts.reset();

    DIR *d = opendir("/system/fonts");
    struct dirent *ent = NULL;
    while(d && (ent = readdir(d)) != NULL) {
        int namelen = strlen(ent->d_name);
        if (namelen > 4 &&
            strcasecmp(ent->d_name + namelen - 4, ".ttf") == 0)
        {
            nsCString s("/system/fonts");
            s.Append("/");
            s.Append(nsDependentCString(ent->d_name));

            AppendFacesFromFontFile(nsPromiseFlatCString(s).get());
        }
    }
    if (d) {
      closedir(d);
    }
    mCodepointsWithNoFonts.SetRange(0,0x1f);     // C0 controls
    mCodepointsWithNoFonts.SetRange(0x7f,0x9f);  // C1 controls

#endif // XP_WIN && ANDROID
}

nsresult
gfxFT2FontList::InitFontList()
{
    // reset font lists
    gfxPlatformFontList::InitFontList();
    
    FindFonts();

    return NS_OK;
}

struct FullFontNameSearch {
    FullFontNameSearch(const nsAString& aFullName)
        : mFullName(aFullName), mFontEntry(nsnull)
    { }

    nsString     mFullName;
    gfxFontEntry *mFontEntry;
};

// callback called for each family name, based on the assumption that the 
// first part of the full name is the family name
static PLDHashOperator
FindFullName(nsStringHashKey::KeyType aKey,
             nsRefPtr<gfxFontFamily>& aFontFamily,
             void* userArg)
{
    FullFontNameSearch *data = reinterpret_cast<FullFontNameSearch*>(userArg);

    // does the family name match up to the length of the family name?
    const nsString& family = aFontFamily->Name();
    
    nsString fullNameFamily;
    data->mFullName.Left(fullNameFamily, family.Length());

    // if so, iterate over faces in this family to see if there is a match
    if (family.Equals(fullNameFamily)) {
        nsTArray<nsRefPtr<gfxFontEntry> >& fontList = aFontFamily->GetFontList();
        int index, len = fontList.Length();
        for (index = 0; index < len; index++) {
            if (fontList[index]->Name().Equals(data->mFullName)) {
                data->mFontEntry = fontList[index];
                return PL_DHASH_STOP;
            }
        }
    }

    return PL_DHASH_NEXT;
}

gfxFontEntry* 
gfxFT2FontList::LookupLocalFont(const gfxProxyFontEntry *aProxyEntry,
                                       const nsAString& aFontName)
{
    // walk over list of names
    FullFontNameSearch data(aFontName);

    mFontFamilies.Enumerate(FindFullName, &data);

    return data.mFontEntry;
}

gfxFontEntry*
gfxFT2FontList::GetDefaultFont(const gfxFontStyle* aStyle, PRBool& aNeedsBold)
{
#ifdef XP_WIN
    HGDIOBJ hGDI = ::GetStockObject(SYSTEM_FONT);
    LOGFONTW logFont;
    if (hGDI && ::GetObjectW(hGDI, sizeof(logFont), &logFont)) {
        nsAutoString resolvedName;
        if (ResolveFontName(nsDependentString(logFont.lfFaceName), resolvedName)) {
            return FindFontForFamily(resolvedName, aStyle, aNeedsBold);
        }
    }
#elif defined(ANDROID)
    nsAutoString resolvedName;
    if (ResolveFontName(nsDependentString(NS_LITERAL_STRING("Droid Sans")), 
                        resolvedName))
        return FindFontForFamily(resolvedName, aStyle, aNeedsBold);
#endif
    /* TODO: what about Qt or other platforms that may use this? */
    return nsnull;
}

gfxFontEntry*
gfxFT2FontList::MakePlatformFont(const gfxProxyFontEntry *aProxyEntry,
                                 const PRUint8 *aFontData,
                                 PRUint32 aLength)
{
    // The FT2 font needs the font data to persist, so we do NOT free it here
    // but instead pass ownership to the font entry.
    // Deallocation will happen later, when the font face is destroyed.
    return FontEntry::CreateFontEntry(*aProxyEntry, aFontData, aLength);
}
