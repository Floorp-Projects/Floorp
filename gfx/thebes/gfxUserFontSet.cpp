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
#include "nsIJARChannel.h"
#include "nsIProtocolHandler.h"
#include "nsIPrincipal.h"
#include "nsIZipReader.h"
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

#define LOG(args) PR_LOG(gfxUserFontSet::GetUserFontsLog(), PR_LOG_DEBUG, args)
#define LOG_ENABLED() PR_LOG_TEST(gfxUserFontSet::GetUserFontsLog(), PR_LOG_DEBUG)

static uint64_t sFontSetGeneration = 0;

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

// TODO: support for unicode ranges not yet implemented

gfxProxyFontEntry::gfxProxyFontEntry(gfxUserFontSet *aFontSet,
             const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList,
             uint32_t aWeight,
             int32_t aStretch,
             uint32_t aItalicStyle,
             const nsTArray<gfxFontFeature>& aFeatureSettings,
             uint32_t aLanguageOverride,
             gfxSparseBitSet *aUnicodeRanges)
    : gfxFontEntry(NS_LITERAL_STRING("Proxy")),
      mLoadingState(NOT_LOADING),
      mUnsupportedFormat(false),
      mLoader(nullptr),
      mFontSet(aFontSet)
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

class gfxOTSContext : public ots::OTSContext {
public:
    gfxOTSContext(gfxMixedFontFamily *aFamily, gfxProxyFontEntry  *aProxy)
        : mFamily(aFamily), mProxy(aProxy) {}

    virtual ots::TableAction GetTableAction(uint32_t aTag) MOZ_OVERRIDE {
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

    virtual void Message(const char *format, ...) MSGFUNC_FMT_ATTR MOZ_OVERRIDE {
        va_list va;
        va_start(va, format);

        // buf should be more than adequate for any message OTS generates,
        // so we don't worry about checking the result of vsnprintf()
        char buf[512];
        (void)vsnprintf(buf, sizeof(buf), format, va);

        va_end(va);

        mProxy->mFontSet->LogMessage(mFamily, mProxy, buf);
    }

private:
    gfxMixedFontFamily *mFamily;
    gfxProxyFontEntry  *mProxy;
};

// Call the OTS library to sanitize an sfnt before attempting to use it.
// Returns a newly-allocated block, or nullptr in case of fatal errors.
const uint8_t*
gfxProxyFontEntry::SanitizeOpenTypeData(gfxMixedFontFamily *aFamily,
                                        const uint8_t* aData,
                                        uint32_t       aLength,
                                        uint32_t&      aSaneLength,
                                        bool           aIsCompressed)
{
    // limit output/expansion to 256MB
    ExpandingMemoryStream output(aIsCompressed ? aLength * 2 : aLength,
                                 1024 * 1024 * 256);

    gfxOTSContext otsContext(aFamily, this);

    if (otsContext.Process(&output, aData, aLength)) {
        aSaneLength = output.Tell();
        return static_cast<uint8_t*>(output.forget());
    } else {
        aSaneLength = 0;
        return nullptr;
    }
}

void
gfxProxyFontEntry::StoreUserFontData(gfxFontEntry* aFontEntry,
                                     bool aPrivate,
                                     const nsAString& aOriginalName,
                                     FallibleTArray<uint8_t>* aMetadata,
                                     uint32_t aMetaOrigLen)
{
    if (!aFontEntry->mUserFontData) {
        aFontEntry->mUserFontData = new gfxUserFontData;
    }
    gfxUserFontData* userFontData = aFontEntry->mUserFontData;
    userFontData->mSrcIndex = mSrcIndex;
    const gfxFontFaceSrc& src = mSrcList[mSrcIndex];
    if (src.mIsLocal) {
        userFontData->mLocalName = src.mLocalName;
    } else {
        userFontData->mURI = src.mURI;
        userFontData->mPrincipal = mPrincipal;
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

static void
CopyWOFFMetadata(const uint8_t* aFontData,
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

gfxProxyFontEntry::LoadStatus
gfxProxyFontEntry::LoadNext(gfxMixedFontFamily *aFamily,
                            bool& aLocalRulesUsed)
{
    uint32_t numSrc = mSrcList.Length();

    NS_ASSERTION(mSrcIndex < numSrc,
                 "already at the end of the src list for user font");

    if (mLoadingState == NOT_LOADING) {
        mLoadingState = LOADING_STARTED;
        mUnsupportedFormat = false;
    } else {
        // we were already loading; move to the next source,
        // but don't reset state - if we've already timed out,
        // that counts against the new download
        mSrcIndex++;
    }

    // load each src entry in turn, until a local face is found
    // or a download begins successfully
    while (mSrcIndex < numSrc) {
        const gfxFontFaceSrc& currSrc = mSrcList[mSrcIndex];

        // src local ==> lookup and load immediately

        if (currSrc.mIsLocal) {
            gfxFontEntry *fe =
                gfxPlatform::GetPlatform()->LookupLocalFont(this,
                                                            currSrc.mLocalName);
            aLocalRulesUsed = true;
            if (fe) {
                LOG(("fontset (%p) [src %d] loaded local: (%s) for (%s) gen: %8.8x\n",
                     mFontSet, mSrcIndex,
                     NS_ConvertUTF16toUTF8(currSrc.mLocalName).get(),
                     NS_ConvertUTF16toUTF8(mFamilyName).get(),
                     uint32_t(mFontSet->mGeneration)));
                fe->mFeatureSettings.AppendElements(mFeatureSettings);
                fe->mLanguageOverride = mLanguageOverride;
                fe->mFamilyName = mFamilyName;
                // For src:local(), we don't care whether the request is from
                // a private window as there's no issue of caching resources;
                // local fonts are just available all the time.
                StoreUserFontData(fe, false, nsString(), nullptr, 0);
                mFontSet->ReplaceFontEntry(aFamily, this, fe);
                return STATUS_LOADED;
            } else {
                LOG(("fontset (%p) [src %d] failed local: (%s) for (%s)\n",
                     mFontSet, mSrcIndex,
                     NS_ConvertUTF16toUTF8(currSrc.mLocalName).get(),
                     NS_ConvertUTF16toUTF8(mFamilyName).get()));
            }
        }

        // src url ==> start the load process
        else {
            if (gfxPlatform::GetPlatform()->IsFontFormatSupported(currSrc.mURI,
                    currSrc.mFormatFlags)) {

                nsIPrincipal *principal = nullptr;
                bool bypassCache;
                nsresult rv = mFontSet->CheckFontLoad(&currSrc, &principal,
                                                      &bypassCache);

                if (NS_SUCCEEDED(rv) && principal != nullptr) {
                    if (!bypassCache) {
                        // see if we have an existing entry for this source
                        gfxFontEntry *fe = gfxUserFontSet::
                            UserFontCache::GetFont(currSrc.mURI,
                                                   principal,
                                                   this,
                                                   mFontSet->GetPrivateBrowsing());
                        if (fe) {
                            mFontSet->ReplaceFontEntry(aFamily, this, fe);
                            return STATUS_LOADED;
                        }
                    }

                    // record the principal returned by CheckFontLoad,
                    // for use when creating a channel
                    // and when caching the loaded entry
                    mPrincipal = principal;

                    bool loadDoesntSpin = false;
                    rv = NS_URIChainHasFlags(currSrc.mURI,
                           nsIProtocolHandler::URI_SYNC_LOAD_IS_OK,
                           &loadDoesntSpin);

                    if (NS_SUCCEEDED(rv) && loadDoesntSpin) {
                        uint8_t *buffer = nullptr;
                        uint32_t bufferLength = 0;

                        // sync load font immediately
                        rv = mFontSet->SyncLoadFontData(this, &currSrc, buffer,
                                                        bufferLength);

                        if (NS_SUCCEEDED(rv) &&
                            LoadFont(aFamily, buffer, bufferLength)) {
                            return STATUS_LOADED;
                        } else {
                            mFontSet->LogMessage(aFamily, this,
                                                 "font load failed",
                                                 nsIScriptError::errorFlag,
                                                 rv);
                        }

                    } else {
                        // otherwise load font async
                        rv = mFontSet->StartLoad(aFamily, this, &currSrc);
                        bool loadOK = NS_SUCCEEDED(rv);

                        if (loadOK) {
#ifdef PR_LOGGING
                            if (LOG_ENABLED()) {
                                nsAutoCString fontURI;
                                currSrc.mURI->GetSpec(fontURI);
                                LOG(("userfonts (%p) [src %d] loading uri: (%s) for (%s)\n",
                                     mFontSet, mSrcIndex, fontURI.get(),
                                     NS_ConvertUTF16toUTF8(mFamilyName).get()));
                            }
#endif
                            return STATUS_LOADING;
                        } else {
                            mFontSet->LogMessage(aFamily, this,
                                                 "download failed",
                                                 nsIScriptError::errorFlag,
                                                 rv);
                        }
                    }
                } else {
                    mFontSet->LogMessage(aFamily, this, "download not allowed",
                                         nsIScriptError::errorFlag, rv);
                }
            } else {
                // We don't log a warning to the web console yet,
                // as another source may load successfully
                mUnsupportedFormat = true;
            }
        }

        mSrcIndex++;
    }

    if (mUnsupportedFormat) {
        mFontSet->LogMessage(aFamily, this, "no supported format found",
                             nsIScriptError::warningFlag);
    }

    // all src's failed; mark this entry as unusable (so fallback will occur)
    LOG(("userfonts (%p) failed all src for (%s)\n",
        mFontSet, NS_ConvertUTF16toUTF8(mFamilyName).get()));
    mLoadingState = LOADING_FAILED;

    return STATUS_END_OF_LIST;
}

gfxFontEntry*
gfxProxyFontEntry::LoadFont(gfxMixedFontFamily *aFamily,
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
        SanitizeOpenTypeData(aFamily, aFontData, aLength, saneLen,
                             fontType == GFX_USERFONT_WOFF);
    if (!saneData) {
        mFontSet->LogMessage(aFamily, this, "rejected by sanitizer");
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
        fe = gfxPlatform::GetPlatform()->MakePlatformFont(this,
                                                          saneData,
                                                          saneLen);
        if (!fe) {
            mFontSet->LogMessage(aFamily, this, "not usable by platform");
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
        fe->mFeatureSettings.AppendElements(mFeatureSettings);
        fe->mLanguageOverride = mLanguageOverride;
        fe->mFamilyName = mFamilyName;
        StoreUserFontData(fe, mFontSet->GetPrivateBrowsing(), originalFullName,
                          &metadata, metaOrigLen);
#ifdef PR_LOGGING
        if (LOG_ENABLED()) {
            nsAutoCString fontURI;
            mSrcList[mSrcIndex].mURI->GetSpec(fontURI);
            LOG(("userfonts (%p) [src %d] loaded uri: (%s) for (%s) gen: %8.8x\n",
                 this, mSrcIndex, fontURI.get(),
                 NS_ConvertUTF16toUTF8(mFamilyName).get(),
                 uint32_t(mFontSet->mGeneration)));
        }
#endif
        mFontSet->ReplaceFontEntry(aFamily, this, fe);
        gfxUserFontSet::UserFontCache::CacheFont(fe);
    } else {
#ifdef PR_LOGGING
        if (LOG_ENABLED()) {
            nsAutoCString fontURI;
            mSrcList[mSrcIndex].mURI->GetSpec(fontURI);
            LOG(("userfonts (%p) [src %d] failed uri: (%s) for (%s)"
                 " error making platform font\n",
                 this, mSrcIndex, fontURI.get(),
                 NS_ConvertUTF16toUTF8(mFamilyName).get()));
        }
#endif
    }

    // The downloaded data can now be discarded; the font entry is using the
    // sanitized copy
    NS_Free((void*)aFontData);

    return fe;
}

gfxUserFontSet::gfxUserFontSet()
    : mFontFamilies(4), mLocalRulesUsed(false)
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

already_AddRefed<gfxProxyFontEntry>
gfxUserFontSet::FindOrCreateFontFace(
                               const nsAString& aFamilyName,
                               const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList,
                               uint32_t aWeight,
                               int32_t aStretch,
                               uint32_t aItalicStyle,
                               const nsTArray<gfxFontFeature>& aFeatureSettings,
                               uint32_t aLanguageOverride,
                               gfxSparseBitSet* aUnicodeRanges)
{
    nsRefPtr<gfxProxyFontEntry> entry;

    // If there's already a proxy in the family whose descriptors all match,
    // we can just move it to the end of the list instead of adding a new
    // face that will always "shadow" the old one.
    // Note that we can't do this for "real" (non-proxy) entries, even if the
    // style descriptors match, as they might have had a different source list,
    // but we no longer have the old source list available to check.
    gfxMixedFontFamily* family = LookupFamily(aFamilyName);
    if (family) {
        entry = FindExistingProxyEntry(family, aFontFaceSrcList, aWeight,
                                       aStretch, aItalicStyle, aFeatureSettings,
                                       aLanguageOverride, aUnicodeRanges);
    }

    if (!entry) {
      entry = CreateFontFace(aFontFaceSrcList, aWeight, aStretch,
                             aItalicStyle, aFeatureSettings, aLanguageOverride,
                             aUnicodeRanges);
      entry->mFamilyName = aFamilyName;

#ifdef PR_LOGGING
      if (LOG_ENABLED()) {
          LOG(("userfonts (%p) created \"%s\" (%p) with style: %s weight: %d "
               "stretch: %d",
               this, NS_ConvertUTF16toUTF8(aFamilyName).get(), entry.get(),
               (aItalicStyle & NS_FONT_STYLE_ITALIC ? "italic" :
                   (aItalicStyle & NS_FONT_STYLE_OBLIQUE ? "oblique" : "normal")),
               aWeight, aStretch));
      }
#endif
    }

    return entry.forget();
}

already_AddRefed<gfxProxyFontEntry>
gfxUserFontSet::CreateFontFace(const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList,
                               uint32_t aWeight,
                               int32_t aStretch,
                               uint32_t aItalicStyle,
                               const nsTArray<gfxFontFeature>& aFeatureSettings,
                               uint32_t aLanguageOverride,
                               gfxSparseBitSet* aUnicodeRanges)
{
    MOZ_ASSERT(aWeight != 0,
               "aWeight must not be 0; use NS_FONT_WEIGHT_NORMAL instead");

    nsRefPtr<gfxProxyFontEntry> proxyEntry =
        new gfxProxyFontEntry(this, aFontFaceSrcList, aWeight,
                              aStretch, aItalicStyle, aFeatureSettings,
                              aLanguageOverride, aUnicodeRanges);
    return proxyEntry.forget();
}

gfxProxyFontEntry*
gfxUserFontSet::FindExistingProxyEntry(
                               gfxMixedFontFamily* aFamily,
                               const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList,
                               uint32_t aWeight,
                               int32_t aStretch,
                               uint32_t aItalicStyle,
                               const nsTArray<gfxFontFeature>& aFeatureSettings,
                               uint32_t aLanguageOverride,
                               gfxSparseBitSet* aUnicodeRanges)
{
    MOZ_ASSERT(aWeight != 0,
               "aWeight must not be 0; use NS_FONT_WEIGHT_NORMAL instead");

    nsTArray<nsRefPtr<gfxFontEntry>>& fontList = aFamily->GetFontList();

    for (size_t i = 0, count = fontList.Length(); i < count; i++) {
        if (!fontList[i]->mIsProxy) {
            continue;
        }

        gfxProxyFontEntry* existingProxyEntry =
            static_cast<gfxProxyFontEntry*>(fontList[i].get());
        if (!existingProxyEntry->Matches(aFontFaceSrcList,
                                         aWeight, aStretch, aItalicStyle,
                                         aFeatureSettings, aLanguageOverride,
                                         aUnicodeRanges)) {
            continue;
        }

        return existingProxyEntry;
    }

    return nullptr;
}

void
gfxUserFontSet::AddFontFace(const nsAString& aFamilyName,
                            gfxFontEntry     *aFontEntry)
{
    gfxMixedFontFamily* family = GetFamily(aFamilyName);
    family->AddFontEntry(aFontEntry);

#ifdef PR_LOGGING
    if (LOG_ENABLED()) {
        LOG(("userfonts (%p) added \"%s\" (%p)",
             this, NS_ConvertUTF16toUTF8(aFamilyName).get(), aFontEntry));
    }
#endif
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
    gfxProxyFontEntry::LoadStatus status;

    // NOTE that if all sources in the entry fail, this will delete proxyEntry,
    // so we cannot use it again if status==STATUS_END_OF_LIST
    status = proxyEntry->LoadNext(family, mLocalRulesUsed);

    // if the load succeeded immediately, the font entry was replaced so
    // search again
    if (status == gfxProxyFontEntry::STATUS_LOADED) {
        return family->FindFontForStyle(aFontStyle, aNeedsBold);
    }

    // check whether we should wait for load to complete before painting
    // a fallback font -- but not if all sources failed (bug 633500)
    aWaitForUserFont = (status != gfxProxyFontEntry::STATUS_END_OF_LIST) &&
        (proxyEntry->mLoadingState < gfxProxyFontEntry::LOADING_SLOWLY);

    // if either loading or an error occurred, return null
    return nullptr;
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
        gfxFontEntry *fe = aProxy->LoadFont(aFamily, aFontData, aLength);
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
        moz_free((void*)aFontData);
    }

    // error occurred, load next src
    (void)aProxy->LoadNext(aFamily, mLocalRulesUsed);

    // We ignore the status returned by LoadNext();
    // even if loading failed, we need to bump the font-set generation
    // and return true in order to trigger reflow, so that fallback
    // will be used where the text was "masked" by the pending download
    IncrementGeneration();
    return true;
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

gfxMixedFontFamily*
gfxUserFontSet::LookupFamily(const nsAString& aFamilyName) const
{
    nsAutoString key(aFamilyName);
    ToLowerCase(key);

    return mFontFamilies.GetWeak(key);
}

gfxMixedFontFamily*
gfxUserFontSet::GetFamily(const nsAString& aFamilyName)
{
    nsAutoString key(aFamilyName);
    ToLowerCase(key);

    gfxMixedFontFamily* family = mFontFamilies.GetWeak(key);
    if (!family) {
        family = new gfxMixedFontFamily(aFamilyName);
        mFontFamilies.Put(key, family);
    }
    return family;
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
    const gfxFontEntry *fe = aKey->mFontEntry;
    // CRC32 checking mode
    if (mLength || aKey->mLength) {
        if (aKey->mLength != mLength ||
            aKey->mCRC32 != mCRC32) {
            return false;
        }
    } else {
        bool result;
        if (NS_FAILED(mURI->Equals(aKey->mURI, &result)) || !result) {
            return false;
        }

        // For data: URIs, we don't care about the principal; otherwise, check it.
        if (!IgnorePrincipal(mURI)) {
            NS_ASSERTION(mPrincipal && aKey->mPrincipal,
                         "only data: URIs are allowed to omit the principal");
            if (NS_FAILED(mPrincipal->Equals(aKey->mPrincipal, &result)) ||
                !result) {
                return false;
            }
        }

        if (mPrivate != aKey->mPrivate) {
            return false;
        }
    }

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
    if (data->mLength) {
        MOZ_ASSERT(aPersistence == kPersistent);
        MOZ_ASSERT(!data->mPrivate);
        sUserFonts->PutEntry(Key(data->mCRC32, data->mLength, aFontEntry,
                                 data->mPrivate, aPersistence));
    } else {
        MOZ_ASSERT(aPersistence == kDiscardable);
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
    }

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

    nsCOMPtr<nsIChannel> chan;
    if (NS_FAILED(NS_NewChannel(getter_AddRefs(chan), aSrcURI))) {
        return nullptr;
    }

    nsCOMPtr<nsIJARChannel> jarchan = do_QueryInterface(chan);
    if (!jarchan) {
        return nullptr;
    }

    nsCOMPtr<nsIZipEntry> zipentry;
    if (NS_FAILED(jarchan->GetZipEntry(getter_AddRefs(zipentry)))) {
        return nullptr;
    }

    uint32_t crc32, length;
    zipentry->GetCRC32(&crc32);
    zipentry->GetRealSize(&length);

    entry = sUserFonts->GetEntry(Key(crc32, length, aProxy, aPrivate));
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
