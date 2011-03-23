/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#ifdef MOZ_IPC
#include "mozilla/dom/ContentChild.h"
#include "nsXULAppAPI.h"
#endif

#include <android/log.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include "gfxAndroidPlatform.h"

#include "cairo.h"
#include "cairo-ft.h"

#include "gfxImageSurface.h"

#include "nsUnicharUtils.h"

#include "nsMathUtils.h"
#include "nsTArray.h"

#include "qcms.h"

#include "ft2build.h"
#include FT_FREETYPE_H
#include "gfxFT2Fonts.h"
#include "gfxPlatformFontList.h"
#include "gfxFT2FontList.h"
#include "mozilla/scache/StartupCache.h"
#include "nsXPCOMStrings.h"

using namespace mozilla;
using namespace dom;

static FT_Library gPlatformFTLibrary = NULL;

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GeckoFonts" , ## args)

gfxAndroidPlatform::gfxAndroidPlatform()
{
    FT_Init_FreeType(&gPlatformFTLibrary);

    mFonts.Init(200);
    mFontAliases.Init(20);
    mFontSubstitutes.Init(50);
    mPrefFonts.Init(10);

    UpdateFontList();
}

gfxAndroidPlatform::~gfxAndroidPlatform()
{
    cairo_debug_reset_static_data();

    FT_Done_FreeType(gPlatformFTLibrary);
    gPlatformFTLibrary = NULL;
}

already_AddRefed<gfxASurface>
gfxAndroidPlatform::CreateOffscreenSurface(const gfxIntSize& size,
                                      gfxASurface::gfxContentType contentType)
{
    nsRefPtr<gfxASurface> newSurface;
    if (contentType == gfxImageSurface::CONTENT_COLOR)
        newSurface = new gfxImageSurface (size, GetOffscreenFormat());
    else
        newSurface = new gfxImageSurface (size, gfxASurface::FormatFromContent(contentType));

    return newSurface.forget();
}

struct FontListData {
    FontListData(nsIAtom *aLangGroup, const nsACString& aGenericFamily, nsTArray<nsString>& aListOfFonts) :
        mLangGroup(aLangGroup), mGenericFamily(aGenericFamily), mStringArray(aListOfFonts) {}
    nsIAtom *mLangGroup;
    const nsACString& mGenericFamily;
    nsTArray<nsString>& mStringArray;
};

static PLDHashOperator
FontListHashEnumFunc(nsStringHashKey::KeyType aKey,
                     nsRefPtr<FontFamily>& aFontFamily,
                     void* userArg)
{
    FontListData *data = (FontListData*)userArg;

    // use the first variation for now.  This data should be the same
    // for all the variations and should probably be moved up to
    // the Family
    gfxFontStyle style;
    style.language = data->mLangGroup;
    nsRefPtr<FontEntry> aFontEntry = aFontFamily->FindFontEntry(style);
    NS_ASSERTION(aFontEntry, "couldn't find any font entry in family");
    if (!aFontEntry)
        return PL_DHASH_NEXT;


    data->mStringArray.AppendElement(aFontFamily->Name());

    return PL_DHASH_NEXT;
}

nsresult
gfxAndroidPlatform::GetFontList(nsIAtom *aLangGroup,
                                const nsACString& aGenericFamily,
                                nsTArray<nsString>& aListOfFonts)
{
    FontListData data(aLangGroup, aGenericFamily, aListOfFonts);

    mFonts.Enumerate(FontListHashEnumFunc, &data);

    aListOfFonts.Sort();
    aListOfFonts.Compact();

    return NS_OK;
}

class FontNameCache {
public:
    typedef nsAutoTArray<PRUint32, 8> IndexList;
    PLDHashTableOps ops;
    FontNameCache() : mWriteNeeded(PR_FALSE) {
        ops = {
            PL_DHashAllocTable,
            PL_DHashFreeTable,
            StringHash,
            HashMatchEntry,
            MoveEntry,
            PL_DHashClearEntryStub,
            PL_DHashFinalizeStub,
            NULL};
        if (!PL_DHashTableInit(&mMap, &ops, nsnull,
                               sizeof(FNCMapEntry), 0)) {
            mMap.ops = nsnull;
            LOG("initializing the map failed");
        }
#ifdef MOZ_IPC
        NS_ABORT_IF_FALSE(XRE_GetProcessType() == GeckoProcessType_Default,
                          "StartupCacheFontNameCache should only be used in chrome procsess");
#endif
        mCache = mozilla::scache::StartupCache::GetSingleton();
        Init();
    }

    void Init()
    {
        if (!mMap.ops || !mCache)
            return;
        nsCAutoString prefName("font.cache");
        PRUint32 size;
        char* buf;
        if (NS_FAILED(mCache->GetBuffer(prefName.get(), &buf, &size)))
            return;

        LOG("got: %s from the cache", nsDependentCString(buf, size).get());
        char* entry = strtok(buf, ";");
        while (entry) {
            nsCString faceList, filename, indexes;
            PRUint32 timestamp, fileSize;
            filename.Assign(entry);
            entry = strtok(NULL, ";");
            if (!entry)
                break;
            faceList.Assign(entry);
            entry = strtok(NULL, ";");
            if (!entry)
                break;
            char* endptr;
            timestamp = strtoul(entry, &endptr, 10);
            if (*endptr != '\0')
                break;
            entry = strtok(NULL, ";");
            if (!entry)
                break;
            fileSize = strtoul(entry, &endptr, 10);
            if (*endptr != '\0')
                break;
            entry = strtok(NULL, ";");
            if (!entry)
                break;
            indexes.Assign(entry);
            FNCMapEntry* mapEntry =
                static_cast<FNCMapEntry*>
                (PL_DHashTableOperate(&mMap, filename.get(), PL_DHASH_ADD));
            if (mapEntry) {
                mapEntry->mFilename = filename;
                mapEntry->mTimestamp = timestamp;
                mapEntry->mFilesize = fileSize;
                mapEntry->mFaces.AssignWithConversion(faceList);
                mapEntry->mIndexes = indexes;
            }
            entry = strtok(NULL, ";");
        }
        free(buf);
    }

    virtual void
    GetInfoForFile(nsCString& aFileName, nsAString& aFaceList,
                   PRUint32 *aTimestamp, PRUint32 *aFileSize,
                   IndexList &aIndexList)
    {
        if (!mMap.ops)
            return;
        PLDHashEntryHdr *hdr = PL_DHashTableOperate(&mMap, aFileName.get(), PL_DHASH_LOOKUP);
        if (!hdr)
            return;
        FNCMapEntry* entry = static_cast<FNCMapEntry*>(hdr);
        if (entry && entry->mTimestamp && entry->mFilesize) {
            *aTimestamp = entry->mTimestamp;
            *aFileSize = entry->mFilesize;
            char* indexes = const_cast<char*>(entry->mIndexes.get());
            char* endptr = indexes + 1;
            unsigned long index = strtoul(indexes, &endptr, 10);
            while (indexes < endptr && indexes[0] != '\0') {
                aIndexList.AppendElement(index);
                indexes = endptr + 1;
            }
            aFaceList.Assign(entry->mFaces);
        }
    }

    virtual void
    CacheFileInfo(nsCString& aFileName, nsAString& aFaceList,
                  PRUint32 aTimestamp, PRUint32 aFileSize,
                  IndexList &aIndexList)
    {
        if (!mMap.ops)
            return;
        FNCMapEntry* entry =
            static_cast<FNCMapEntry*>
            (PL_DHashTableOperate(&mMap, aFileName.get(), PL_DHASH_ADD));
        if (entry) {
            entry->mFilename = aFileName;
            entry->mTimestamp = aTimestamp;
            entry->mFilesize = aFileSize;
            entry->mFaces.Assign(aFaceList);
            for (PRUint32 i = 0; i < aIndexList.Length(); i++) {
                entry->mIndexes.AppendInt(aIndexList[i]);
                entry->mIndexes.Append(",");
            }
        }
        mWriteNeeded = PR_TRUE;
    }
    ~FontNameCache() {
        if (!mMap.ops)
            return;

        if (!mWriteNeeded || !mCache) {
            PL_DHashTableFinish(&mMap);
            return;
        }

        nsCAutoString buf;
        PL_DHashTableEnumerate(&mMap, WriteOutMap, &buf);
        PL_DHashTableFinish(&mMap);
        nsCAutoString prefName("font.cache");
        mCache->PutBuffer(prefName.get(), buf.get(), buf.Length() + 1);
    }
private:
    mozilla::scache::StartupCache* mCache;
    PLDHashTable mMap;
    PRBool mWriteNeeded;

    static PLDHashOperator WriteOutMap(PLDHashTable *aTable,
                                       PLDHashEntryHdr *aHdr,
                                       PRUint32 aNumber, void *aData)
    {
        nsCAutoString* buf = (nsCAutoString*)aData;
        FNCMapEntry* entry = static_cast<FNCMapEntry*>(aHdr);

        buf->Append(entry->mFilename);
        buf->Append(";");
        buf->AppendWithConversion(entry->mFaces);
        buf->Append(";");
        buf->AppendInt(entry->mTimestamp);
        buf->Append(";");
        buf->AppendInt(entry->mFilesize);
        buf->Append(";");
        buf->Append(entry->mIndexes);
        buf->Append(";");

        return PL_DHASH_NEXT;
    }

    typedef struct : public PLDHashEntryHdr {
    public:
        nsCString mFilename;
        PRUint32 mTimestamp;
        PRUint32 mFilesize;
        nsString mFaces;
        nsCString mIndexes;
    } FNCMapEntry;


    static PLDHashNumber StringHash(PLDHashTable *table, const void *key) {
        PLDHashNumber h = 0;
        for (const char *s = reinterpret_cast<const char*>(key); *s; ++s)
            h = PR_ROTATE_LEFT32(h, 4) ^ NS_ToLower(*s);
        return h;
    }

    static PRBool HashMatchEntry(PLDHashTable *table,
                                 const PLDHashEntryHdr *aHdr, const void *key)
    {
        const FNCMapEntry* entry =
            static_cast<const FNCMapEntry*>(aHdr);
        return entry->mFilename.Equals((char*)key);
    }

    static void MoveEntry(PLDHashTable *table, const PLDHashEntryHdr *aFrom,
                          PLDHashEntryHdr *aTo)
    {
        FNCMapEntry* to =
            static_cast<FNCMapEntry*>(aTo);
        const FNCMapEntry* from =
            static_cast<const FNCMapEntry*>(aFrom);
        to->mFilename.Assign(from->mFilename);
        to->mTimestamp = from->mTimestamp;
        to->mFilesize = from->mFilesize;
        to->mFaces.Assign(from->mFaces);
        to->mIndexes.Assign(from->mIndexes);
    }

};

void
gfxAndroidPlatform::AppendFacesFromFontFile(const char *aFileName, FontNameCache* aFontCache, InfallibleTArray<FontListEntry>* aFontList)
{
    nsString faceList;
    PRUint32 timestamp = 0;
    PRUint32 filesize = 0;
    FontNameCache::IndexList indexList;
    nsCString fileName(aFileName);
    if (aFontCache)
        aFontCache->GetInfoForFile(fileName, faceList, &timestamp, &filesize, indexList);
    struct stat s;
    int stat_ret = stat(aFileName, &s);
    if (!faceList.IsEmpty() && indexList.Length() && 0 == stat_ret &&
        s.st_mtime == timestamp && s.st_size == filesize) {
        PRInt32 beginning = 0;
        PRInt32 end = faceList.Find(",", PR_TRUE, beginning, -1);
        for (PRUint32 i = 0; i < indexList.Length() && end != kNotFound; i++) {
            nsDependentSubstring name(faceList, beginning, end);
            ToLowerCase(name);
            FontListEntry fle(NS_ConvertUTF16toUTF8(name), fileName,
                              indexList[i]);
            aFontList->AppendElement(fle);
            beginning = end + 1;
            end = faceList.Find(",", PR_TRUE, beginning, -1);
        }
        return;
    }

    faceList.AssignLiteral("");
    timestamp = s.st_mtime;
    filesize = s.st_size;
    FT_Face dummy;
    if (FT_Err_Ok == FT_New_Face(GetFTLibrary(), aFileName, -1, &dummy)) {
        for (FT_Long i = 0; i < dummy->num_faces; i++) {
            FT_Face face;
            if (FT_Err_Ok != FT_New_Face(GetFTLibrary(), aFileName,
                                         i, &face))
                continue;
            nsDependentCString name(face->family_name);
            ToLowerCase(name);

            nsRefPtr<FontFamily> ff;
            faceList.AppendWithConversion(name);
            faceList.AppendLiteral(",");
            indexList.AppendElement(i);
            ToLowerCase(name);
            FontListEntry fle(name, fileName, i);
            aFontList->AppendElement(fle);
        }
        FT_Done_Face(dummy);
        if (aFontCache && 0 == stat_ret)
            aFontCache->CacheFileInfo(fileName, faceList, timestamp, filesize, indexList);
    }
}

void
gfxAndroidPlatform::FindFontsInDirectory(const nsCString& aFontsDir,
                                         FontNameCache* aFontCache)
{
    DIR *d = opendir(aFontsDir.get());
    struct dirent *ent = NULL;
    while(d && (ent = readdir(d)) != NULL) {
        int namelen = strlen(ent->d_name);
        if (namelen > 4 &&
            strcasecmp(ent->d_name + namelen - 4, ".ttf") == 0)
        {
            nsCString s(aFontsDir);
            s.Append(nsDependentCString(ent->d_name));

            AppendFacesFromFontFile(nsPromiseFlatCString(s).get(),
                                    aFontCache, &mFontList);
        }
    }
    closedir(d);
}

void
gfxAndroidPlatform::GetFontList(InfallibleTArray<FontListEntry>* retValue)
{
#ifdef MOZ_IPC
    if (XRE_GetProcessType() != GeckoProcessType_Default) {
        mozilla::dom::ContentChild::GetSingleton()->SendReadFontList(retValue);
        return;
    }
#endif

    if (mFontList.Length() > 0) {
        *retValue = mFontList;
        return;
    }

    // ANDROID_ROOT is the root of the android system, typically /system
    // font files are in /$ANDROID_ROOT/fonts/
    FontNameCache fnc;
    FindFontsInDirectory(NS_LITERAL_CSTRING("/system/fonts/"), &fnc);
    char *androidRoot = PR_GetEnv("ANDROID_ROOT");
    if (androidRoot && strcmp(androidRoot, "/system")) {
        nsCString root(androidRoot);
        root.Append("/fonts/");
        FindFontsInDirectory(root, &fnc);
    }

    *retValue = mFontList;
}

nsresult
gfxAndroidPlatform::UpdateFontList()
{
    gfxFontCache *fc = gfxFontCache::GetCache();
    if (fc)
        fc->AgeAllGenerations();
    mFonts.Clear();
    mFontAliases.Clear();
    mFontSubstitutes.Clear();
    mPrefFonts.Clear();
    mCodepointsWithNoFonts.reset();

    InfallibleTArray<FontListEntry> fontList;
    GetFontList(&fontList);
    for (PRUint32 i = 0; i < fontList.Length(); i++) {
        NS_ConvertUTF8toUTF16 name(fontList[i].familyName());
        nsRefPtr<FontFamily> ff;
        if (!mFonts.Get(name, &ff)) {
            ff = new FontFamily(name);
            mFonts.Put(name, ff);
        }
        ff->AddFontFileAndIndex(fontList[i].filepath(), fontList[i].index());
    }

    // initialize the cmap loading process after font list has been initialized
    //StartLoader(kDelayBeforeLoadingCmaps, kIntervalBetweenLoadingCmaps);

    // initialize ranges of characters for which system-wide font search should be skipped
    mCodepointsWithNoFonts.SetRange(0,0x1f);     // C0 controls
    mCodepointsWithNoFonts.SetRange(0x7f,0x9f);  // C1 controls

    return NS_OK;
}

nsresult
gfxAndroidPlatform::ResolveFontName(const nsAString& aFontName,
                                    FontResolverCallback aCallback,
                                    void *aClosure,
                                    PRBool& aAborted)
{
    if (aFontName.IsEmpty())
        return NS_ERROR_FAILURE;

    nsAutoString resolvedName;
    gfxPlatformFontList* platformFontList = gfxPlatformFontList::PlatformFontList();
    if (platformFontList) {
        if (!platformFontList->ResolveFontName(aFontName, resolvedName)) {
            aAborted = PR_FALSE;
            return NS_OK;
        }
    }

    nsAutoString keyName(aFontName);
    ToLowerCase(keyName);

    nsRefPtr<FontFamily> ff;
    if (mFonts.Get(keyName, &ff) ||
        mFontSubstitutes.Get(keyName, &ff) ||
        mFontAliases.Get(keyName, &ff))
    {
        aAborted = !(*aCallback)(ff->Name(), aClosure);
    } else {
        aAborted = PR_FALSE;
    }

    return NS_OK;
}

static PRBool SimpleResolverCallback(const nsAString& aName, void* aClosure)
{
    nsString *result = static_cast<nsString*>(aClosure);
    result->Assign(aName);
    return PR_FALSE;
}

nsresult
gfxAndroidPlatform::GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName)
{
    aFamilyName.Truncate();
    PRBool aborted;
    return ResolveFontName(aFontName, SimpleResolverCallback, &aFamilyName, aborted);
}

gfxPlatformFontList*
gfxAndroidPlatform::CreatePlatformFontList()
{
    gfxPlatformFontList* list = new gfxFT2FontList();
    if (NS_SUCCEEDED(list->InitFontList())) {
        return list;
    }
    gfxPlatformFontList::Shutdown();
    return nsnull;
}

PRBool
gfxAndroidPlatform::IsFontFormatSupported(nsIURI *aFontURI, PRUint32 aFormatFlags)
{
    // check for strange format flags
    NS_ASSERTION(!(aFormatFlags & gfxUserFontSet::FLAG_FORMAT_NOT_USED),
                 "strange font format hint set");

    // accept supported formats
    if (aFormatFlags & (gfxUserFontSet::FLAG_FORMAT_OPENTYPE |
                        gfxUserFontSet::FLAG_FORMAT_WOFF |
                        gfxUserFontSet::FLAG_FORMAT_TRUETYPE)) {
        return PR_TRUE;
    }

    // reject all other formats, known and unknown
    if (aFormatFlags != 0) {
        return PR_FALSE;
    }

    // no format hint set, need to look at data
    return PR_TRUE;
}

gfxFontGroup *
gfxAndroidPlatform::CreateFontGroup(const nsAString &aFamilies,
                               const gfxFontStyle *aStyle,
                               gfxUserFontSet* aUserFontSet)
{
    return new gfxFT2FontGroup(aFamilies, aStyle, aUserFontSet);
}

FT_Library
gfxAndroidPlatform::GetFTLibrary()
{
    return gPlatformFTLibrary;
}

FontFamily *
gfxAndroidPlatform::FindFontFamily(const nsAString& aName)
{
    nsAutoString name(aName);
    ToLowerCase(name);

    nsRefPtr<FontFamily> ff;
    if (!mFonts.Get(name, &ff) &&
        !mFontSubstitutes.Get(name, &ff) &&
        !mFontAliases.Get(name, &ff)) {
        return nsnull;
    }
    return ff.get();
}

FontEntry *
gfxAndroidPlatform::FindFontEntry(const nsAString& aName, const gfxFontStyle& aFontStyle)
{
    nsRefPtr<FontFamily> ff = FindFontFamily(aName);
    if (!ff)
        return nsnull;

    return ff->FindFontEntry(aFontStyle);
}

static PLDHashOperator
FindFontForCharProc(nsStringHashKey::KeyType aKey,
                    nsRefPtr<FontFamily>& aFontFamily,
                    void* aUserArg)
{
    FontSearch *data = (FontSearch*)aUserArg;
    aFontFamily->FindFontForChar(data);
    return PL_DHASH_NEXT;
}

already_AddRefed<gfxFont>
gfxAndroidPlatform::FindFontForChar(PRUint32 aCh, gfxFont *aFont)
{
    // is codepoint with no matching font? return null immediately
    if (mCodepointsWithNoFonts.test(aCh)) {
        return nsnull;
    }

    FontSearch data(aCh, aFont);

    // find fonts that support the character
    mFonts.Enumerate(FindFontForCharProc, &data);

    if (data.mBestMatch) {
        nsRefPtr<gfxFT2Font> font =
            gfxFT2Font::GetOrMakeFont(static_cast<FontEntry*>(data.mBestMatch.get()), 
                                      aFont->GetStyle()); 
        gfxFont* ret = font.forget().get();
        return already_AddRefed<gfxFont>(ret);
    }

    // no match? add to set of non-matching codepoints
    mCodepointsWithNoFonts.set(aCh);

    return nsnull;
}

gfxFontEntry* 
gfxAndroidPlatform::MakePlatformFont(const gfxProxyFontEntry *aProxyEntry,
                                     const PRUint8 *aFontData, PRUint32 aLength)
{
    return gfxPlatformFontList::PlatformFontList()->MakePlatformFont(aProxyEntry,
                                                                     aFontData,
                                                                     aLength);
}

PRBool
gfxAndroidPlatform::GetPrefFontEntries(const nsCString& aKey, nsTArray<nsRefPtr<gfxFontEntry> > *aFontEntryList)
{
    return mPrefFonts.Get(aKey, aFontEntryList);
}

void
gfxAndroidPlatform::SetPrefFontEntries(const nsCString& aKey, nsTArray<nsRefPtr<gfxFontEntry> >& aFontEntryList)
{
    mPrefFonts.Put(aKey, aFontEntryList);
}
