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
 * The Original Code is thebes gfx code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008-2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif /* MOZ_LOGGING */
#include "prlog.h"

#include "gfxUserFontSet.h"
#include "gfxPlatform.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "prlong.h"

#include "woff.h"

#include "opentype-sanitiser.h"
#include "ots-memory-stream.h"

using namespace mozilla;

#ifdef PR_LOGGING
static PRLogModuleInfo *gUserFontsLog = PR_NewLogModule("userfonts");
#endif /* PR_LOGGING */

#define LOG(args) PR_LOG(gUserFontsLog, PR_LOG_DEBUG, args)
#define LOG_ENABLED() PR_LOG_TEST(gUserFontsLog, PR_LOG_DEBUG)

static PRUint64 sFontSetGeneration = LL_INIT(0, 0);

// TODO: support for unicode ranges not yet implemented

gfxProxyFontEntry::gfxProxyFontEntry(const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList,
             gfxMixedFontFamily *aFamily,
             PRUint32 aWeight,
             PRUint32 aStretch,
             PRUint32 aItalicStyle,
             const nsTArray<gfxFontFeature> *aFeatureSettings,
             PRUint32 aLanguageOverride,
             gfxSparseBitSet *aUnicodeRanges)
    : gfxFontEntry(NS_LITERAL_STRING("Proxy"), aFamily), mIsLoading(PR_FALSE)
{
    mIsProxy = PR_TRUE;
    mSrcList = aFontFaceSrcList;
    mSrcIndex = 0;
    mWeight = aWeight;
    mStretch = aStretch;
    mItalic = (aItalicStyle & (FONT_STYLE_ITALIC | FONT_STYLE_OBLIQUE)) != 0;
    if (aFeatureSettings) {
        mFeatureSettings = new nsTArray<gfxFontFeature>;
        mFeatureSettings->AppendElements(*aFeatureSettings);
    }
    mLanguageOverride = aLanguageOverride;
    mIsUserFont = PR_TRUE;
}

gfxProxyFontEntry::~gfxProxyFontEntry()
{
}

gfxFont*
gfxProxyFontEntry::CreateFontInstance(const gfxFontStyle *aFontStyle, PRBool aNeedsBold)
{
    // cannot create an actual font for a proxy entry
    return nsnull;
}

gfxUserFontSet::gfxUserFontSet()
{
    mFontFamilies.Init(5);
    IncrementGeneration();
}

gfxUserFontSet::~gfxUserFontSet()
{
}

void
gfxUserFontSet::AddFontFace(const nsAString& aFamilyName,
                            const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList,
                            PRUint32 aWeight,
                            PRUint32 aStretch,
                            PRUint32 aItalicStyle,
                            const nsString& aFeatureSettings,
                            const nsString& aLanguageOverride,
                            gfxSparseBitSet *aUnicodeRanges)
{
    nsAutoString key(aFamilyName);
    ToLowerCase(key);

    PRBool found;

    if (aWeight == 0)
        aWeight = FONT_WEIGHT_NORMAL;

    // stretch, italic/oblique ==> zero implies normal

    gfxMixedFontFamily *family = mFontFamilies.GetWeak(key, &found);
    if (!family) {
        family = new gfxMixedFontFamily(aFamilyName);
        mFontFamilies.Put(key, family);
    }

    // construct a new face and add it into the family
    if (family) {
        nsTArray<gfxFontFeature> featureSettings;
        gfxFontStyle::ParseFontFeatureSettings(aFeatureSettings,
                                               featureSettings);
        PRUint32 languageOverride =
            gfxFontStyle::ParseFontLanguageOverride(aLanguageOverride);
        gfxProxyFontEntry *proxyEntry = 
            new gfxProxyFontEntry(aFontFaceSrcList, family, aWeight, aStretch, 
                                  aItalicStyle,
                                  featureSettings.Length() > 0 ?
                                      &featureSettings : nsnull,
                                  languageOverride,
                                  aUnicodeRanges);
        family->AddFontEntry(proxyEntry);
#ifdef PR_LOGGING
        if (LOG_ENABLED()) {
            LOG(("userfonts (%p) added (%s) with style: %s weight: %d stretch: %d", 
                 this, NS_ConvertUTF16toUTF8(aFamilyName).get(), 
                 (aItalicStyle & FONT_STYLE_ITALIC ? "italic" : 
                     (aItalicStyle & FONT_STYLE_OBLIQUE ? "oblique" : "normal")), 
                 aWeight, aStretch));
        }
#endif
    }
}

gfxFontEntry*
gfxUserFontSet::FindFontEntry(const nsAString& aName, 
                              const gfxFontStyle& aFontStyle, 
                              PRBool& aNeedsBold)
{
    gfxMixedFontFamily *family = GetFamily(aName);

    // no user font defined for this name
    if (!family)
        return nsnull;

    gfxFontEntry* fe = family->FindFontForStyle(aFontStyle, aNeedsBold);

    // if not a proxy, font has already been loaded
    if (!fe->mIsProxy)
        return fe;

    gfxProxyFontEntry *proxyEntry = static_cast<gfxProxyFontEntry*> (fe);

    // if currently loading, return null for now
    if (proxyEntry->mIsLoading)
        return nsnull;

    // hasn't been loaded yet, start the load process
    LoadStatus status;

    status = LoadNext(proxyEntry);

    // if the load succeeded immediately, the font entry was replaced so
    // search again
    if (status == STATUS_LOADED)
        return family->FindFontForStyle(aFontStyle, aNeedsBold);

    // if either loading or an error occurred, return null
    return nsnull;
}

// Given a buffer of downloaded font data, do any necessary preparation
// to make it into usable OpenType.
// May return the original pointer unchanged, or a newly-allocated
// block (in which case the passed-in block is NS_Free'd).
// aLength is updated if necessary to the new length of the data.
// Returns NULL and NS_Free's the incoming data in case of errors.
static const PRUint8*
PrepareOpenTypeData(const PRUint8* aData, PRUint32* aLength)
{
    switch(gfxFontUtils::DetermineFontDataType(aData, *aLength)) {
    
    case GFX_USERFONT_OPENTYPE:
        // nothing to do
        return aData;
        
    case GFX_USERFONT_WOFF: {
        PRUint32 status = eWOFF_ok;
        PRUint32 bufferSize = woffGetDecodedSize(aData, *aLength, &status);
        if (WOFF_FAILURE(status)) {
            break;
        }
        PRUint8* decodedData = static_cast<PRUint8*>(NS_Alloc(bufferSize));
        if (!decodedData) {
            break;
        }
        woffDecodeToBuffer(aData, *aLength,
                           decodedData, bufferSize,
                           aLength, &status);
        // replace original data with the decoded version
        NS_Free((void*)aData);
        aData = decodedData;
        if (WOFF_FAILURE(status)) {
            // something went wrong, discard the data and return NULL
            break;
        }
        // success, return the decoded data
        return aData;
    }

    // xxx - add support for other wrappers here

    default:
        NS_WARNING("unknown font format");
        break;
    }

    // discard downloaded data that couldn't be used
    NS_Free((void*)aData);

    return nsnull;
}

// Based on ots::ExpandingMemoryStream from ots-memory-stream.h,
// adapted to use Mozilla allocators and to allow the final
// memory buffer to be adopted by the client.
class ExpandingMemoryStream : public ots::OTSStream {
public:
    ExpandingMemoryStream(size_t initial, size_t limit)
        : mLength(initial), mLimit(limit), mOff(0) {
        mPtr = NS_Alloc(mLength);
    }

    ~ExpandingMemoryStream() {
        NS_Free(mPtr);
    }

    // return the buffer, and give up ownership of it
    // so the caller becomes responsible to call NS_Free
    // when finished with it
    void* forget() {
        void* p = mPtr;
        mPtr = nsnull;
        return p;
    }

    bool WriteRaw(const void *data, size_t length) {
        if ((mOff + length > mLength) ||
            (mLength > std::numeric_limits<size_t>::max() - mOff)) {
            if (mLength == mLimit) {
                return false;
            }
            size_t newLength = (mLength + 1) * 2;
            if (newLength < mLength) {
                return false;
            }
            if (newLength > mLimit) {
                newLength = mLimit;
            }
            mPtr = NS_Realloc(mPtr, newLength);
            mLength = newLength;
            return WriteRaw(data, length);
        }
        std::memcpy(static_cast<char*>(mPtr) + mOff, data, length);
        mOff += length;
        return true;
    }

    bool Seek(off_t position) {
        if (position < 0) {
            return false;
        }
        if (static_cast<size_t>(position) > mLength) {
            return false;
        }
        mOff = position;
        return true;
    }

    off_t Tell() const {
        return mOff;
    }

private:
    void*        mPtr;
    size_t       mLength;
    const size_t mLimit;
    off_t        mOff;
};

// Call the OTS library to sanitize an sfnt before attempting to use it.
// Returns a newly-allocated block, or NULL in case of fatal errors.
static const PRUint8*
SanitizeOpenTypeData(const PRUint8* aData, PRUint32 aLength,
                     PRUint32& aSaneLength, bool aIsCompressed)
{
    // limit output/expansion to 256MB
    ExpandingMemoryStream output(aIsCompressed ? aLength * 2 : aLength,
                                 1024 * 1024 * 256);
    if (ots::Process(&output, aData, aLength,
        gfxPlatform::GetPlatform()->PreserveOTLTablesWhenSanitizing())) {
        aSaneLength = output.Tell();
        return static_cast<PRUint8*>(output.forget());
    } else {
        aSaneLength = 0;
        return nsnull;
    }
}

// Find the GDEF, GSUB, GPOS tables in aFontData (if present)
// and cache copies in the given font entry.
// The sfnt table directory has already been accepted by the OTS
// sanitizer before this is called, so we can assume entries are valid.
//
// This is a temporary workaround until OTS has full support for the
// G*** tables, so that they can safely be left in the main font.
// When http://code.google.com/p/chromium/issues/detail?id=27131 gets fixed,
// we should remove this hack.
static void
CacheLayoutTablesFromSFNT(const PRUint8* aFontData, PRUint32 aLength,
                          gfxFontEntry* aFontEntry)
{
    const SFNTHeader *sfntHeader = reinterpret_cast<const SFNTHeader*>(aFontData);
    PRUint16 numTables = sfntHeader->numTables;
    
    // table directory entries begin immediately following SFNT header
    const TableDirEntry *dirEntry = 
        reinterpret_cast<const TableDirEntry*>(aFontData + sizeof(SFNTHeader));

    while (numTables-- > 0) {
        switch (dirEntry->tag) {
        case TRUETYPE_TAG('G','D','E','F'):
        case TRUETYPE_TAG('G','P','O','S'):
        case TRUETYPE_TAG('G','S','U','B'): {
                nsTArray<PRUint8> buffer;
                if (!buffer.AppendElements(aFontData + dirEntry->offset,
                                           dirEntry->length)) {
                    NS_WARNING("failed to cache font table - out of memory?");
                    break;
                }
                aFontEntry->PreloadFontTable(dirEntry->tag, buffer);
            }
            break;

        default:
            if (dirEntry->tag > TRUETYPE_TAG('G','S','U','B')) {
                // directory entries are required to be sorted,
                // so we can terminate as soon as we find a tag > 'GSUB'
                numTables = 0;
            }
            break;
        }
        ++dirEntry;
    }
}

// OTS drops the OT Layout tables when decoding a WOFF file, so retrieve them
// separately and cache them (unchecked) in the font entry; harfbuzz will
// sanitize them when it needs to use them.
static void
PreloadTableFromWOFF(const PRUint8* aFontData, PRUint32 aLength,
                     PRUint32 aTableTag, gfxFontEntry* aFontEntry)
{
    PRUint32 status = eWOFF_ok;
    PRUint32 len = woffGetTableSize(aFontData, aLength, aTableTag, &status);
    if (WOFF_SUCCESS(status) && len > 0) {
        nsTArray<PRUint8> buffer;
        if (!buffer.AppendElements(len)) {
            NS_WARNING("failed to cache font table - out of memory?");
            return;
        }
        woffGetTableToBuffer(aFontData, aLength, aTableTag,
                             buffer.Elements(), buffer.Length(),
                             &len, &status);
        if (WOFF_FAILURE(status)) {
            NS_WARNING("failed to cache font table - WOFF decoding error?");
            return;
        }
        aFontEntry->PreloadFontTable(aTableTag, buffer);
    }
}

static void
CacheLayoutTablesFromWOFF(const PRUint8* aFontData, PRUint32 aLength,
                          gfxFontEntry* aFontEntry)
{
    PreloadTableFromWOFF(aFontData, aLength, TRUETYPE_TAG('G','D','E','F'),
                         aFontEntry);
    PreloadTableFromWOFF(aFontData, aLength, TRUETYPE_TAG('G','P','O','S'),
                         aFontEntry);
    PreloadTableFromWOFF(aFontData, aLength, TRUETYPE_TAG('G','S','U','B'),
                         aFontEntry);
}

// This is called when a font download finishes.
// Ownership of aFontData passes in here, and the font set must
// ensure that it is eventually deleted via NS_Free().
PRBool 
gfxUserFontSet::OnLoadComplete(gfxFontEntry *aFontToLoad,
                               const PRUint8 *aFontData, PRUint32 aLength, 
                               nsresult aDownloadStatus)
{
    NS_ASSERTION(aFontToLoad->mIsProxy,
                 "trying to load font data for wrong font entry type");

    if (!aFontToLoad->mIsProxy) {
        NS_Free((void*)aFontData);
        return PR_FALSE;
    }

    gfxProxyFontEntry *pe = static_cast<gfxProxyFontEntry*> (aFontToLoad);

    // download successful, make platform font using font data
    if (NS_SUCCEEDED(aDownloadStatus)) {
        gfxFontEntry *fe = nsnull;

        // Unwrap/decompress/sanitize or otherwise munge the downloaded data
        // to make a usable sfnt structure.

        if (gfxPlatform::GetPlatform()->SanitizeDownloadedFonts()) {
            gfxUserFontType fontType =
                gfxFontUtils::DetermineFontDataType(aFontData, aLength);

            // Call the OTS sanitizer; this will also decode WOFF to sfnt
            // if necessary. The original data in aFontData is left unchanged.
            PRUint32 saneLen;
            const PRUint8* saneData =
                SanitizeOpenTypeData(aFontData, aLength, saneLen,
                                     fontType == GFX_USERFONT_WOFF);
#ifdef DEBUG
            if (!saneData) {
                char buf[1000];
                sprintf(buf, "downloaded font rejected for \"%s\"",
                        NS_ConvertUTF16toUTF8(pe->FamilyName()).get());
                NS_WARNING(buf);
            }
#endif
            if (saneData) {
                // Here ownership of saneData is passed to the platform,
                // which will delete it when no longer required
                fe = gfxPlatform::GetPlatform()->MakePlatformFont(pe,
                                                                  saneData,
                                                                  saneLen);
                if (fe) {
                    // if aFontData includes OpenType layout tables, we need to
                    // cache them in the font entry for harfbuzz to use,
                    // as they will have been dropped from the sanitized sfnt
                    // (temporary hack, see CacheLayoutTablesFromSFNT)
                    switch (fontType) {
                    case GFX_USERFONT_OPENTYPE:
                        CacheLayoutTablesFromSFNT(aFontData, aLength, fe);
                        break;

                    case GFX_USERFONT_WOFF:
                        CacheLayoutTablesFromWOFF(aFontData, aLength, fe);
                        break;

                    default:
                        break;
                    }
                } else {
                    NS_WARNING("failed to make platform font from download");
                }
            }
        } else {
            // FIXME: this code can be removed once we remove the pref to
            // disable the sanitizer; the PrepareOpenTypeData and
            // ValidateSFNTHeaders functions will then be obsolete.
            aFontData = PrepareOpenTypeData(aFontData, &aLength);

            if (aFontData) {
                if (gfxFontUtils::ValidateSFNTHeaders(aFontData, aLength)) {
                    // Here ownership of aFontData is passed to the platform,
                    // which will delete it when no longer required
                    fe = gfxPlatform::GetPlatform()->MakePlatformFont(pe,
                                                                      aFontData,
                                                                      aLength);
                    aFontData = nsnull; // we must NOT free this below!
                } else {
                    // the data was unusable, so just discard it
                    // (error will be reported below, if logging is enabled)
                    NS_WARNING("failed to make platform font from download");
                }
            }
        }

        if (aFontData) {
            NS_Free((void*)aFontData);
            aFontData = nsnull;
        }

        if (fe) {
            // copy OpenType feature/language settings from the proxy to the
            // newly-created font entry
            if (pe->mFeatureSettings) {
                fe->mFeatureSettings = new nsTArray<gfxFontFeature>;
                fe->mFeatureSettings->AppendElements(*pe->mFeatureSettings);
            }
            fe->mLanguageOverride = pe->mLanguageOverride;

            static_cast<gfxMixedFontFamily*>(pe->mFamily)->ReplaceFontEntry(pe, fe);
            IncrementGeneration();
#ifdef PR_LOGGING
            if (LOG_ENABLED()) {
                nsCAutoString fontURI;
                pe->mSrcList[pe->mSrcIndex].mURI->GetSpec(fontURI);
                LOG(("userfonts (%p) [src %d] loaded uri: (%s) for (%s) gen: %8.8x\n",
                     this, pe->mSrcIndex, fontURI.get(),
                     NS_ConvertUTF16toUTF8(pe->mFamily->Name()).get(),
                     PRUint32(mGeneration)));
            }
#endif
            return PR_TRUE;
        } else {
#ifdef PR_LOGGING
            if (LOG_ENABLED()) {
                nsCAutoString fontURI;
                pe->mSrcList[pe->mSrcIndex].mURI->GetSpec(fontURI);
                LOG(("userfonts (%p) [src %d] failed uri: (%s) for (%s) error making platform font\n",
                     this, pe->mSrcIndex, fontURI.get(),
                     NS_ConvertUTF16toUTF8(pe->mFamily->Name()).get()));
            }
#endif
        }
    } else {
        // download failed
#ifdef PR_LOGGING
        if (LOG_ENABLED()) {
            nsCAutoString fontURI;
            pe->mSrcList[pe->mSrcIndex].mURI->GetSpec(fontURI);
            LOG(("userfonts (%p) [src %d] failed uri: (%s) for (%s) error %8.8x downloading font data\n",
                 this, pe->mSrcIndex, fontURI.get(),
                 NS_ConvertUTF16toUTF8(pe->mFamily->Name()).get(),
                 aDownloadStatus));
        }
#endif
    }

    if (aFontData) {
        NS_Free((void*)aFontData);
    }

    // error occurred, load next src
    LoadStatus status;

    status = LoadNext(pe);
    if (status == STATUS_LOADED) {
        // load may succeed if external font resource followed by
        // local font, in this case need to bump generation
        IncrementGeneration();
        return PR_TRUE;
    }

    return PR_FALSE;
}


gfxUserFontSet::LoadStatus
gfxUserFontSet::LoadNext(gfxProxyFontEntry *aProxyEntry)
{
    PRUint32 numSrc = aProxyEntry->mSrcList.Length();

    NS_ASSERTION(aProxyEntry->mSrcIndex < numSrc, "already at the end of the src list for user font");

    if (aProxyEntry->mIsLoading) {
        aProxyEntry->mSrcIndex++;
    } else {
        aProxyEntry->mIsLoading = PR_TRUE;
    }

    // load each src entry in turn, until a local face is found or a download begins successfully
    while (aProxyEntry->mSrcIndex < numSrc) {
        const gfxFontFaceSrc& currSrc = aProxyEntry->mSrcList[aProxyEntry->mSrcIndex];

        // src local ==> lookup and load   

        if (currSrc.mIsLocal) {
            gfxFontEntry *fe =
                gfxPlatform::GetPlatform()->LookupLocalFont(aProxyEntry,
                                                            currSrc.mLocalName);
            if (fe) {
                LOG(("userfonts (%p) [src %d] loaded local: (%s) for (%s) gen: %8.8x\n", 
                     this, aProxyEntry->mSrcIndex, 
                     NS_ConvertUTF16toUTF8(currSrc.mLocalName).get(), 
                     NS_ConvertUTF16toUTF8(aProxyEntry->mFamily->Name()).get(), 
                     PRUint32(mGeneration)));
                if (aProxyEntry->mFeatureSettings) {
                    fe->mFeatureSettings = new nsTArray<gfxFontFeature>;
                    fe->mFeatureSettings->AppendElements(*aProxyEntry->mFeatureSettings);
                }
                fe->mLanguageOverride = aProxyEntry->mLanguageOverride;
                static_cast<gfxMixedFontFamily*>(aProxyEntry->mFamily)->ReplaceFontEntry(aProxyEntry, fe);
                return STATUS_LOADED;
            } else {
                LOG(("userfonts (%p) [src %d] failed local: (%s) for (%s)\n", 
                     this, aProxyEntry->mSrcIndex, 
                     NS_ConvertUTF16toUTF8(currSrc.mLocalName).get(), 
                     NS_ConvertUTF16toUTF8(aProxyEntry->mFamily->Name()).get()));            
            }
        } 

        // src url ==> start the load process
        else {
            if (gfxPlatform::GetPlatform()->IsFontFormatSupported(currSrc.mURI, 
                    currSrc.mFormatFlags)) {
                nsresult rv = StartLoad(aProxyEntry, &currSrc);
                PRBool loadOK = NS_SUCCEEDED(rv);
                
                if (loadOK) {
#ifdef PR_LOGGING
                    if (LOG_ENABLED()) {
                        nsCAutoString fontURI;
                        currSrc.mURI->GetSpec(fontURI);
                        LOG(("userfonts (%p) [src %d] loading uri: (%s) for (%s)\n", 
                             this, aProxyEntry->mSrcIndex, fontURI.get(), 
                             NS_ConvertUTF16toUTF8(aProxyEntry->mFamily->Name()).get()));
                    }
#endif
                    return STATUS_LOADING;                  
                } else {
#ifdef PR_LOGGING
                    if (LOG_ENABLED()) {
                        nsCAutoString fontURI;
                        currSrc.mURI->GetSpec(fontURI);
                        LOG(("userfonts (%p) [src %d] failed uri: (%s) for (%s) download failed\n", 
                             this, aProxyEntry->mSrcIndex, fontURI.get(), 
                             NS_ConvertUTF16toUTF8(aProxyEntry->mFamily->Name()).get()));
                    }
#endif
                }
            } else {
#ifdef PR_LOGGING
                if (LOG_ENABLED()) {
                    nsCAutoString fontURI;
                    currSrc.mURI->GetSpec(fontURI);
                    LOG(("userfonts (%p) [src %d] failed uri: (%s) for (%s) format not supported\n", 
                         this, aProxyEntry->mSrcIndex, fontURI.get(), 
                         NS_ConvertUTF16toUTF8(aProxyEntry->mFamily->Name()).get()));
                }
#endif
            }
        }

        aProxyEntry->mSrcIndex++;
    }

    // all src's failed, remove this face
    LOG(("userfonts (%p) failed all src for (%s)\n", 
               this, NS_ConvertUTF16toUTF8(aProxyEntry->mFamily->Name()).get()));            

    gfxMixedFontFamily *family = static_cast<gfxMixedFontFamily*>(aProxyEntry->mFamily);

    family->RemoveFontEntry(aProxyEntry);

    // no more faces?  remove the entire family
    if (family->mAvailableFonts.Length() == 0) {
        LOG(("userfonts (%p) failed all faces, remove family (%s)\n", 
             this, NS_ConvertUTF16toUTF8(family->Name()).get()));            
        RemoveFamily(family->Name());
    }

    return STATUS_END_OF_LIST;
}


void
gfxUserFontSet::IncrementGeneration()
{
    // add one, increment again if zero
    LL_ADD(sFontSetGeneration, sFontSetGeneration, 1);
    if (LL_IS_ZERO(sFontSetGeneration))
        LL_ADD(sFontSetGeneration, sFontSetGeneration, 1);
    mGeneration = sFontSetGeneration;
}


gfxMixedFontFamily*
gfxUserFontSet::GetFamily(const nsAString& aFamilyName) const
{
    nsAutoString key(aFamilyName);
    ToLowerCase(key);

    return mFontFamilies.GetWeak(key);
}


void 
gfxUserFontSet::RemoveFamily(const nsAString& aFamilyName)
{
    nsAutoString key(aFamilyName);
    ToLowerCase(key);

    mFontFamilies.Remove(key);
}
