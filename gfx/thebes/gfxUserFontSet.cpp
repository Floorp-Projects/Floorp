/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif /* MOZ_LOGGING */
#include "prlog.h"

#include "gfxUserFontSet.h"
#include "gfxPlatform.h"
#include "nsUnicharUtils.h"
#include "nsNetUtil.h"
#include "nsIProtocolHandler.h"
#include "nsIPrincipal.h"
#include "gfxFontConstants.h"
#include "mozilla/Services.h"
#include "mozilla/gfx/2D.h"
#include "gfxPlatformFontList.h"

#include "opentype-sanitiser.h"
#include "ots-memory-stream.h"

using namespace mozilla;

#ifdef PR_LOGGING
PRLogModuleInfo *
gfxUserFontSet::GetUserFontsLog()
{
    static PRLogModuleInfo *sLog;
    if (!sLog)
        sLog = PR_NewLogModule("userfonts");
    return sLog;
}
#endif /* PR_LOGGING */

#define LOG(args) PR_LOG(GetUserFontsLog(), PR_LOG_DEBUG, args)
#define LOG_ENABLED() PR_LOG_TEST(GetUserFontsLog(), PR_LOG_DEBUG)

static uint64_t sFontSetGeneration = 0;

// TODO: support for unicode ranges not yet implemented

gfxProxyFontEntry::gfxProxyFontEntry(const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList,
             uint32_t aWeight,
             int32_t aStretch,
             uint32_t aItalicStyle,
             const nsTArray<gfxFontFeature>& aFeatureSettings,
             uint32_t aLanguageOverride,
             gfxSparseBitSet *aUnicodeRanges)
    : gfxFontEntry(NS_LITERAL_STRING("Proxy")),
      mLoadingState(NOT_LOADING),
      mUnsupportedFormat(false),
      mLoader(nullptr)
{
    mIsProxy = true;
    mSrcList = aFontFaceSrcList;
    mSrcIndex = 0;
    mWeight = aWeight;
    mStretch = aStretch;
    // XXX Currently, we don't distinguish 'italic' and 'oblique' styles;
    // we need to fix this. (Bug 543715)
    mItalic = (aItalicStyle & (NS_FONT_STYLE_ITALIC | NS_FONT_STYLE_OBLIQUE)) != 0;
    mFeatureSettings.AppendElements(aFeatureSettings);
    mLanguageOverride = aLanguageOverride;
    mIsUserFont = true;
}

gfxProxyFontEntry::~gfxProxyFontEntry()
{
}

bool
gfxProxyFontEntry::Matches(const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList,
                           uint32_t aWeight,
                           int32_t aStretch,
                           uint32_t aItalicStyle,
                           const nsTArray<gfxFontFeature>& aFeatureSettings,
                           uint32_t aLanguageOverride,
                           gfxSparseBitSet *aUnicodeRanges)
{
    // XXX font entries don't distinguish italic from oblique (bug 543715)
    bool isItalic =
        (aItalicStyle & (NS_FONT_STYLE_ITALIC | NS_FONT_STYLE_OBLIQUE)) != 0;

    return mWeight == aWeight &&
           mStretch == aStretch &&
           mItalic == isItalic &&
           mFeatureSettings == aFeatureSettings &&
           mLanguageOverride == aLanguageOverride &&
           mSrcList == aFontFaceSrcList;
           // XXX once we support unicode-range (bug 475891),
           // we'll need to compare that here as well
}

gfxFont*
gfxProxyFontEntry::CreateFontInstance(const gfxFontStyle *aFontStyle, bool aNeedsBold)
{
    // cannot create an actual font for a proxy entry
    return nullptr;
}

gfxUserFontSet::gfxUserFontSet()
    : mFontFamilies(5), mLocalRulesUsed(false)
{
    IncrementGeneration();
    gfxPlatformFontList *fp = gfxPlatformFontList::PlatformFontList();
    if (fp) {
        fp->AddUserFontSet(this);
    }
}

gfxUserFontSet::~gfxUserFontSet()
{
    gfxPlatformFontList *fp = gfxPlatformFontList::PlatformFontList();
    if (fp) {
        fp->RemoveUserFontSet(this);
    }
}

gfxFontEntry*
gfxUserFontSet::AddFontFace(const nsAString& aFamilyName,
                            const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList,
                            uint32_t aWeight,
                            int32_t aStretch,
                            uint32_t aItalicStyle,
                            const nsTArray<gfxFontFeature>& aFeatureSettings,
                            uint32_t aLanguageOverride,
                            gfxSparseBitSet *aUnicodeRanges)
{
    MOZ_ASSERT(aWeight != 0,
               "aWeight must not be 0; use NS_FONT_WEIGHT_NORMAL instead");

    nsAutoString key(aFamilyName);
    ToLowerCase(key);

    bool found;

    // stretch, italic/oblique ==> zero implies normal

    gfxMixedFontFamily *family = mFontFamilies.GetWeak(key, &found);
    if (!family) {
        family = new gfxMixedFontFamily(aFamilyName);
        mFontFamilies.Put(key, family);
    }

    // If there's already a proxy in the family whose descriptors all match,
    // we can just move it to the end of the list instead of adding a new
    // face that will always "shadow" the old one.
    // Note that we can't do this for "real" (non-proxy) entries, even if the
    // style descriptors match, as they might have had a different source list,
    // but we no longer have the old source list available to check.
    nsTArray<nsRefPtr<gfxFontEntry> >& fontList = family->GetFontList();
    for (uint32_t i = 0, count = fontList.Length(); i < count; i++) {
        if (!fontList[i]->mIsProxy) {
            continue;
        }

        gfxProxyFontEntry *existingProxyEntry =
            static_cast<gfxProxyFontEntry*>(fontList[i].get());
        if (!existingProxyEntry->Matches(aFontFaceSrcList,
                                         aWeight, aStretch, aItalicStyle,
                                         aFeatureSettings, aLanguageOverride,
                                         aUnicodeRanges)) {
            continue;
        }

        // We've found an entry that matches the new face exactly, so just add
        // it to the end of the list. gfxMixedFontFamily::AddFontEntry() will
        // automatically remove any earlier occurrence of the same proxy.
        family->AddFontEntry(existingProxyEntry);
        return existingProxyEntry;
    }

    // construct a new face and add it into the family
    gfxProxyFontEntry *proxyEntry =
        new gfxProxyFontEntry(aFontFaceSrcList, aWeight, aStretch,
                              aItalicStyle,
                              aFeatureSettings,
                              aLanguageOverride,
                              aUnicodeRanges);
    family->AddFontEntry(proxyEntry);
#ifdef PR_LOGGING
    if (LOG_ENABLED()) {
        LOG(("userfonts (%p) added (%s) with style: %s weight: %d stretch: %d",
             this, NS_ConvertUTF16toUTF8(aFamilyName).get(),
             (aItalicStyle & NS_FONT_STYLE_ITALIC ? "italic" :
                 (aItalicStyle & NS_FONT_STYLE_OBLIQUE ? "oblique" : "normal")),
             aWeight, aStretch));
    }
#endif

    return proxyEntry;
}

void
gfxUserFontSet::AddFontFace(const nsAString& aFamilyName,
                            gfxFontEntry     *aFontEntry)
{
    nsAutoString key(aFamilyName);
    ToLowerCase(key);

    bool found;

    gfxMixedFontFamily *family = mFontFamilies.GetWeak(key, &found);
    if (!family) {
        family = new gfxMixedFontFamily(aFamilyName);
        mFontFamilies.Put(key, family);
    }

    family->AddFontEntry(aFontEntry);
}

gfxFontEntry*
gfxUserFontSet::FindFontEntry(gfxFontFamily *aFamily,
                              const gfxFontStyle& aFontStyle,
                              bool& aNeedsBold,
                              bool& aWaitForUserFont)
{
    aWaitForUserFont = false;
    gfxMixedFontFamily *family = static_cast<gfxMixedFontFamily*>(aFamily);

    gfxFontEntry* fe = family->FindFontForStyle(aFontStyle, aNeedsBold);

    // if not a proxy, font has already been loaded
    if (!fe->mIsProxy) {
        return fe;
    }

    gfxProxyFontEntry *proxyEntry = static_cast<gfxProxyFontEntry*> (fe);

    // if currently loading, return null for now
    if (proxyEntry->mLoadingState > gfxProxyFontEntry::NOT_LOADING) {
        aWaitForUserFont =
            (proxyEntry->mLoadingState < gfxProxyFontEntry::LOADING_SLOWLY);
        return nullptr;
    }

    // hasn't been loaded yet, start the load process
    LoadStatus status;

    // NOTE that if all sources in the entry fail, this will delete proxyEntry,
    // so we cannot use it again if status==STATUS_END_OF_LIST
    status = LoadNext(family, proxyEntry);

    // if the load succeeded immediately, the font entry was replaced so
    // search again
    if (status == STATUS_LOADED) {
        return family->FindFontForStyle(aFontStyle, aNeedsBold);
    }

    // check whether we should wait for load to complete before painting
    // a fallback font -- but not if all sources failed (bug 633500)
    aWaitForUserFont = (status != STATUS_END_OF_LIST) &&
        (proxyEntry->mLoadingState < gfxProxyFontEntry::LOADING_SLOWLY);

    // if either loading or an error occurred, return null
    return nullptr;
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
        mPtr = nullptr;
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

static ots::TableAction
OTSTableAction(uint32_t aTag, void *aUserData)
{
    // preserve Graphite, color glyph and SVG tables
    if (aTag == TRUETYPE_TAG('S', 'i', 'l', 'f') ||
        aTag == TRUETYPE_TAG('S', 'i', 'l', 'l') ||
        aTag == TRUETYPE_TAG('G', 'l', 'o', 'c') ||
        aTag == TRUETYPE_TAG('G', 'l', 'a', 't') ||
        aTag == TRUETYPE_TAG('F', 'e', 'a', 't') ||
        aTag == TRUETYPE_TAG('S', 'V', 'G', ' ') ||
        aTag == TRUETYPE_TAG('C', 'O', 'L', 'R') ||
        aTag == TRUETYPE_TAG('C', 'P', 'A', 'L')) {
        return ots::TABLE_ACTION_PASSTHRU;
    }
    return ots::TABLE_ACTION_DEFAULT;
}

struct OTSCallbackUserData {
    gfxUserFontSet     *mFontSet;
    gfxMixedFontFamily *mFamily;
    gfxProxyFontEntry  *mProxy;
};

/* static */ bool
gfxUserFontSet::OTSMessage(void *aUserData, const char *format, ...)
{
    va_list va;
    va_start(va, format);

    // buf should be more than adequate for any message OTS generates,
    // so we don't worry about checking the result of vsnprintf()
    char buf[512];
    (void)vsnprintf(buf, sizeof(buf), format, va);

    va_end(va);

    OTSCallbackUserData *d = static_cast<OTSCallbackUserData*>(aUserData);
    d->mFontSet->LogMessage(d->mFamily, d->mProxy, buf);

    return false;
}

// Call the OTS library to sanitize an sfnt before attempting to use it.
// Returns a newly-allocated block, or nullptr in case of fatal errors.
const uint8_t*
gfxUserFontSet::SanitizeOpenTypeData(gfxMixedFontFamily *aFamily,
                                     gfxProxyFontEntry *aProxy,
                                     const uint8_t* aData, uint32_t aLength,
                                     uint32_t& aSaneLength, bool aIsCompressed)
{
    // limit output/expansion to 256MB
    ExpandingMemoryStream output(aIsCompressed ? aLength * 2 : aLength,
                                 1024 * 1024 * 256);

    OTSCallbackUserData userData;
    userData.mFontSet = this;
    userData.mFamily = aFamily;
    userData.mProxy = aProxy;

    ots::OTSContext otsContext;
    otsContext.SetTableActionCallback(&OTSTableAction, nullptr);
    otsContext.SetMessageCallback(&gfxUserFontSet::OTSMessage, &userData);

    if (otsContext.Process(&output, aData, aLength)) {
        aSaneLength = output.Tell();
        return static_cast<uint8_t*>(output.forget());
    } else {
        aSaneLength = 0;
        return nullptr;
    }
}

static void
StoreUserFontData(gfxFontEntry* aFontEntry, gfxProxyFontEntry* aProxy,
                  bool aPrivate, const nsAString& aOriginalName,
                  FallibleTArray<uint8_t>* aMetadata, uint32_t aMetaOrigLen)
{
    if (!aFontEntry->mUserFontData) {
        aFontEntry->mUserFontData = new gfxUserFontData;
    }
    gfxUserFontData* userFontData = aFontEntry->mUserFontData;
    userFontData->mSrcIndex = aProxy->mSrcIndex;
    const gfxFontFaceSrc& src = aProxy->mSrcList[aProxy->mSrcIndex];
    if (src.mIsLocal) {
        userFontData->mLocalName = src.mLocalName;
    } else {
        userFontData->mURI = src.mURI;
        userFontData->mPrincipal = aProxy->mPrincipal;
    }
    userFontData->mPrivate = aPrivate;
    userFontData->mFormat = src.mFormatFlags;
    userFontData->mRealName = aOriginalName;
    if (aMetadata) {
        userFontData->mMetadata.SwapElements(*aMetadata);
        userFontData->mMetaOrigLen = aMetaOrigLen;
    }
}

struct WOFFHeader {
    AutoSwap_PRUint32 signature;
    AutoSwap_PRUint32 flavor;
    AutoSwap_PRUint32 length;
    AutoSwap_PRUint16 numTables;
    AutoSwap_PRUint16 reserved;
    AutoSwap_PRUint32 totalSfntSize;
    AutoSwap_PRUint16 majorVersion;
    AutoSwap_PRUint16 minorVersion;
    AutoSwap_PRUint32 metaOffset;
    AutoSwap_PRUint32 metaCompLen;
    AutoSwap_PRUint32 metaOrigLen;
    AutoSwap_PRUint32 privOffset;
    AutoSwap_PRUint32 privLen;
};

void
gfxUserFontSet::CopyWOFFMetadata(const uint8_t* aFontData,
                                 uint32_t aLength,
                                 FallibleTArray<uint8_t>* aMetadata,
                                 uint32_t* aMetaOrigLen)
{
    // This function may be called with arbitrary, unvalidated "font" data
    // from @font-face, so it needs to be careful to bounds-check, etc.,
    // before trying to read anything.
    // This just saves a copy of the compressed data block; it does NOT check
    // that the block can be successfully decompressed, or that it contains
    // well-formed/valid XML metadata.
    if (aLength < sizeof(WOFFHeader)) {
        return;
    }
    const WOFFHeader* woff = reinterpret_cast<const WOFFHeader*>(aFontData);
    uint32_t metaOffset = woff->metaOffset;
    uint32_t metaCompLen = woff->metaCompLen;
    if (!metaOffset || !metaCompLen || !woff->metaOrigLen) {
        return;
    }
    if (metaOffset >= aLength || metaCompLen > aLength - metaOffset) {
        return;
    }
    if (!aMetadata->SetLength(woff->metaCompLen)) {
        return;
    }
    memcpy(aMetadata->Elements(), aFontData + metaOffset, metaCompLen);
    *aMetaOrigLen = woff->metaOrigLen;
}

// This is called when a font download finishes.
// Ownership of aFontData passes in here, and the font set must
// ensure that it is eventually deleted via NS_Free().
bool
gfxUserFontSet::OnLoadComplete(gfxMixedFontFamily *aFamily,
                               gfxProxyFontEntry *aProxy,
                               const uint8_t *aFontData, uint32_t aLength,
                               nsresult aDownloadStatus)
{
    // forget about the loader, as we no longer potentially need to cancel it
    // if the entry is obsoleted
    aProxy->mLoader = nullptr;

    // download successful, make platform font using font data
    if (NS_SUCCEEDED(aDownloadStatus)) {
        gfxFontEntry *fe = LoadFont(aFamily, aProxy, aFontData, aLength);
        aFontData = nullptr;

        if (fe) {
            IncrementGeneration();
            return true;
        }

    } else {
        // download failed
        LogMessage(aFamily, aProxy,
                   "download failed", nsIScriptError::errorFlag,
                   aDownloadStatus);
    }

    if (aFontData) {
        NS_Free((void*)aFontData);
    }

    // error occurred, load next src
    (void)LoadNext(aFamily, aProxy);

    // We ignore the status returned by LoadNext();
    // even if loading failed, we need to bump the font-set generation
    // and return true in order to trigger reflow, so that fallback
    // will be used where the text was "masked" by the pending download
    IncrementGeneration();
    return true;
}


gfxUserFontSet::LoadStatus
gfxUserFontSet::LoadNext(gfxMixedFontFamily *aFamily,
                         gfxProxyFontEntry *aProxyEntry)
{
    uint32_t numSrc = aProxyEntry->mSrcList.Length();

    NS_ASSERTION(aProxyEntry->mSrcIndex < numSrc,
                 "already at the end of the src list for user font");

    if (aProxyEntry->mLoadingState == gfxProxyFontEntry::NOT_LOADING) {
        aProxyEntry->mLoadingState = gfxProxyFontEntry::LOADING_STARTED;
        aProxyEntry->mUnsupportedFormat = false;
    } else {
        // we were already loading; move to the next source,
        // but don't reset state - if we've already timed out,
        // that counts against the new download
        aProxyEntry->mSrcIndex++;
    }

    // load each src entry in turn, until a local face is found
    // or a download begins successfully
    while (aProxyEntry->mSrcIndex < numSrc) {
        const gfxFontFaceSrc& currSrc = aProxyEntry->mSrcList[aProxyEntry->mSrcIndex];

        // src local ==> lookup and load immediately

        if (currSrc.mIsLocal) {
            gfxFontEntry *fe =
                gfxPlatform::GetPlatform()->LookupLocalFont(aProxyEntry,
                                                            currSrc.mLocalName);
            mLocalRulesUsed = true;
            if (fe) {
                LOG(("userfonts (%p) [src %d] loaded local: (%s) for (%s) gen: %8.8x\n",
                     this, aProxyEntry->mSrcIndex,
                     NS_ConvertUTF16toUTF8(currSrc.mLocalName).get(),
                     NS_ConvertUTF16toUTF8(aFamily->Name()).get(),
                     uint32_t(mGeneration)));
                fe->mFeatureSettings.AppendElements(aProxyEntry->mFeatureSettings);
                fe->mLanguageOverride = aProxyEntry->mLanguageOverride;
                // For src:local(), we don't care whether the request is from
                // a private window as there's no issue of caching resources;
                // local fonts are just available all the time.
                StoreUserFontData(fe, aProxyEntry, false, nsString(), nullptr, 0);
                ReplaceFontEntry(aFamily, aProxyEntry, fe);
                return STATUS_LOADED;
            } else {
                LOG(("userfonts (%p) [src %d] failed local: (%s) for (%s)\n",
                     this, aProxyEntry->mSrcIndex,
                     NS_ConvertUTF16toUTF8(currSrc.mLocalName).get(),
                     NS_ConvertUTF16toUTF8(aFamily->Name()).get()));
            }
        }

        // src url ==> start the load process
        else {
            if (gfxPlatform::GetPlatform()->IsFontFormatSupported(currSrc.mURI,
                    currSrc.mFormatFlags)) {

                nsIPrincipal *principal = nullptr;
                bool bypassCache;
                nsresult rv = CheckFontLoad(&currSrc, &principal, &bypassCache);

                if (NS_SUCCEEDED(rv) && principal != nullptr) {
                    if (!bypassCache) {
                        // see if we have an existing entry for this source
                        gfxFontEntry *fe =
                            UserFontCache::GetFont(currSrc.mURI, principal,
                                                   aProxyEntry,
                                                   GetPrivateBrowsing());
                        if (fe) {
                            ReplaceFontEntry(aFamily, aProxyEntry, fe);
                            return STATUS_LOADED;
                        }
                    }

                    // record the principal returned by CheckFontLoad,
                    // for use when creating a channel
                    // and when caching the loaded entry
                    aProxyEntry->mPrincipal = principal;

                    bool loadDoesntSpin = false;
                    rv = NS_URIChainHasFlags(currSrc.mURI,
                           nsIProtocolHandler::URI_SYNC_LOAD_IS_OK,
                           &loadDoesntSpin);
                    if (NS_SUCCEEDED(rv) && loadDoesntSpin) {
                        uint8_t *buffer = nullptr;
                        uint32_t bufferLength = 0;

                        // sync load font immediately
                        rv = SyncLoadFontData(aProxyEntry, &currSrc,
                                              buffer, bufferLength);
                        if (NS_SUCCEEDED(rv) && LoadFont(aFamily, aProxyEntry,
                                                         buffer, bufferLength)) {
                            return STATUS_LOADED;
                        } else {
                            LogMessage(aFamily, aProxyEntry,
                                       "font load failed",
                                       nsIScriptError::errorFlag, rv);
                        }
                    } else {
                        // otherwise load font async
                        rv = StartLoad(aFamily, aProxyEntry, &currSrc);
                        if (NS_SUCCEEDED(rv)) {
#ifdef PR_LOGGING
                            if (LOG_ENABLED()) {
                                nsAutoCString fontURI;
                                currSrc.mURI->GetSpec(fontURI);
                                LOG(("userfonts (%p) [src %d] loading uri: (%s) for (%s)\n",
                                     this, aProxyEntry->mSrcIndex, fontURI.get(),
                                     NS_ConvertUTF16toUTF8(aFamily->Name()).get()));
                            }
#endif
                            return STATUS_LOADING;
                        } else {
                            LogMessage(aFamily, aProxyEntry,
                                       "download failed",
                                       nsIScriptError::errorFlag, rv);
                        }
                    }
                } else {
                    LogMessage(aFamily, aProxyEntry, "download not allowed",
                               nsIScriptError::errorFlag, rv);
                }
            } else {
                // We don't log a warning to the web console yet,
                // as another source may load successfully
                aProxyEntry->mUnsupportedFormat = true;
            }
        }

        aProxyEntry->mSrcIndex++;
    }

    if (aProxyEntry->mUnsupportedFormat) {
        LogMessage(aFamily, aProxyEntry, "no supported format found",
                   nsIScriptError::warningFlag);
    }

    // all src's failed; mark this entry as unusable (so fallback will occur)
    LOG(("userfonts (%p) failed all src for (%s)\n",
        this, NS_ConvertUTF16toUTF8(aFamily->Name()).get()));
    aProxyEntry->mLoadingState = gfxProxyFontEntry::LOADING_FAILED;

    return STATUS_END_OF_LIST;
}

void
gfxUserFontSet::IncrementGeneration()
{
    // add one, increment again if zero
    ++sFontSetGeneration;
    if (sFontSetGeneration == 0)
       ++sFontSetGeneration;
    mGeneration = sFontSetGeneration;
}

void
gfxUserFontSet::RebuildLocalRules()
{
    if (mLocalRulesUsed) {
        DoRebuildUserFontSet();
    }
}

gfxFontEntry*
gfxUserFontSet::LoadFont(gfxMixedFontFamily *aFamily,
                         gfxProxyFontEntry *aProxy,
                         const uint8_t *aFontData, uint32_t &aLength)
{
    gfxFontEntry *fe = nullptr;

    gfxUserFontType fontType =
        gfxFontUtils::DetermineFontDataType(aFontData, aLength);

    // Unwrap/decompress/sanitize or otherwise munge the downloaded data
    // to make a usable sfnt structure.

    // Because platform font activation code may replace the name table
    // in the font with a synthetic one, we save the original name so that
    // it can be reported via the nsIDOMFontFace API.
    nsAutoString originalFullName;

    // Call the OTS sanitizer; this will also decode WOFF to sfnt
    // if necessary. The original data in aFontData is left unchanged.
    uint32_t saneLen;
    const uint8_t* saneData =
        SanitizeOpenTypeData(aFamily, aProxy, aFontData, aLength, saneLen,
                             fontType == GFX_USERFONT_WOFF);
    if (!saneData) {
        LogMessage(aFamily, aProxy, "rejected by sanitizer");
    }
    if (saneData) {
        // The sanitizer ensures that we have a valid sfnt and a usable
        // name table, so this should never fail unless we're out of
        // memory, and GetFullNameFromSFNT is not directly exposed to
        // arbitrary/malicious data from the web.
        gfxFontUtils::GetFullNameFromSFNT(saneData, saneLen,
                                          originalFullName);
        // Here ownership of saneData is passed to the platform,
        // which will delete it when no longer required
        fe = gfxPlatform::GetPlatform()->MakePlatformFont(aProxy,
                                                          saneData,
                                                          saneLen);
        if (!fe) {
            LogMessage(aFamily, aProxy, "not usable by platform");
        }
    }

    if (fe) {
        // Save a copy of the metadata block (if present) for nsIDOMFontFace
        // to use if required. Ownership of the metadata block will be passed
        // to the gfxUserFontData record below.
        FallibleTArray<uint8_t> metadata;
        uint32_t metaOrigLen = 0;
        if (fontType == GFX_USERFONT_WOFF) {
            CopyWOFFMetadata(aFontData, aLength, &metadata, &metaOrigLen);
        }

        // copy OpenType feature/language settings from the proxy to the
        // newly-created font entry
        fe->mFeatureSettings.AppendElements(aProxy->mFeatureSettings);
        fe->mLanguageOverride = aProxy->mLanguageOverride;
        StoreUserFontData(fe, aProxy, GetPrivateBrowsing(),
                          originalFullName, &metadata, metaOrigLen);
#ifdef PR_LOGGING
        if (LOG_ENABLED()) {
            nsAutoCString fontURI;
            aProxy->mSrcList[aProxy->mSrcIndex].mURI->GetSpec(fontURI);
            LOG(("userfonts (%p) [src %d] loaded uri: (%s) for (%s) gen: %8.8x\n",
                 this, aProxy->mSrcIndex, fontURI.get(),
                 NS_ConvertUTF16toUTF8(aFamily->Name()).get(),
                 uint32_t(mGeneration)));
        }
#endif
        ReplaceFontEntry(aFamily, aProxy, fe);
        UserFontCache::CacheFont(fe);
    } else {
#ifdef PR_LOGGING
        if (LOG_ENABLED()) {
            nsAutoCString fontURI;
            aProxy->mSrcList[aProxy->mSrcIndex].mURI->GetSpec(fontURI);
            LOG(("userfonts (%p) [src %d] failed uri: (%s) for (%s)"
                 " error making platform font\n",
                 this, aProxy->mSrcIndex, fontURI.get(),
                 NS_ConvertUTF16toUTF8(aFamily->Name()).get()));
        }
#endif
    }

    // The downloaded data can now be discarded; the font entry is using the
    // sanitized copy
    NS_Free((void*)aFontData);

    return fe;
}

gfxFontFamily*
gfxUserFontSet::GetFamily(const nsAString& aFamilyName) const
{
    nsAutoString key(aFamilyName);
    ToLowerCase(key);

    return mFontFamilies.GetWeak(key);
}

struct FindFamilyCallbackData {
    gfxFontEntry  *mFontEntry;
    gfxFontFamily *mFamily;
};

static PLDHashOperator
FindFamilyCallback(const nsAString&    aName,
                   gfxMixedFontFamily* aFamily,
                   void*               aUserArg)
{
    FindFamilyCallbackData *d = static_cast<FindFamilyCallbackData*>(aUserArg);
    if (aFamily->ContainsFace(d->mFontEntry)) {
        d->mFamily = aFamily;
        return PL_DHASH_STOP;
    }

    return PL_DHASH_NEXT;
}

gfxFontFamily*
gfxUserFontSet::FindFamilyFor(gfxFontEntry* aFontEntry) const
{
    FindFamilyCallbackData d = { aFontEntry, nullptr };
    mFontFamilies.EnumerateRead(FindFamilyCallback, &d);
    return d.mFamily;
}

///////////////////////////////////////////////////////////////////////////////
// gfxUserFontSet::UserFontCache - re-use platform font entries for user fonts
// across pages/fontsets rather than instantiating new platform fonts.
//
// Entries are added to this cache when a platform font is instantiated from
// downloaded data, and removed when the platform font entry is destroyed.
// We don't need to use a timed expiration scheme here because the gfxFontEntry
// for a downloaded font will be kept alive by its corresponding gfxFont
// instance(s) until they are deleted, and *that* happens using an expiration
// tracker (gfxFontCache). The result is that the downloaded font instances
// recorded here will persist between pages and can get reused (provided the
// source URI and principal match, of course).
///////////////////////////////////////////////////////////////////////////////

nsTHashtable<gfxUserFontSet::UserFontCache::Entry>*
    gfxUserFontSet::UserFontCache::sUserFonts = nullptr;

NS_IMPL_ISUPPORTS(gfxUserFontSet::UserFontCache::Flusher, nsIObserver)

PLDHashOperator
gfxUserFontSet::UserFontCache::Entry::RemoveUnlessPersistent(Entry* aEntry,
                                                             void* aUserData)
{
    return aEntry->mPersistence == kPersistent ? PL_DHASH_NEXT :
                                                 PL_DHASH_REMOVE;
}

PLDHashOperator
gfxUserFontSet::UserFontCache::Entry::RemoveIfPrivate(Entry* aEntry,
                                                      void* aUserData)
{
    return aEntry->mPrivate ? PL_DHASH_REMOVE : PL_DHASH_NEXT;
}

PLDHashOperator
gfxUserFontSet::UserFontCache::Entry::RemoveIfMatches(Entry* aEntry,
                                                      void* aUserData)
{
    return aEntry->GetFontEntry() == static_cast<gfxFontEntry*>(aUserData) ?
        PL_DHASH_REMOVE : PL_DHASH_NEXT;
}

PLDHashOperator
gfxUserFontSet::UserFontCache::Entry::DisconnectSVG(Entry* aEntry,
                                                    void* aUserData)
{
    aEntry->GetFontEntry()->DisconnectSVG();
    return PL_DHASH_NEXT;
}

NS_IMETHODIMP
gfxUserFontSet::UserFontCache::Flusher::Observe(nsISupports* aSubject,
                                                const char* aTopic,
                                                const char16_t* aData)
{
    if (!sUserFonts) {
        return NS_OK;
    }

    if (!strcmp(aTopic, "cacheservice:empty-cache")) {
        sUserFonts->EnumerateEntries(Entry::RemoveUnlessPersistent, nullptr);
    } else if (!strcmp(aTopic, "last-pb-context-exited")) {
        sUserFonts->EnumerateEntries(Entry::RemoveIfPrivate, nullptr);
    } else if (!strcmp(aTopic, "xpcom-shutdown")) {
        sUserFonts->EnumerateEntries(Entry::DisconnectSVG, nullptr);
    } else {
        NS_NOTREACHED("unexpected topic");
    }

    return NS_OK;
}

static bool
IgnorePrincipal(nsIURI *aURI)
{
    nsresult rv;
    bool inherits = false;
    rv = NS_URIChainHasFlags(aURI,
                             nsIProtocolHandler::URI_INHERITS_SECURITY_CONTEXT,
                             &inherits);
    return NS_SUCCEEDED(rv) && inherits;
}

bool
gfxUserFontSet::UserFontCache::Entry::KeyEquals(const KeyTypePointer aKey) const
{
    bool result;
    if (NS_FAILED(mURI->Equals(aKey->mURI, &result)) || !result) {
        return false;
    }

    // For data: URIs, we don't care about the principal; otherwise, check it.
    if (!IgnorePrincipal(mURI)) {
        NS_ASSERTION(mPrincipal && aKey->mPrincipal,
                     "only data: URIs are allowed to omit the principal");
        if (NS_FAILED(mPrincipal->Equals(aKey->mPrincipal, &result)) || !result) {
            return false;
        }
    }

    if (mPrivate != aKey->mPrivate) {
        return false;
    }

    const gfxFontEntry *fe = aKey->mFontEntry;
    if (mFontEntry->mItalic           != fe->mItalic          ||
        mFontEntry->mWeight           != fe->mWeight          ||
        mFontEntry->mStretch          != fe->mStretch         ||
        mFontEntry->mFeatureSettings  != fe->mFeatureSettings ||
        mFontEntry->mLanguageOverride != fe->mLanguageOverride ||
        mFontEntry->mFamilyName       != fe->mFamilyName) {
        return false;
    }

    return true;
}

void
gfxUserFontSet::UserFontCache::CacheFont(gfxFontEntry *aFontEntry,
                                         EntryPersistence aPersistence)
{
    NS_ASSERTION(aFontEntry->mFamilyName.Length() != 0,
                 "caching a font associated with no family yet");
    if (!sUserFonts) {
        sUserFonts = new nsTHashtable<Entry>;

        nsCOMPtr<nsIObserverService> obs =
            mozilla::services::GetObserverService();
        if (obs) {
            Flusher *flusher = new Flusher;
            obs->AddObserver(flusher, "cacheservice:empty-cache",
                             false);
            obs->AddObserver(flusher, "last-pb-context-exited", false);
            obs->AddObserver(flusher, "xpcom-shutdown", false);
        }
    }

    gfxUserFontData *data = aFontEntry->mUserFontData;
    // For data: URIs, the principal is ignored; anyone who has the same
    // data: URI is able to load it and get an equivalent font.
    // Otherwise, the principal is used as part of the cache key.
    nsIPrincipal *principal;
    if (IgnorePrincipal(data->mURI)) {
        principal = nullptr;
    } else {
        principal = data->mPrincipal;
    }
    sUserFonts->PutEntry(Key(data->mURI, principal, aFontEntry,
                             data->mPrivate, aPersistence));

#ifdef DEBUG_USERFONT_CACHE
    printf("userfontcache added fontentry: %p\n", aFontEntry);
    Dump();
#endif
}

void
gfxUserFontSet::UserFontCache::ForgetFont(gfxFontEntry *aFontEntry)
{
    if (!sUserFonts) {
        // if we've already deleted the cache (i.e. during shutdown),
        // just ignore this
        return;
    }

    // We can't simply use RemoveEntry here because it's possible the principal
    // may have changed since the font was cached, in which case the lookup
    // would no longer find the entry (bug 838105).
    sUserFonts->EnumerateEntries(
        gfxUserFontSet::UserFontCache::Entry::RemoveIfMatches, aFontEntry);

#ifdef DEBUG_USERFONT_CACHE
    printf("userfontcache removed fontentry: %p\n", aFontEntry);
    Dump();
#endif
}

gfxFontEntry*
gfxUserFontSet::UserFontCache::GetFont(nsIURI            *aSrcURI,
                                       nsIPrincipal      *aPrincipal,
                                       gfxProxyFontEntry *aProxy,
                                       bool               aPrivate)
{
    if (!sUserFonts) {
        return nullptr;
    }

    // Ignore principal when looking up a data: URI.
    nsIPrincipal *principal;
    if (IgnorePrincipal(aSrcURI)) {
        principal = nullptr;
    } else {
        principal = aPrincipal;
    }

    Entry* entry = sUserFonts->GetEntry(Key(aSrcURI, principal, aProxy,
                                            aPrivate));
    if (entry) {
        return entry->GetFontEntry();
    }

    return nullptr;
}

void
gfxUserFontSet::UserFontCache::Shutdown()
{
    if (sUserFonts) {
        delete sUserFonts;
        sUserFonts = nullptr;
    }
}

#ifdef DEBUG_USERFONT_CACHE

PLDHashOperator
gfxUserFontSet::UserFontCache::Entry::DumpEntry(Entry* aEntry, void* aUserData)
{
    nsresult rv;

    nsAutoCString principalURISpec("(null)");
    bool setDomain = false;

    if (aEntry->mPrincipal) {
        nsCOMPtr<nsIURI> principalURI;
        rv = aEntry->mPrincipal->GetURI(getter_AddRefs(principalURI));
        if (NS_SUCCEEDED(rv)) {
            principalURI->GetSpec(principalURISpec);
        }

        nsCOMPtr<nsIURI> domainURI;
        aEntry->mPrincipal->GetDomain(getter_AddRefs(domainURI));
        if (domainURI) {
            setDomain = true;
        }
    }

    NS_ASSERTION(aEntry->mURI, "null URI in userfont cache entry");

    printf("userfontcache fontEntry: %p fonturihash: %8.8x family: %s domainset: %s principal: [%s]\n",
            aEntry->mFontEntry,
            nsURIHashKey::HashKey(aEntry->mURI),
            NS_ConvertUTF16toUTF8(aEntry->mFontEntry->FamilyName()).get(),
            (setDomain ? "true" : "false"),
            principalURISpec.get()
           );
    return PL_DHASH_NEXT;
}

void
gfxUserFontSet::UserFontCache::Dump()
{
    if (!sUserFonts) {
        return;
    }

    printf("userfontcache dump count: %d ========\n", sUserFonts->Count());
    sUserFonts->EnumerateEntries(Entry::DumpEntry, nullptr);
    printf("userfontcache dump ==================\n");
}

#endif
