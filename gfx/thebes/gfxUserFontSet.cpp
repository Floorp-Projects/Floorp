/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"

#include "gfxUserFontSet.h"
#include "gfxPlatform.h"
#include "nsContentPolicyUtils.h"
#include "nsUnicharUtils.h"
#include "nsNetUtil.h"
#include "nsIJARChannel.h"
#include "nsIProtocolHandler.h"
#include "nsIPrincipal.h"
#include "nsIZipReader.h"
#include "gfxFontConstants.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/Telemetry.h"
#include "mozilla/gfx/2D.h"
#include "gfxPlatformFontList.h"

#include "opentype-sanitiser.h"
#include "ots-memory-stream.h"

using namespace mozilla;

mozilla::LogModule*
gfxUserFontSet::GetUserFontsLog()
{
    static LazyLogModule sLog("userfonts");
    return sLog;
}

#define LOG(args) MOZ_LOG(gfxUserFontSet::GetUserFontsLog(), mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() MOZ_LOG_TEST(gfxUserFontSet::GetUserFontsLog(), mozilla::LogLevel::Debug)

static uint64_t sFontSetGeneration = 0;

// Based on ots::ExpandingMemoryStream from ots-memory-stream.h,
// adapted to use Mozilla allocators and to allow the final
// memory buffer to be adopted by the client.
class ExpandingMemoryStream : public ots::OTSStream {
public:
    ExpandingMemoryStream(size_t initial, size_t limit)
        : mLength(initial), mLimit(limit), mOff(0) {
        mPtr = moz_xmalloc(mLength);
    }

    ~ExpandingMemoryStream() {
        free(mPtr);
    }

    // Return the buffer, resized to fit its contents (as it may have been
    // over-allocated during growth), and give up ownership of it so the
    // caller becomes responsible to call free() when finished with it.
    void* forget() {
        void* p = moz_xrealloc(mPtr, mOff);
        mPtr = nullptr;
        return p;
    }

    bool WriteRaw(const void* data, size_t length) {
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
            mPtr = moz_xrealloc(mPtr, newLength);
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

gfxUserFontEntry::gfxUserFontEntry(gfxUserFontSet* aFontSet,
             const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList,
             uint32_t aWeight,
             int32_t aStretch,
             uint8_t aStyle,
             const nsTArray<gfxFontFeature>& aFeatureSettings,
             uint32_t aLanguageOverride,
             gfxSparseBitSet* aUnicodeRanges,
             uint8_t aFontDisplay)
    : gfxFontEntry(NS_LITERAL_STRING("userfont")),
      mUserFontLoadState(STATUS_NOT_LOADED),
      mFontDataLoadingState(NOT_LOADING),
      mUnsupportedFormat(false),
      mFontDisplay(aFontDisplay),
      mLoader(nullptr),
      mFontSet(aFontSet)
{
    MOZ_ASSERT(aWeight != 0,
               "aWeight must not be 0; use NS_FONT_WEIGHT_NORMAL instead");
    mIsUserFontContainer = true;
    mSrcList = aFontFaceSrcList;
    mSrcIndex = 0;
    mWeight = aWeight;
    mStretch = aStretch;
    mStyle = aStyle;
    mFeatureSettings.AppendElements(aFeatureSettings);
    mLanguageOverride = aLanguageOverride;

    if (aUnicodeRanges) {
        mCharacterMap = new gfxCharacterMap(*aUnicodeRanges);
    }
}

gfxUserFontEntry::~gfxUserFontEntry()
{
}

bool
gfxUserFontEntry::Matches(const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList,
                          uint32_t aWeight,
                          int32_t aStretch,
                          uint8_t aStyle,
                          const nsTArray<gfxFontFeature>& aFeatureSettings,
                          uint32_t aLanguageOverride,
                          gfxSparseBitSet* aUnicodeRanges,
                          uint8_t aFontDisplay)
{
    return mWeight == aWeight &&
           mStretch == aStretch &&
           mStyle == aStyle &&
           mFeatureSettings == aFeatureSettings &&
           mLanguageOverride == aLanguageOverride &&
           mSrcList == aFontFaceSrcList &&
           mFontDisplay == aFontDisplay &&
           ((!aUnicodeRanges && !mCharacterMap) ||
            (aUnicodeRanges && mCharacterMap && mCharacterMap->Equals(aUnicodeRanges)));
}

gfxFont*
gfxUserFontEntry::CreateFontInstance(const gfxFontStyle* aFontStyle, bool aNeedsBold)
{
    NS_NOTREACHED("should only be creating a gfxFont"
                  " with an actual platform font entry");

    // userfont entry is a container, can't create font from the container
    return nullptr;
}

class gfxOTSContext : public ots::OTSContext {
public:
    explicit gfxOTSContext(gfxUserFontEntry* aUserFontEntry)
        : mUserFontEntry(aUserFontEntry) {}

    virtual ots::TableAction GetTableAction(uint32_t aTag) override {
        // Preserve Graphite, color glyph and SVG tables
        if (
#ifdef RELEASE_BUILD // For Beta/Release, also allow OT Layout tables through
                     // unchecked, and rely on harfbuzz to handle them safely.
            aTag == TRUETYPE_TAG('G', 'D', 'E', 'F') ||
            aTag == TRUETYPE_TAG('G', 'P', 'O', 'S') ||
            aTag == TRUETYPE_TAG('G', 'S', 'U', 'B') ||
#endif
            aTag == TRUETYPE_TAG('S', 'i', 'l', 'f') ||
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

    virtual void Message(int level, const char* format,
                         ...) MSGFUNC_FMT_ATTR override {
        va_list va;
        va_start(va, format);

        nsCString msg;
        msg.AppendPrintf(format, va);

        va_end(va);

        if (level > 0) {
            // For warnings (rather than errors that cause the font to fail),
            // we only report the first instance of any given message.
            if (mWarningsIssued.Contains(msg)) {
                return;
            }
            mWarningsIssued.PutEntry(msg);
        }

        mUserFontEntry->mFontSet->LogMessage(mUserFontEntry, msg.get());
    }

private:
    gfxUserFontEntry* mUserFontEntry;
    nsTHashtable<nsCStringHashKey> mWarningsIssued;
};

// Call the OTS library to sanitize an sfnt before attempting to use it.
// Returns a newly-allocated block, or nullptr in case of fatal errors.
const uint8_t*
gfxUserFontEntry::SanitizeOpenTypeData(const uint8_t* aData,
                                       uint32_t       aLength,
                                       uint32_t&      aSaneLength,
                                       gfxUserFontType aFontType)
{
    if (aFontType == GFX_USERFONT_UNKNOWN) {
        aSaneLength = 0;
        return nullptr;
    }

    uint32_t lengthHint = aLength;
    if (aFontType == GFX_USERFONT_WOFF) {
        lengthHint *= 2;
    } else if (aFontType == GFX_USERFONT_WOFF2) {
        lengthHint *= 3;
    }

    // limit output/expansion to 256MB
    ExpandingMemoryStream output(lengthHint, 1024 * 1024 * 256);

    gfxOTSContext otsContext(this);
    if (!otsContext.Process(&output, aData, aLength)) {
        // Failed to decode/sanitize the font, so discard it.
        aSaneLength = 0;
        return nullptr;
    }

    aSaneLength = output.Tell();
    return static_cast<const uint8_t*>(output.forget());
}

void
gfxUserFontEntry::StoreUserFontData(gfxFontEntry* aFontEntry,
                                    bool aPrivate,
                                    const nsAString& aOriginalName,
                                    FallibleTArray<uint8_t>* aMetadata,
                                    uint32_t aMetaOrigLen,
                                    uint8_t aCompression)
{
    if (!aFontEntry->mUserFontData) {
        aFontEntry->mUserFontData = MakeUnique<gfxUserFontData>();
    }
    gfxUserFontData* userFontData = aFontEntry->mUserFontData.get();
    userFontData->mSrcIndex = mSrcIndex;
    const gfxFontFaceSrc& src = mSrcList[mSrcIndex];
    switch (src.mSourceType) {
        case gfxFontFaceSrc::eSourceType_Local:
            userFontData->mLocalName = src.mLocalName;
            break;
        case gfxFontFaceSrc::eSourceType_URL:
            userFontData->mURI = src.mURI;
            userFontData->mPrincipal = mPrincipal;
            break;
        case gfxFontFaceSrc::eSourceType_Buffer:
            userFontData->mIsBuffer = true;
            break;
    }
    userFontData->mPrivate = aPrivate;
    userFontData->mFormat = src.mFormatFlags;
    userFontData->mRealName = aOriginalName;
    if (aMetadata) {
        userFontData->mMetadata.SwapElements(*aMetadata);
        userFontData->mMetaOrigLen = aMetaOrigLen;
        userFontData->mCompression = aCompression;
    }
}

size_t
gfxUserFontData::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
    return aMallocSizeOf(this)
           + mMetadata.ShallowSizeOfExcludingThis(aMallocSizeOf)
           + mLocalName.SizeOfExcludingThisIfUnshared(aMallocSizeOf)
           + mRealName.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
    // Not counting mURI and mPrincipal, as those will be shared.
}

void
gfxUserFontEntry::GetFamilyNameAndURIForLogging(nsACString& aFamilyName,
                                                nsACString& aURI)
{
  aFamilyName.Assign(NS_ConvertUTF16toUTF8(mFamilyName));

  aURI.Truncate();
  if (mSrcIndex == mSrcList.Length()) {
    aURI.AppendLiteral("(end of source list)");
  } else {
    if (mSrcList[mSrcIndex].mURI) {
      mSrcList[mSrcIndex].mURI->GetSpec(aURI);
    } else {
      aURI.AppendLiteral("(invalid URI)");
    }
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

struct WOFF2Header {
    AutoSwap_PRUint32 signature;
    AutoSwap_PRUint32 flavor;
    AutoSwap_PRUint32 length;
    AutoSwap_PRUint16 numTables;
    AutoSwap_PRUint16 reserved;
    AutoSwap_PRUint32 totalSfntSize;
    AutoSwap_PRUint32 totalCompressedSize;
    AutoSwap_PRUint16 majorVersion;
    AutoSwap_PRUint16 minorVersion;
    AutoSwap_PRUint32 metaOffset;
    AutoSwap_PRUint32 metaCompLen;
    AutoSwap_PRUint32 metaOrigLen;
    AutoSwap_PRUint32 privOffset;
    AutoSwap_PRUint32 privLen;
};

template<typename HeaderT>
void
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
    if (aLength < sizeof(HeaderT)) {
        return;
    }
    const HeaderT* woff =
        reinterpret_cast<const HeaderT*>(aFontData);
    uint32_t metaOffset = woff->metaOffset;
    uint32_t metaCompLen = woff->metaCompLen;
    if (!metaOffset || !metaCompLen || !woff->metaOrigLen) {
        return;
    }
    if (metaOffset >= aLength || metaCompLen > aLength - metaOffset) {
        return;
    }
    if (!aMetadata->SetLength(woff->metaCompLen, fallible)) {
        return;
    }
    memcpy(aMetadata->Elements(), aFontData + metaOffset, metaCompLen);
    *aMetaOrigLen = woff->metaOrigLen;
}

void
gfxUserFontEntry::LoadNextSrc()
{
    uint32_t numSrc = mSrcList.Length();

    NS_ASSERTION(mSrcIndex < numSrc,
                 "already at the end of the src list for user font");
    NS_ASSERTION((mUserFontLoadState == STATUS_NOT_LOADED ||
                  mUserFontLoadState == STATUS_LOADING) &&
                 mFontDataLoadingState < LOADING_FAILED,
                 "attempting to load a font that has either completed or failed");

    if (mUserFontLoadState == STATUS_NOT_LOADED) {
        SetLoadState(STATUS_LOADING);
        mFontDataLoadingState = LOADING_STARTED;
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
        gfxFontFaceSrc& currSrc = mSrcList[mSrcIndex];

        // src local ==> lookup and load immediately

        if (currSrc.mSourceType == gfxFontFaceSrc::eSourceType_Local) {
            gfxFontEntry* fe =
                gfxPlatform::GetPlatform()->LookupLocalFont(currSrc.mLocalName,
                                                            mWeight,
                                                            mStretch,
                                                            mStyle);
            nsTArray<gfxUserFontSet*> fontSets;
            GetUserFontSets(fontSets);
            for (gfxUserFontSet* fontSet : fontSets) {
                // We need to note on each gfxUserFontSet that contains the user
                // font entry that we used a local() rule.
                fontSet->SetLocalRulesUsed();
            }
            if (fe) {
                LOG(("userfonts (%p) [src %d] loaded local: (%s) for (%s) gen: %8.8x\n",
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
                StoreUserFontData(fe, false, nsString(), nullptr, 0,
                                  gfxUserFontData::kUnknownCompression);
                mPlatformFontEntry = fe;
                SetLoadState(STATUS_LOADED);
                Telemetry::Accumulate(Telemetry::WEBFONT_SRCTYPE,
                                      currSrc.mSourceType + 1);
                return;
            } else {
                LOG(("userfonts (%p) [src %d] failed local: (%s) for (%s)\n",
                     mFontSet, mSrcIndex,
                     NS_ConvertUTF16toUTF8(currSrc.mLocalName).get(),
                     NS_ConvertUTF16toUTF8(mFamilyName).get()));
            }
        }

        // src url ==> start the load process
        else if (currSrc.mSourceType == gfxFontFaceSrc::eSourceType_URL) {
            if (gfxPlatform::GetPlatform()->IsFontFormatSupported(currSrc.mURI,
                    currSrc.mFormatFlags)) {

                nsIPrincipal* principal = nullptr;
                bool bypassCache;
                nsresult rv = mFontSet->CheckFontLoad(&currSrc, &principal,
                                                      &bypassCache);

                if (NS_SUCCEEDED(rv) && principal != nullptr) {
                    if (!bypassCache) {
                        // see if we have an existing entry for this source
                        gfxFontEntry* fe = gfxUserFontSet::
                            UserFontCache::GetFont(currSrc.mURI,
                                                   principal,
                                                   this,
                                                   mFontSet->GetPrivateBrowsing());
                        if (fe) {
                            mPlatformFontEntry = fe;
                            SetLoadState(STATUS_LOADED);
                            if (LOG_ENABLED()) {
                                LOG(("userfonts (%p) [src %d] "
                                     "loaded uri from cache: (%s) for (%s)\n",
                                     mFontSet, mSrcIndex,
                                     currSrc.mURI->GetSpecOrDefault().get(),
                                     NS_ConvertUTF16toUTF8(mFamilyName).get()));
                            }
                            return;
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
                        uint8_t* buffer = nullptr;
                        uint32_t bufferLength = 0;

                        // sync load font immediately
                        rv = mFontSet->SyncLoadFontData(this, &currSrc, buffer,
                                                        bufferLength);

                        if (NS_SUCCEEDED(rv) &&
                            LoadPlatformFont(buffer, bufferLength)) {
                            SetLoadState(STATUS_LOADED);
                            Telemetry::Accumulate(Telemetry::WEBFONT_SRCTYPE,
                                                  currSrc.mSourceType + 1);
                            return;
                        } else {
                            mFontSet->LogMessage(this,
                                                 "font load failed",
                                                 nsIScriptError::errorFlag,
                                                 rv);
                        }

                    } else {
                        // otherwise load font async
                        rv = mFontSet->StartLoad(this, &currSrc);
                        bool loadOK = NS_SUCCEEDED(rv);

                        if (loadOK) {
                            if (LOG_ENABLED()) {
                                LOG(("userfonts (%p) [src %d] loading uri: (%s) for (%s)\n",
                                     mFontSet, mSrcIndex,
                                     currSrc.mURI->GetSpecOrDefault().get(),
                                     NS_ConvertUTF16toUTF8(mFamilyName).get()));
                            }
                            return;
                        } else {
                            mFontSet->LogMessage(this,
                                                 "download failed",
                                                 nsIScriptError::errorFlag,
                                                 rv);
                        }
                    }
                } else {
                    mFontSet->LogMessage(this, "download not allowed",
                                         nsIScriptError::errorFlag, rv);
                }
            } else {
                // We don't log a warning to the web console yet,
                // as another source may load successfully
                mUnsupportedFormat = true;
            }
        }

        // FontFace buffer ==> load immediately

        else {
            MOZ_ASSERT(currSrc.mSourceType == gfxFontFaceSrc::eSourceType_Buffer);

            uint8_t* buffer = nullptr;
            uint32_t bufferLength = 0;

            // sync load font immediately
            currSrc.mBuffer->TakeBuffer(buffer, bufferLength);
            if (buffer && LoadPlatformFont(buffer, bufferLength)) {
                // LoadPlatformFont takes ownership of the buffer, so no need
                // to free it here.
                SetLoadState(STATUS_LOADED);
                Telemetry::Accumulate(Telemetry::WEBFONT_SRCTYPE,
                                      currSrc.mSourceType + 1);
                return;
            } else {
                mFontSet->LogMessage(this,
                                     "font load failed",
                                     nsIScriptError::errorFlag);
            }
        }

        mSrcIndex++;
    }

    if (mUnsupportedFormat) {
        mFontSet->LogMessage(this, "no supported format found",
                             nsIScriptError::warningFlag);
    }

    // all src's failed; mark this entry as unusable (so fallback will occur)
    LOG(("userfonts (%p) failed all src for (%s)\n",
        mFontSet, NS_ConvertUTF16toUTF8(mFamilyName).get()));
    mFontDataLoadingState = LOADING_FAILED;
    SetLoadState(STATUS_FAILED);
}

void
gfxUserFontEntry::SetLoadState(UserFontLoadState aLoadState)
{
    mUserFontLoadState = aLoadState;
}

MOZ_DEFINE_MALLOC_SIZE_OF_ON_ALLOC(UserFontMallocSizeOfOnAlloc)

bool
gfxUserFontEntry::LoadPlatformFont(const uint8_t* aFontData, uint32_t& aLength)
{
    NS_ASSERTION((mUserFontLoadState == STATUS_NOT_LOADED ||
                  mUserFontLoadState == STATUS_LOADING) &&
                 mFontDataLoadingState < LOADING_FAILED,
                 "attempting to load a font that has either completed or failed");

    gfxFontEntry* fe = nullptr;

    gfxUserFontType fontType =
        gfxFontUtils::DetermineFontDataType(aFontData, aLength);
    Telemetry::Accumulate(Telemetry::WEBFONT_FONTTYPE, uint32_t(fontType));

    // Unwrap/decompress/sanitize or otherwise munge the downloaded data
    // to make a usable sfnt structure.

    // Because platform font activation code may replace the name table
    // in the font with a synthetic one, we save the original name so that
    // it can be reported via the nsIDOMFontFace API.
    nsAutoString originalFullName;

    // Call the OTS sanitizer; this will also decode WOFF to sfnt
    // if necessary. The original data in aFontData is left unchanged.
    uint32_t saneLen;
    uint32_t fontCompressionRatio = 0;
    size_t computedSize = 0;
    const uint8_t* saneData =
        SanitizeOpenTypeData(aFontData, aLength, saneLen, fontType);
    if (!saneData) {
        mFontSet->LogMessage(this, "rejected by sanitizer");
    }
    if (saneData) {
        if (saneLen) {
            fontCompressionRatio = uint32_t(100.0 * aLength / saneLen + 0.5);
            if (fontType == GFX_USERFONT_WOFF ||
                fontType == GFX_USERFONT_WOFF2) {
                Telemetry::Accumulate(fontType == GFX_USERFONT_WOFF ?
                                      Telemetry::WEBFONT_COMPRESSION_WOFF :
                                      Telemetry::WEBFONT_COMPRESSION_WOFF2,
                                      fontCompressionRatio);
                }
        }

        // The sanitizer ensures that we have a valid sfnt and a usable
        // name table, so this should never fail unless we're out of
        // memory, and GetFullNameFromSFNT is not directly exposed to
        // arbitrary/malicious data from the web.
        gfxFontUtils::GetFullNameFromSFNT(saneData, saneLen,
                                          originalFullName);

        // Record size for memory reporting purposes. We measure this now
        // because by the time we potentially want to collect reports, this
        // data block may have been handed off to opaque OS font APIs that
        // don't allow us to retrieve or measure it directly.
        // The *OnAlloc function will also tell DMD about this block, as the
        // OS font code may hold on to it for an extended period.
        computedSize = UserFontMallocSizeOfOnAlloc(saneData);

        // Here ownership of saneData is passed to the platform,
        // which will delete it when no longer required
        fe = gfxPlatform::GetPlatform()->MakePlatformFont(mName,
                                                          mWeight,
                                                          mStretch,
                                                          mStyle,
                                                          saneData,
                                                          saneLen);
        if (!fe) {
            mFontSet->LogMessage(this, "not usable by platform");
        }
    }

    if (fe) {
        fe->mComputedSizeOfUserFont = computedSize;

        // Save a copy of the metadata block (if present) for nsIDOMFontFace
        // to use if required. Ownership of the metadata block will be passed
        // to the gfxUserFontData record below.
        FallibleTArray<uint8_t> metadata;
        uint32_t metaOrigLen = 0;
        uint8_t compression = gfxUserFontData::kUnknownCompression;
        if (fontType == GFX_USERFONT_WOFF) {
            CopyWOFFMetadata<WOFFHeader>(aFontData, aLength,
                                         &metadata, &metaOrigLen);
            compression = gfxUserFontData::kZlibCompression;
        } else if (fontType == GFX_USERFONT_WOFF2) {
            CopyWOFFMetadata<WOFF2Header>(aFontData, aLength,
                                          &metadata, &metaOrigLen);
            compression = gfxUserFontData::kBrotliCompression;
        }

        // copy OpenType feature/language settings from the userfont entry to the
        // newly-created font entry
        fe->mFeatureSettings.AppendElements(mFeatureSettings);
        fe->mLanguageOverride = mLanguageOverride;
        fe->mFamilyName = mFamilyName;
        StoreUserFontData(fe, mFontSet->GetPrivateBrowsing(), originalFullName,
                          &metadata, metaOrigLen, compression);
        if (LOG_ENABLED()) {
            LOG(("userfonts (%p) [src %d] loaded uri: (%s) for (%s) "
                 "(%p) gen: %8.8x compress: %d%%\n",
                 mFontSet, mSrcIndex,
                 mSrcList[mSrcIndex].mURI->GetSpecOrDefault().get(),
                 NS_ConvertUTF16toUTF8(mFamilyName).get(),
                 this, uint32_t(mFontSet->mGeneration), fontCompressionRatio));
        }
        mPlatformFontEntry = fe;
        SetLoadState(STATUS_LOADED);
        gfxUserFontSet::UserFontCache::CacheFont(fe);
    } else {
        if (LOG_ENABLED()) {
            LOG(("userfonts (%p) [src %d] failed uri: (%s) for (%s)"
                 " error making platform font\n",
                 mFontSet, mSrcIndex,
                 mSrcList[mSrcIndex].mURI->GetSpecOrDefault().get(),
                 NS_ConvertUTF16toUTF8(mFamilyName).get()));
        }
    }

    // The downloaded data can now be discarded; the font entry is using the
    // sanitized copy
    free((void*)aFontData);

    return fe != nullptr;
}

void
gfxUserFontEntry::Load()
{
    if (mUserFontLoadState == STATUS_NOT_LOADED) {
        LoadNextSrc();
    }
}

void
gfxUserFontEntry::IncrementGeneration()
{
    nsTArray<gfxUserFontSet*> fontSets;
    GetUserFontSets(fontSets);
    for (gfxUserFontSet* fontSet : fontSets) {
        fontSet->IncrementGeneration();
    }
}

// This is called when a font download finishes.
// Ownership of aFontData passes in here, and the font set must
// ensure that it is eventually deleted via free().
bool
gfxUserFontEntry::FontDataDownloadComplete(const uint8_t* aFontData,
                                           uint32_t aLength,
                                           nsresult aDownloadStatus)
{
    // forget about the loader, as we no longer potentially need to cancel it
    // if the entry is obsoleted
    mLoader = nullptr;

    // download successful, make platform font using font data
    if (NS_SUCCEEDED(aDownloadStatus) &&
        mFontDataLoadingState != LOADING_TIMED_OUT) {
        bool loaded = LoadPlatformFont(aFontData, aLength);
        aFontData = nullptr;

        if (loaded) {
            IncrementGeneration();
            return true;
        }

    } else {
        // download failed
        mFontSet->LogMessage(this,
                             (mFontDataLoadingState != LOADING_TIMED_OUT ?
                              "download failed" : "download timed out"),
                             nsIScriptError::errorFlag,
                             aDownloadStatus);
    }

    if (aFontData) {
        free((void*)aFontData);
    }

    // error occurred, load next src if load not yet timed out
    if (mFontDataLoadingState != LOADING_TIMED_OUT) {
      LoadNextSrc();
    }

    // We ignore the status returned by LoadNext();
    // even if loading failed, we need to bump the font-set generation
    // and return true in order to trigger reflow, so that fallback
    // will be used where the text was "masked" by the pending download
    IncrementGeneration();
    return true;
}

void
gfxUserFontEntry::GetUserFontSets(nsTArray<gfxUserFontSet*>& aResult)
{
    aResult.Clear();
    aResult.AppendElement(mFontSet);
}

gfxUserFontSet::gfxUserFontSet()
    : mFontFamilies(4),
      mLocalRulesUsed(false),
      mRebuildLocalRules(false),
      mDownloadCount(0),
      mDownloadSize(0)
{
    IncrementGeneration(true);
    gfxPlatformFontList* fp = gfxPlatformFontList::PlatformFontList();
    if (fp) {
        fp->AddUserFontSet(this);
    }
}

gfxUserFontSet::~gfxUserFontSet()
{
    gfxPlatformFontList* fp = gfxPlatformFontList::PlatformFontList();
    if (fp) {
        fp->RemoveUserFontSet(this);
    }
}

already_AddRefed<gfxUserFontEntry>
gfxUserFontSet::FindOrCreateUserFontEntry(
                               const nsAString& aFamilyName,
                               const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList,
                               uint32_t aWeight,
                               int32_t aStretch,
                               uint8_t aStyle,
                               const nsTArray<gfxFontFeature>& aFeatureSettings,
                               uint32_t aLanguageOverride,
                               gfxSparseBitSet* aUnicodeRanges,
                               uint8_t aFontDisplay)
{
    RefPtr<gfxUserFontEntry> entry;

    // If there's already a userfont entry in the family whose descriptors all match,
    // we can just move it to the end of the list instead of adding a new
    // face that will always "shadow" the old one.
    // Note that we can't do this for platform font entries, even if the
    // style descriptors match, as they might have had a different source list,
    // but we no longer have the old source list available to check.
    gfxUserFontFamily* family = LookupFamily(aFamilyName);
    if (family) {
        entry = FindExistingUserFontEntry(family, aFontFaceSrcList, aWeight,
                                          aStretch, aStyle,
                                          aFeatureSettings, aLanguageOverride,
                                          aUnicodeRanges, aFontDisplay);
    }

    if (!entry) {
      entry = CreateUserFontEntry(aFontFaceSrcList, aWeight, aStretch,
                                  aStyle, aFeatureSettings,
                                  aLanguageOverride, aUnicodeRanges,
                                  aFontDisplay);
      entry->mFamilyName = aFamilyName;
    }

    return entry.forget();
}

already_AddRefed<gfxUserFontEntry>
gfxUserFontSet::CreateUserFontEntry(
                               const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList,
                               uint32_t aWeight,
                               int32_t aStretch,
                               uint8_t aStyle,
                               const nsTArray<gfxFontFeature>& aFeatureSettings,
                               uint32_t aLanguageOverride,
                               gfxSparseBitSet* aUnicodeRanges,
                               uint8_t aFontDisplay)
{

    RefPtr<gfxUserFontEntry> userFontEntry =
        new gfxUserFontEntry(this, aFontFaceSrcList, aWeight,
                              aStretch, aStyle, aFeatureSettings,
                              aLanguageOverride, aUnicodeRanges, aFontDisplay);
    return userFontEntry.forget();
}

gfxUserFontEntry*
gfxUserFontSet::FindExistingUserFontEntry(
                               gfxUserFontFamily* aFamily,
                               const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList,
                               uint32_t aWeight,
                               int32_t aStretch,
                               uint8_t aStyle,
                               const nsTArray<gfxFontFeature>& aFeatureSettings,
                               uint32_t aLanguageOverride,
                               gfxSparseBitSet* aUnicodeRanges,
                               uint8_t aFontDisplay)
{
    MOZ_ASSERT(aWeight != 0,
               "aWeight must not be 0; use NS_FONT_WEIGHT_NORMAL instead");

    nsTArray<RefPtr<gfxFontEntry>>& fontList = aFamily->GetFontList();

    for (size_t i = 0, count = fontList.Length(); i < count; i++) {
        if (!fontList[i]->mIsUserFontContainer) {
            continue;
        }

        gfxUserFontEntry* existingUserFontEntry =
            static_cast<gfxUserFontEntry*>(fontList[i].get());
        if (!existingUserFontEntry->Matches(aFontFaceSrcList,
                                            aWeight, aStretch, aStyle,
                                            aFeatureSettings, aLanguageOverride,
                                            aUnicodeRanges, aFontDisplay)) {
            continue;
        }

        return existingUserFontEntry;
    }

    return nullptr;
}

void
gfxUserFontSet::AddUserFontEntry(const nsAString& aFamilyName,
                                 gfxUserFontEntry* aUserFontEntry)
{
    gfxUserFontFamily* family = GetFamily(aFamilyName);
    family->AddFontEntry(aUserFontEntry);

    if (LOG_ENABLED()) {
        LOG(("userfonts (%p) added to \"%s\" (%p) style: %s weight: %d "
             "stretch: %d display: %d",
             this, NS_ConvertUTF16toUTF8(aFamilyName).get(), aUserFontEntry,
             (aUserFontEntry->IsItalic() ? "italic" :
              (aUserFontEntry->IsOblique() ? "oblique" : "normal")),
             aUserFontEntry->Weight(), aUserFontEntry->Stretch(),
             aUserFontEntry->GetFontDisplay()));
    }
}

gfxUserFontEntry*
gfxUserFontSet::FindUserFontEntryAndLoad(gfxFontFamily* aFamily,
                                         const gfxFontStyle& aFontStyle,
                                         bool& aNeedsBold,
                                         bool& aWaitForUserFont)
{
    aWaitForUserFont = false;
    gfxFontEntry* fe = aFamily->FindFontForStyle(aFontStyle, aNeedsBold);
    NS_ASSERTION(!fe || fe->mIsUserFontContainer,
                 "should only have userfont entries in userfont families");
    if (!fe) {
        return nullptr;
    }

    gfxUserFontEntry* userFontEntry = static_cast<gfxUserFontEntry*>(fe);

    // start the load if it hasn't been loaded
    userFontEntry->Load();
    if (userFontEntry->GetPlatformFontEntry()) {
        return userFontEntry;
    }

    aWaitForUserFont = userFontEntry->WaitForUserFont();
    return nullptr;
}

void
gfxUserFontSet::IncrementGeneration(bool aIsRebuild)
{
    // add one, increment again if zero
    ++sFontSetGeneration;
    if (sFontSetGeneration == 0)
       ++sFontSetGeneration;
    mGeneration = sFontSetGeneration;
    if (aIsRebuild) {
        mRebuildGeneration = mGeneration;
    }
}

void
gfxUserFontSet::RebuildLocalRules()
{
    if (mLocalRulesUsed) {
        mRebuildLocalRules = true;
        DoRebuildUserFontSet();
    }
}

gfxUserFontFamily*
gfxUserFontSet::LookupFamily(const nsAString& aFamilyName) const
{
    nsAutoString key(aFamilyName);
    ToLowerCase(key);

    return mFontFamilies.GetWeak(key);
}

bool
gfxUserFontSet::ContainsUserFontSetFonts(const FontFamilyList& aFontList) const
{
    for (const FontFamilyName& name : aFontList.GetFontlist()) {
        if (name.mType != eFamily_named &&
            name.mType != eFamily_named_quoted) {
            continue;
        }
        if (LookupFamily(name.mName)) {
            return true;
        }
    }
    return false;
}

gfxUserFontFamily*
gfxUserFontSet::GetFamily(const nsAString& aFamilyName)
{
    nsAutoString key(aFamilyName);
    ToLowerCase(key);

    gfxUserFontFamily* family = mFontFamilies.GetWeak(key);
    if (!family) {
        family = new gfxUserFontFamily(aFamilyName);
        mFontFamilies.Put(key, family);
    }
    return family;
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

NS_IMETHODIMP
gfxUserFontSet::UserFontCache::Flusher::Observe(nsISupports* aSubject,
                                                const char* aTopic,
                                                const char16_t* aData)
{
    if (!sUserFonts) {
        return NS_OK;
    }

    if (!strcmp(aTopic, "cacheservice:empty-cache")) {
        for (auto i = sUserFonts->Iter(); !i.Done(); i.Next()) {
            if (!i.Get()->IsPersistent()) {
                i.Remove();
            }
        }
    } else if (!strcmp(aTopic, "last-pb-context-exited")) {
        for (auto i = sUserFonts->Iter(); !i.Done(); i.Next()) {
            if (i.Get()->IsPrivate()) {
                i.Remove();
            }
        }
    } else if (!strcmp(aTopic, "xpcom-shutdown")) {
        for (auto i = sUserFonts->Iter(); !i.Done(); i.Next()) {
            i.Get()->GetFontEntry()->DisconnectSVG();
        }
    } else {
        NS_NOTREACHED("unexpected topic");
    }

    return NS_OK;
}

static bool
IgnorePrincipal(nsIURI* aURI)
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
    const gfxFontEntry* fe = aKey->mFontEntry;
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

    if (mFontEntry->mStyle            != fe->mStyle     ||
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
gfxUserFontSet::UserFontCache::CacheFont(gfxFontEntry* aFontEntry,
                                         EntryPersistence aPersistence)
{
    NS_ASSERTION(aFontEntry->mFamilyName.Length() != 0,
                 "caching a font associated with no family yet");

    // if caching is disabled, simply return
    if (Preferences::GetBool("gfx.downloadable_fonts.disable_cache")) {
        return;
    }

    gfxUserFontData* data = aFontEntry->mUserFontData.get();
    if (data->mIsBuffer) {
#ifdef DEBUG_USERFONT_CACHE
        printf("userfontcache skipped fontentry with buffer source: %p\n",
               aFontEntry);
#endif
        return;
    }

    if (!sUserFonts) {
        sUserFonts = new nsTHashtable<Entry>;

        nsCOMPtr<nsIObserverService> obs =
            mozilla::services::GetObserverService();
        if (obs) {
            Flusher* flusher = new Flusher;
            obs->AddObserver(flusher, "cacheservice:empty-cache",
                             false);
            obs->AddObserver(flusher, "last-pb-context-exited", false);
            obs->AddObserver(flusher, "xpcom-shutdown", false);
        }

        // Create and register a memory reporter for sUserFonts.
        // This reporter is never unregistered, but that's OK because
        // the reporter checks whether sUserFonts is null, so it would
        // be safe to call even after UserFontCache::Shutdown has deleted
        // the cache.
        RegisterStrongMemoryReporter(new MemoryReporter());
    }

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
        nsIPrincipal* principal;
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
gfxUserFontSet::UserFontCache::ForgetFont(gfxFontEntry* aFontEntry)
{
    if (!sUserFonts) {
        // if we've already deleted the cache (i.e. during shutdown),
        // just ignore this
        return;
    }

    // We can't simply use RemoveEntry here because it's possible the principal
    // may have changed since the font was cached, in which case the lookup
    // would no longer find the entry (bug 838105).
    for (auto i = sUserFonts->Iter(); !i.Done(); i.Next()) {
        if (i.Get()->GetFontEntry() == aFontEntry)  {
            i.Remove();
        }
    }

#ifdef DEBUG_USERFONT_CACHE
    printf("userfontcache removed fontentry: %p\n", aFontEntry);
    Dump();
#endif
}

gfxFontEntry*
gfxUserFontSet::UserFontCache::GetFont(nsIURI* aSrcURI,
                                       nsIPrincipal* aPrincipal,
                                       gfxUserFontEntry* aUserFontEntry,
                                       bool aPrivate)
{
    if (!sUserFonts ||
        Preferences::GetBool("gfx.downloadable_fonts.disable_cache")) {
        return nullptr;
    }

    // We have to perform another content policy check here to prevent
    // cache poisoning. E.g. a.com loads a font into the cache but
    // b.com has a CSP not allowing any fonts to be loaded.
    if (!aUserFontEntry->mFontSet->IsFontLoadAllowed(aSrcURI, aPrincipal)) {
        return nullptr;
    }

    // Ignore principal when looking up a data: URI.
    nsIPrincipal* principal;
    if (IgnorePrincipal(aSrcURI)) {
        principal = nullptr;
    } else {
        principal = aPrincipal;
    }

    Entry* entry = sUserFonts->GetEntry(Key(aSrcURI, principal, aUserFontEntry,
                                            aPrivate));
    if (entry) {
        return entry->GetFontEntry();
    }

    // The channel is never openend; to be conservative we use the most
    // restrictive security flag: SEC_REQUIRE_SAME_ORIGIN_DATA_INHERITS.
    nsCOMPtr<nsIChannel> chan;
    if (NS_FAILED(NS_NewChannel(getter_AddRefs(chan),
                                aSrcURI,
                                aPrincipal,
                                nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_INHERITS,
                                nsIContentPolicy::TYPE_FONT))) {
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

    entry = sUserFonts->GetEntry(Key(crc32, length, aUserFontEntry, aPrivate));
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

MOZ_DEFINE_MALLOC_SIZE_OF(UserFontsMallocSizeOf)

void
gfxUserFontSet::UserFontCache::Entry::ReportMemory(
    nsIHandleReportCallback* aHandleReport, nsISupports* aData, bool aAnonymize)
{
    MOZ_ASSERT(mFontEntry);
    nsAutoCString path("explicit/gfx/user-fonts/font(");

    if (aAnonymize) {
        path.AppendPrintf("<anonymized-%p>", this);
    } else {
        NS_ConvertUTF16toUTF8 familyName(mFontEntry->mFamilyName);
        path.AppendPrintf("family=%s", familyName.get());
        if (mURI) {
            nsCString spec = mURI->GetSpecOrDefault();
            spec.ReplaceChar('/', '\\');
            // Some fonts are loaded using horrendously-long data: URIs;
            // truncate those before reporting them.
            bool isData;
            if (NS_SUCCEEDED(mURI->SchemeIs("data", &isData)) && isData &&
                spec.Length() > 255) {
                spec.Truncate(252);
                spec.Append("...");
            }
            path.AppendPrintf(", url=%s", spec.get());
        }
        if (mPrincipal) {
            nsCOMPtr<nsIURI> uri;
            mPrincipal->GetURI(getter_AddRefs(uri));
            if (uri) {
                nsCString spec = uri->GetSpecOrDefault();
                if (!spec.IsEmpty()) {
                    // Include a clue as to who loaded this resource. (Note
                    // that because of font entry sharing, other pages may now
                    // be using this resource, and the original page may not
                    // even be loaded any longer.)
                    spec.ReplaceChar('/', '\\');
                    path.AppendPrintf(", principal=%s", spec.get());
                }
            }
        }
    }
    path.Append(')');

    aHandleReport->Callback(
        EmptyCString(), path,
        nsIMemoryReporter::KIND_HEAP, nsIMemoryReporter::UNITS_BYTES,
        mFontEntry->ComputedSizeOfExcludingThis(UserFontsMallocSizeOf),
        NS_LITERAL_CSTRING("Memory used by @font-face resource."),
        aData);
}

NS_IMPL_ISUPPORTS(gfxUserFontSet::UserFontCache::MemoryReporter,
                  nsIMemoryReporter)

NS_IMETHODIMP
gfxUserFontSet::UserFontCache::MemoryReporter::CollectReports(
    nsIHandleReportCallback* aHandleReport, nsISupports* aData, bool aAnonymize)
{
    if (!sUserFonts) {
        return NS_OK;
    }

    for (auto it = sUserFonts->Iter(); !it.Done(); it.Next()) {
        it.Get()->ReportMemory(aHandleReport, aData, aAnonymize);
    }

    MOZ_COLLECT_REPORT(
        "explicit/gfx/user-fonts/cache-overhead", KIND_HEAP, UNITS_BYTES,
        sUserFonts->ShallowSizeOfIncludingThis(UserFontsMallocSizeOf),
        "Memory used by the @font-face cache, not counting the actual font "
        "resources.");

    return NS_OK;
}

#ifdef DEBUG_USERFONT_CACHE

void
gfxUserFontSet::UserFontCache::Entry::Dump()
{
    nsresult rv;

    nsAutoCString principalURISpec("(null)");
    bool setDomain = false;

    if (mPrincipal) {
        nsCOMPtr<nsIURI> principalURI;
        rv = mPrincipal->GetURI(getter_AddRefs(principalURI));
        if (NS_SUCCEEDED(rv)) {
            principalURI->GetSpec(principalURISpec);
        }

        nsCOMPtr<nsIURI> domainURI;
        mPrincipal->GetDomain(getter_AddRefs(domainURI));
        if (domainURI) {
            setDomain = true;
        }
    }

    NS_ASSERTION(mURI, "null URI in userfont cache entry");

    printf("userfontcache fontEntry: %p fonturihash: %8.8x "
           "family: %s domainset: %s principal: [%s]\n",
           mFontEntry,
           nsURIHashKey::HashKey(mURI),
           NS_ConvertUTF16toUTF8(mFontEntry->FamilyName()).get(),
           setDomain ? "true" : "false",
           principalURISpec.get());
}

void
gfxUserFontSet::UserFontCache::Dump()
{
    if (!sUserFonts) {
        return;
    }

    printf("userfontcache dump count: %d ========\n", sUserFonts->Count());
    for (auto it = sUserFonts->Iter(); !it.Done(); it.Next()) {
        it.Get()->Dump();
    }
    printf("userfontcache dump ==================\n");
}

#endif

#undef LOG
#undef LOG_ENABLED
