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

#include "mozilla/Util.h"

#if defined(MOZ_WIDGET_GTK2)
#include "gfxPlatformGtk.h"
#define gfxToolkitPlatform gfxPlatformGtk
#elif defined(MOZ_WIDGET_QT)
#include <qfontinfo.h>
#include "gfxQtPlatform.h"
#define gfxToolkitPlatform gfxQtPlatform
#elif defined(XP_WIN)
#include "gfxWindowsPlatform.h"
#define gfxToolkitPlatform gfxWindowsPlatform
#elif defined(ANDROID)
#include "mozilla/dom/ContentChild.h"
#include "gfxAndroidPlatform.h"
#define gfxToolkitPlatform gfxAndroidPlatform
#endif

#ifdef ANDROID
#include "nsXULAppAPI.h"
#include <dirent.h>
#include <android/log.h>
#define ALOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Gecko" , ## args)
#endif

#include "ft2build.h"
#include FT_FREETYPE_H
#include FT_TRUETYPE_TAGS_H
#include FT_TRUETYPE_TABLES_H
#include "cairo-ft.h"

#include "gfxFT2FontList.h"
#include "gfxFT2Fonts.h"
#include "gfxUserFontSet.h"
#include "gfxFontUtils.h"

#include "nsServiceManagerUtils.h"
#include "nsTArray.h"
#include "nsUnicharUtils.h"

#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsISimpleEnumerator.h"

#include "mozilla/scache/StartupCache.h"
#include <sys/stat.h>

#ifdef XP_WIN
#include "nsIWindowsRegKey.h"
#include <windows.h>
#endif

using namespace mozilla;

#ifdef PR_LOGGING
static PRLogModuleInfo *gFontInfoLog = PR_NewLogModule("fontInfoLog");
#endif /* PR_LOGGING */

#undef LOG
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

/*
 * FT2FontEntry
 * gfxFontEntry subclass corresponding to a specific face that can be
 * rendered by freetype. This is associated with a face index in a
 * file (normally a .ttf/.otf file holding a single face, but in principle
 * there could be .ttc files with multiple faces).
 * The FT2FontEntry can create the necessary FT_Face on demand, and can
 * then create a Cairo font_face and scaled_font for drawing.
 */

cairo_scaled_font_t *
FT2FontEntry::CreateScaledFont(const gfxFontStyle *aStyle)
{
    cairo_scaled_font_t *scaledFont = NULL;

    cairo_matrix_t sizeMatrix;
    cairo_matrix_t identityMatrix;

    // XXX deal with adjusted size
    cairo_matrix_init_scale(&sizeMatrix, aStyle->size, aStyle->size);
    cairo_matrix_init_identity(&identityMatrix);

    // synthetic oblique by skewing via the font matrix
    bool needsOblique = !IsItalic() &&
            (aStyle->style & (NS_FONT_STYLE_ITALIC | NS_FONT_STYLE_OBLIQUE));

    if (needsOblique) {
        const double kSkewFactor = 0.25;

        cairo_matrix_t style;
        cairo_matrix_init(&style,
                          1,                //xx
                          0,                //yx
                          -1 * kSkewFactor,  //xy
                          1,                //yy
                          0,                //x0
                          0);               //y0
        cairo_matrix_multiply(&sizeMatrix, &sizeMatrix, &style);
    }

    cairo_font_options_t *fontOptions = cairo_font_options_create();

    if (!gfxPlatform::GetPlatform()->FontHintingEnabled()) {
        cairo_font_options_set_hint_metrics(fontOptions, CAIRO_HINT_METRICS_OFF);
    }

    scaledFont = cairo_scaled_font_create(CairoFontFace(),
                                          &sizeMatrix,
                                          &identityMatrix, fontOptions);
    cairo_font_options_destroy(fontOptions);

    NS_ASSERTION(cairo_scaled_font_status(scaledFont) == CAIRO_STATUS_SUCCESS,
                 "Failed to make scaled font");

    return scaledFont;
}

FT2FontEntry::~FT2FontEntry()
{
    // Do nothing for mFTFace here since FTFontDestroyFunc is called by cairo.
    mFTFace = nsnull;

#ifndef ANDROID
    if (mFontFace) {
        cairo_font_face_destroy(mFontFace);
        mFontFace = nsnull;
    }
#endif
}

gfxFont*
FT2FontEntry::CreateFontInstance(const gfxFontStyle *aFontStyle, bool aNeedsBold)
{
    cairo_scaled_font_t *scaledFont = CreateScaledFont(aFontStyle);
    gfxFont *font = new gfxFT2Font(scaledFont, this, aFontStyle, aNeedsBold);
    cairo_scaled_font_destroy(scaledFont);
    return font;
}

/* static */
FT2FontEntry*
FT2FontEntry::CreateFontEntry(const gfxProxyFontEntry &aProxyEntry,
                              const PRUint8 *aFontData,
                              PRUint32 aLength)
{
    // Ownership of aFontData is passed in here; the fontEntry must
    // retain it as long as the FT_Face needs it, and ensure it is
    // eventually deleted.
    FT_Face face;
    FT_Error error =
        FT_New_Memory_Face(gfxToolkitPlatform::GetPlatform()->GetFTLibrary(),
                           aFontData, aLength, 0, &face);
    if (error != FT_Err_Ok) {
        NS_Free((void*)aFontData);
        return nsnull;
    }
    FT2FontEntry* fe = FT2FontEntry::CreateFontEntry(face, nsnull, 0, aFontData);
    if (fe) {
        fe->mItalic = aProxyEntry.mItalic;
        fe->mWeight = aProxyEntry.mWeight;
        fe->mStretch = aProxyEntry.mStretch;
    }
    return fe;
}

class FTUserFontData {
public:
    FTUserFontData(FT_Face aFace, const PRUint8* aData)
        : mFace(aFace), mFontData(aData)
    {
    }

    ~FTUserFontData()
    {
        FT_Done_Face(mFace);
        if (mFontData) {
            NS_Free((void*)mFontData);
        }
    }

private:
    FT_Face        mFace;
    const PRUint8 *mFontData;
};

static void
FTFontDestroyFunc(void *data)
{
    FTUserFontData *userFontData = static_cast<FTUserFontData*>(data);
    delete userFontData;
}

/* static */
FT2FontEntry*
FT2FontEntry::CreateFontEntry(const FontListEntry& aFLE)
{
    FT2FontEntry *fe = new FT2FontEntry(aFLE.faceName());
    fe->mFilename = aFLE.filepath();
    fe->mFTFontIndex = aFLE.index();
    fe->mWeight = aFLE.weight();
    fe->mStretch = aFLE.stretch();
    fe->mItalic = aFLE.italic();
    return fe;
}

/* static */
FT2FontEntry*
FT2FontEntry::CreateFontEntry(FT_Face aFace,
                              const char* aFilename, PRUint8 aIndex,
                              const PRUint8 *aFontData)
{
    static cairo_user_data_key_t key;

    if (!aFace->family_name) {
        FT_Done_Face(aFace);
        return nsnull;
    }
    // Construct font name from family name and style name, regular fonts
    // do not have the modifier by convention.
    NS_ConvertUTF8toUTF16 fontName(aFace->family_name);
    if (aFace->style_name && strcmp("Regular", aFace->style_name)) {
        fontName.AppendLiteral(" ");
        AppendUTF8toUTF16(aFace->style_name, fontName);
    }
    FT2FontEntry *fe = new FT2FontEntry(fontName);
    fe->mItalic = aFace->style_flags & FT_STYLE_FLAG_ITALIC;
    fe->mFTFace = aFace;
    int flags = gfxPlatform::GetPlatform()->FontHintingEnabled() ?
                FT_LOAD_DEFAULT :
                (FT_LOAD_NO_AUTOHINT | FT_LOAD_NO_HINTING);
    fe->mFontFace = cairo_ft_font_face_create_for_ft_face(aFace, flags);
    fe->mFilename = aFilename;
    fe->mFTFontIndex = aIndex;
    FTUserFontData *userFontData = new FTUserFontData(aFace, aFontData);
    cairo_font_face_set_user_data(fe->mFontFace, &key,
                                  userFontData, FTFontDestroyFunc);

    TT_OS2 *os2 = static_cast<TT_OS2*>(FT_Get_Sfnt_Table(aFace, ft_sfnt_os2));
    PRUint16 os2weight = 0;
    if (os2 && os2->version != 0xffff) {
        // Technically, only 100 to 900 are valid, but some fonts
        // have this set wrong -- e.g. "Microsoft Logo Bold Italic" has
        // it set to 6 instead of 600.  We try to be nice and handle that
        // as well.
        if (os2->usWeightClass >= 100 && os2->usWeightClass <= 900)
            os2weight = os2->usWeightClass;
        else if (os2->usWeightClass >= 1 && os2->usWeightClass <= 9)
            os2weight = os2->usWeightClass * 100;
    }

    if (os2weight != 0)
        fe->mWeight = os2weight;
    else if (aFace->style_flags & FT_STYLE_FLAG_BOLD)
        fe->mWeight = 700;
    else
        fe->mWeight = 400;

    NS_ASSERTION(fe->mWeight >= 100 && fe->mWeight <= 900, "Invalid final weight in font!");

    return fe;
}

FT2FontEntry*
gfxFT2Font::GetFontEntry()
{
    return static_cast<FT2FontEntry*> (mFontEntry.get());
}

cairo_font_face_t *
FT2FontEntry::CairoFontFace()
{
    static cairo_user_data_key_t key;

    if (!mFontFace) {
        FT_Face face;
        FT_New_Face(gfxToolkitPlatform::GetPlatform()->GetFTLibrary(), mFilename.get(), mFTFontIndex, &face);
        mFTFace = face;
        int flags = gfxPlatform::GetPlatform()->FontHintingEnabled() ?
                    FT_LOAD_DEFAULT :
                    (FT_LOAD_NO_AUTOHINT | FT_LOAD_NO_HINTING);
        mFontFace = cairo_ft_font_face_create_for_ft_face(face, flags);
        FTUserFontData *userFontData = new FTUserFontData(face, nsnull);
        cairo_font_face_set_user_data(mFontFace, &key,
                                      userFontData, FTFontDestroyFunc);
    }
    return mFontFace;
}

nsresult
FT2FontEntry::ReadCMAP()
{
    if (mCmapInitialized) {
        return NS_OK;
    }

    // attempt this once, if errors occur leave a blank cmap
    mCmapInitialized = true;

    AutoFallibleTArray<PRUint8,16384> buffer;
    nsresult rv = GetFontTable(TTAG_cmap, buffer);
    
    if (NS_SUCCEEDED(rv)) {
        bool unicodeFont;
        bool symbolFont;
        rv = gfxFontUtils::ReadCMAP(buffer.Elements(), buffer.Length(),
                                    mCharacterMap, mUVSOffset,
                                    unicodeFont, symbolFont);
    }

    mHasCmapTable = NS_SUCCEEDED(rv);
    return rv;
}

nsresult
FT2FontEntry::GetFontTable(PRUint32 aTableTag,
                           FallibleTArray<PRUint8>& aBuffer)
{
    // Ensure existence of mFTFace
    CairoFontFace();
    NS_ENSURE_TRUE(mFTFace, NS_ERROR_FAILURE);

    FT_Error status;
    FT_ULong len = 0;
    status = FT_Load_Sfnt_Table(mFTFace, aTableTag, 0, nsnull, &len);
    NS_ENSURE_TRUE(status == 0, NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(len != 0, NS_ERROR_FAILURE);

    if (!aBuffer.SetLength(len)) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    PRUint8 *buf = aBuffer.Elements();
    status = FT_Load_Sfnt_Table(mFTFace, aTableTag, 0, buf, &len);
    NS_ENSURE_TRUE(status == 0, NS_ERROR_FAILURE);

    return NS_OK;
}

void
FT2FontEntry::SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf,
                                  FontListSizes*    aSizes) const
{
    gfxFontEntry::SizeOfExcludingThis(aMallocSizeOf, aSizes);
    aSizes->mFontListSize +=
        mFilename.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
}

void
FT2FontEntry::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf,
                                  FontListSizes*    aSizes) const
{
    aSizes->mFontListSize += aMallocSizeOf(this);
    SizeOfExcludingThis(aMallocSizeOf, aSizes);
}

/*
 * FT2FontFamily
 * A standard gfxFontFamily; just adds a method used to support sending
 * the font list from chrome to content via IPC.
 */

void
FT2FontFamily::AddFacesToFontList(InfallibleTArray<FontListEntry>* aFontList)
{
    for (int i = 0, n = mAvailableFonts.Length(); i < n; ++i) {
        const FT2FontEntry *fe =
            static_cast<const FT2FontEntry*>(mAvailableFonts[i].get());
        if (!fe) {
            continue;
        }
        
        aFontList->AppendElement(FontListEntry(Name(), fe->Name(),
                                               fe->mFilename,
                                               fe->Weight(), fe->Stretch(),
                                               fe->IsItalic(),
                                               fe->mFTFontIndex));
    }
}

/*
 * Startup cache support for the font list:
 * We store the list of families and faces, with their style attributes and the
 * corresponding font files, in the startup cache.
 * This allows us to recreate the gfxFT2FontList collection of families and
 * faces without instantiating Freetype faces for each font file (in order to
 * find their attributes), leading to significantly quicker startup.
 */

#define CACHE_KEY "font.cached-list"

class FontNameCache {
public:
    FontNameCache()
        : mWriteNeeded(false)
    {
        mOps = (PLDHashTableOps) {
            PL_DHashAllocTable,
            PL_DHashFreeTable,
            StringHash,
            HashMatchEntry,
            MoveEntry,
            PL_DHashClearEntryStub,
            PL_DHashFinalizeStub,
            NULL
        };

        if (!PL_DHashTableInit(&mMap, &mOps, nsnull,
                               sizeof(FNCMapEntry), 0))
        {
            mMap.ops = nsnull;
            LOG(("initializing the map failed"));
        }

        NS_ABORT_IF_FALSE(XRE_GetProcessType() == GeckoProcessType_Default,
                          "StartupCacheFontNameCache should only be used in chrome process");
        mCache = mozilla::scache::StartupCache::GetSingleton();

        Init();
    }

    ~FontNameCache()
    {
        if (!mMap.ops) {
            return;
        }
        if (!mWriteNeeded || !mCache) {
            PL_DHashTableFinish(&mMap);
            return;
        }

        nsCAutoString buf;
        PL_DHashTableEnumerate(&mMap, WriteOutMap, &buf);
        PL_DHashTableFinish(&mMap);
        mCache->PutBuffer(CACHE_KEY, buf.get(), buf.Length() + 1);
    }

    void Init()
    {
        if (!mMap.ops || !mCache) {
            return;
        }
        PRUint32 size;
        char* buf;
        if (NS_FAILED(mCache->GetBuffer(CACHE_KEY, &buf, &size))) {
            return;
        }

        LOG(("got: %s from the cache", nsDependentCString(buf, size).get()));

        const char* beginning = buf;
        const char* end = strchr(beginning, ';');
        while (end) {
            nsCString filename(beginning, end - beginning);
            beginning = end + 1;
            if (!(end = strchr(beginning, ';'))) {
                break;
            }
            nsCString faceList(beginning, end - beginning);
            beginning = end + 1;
            if (!(end = strchr(beginning, ';'))) {
                break;
            }
            PRUint32 timestamp = strtoul(beginning, NULL, 10);
            beginning = end + 1;
            if (!(end = strchr(beginning, ';'))) {
                break;
            }
            PRUint32 filesize = strtoul(beginning, NULL, 10);

            FNCMapEntry* mapEntry =
                static_cast<FNCMapEntry*>
                (PL_DHashTableOperate(&mMap, filename.get(), PL_DHASH_ADD));
            if (mapEntry) {
                mapEntry->mFilename.Assign(filename);
                mapEntry->mTimestamp = timestamp;
                mapEntry->mFilesize = filesize;
                mapEntry->mFaces.Assign(faceList);
                // entries from the startupcache are marked "non-existing"
                // until we have confirmed that the file still exists
                mapEntry->mFileExists = false;
            }

            beginning = end + 1;
            end = strchr(beginning, ';');
        }

        // Should we use free() or delete[] here? See bug 684700.
        free(buf);
    }

    virtual void
    GetInfoForFile(nsCString& aFileName, nsCString& aFaceList,
                   PRUint32 *aTimestamp, PRUint32 *aFilesize)
    {
        if (!mMap.ops) {
            return;
        }
        PLDHashEntryHdr *hdr =
            PL_DHashTableOperate(&mMap, aFileName.get(), PL_DHASH_LOOKUP);
        if (!hdr) {
            return;
        }
        FNCMapEntry* entry = static_cast<FNCMapEntry*>(hdr);
        if (entry && entry->mTimestamp && entry->mFilesize) {
            *aTimestamp = entry->mTimestamp;
            *aFilesize = entry->mFilesize;
            aFaceList.Assign(entry->mFaces);
            // this entry does correspond to an existing file
            // (although it might not be up-to-date, in which case
            // it will get overwritten via CacheFileInfo)
            entry->mFileExists = true;
        }
    }

    virtual void
    CacheFileInfo(nsCString& aFileName, nsCString& aFaceList,
                  PRUint32 aTimestamp, PRUint32 aFilesize)
    {
        if (!mMap.ops) {
            return;
        }
        FNCMapEntry* entry =
            static_cast<FNCMapEntry*>
            (PL_DHashTableOperate(&mMap, aFileName.get(), PL_DHASH_ADD));
        if (entry) {
            entry->mFilename.Assign(aFileName);
            entry->mTimestamp = aTimestamp;
            entry->mFilesize = aFilesize;
            entry->mFaces.Assign(aFaceList);
            entry->mFileExists = true;
        }
        mWriteNeeded = true;
    }

private:
    mozilla::scache::StartupCache* mCache;
    PLDHashTable mMap;
    bool mWriteNeeded;

    PLDHashTableOps mOps;

    static PLDHashOperator WriteOutMap(PLDHashTable *aTable,
                                       PLDHashEntryHdr *aHdr,
                                       PRUint32 aNumber, void *aData)
    {
        FNCMapEntry* entry = static_cast<FNCMapEntry*>(aHdr);
        if (!entry->mFileExists) {
            // skip writing entries for files that are no longer present
            return PL_DHASH_NEXT;
        }

        nsCAutoString* buf = reinterpret_cast<nsCAutoString*>(aData);
        buf->Append(entry->mFilename);
        buf->Append(';');
        buf->Append(entry->mFaces);
        buf->Append(';');
        buf->AppendInt(entry->mTimestamp);
        buf->Append(';');
        buf->AppendInt(entry->mFilesize);
        buf->Append(';');
        return PL_DHASH_NEXT;
    }

    typedef struct : public PLDHashEntryHdr {
    public:
        nsCString mFilename;
        PRUint32  mTimestamp;
        PRUint32  mFilesize;
        nsCString mFaces;
        bool      mFileExists;
    } FNCMapEntry;

    static PLDHashNumber StringHash(PLDHashTable *table, const void *key)
    {
        return HashString(reinterpret_cast<const char*>(key));
    }

    static bool HashMatchEntry(PLDHashTable *table,
                                 const PLDHashEntryHdr *aHdr, const void *key)
    {
        const FNCMapEntry* entry =
            static_cast<const FNCMapEntry*>(aHdr);
        return entry->mFilename.Equals(reinterpret_cast<const char*>(key));
    }

    static void MoveEntry(PLDHashTable *table, const PLDHashEntryHdr *aFrom,
                          PLDHashEntryHdr *aTo)
    {
        FNCMapEntry* to = static_cast<FNCMapEntry*>(aTo);
        const FNCMapEntry* from = static_cast<const FNCMapEntry*>(aFrom);
        to->mFilename.Assign(from->mFilename);
        to->mTimestamp = from->mTimestamp;
        to->mFilesize = from->mFilesize;
        to->mFaces.Assign(from->mFaces);
        to->mFileExists = from->mFileExists;
    }
};

/***************************************************************
 *
 * gfxFT2FontList
 *
 */

// For Mobile, we use gfxFT2Fonts, and we build the font list by directly
// scanning the system's Fonts directory for OpenType and TrueType files.

gfxFT2FontList::gfxFT2FontList()
{
}

void
gfxFT2FontList::AppendFacesFromCachedFaceList(nsCString& aFileName,
                                              bool aStdFile,
                                              nsCString& aFaceList)
{
    const char *beginning = aFaceList.get();
    const char *end = strchr(beginning, ',');
    while (end) {
        nsString familyName =
            NS_ConvertUTF8toUTF16(beginning, end - beginning);
        ToLowerCase(familyName);
        beginning = end + 1;
        if (!(end = strchr(beginning, ','))) {
            break;
        }
        nsString faceName =
            NS_ConvertUTF8toUTF16(beginning, end - beginning);
        beginning = end + 1;
        if (!(end = strchr(beginning, ','))) {
            break;
        }
        PRUint32 index = strtoul(beginning, NULL, 10);
        beginning = end + 1;
        if (!(end = strchr(beginning, ','))) {
            break;
        }
        bool italic = (*beginning != '0');
        beginning = end + 1;
        if (!(end = strchr(beginning, ','))) {
            break;
        }
        PRUint32 weight = strtoul(beginning, NULL, 10);
        beginning = end + 1;
        if (!(end = strchr(beginning, ','))) {
            break;
        }
        PRInt32 stretch = strtol(beginning, NULL, 10);

        FontListEntry fle(familyName, faceName, aFileName,
                          weight, stretch, italic, index);
        AppendFaceFromFontListEntry(fle, aStdFile);

        beginning = end + 1;
        end = strchr(beginning, ',');
    }
}

static void
AppendToFaceList(nsCString& aFaceList,
                 nsAString& aFamilyName, FT2FontEntry* aFontEntry)
{
    aFaceList.Append(NS_ConvertUTF16toUTF8(aFamilyName));
    aFaceList.Append(',');
    aFaceList.Append(NS_ConvertUTF16toUTF8(aFontEntry->Name()));
    aFaceList.Append(',');
    aFaceList.AppendInt(aFontEntry->mFTFontIndex);
    aFaceList.Append(',');
    aFaceList.Append(aFontEntry->IsItalic() ? '1' : '0');
    aFaceList.Append(',');
    aFaceList.AppendInt(aFontEntry->Weight());
    aFaceList.Append(',');
    aFaceList.AppendInt(aFontEntry->Stretch());
    aFaceList.Append(',');
}

void
gfxFT2FontList::AppendFacesFromFontFile(nsCString& aFileName,
                                        bool aStdFile,
                                        FontNameCache *aCache)
{
    nsCString faceList;
    PRUint32 filesize = 0, timestamp = 0;
    if (aCache) {
        aCache->GetInfoForFile(aFileName, faceList, &timestamp, &filesize);
    }

    struct stat s;
    int statRetval = stat(aFileName.get(), &s);
    if (!faceList.IsEmpty() && 0 == statRetval &&
        s.st_mtime == timestamp && s.st_size == filesize)
    {
        LOG(("using cached font info for %s", aFileName.get()));
        AppendFacesFromCachedFaceList(aFileName, aStdFile, faceList);
        return;
    }

#ifdef XP_WIN
    FT_Library ftLibrary = gfxWindowsPlatform::GetPlatform()->GetFTLibrary();
#elif defined(ANDROID)
    FT_Library ftLibrary = gfxAndroidPlatform::GetPlatform()->GetFTLibrary();
#endif
    FT_Face dummy;
    if (FT_Err_Ok == FT_New_Face(ftLibrary, aFileName.get(), -1, &dummy)) {
        LOG(("reading font info via FreeType for %s", aFileName.get()));
        nsCString faceList;
        timestamp = s.st_mtime;
        filesize = s.st_size;
        for (FT_Long i = 0; i < dummy->num_faces; i++) {
            FT_Face face;
            if (FT_Err_Ok != FT_New_Face(ftLibrary, aFileName.get(), i, &face)) {
                continue;
            }

            FT2FontEntry* fe =
                FT2FontEntry::CreateFontEntry(face, aFileName.get(), i);
            if (fe) {
                NS_ConvertUTF8toUTF16 name(face->family_name);
                BuildKeyNameFromFontName(name);       
                gfxFontFamily *family = mFontFamilies.GetWeak(name);
                if (!family) {
                    family = new FT2FontFamily(name);
                    mFontFamilies.Put(name, family);
                    if (mBadUnderlineFamilyNames.Contains(name)) {
                        family->SetBadUnderlineFamily();
                    }
                }
                fe->mStandardFace = aStdFile;
                family->AddFontEntry(fe);
                if (family->IsBadUnderlineFamily()) {
                    fe->mIsBadUnderlineFont = true;
                }

                // bug 721719 - set the IgnoreGSUB flag on entries for Roboto
                // because of unwanted on-by-default "ae" ligature.
                // (See also AppendFaceFromFontListEntry.)
                if (name.EqualsLiteral("roboto")) {
                    fe->mIgnoreGSUB = true;
                }

                AppendToFaceList(faceList, name, fe);
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
        if (aCache && 0 == statRetval && !faceList.IsEmpty()) {
            aCache->CacheFileInfo(aFileName, faceList, timestamp, filesize);
        }
    }
}

// Called on each family after all fonts are added to the list;
// this will sort faces to give priority to "standard" font files
// if aUserArg is non-null (i.e. we're using it as a boolean flag)
static PLDHashOperator
FinalizeFamilyMemberList(nsStringHashKey::KeyType aKey,
                         nsRefPtr<gfxFontFamily>& aFamily,
                         void* aUserArg)
{
    gfxFontFamily *family = aFamily.get();
    bool sortFaces = (aUserArg != nsnull);

    family->SetHasStyles(true);

    if (sortFaces) {
        family->SortAvailableFonts();
    }
    family->CheckForSimpleFamily();

    return PL_DHASH_NEXT;
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
            bool moreFiles = handle != INVALID_HANDLE_VALUE;
            while (moreFiles) {
                nsAutoString filePath(path);
                filePath.AppendLiteral("\\");
                filePath.Append(results.cFileName);
                AppendFacesFromFontFile(NS_ConvertUTF16toUTF8(filePath));
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

    mCodepointsWithNoFonts.SetRange(0,0x1f);     // C0 controls
    mCodepointsWithNoFonts.SetRange(0x7f,0x9f);  // C1 controls

    if (XRE_GetProcessType() != GeckoProcessType_Default) {
        // Content process: ask the Chrome process to give us the list
        InfallibleTArray<FontListEntry> fonts;
        mozilla::dom::ContentChild::GetSingleton()->SendReadFontList(&fonts);
        for (PRUint32 i = 0, n = fonts.Length(); i < n; ++i) {
            AppendFaceFromFontListEntry(fonts[i], false);
        }
        // Passing null for userdata tells Finalize that it does not need
        // to sort faces (because they were already sorted by chrome,
        // so we just maintain the existing order)
        mFontFamilies.Enumerate(FinalizeFamilyMemberList, nsnull);
        LOG(("got font list from chrome process: %d faces in %d families",
            fonts.Length(), mFontFamilies.Count()));
        return;
    }

    // Chrome process: get the cached list (if any)
    FontNameCache fnc;

    // ANDROID_ROOT is the root of the android system, typically /system;
    // font files are in /$ANDROID_ROOT/fonts/
    nsCString root;
    char *androidRoot = PR_GetEnv("ANDROID_ROOT");
    if (androidRoot) {
        root = androidRoot;
    } else {
        root = NS_LITERAL_CSTRING("/system");
    }
    root.Append("/fonts");

    static const char* sStandardFonts[] = {
        "DroidSans.ttf",
        "DroidSans-Bold.ttf",
        "DroidSerif-Regular.ttf",
        "DroidSerif-Bold.ttf",
        "DroidSerif-Italic.ttf",
        "DroidSerif-BoldItalic.ttf",
        "DroidSansMono.ttf",
        "DroidSansArabic.ttf",
        "DroidSansHebrew.ttf",
        "DroidSansThai.ttf",
        "MTLmr3m.ttf",
        "MTLc3m.ttf",
        "DroidSansJapanese.ttf",
        "DroidSansFallback.ttf"
    };

    DIR *d = opendir(root.get());
    if (!d) {
        // if we can't find/read the font directory, we are doomed!
        NS_RUNTIMEABORT("Could not read the system fonts directory");
    }

    struct dirent *ent = NULL;
    while ((ent = readdir(d)) != NULL) {
        int namelen = strlen(ent->d_name);
        if (namelen <= 4) {
            // cannot be a usable font filename
            continue;
        }
        const char *ext = ent->d_name + namelen - 4;
        if (strcasecmp(ext, ".ttf") == 0 ||
            strcasecmp(ext, ".otf") == 0 ||
            strcasecmp(ext, ".ttc") == 0)
        {
            bool isStdFont = false;
            for (unsigned int i = 0;
                 i < ArrayLength(sStandardFonts) && !isStdFont; i++)
            {
                isStdFont = strcmp(sStandardFonts[i], ent->d_name) == 0;
            }

            nsCString s(root);
            s.Append('/');
            s.Append(ent->d_name, namelen);

            // Add the face(s) from this file to our font list;
            // note that if we have cached info for this file in fnc,
            // and the file is unchanged, we won't actually need to read it.
            // If the file is new/changed, this will update the FontNameCache.
            AppendFacesFromFontFile(s, isStdFont, &fnc);
        }
    }
    closedir(d);

    // Finalize the families by sorting faces into standard order
    // and marking "simple" families.
    // Passing non-null userData here says that we want faces to be sorted.
    mFontFamilies.Enumerate(FinalizeFamilyMemberList, this);
#endif // XP_WIN && ANDROID
}

void
gfxFT2FontList::AppendFaceFromFontListEntry(const FontListEntry& aFLE,
                                            bool aStdFile)
{
    FT2FontEntry* fe = FT2FontEntry::CreateFontEntry(aFLE);
    if (fe) {
        fe->mStandardFace = aStdFile;
        nsAutoString name(aFLE.familyName());
        gfxFontFamily *family = mFontFamilies.GetWeak(name);
        if (!family) {
            family = new FT2FontFamily(name);
            mFontFamilies.Put(name, family);
            if (mBadUnderlineFamilyNames.Contains(name)) {
                family->SetBadUnderlineFamily();
            }
        }
        family->AddFontEntry(fe);
        if (family->IsBadUnderlineFamily()) {
            fe->mIsBadUnderlineFont = true;
        }

        // bug 721719 - set the IgnoreGSUB flag on entries for Roboto
        // because of unwanted on-by-default "ae" ligature.
        // This totally sucks, but if we don't hack around these broken fonts
        // we get really bad text rendering, which we can't inflict on users. :(
        // If we accumulate a few more examples of this stuff, it'll be time
        // to create some prefs for the list of fonts where we need to ignore
        // layout tables. Sigh.
        if (name.EqualsLiteral("roboto")) {
            fe->mIgnoreGSUB = true;
        }
    }
}

static PLDHashOperator
AddFamilyToFontList(nsStringHashKey::KeyType aKey,
                    nsRefPtr<gfxFontFamily>& aFamily,
                    void* aUserArg)
{
    InfallibleTArray<FontListEntry>* fontlist =
        reinterpret_cast<InfallibleTArray<FontListEntry>*>(aUserArg);

    FT2FontFamily *family = static_cast<FT2FontFamily*>(aFamily.get());
    family->AddFacesToFontList(fontlist);

    return PL_DHASH_NEXT;
}

void
gfxFT2FontList::GetFontList(InfallibleTArray<FontListEntry>* retValue)
{
    mFontFamilies.Enumerate(AddFamilyToFontList, retValue);
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
gfxFT2FontList::GetDefaultFont(const gfxFontStyle* aStyle, bool& aNeedsBold)
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
    if (ResolveFontName(NS_LITERAL_STRING("Droid Sans"), resolvedName))
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
    return FT2FontEntry::CreateFontEntry(*aProxyEntry, aFontData, aLength);
}

