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
#include "nsNetUtil.h"
#include "nsIProtocolHandler.h"

#include "woff.h"

#include "opentype-sanitiser.h"
#include "ots-memory-stream.h"

using namespace mozilla;

#ifdef PR_LOGGING
PRLogModuleInfo *gfxUserFontSet::sUserFontsLog = PR_NewLogModule("userfonts");
#endif /* PR_LOGGING */

#define LOG(args) PR_LOG(sUserFontsLog, PR_LOG_DEBUG, args)
#define LOG_ENABLED() PR_LOG_TEST(sUserFontsLog, PR_LOG_DEBUG)

static PRUint64 sFontSetGeneration = LL_INIT(0, 0);

// TODO: support for unicode ranges not yet implemented

gfxProxyFontEntry::gfxProxyFontEntry(const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList,
             gfxMixedFontFamily *aFamily,
             PRUint32 aWeight,
             PRUint32 aStretch,
             PRUint32 aItalicStyle,
             const nsTArray<gfxFontFeature>& aFeatureSettings,
             PRUint32 aLanguageOverride,
             gfxSparseBitSet *aUnicodeRanges)
    : gfxFontEntry(NS_LITERAL_STRING("Proxy"), aFamily),
      mLoadingState(NOT_LOADING),
      mUnsupportedFormat(false),
      mLoader(nsnull)
{
    mIsProxy = true;
    mSrcList = aFontFaceSrcList;
    mSrcIndex = 0;
    mWeight = aWeight;
    mStretch = aStretch;
    mItalic = (aItalicStyle & (NS_FONT_STYLE_ITALIC | NS_FONT_STYLE_OBLIQUE)) != 0;
    mFeatureSettings.AppendElements(aFeatureSettings);
    mLanguageOverride = aLanguageOverride;
    mIsUserFont = true;
}

gfxProxyFontEntry::~gfxProxyFontEntry()
{
}

gfxFont*
gfxProxyFontEntry::CreateFontInstance(const gfxFontStyle *aFontStyle, bool aNeedsBold)
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

gfxFontEntry*
gfxUserFontSet::AddFontFace(const nsAString& aFamilyName,
                            const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList,
                            PRUint32 aWeight,
                            PRUint32 aStretch,
                            PRUint32 aItalicStyle,
                            const nsTArray<gfxFontFeature>& aFeatureSettings,
                            const nsString& aLanguageOverride,
                            gfxSparseBitSet *aUnicodeRanges)
{
    gfxProxyFontEntry *proxyEntry = nsnull;

    nsAutoString key(aFamilyName);
    ToLowerCase(key);

    bool found;

    if (aWeight == 0)
        aWeight = NS_FONT_WEIGHT_NORMAL;

    // stretch, italic/oblique ==> zero implies normal

    gfxMixedFontFamily *family = mFontFamilies.GetWeak(key, &found);
    if (!family) {
        family = new gfxMixedFontFamily(aFamilyName);
        mFontFamilies.Put(key, family);
    }

    // construct a new face and add it into the family
    PRUint32 languageOverride =
        gfxFontStyle::ParseFontLanguageOverride(aLanguageOverride);
    proxyEntry =
        new gfxProxyFontEntry(aFontFaceSrcList, family, aWeight, aStretch,
                              aItalicStyle,
                              aFeatureSettings,
                              languageOverride,
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
gfxUserFontSet::FindFontEntry(const nsAString& aName, 
                              const gfxFontStyle& aFontStyle, 
                              bool& aFoundFamily,
                              bool& aNeedsBold,
                              bool& aWaitForUserFont)
{
    aWaitForUserFont = false;
    gfxMixedFontFamily *family = GetFamily(aName);

    // no user font defined for this name
    if (!family) {
        aFoundFamily = false;
        return nsnull;
    }

    aFoundFamily = true;
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
        return nsnull;
    }

    // hasn't been loaded yet, start the load process
    LoadStatus status;

    // NOTE that if all sources in the entry fail, this will delete proxyEntry,
    // so we cannot use it again if status==STATUS_END_OF_LIST
    status = LoadNext(proxyEntry);

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
#ifdef MOZ_GRAPHITE
#define PRESERVE_GRAPHITE true
#else
#define PRESERVE_GRAPHITE false
#endif
    if (ots::Process(&output, aData, aLength, PRESERVE_GRAPHITE)) {
        aSaneLength = output.Tell();
        return static_cast<PRUint8*>(output.forget());
    } else {
        aSaneLength = 0;
        return nsnull;
    }
}

static void
StoreUserFontData(gfxFontEntry* aFontEntry, gfxProxyFontEntry* aProxy,
                  const nsAString& aOriginalName,
                  nsTArray<PRUint8>* aMetadata, PRUint32 aMetaOrigLen)
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
    }
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
gfxUserFontSet::CopyWOFFMetadata(const PRUint8* aFontData,
                                 PRUint32 aLength,
                                 nsTArray<PRUint8>* aMetadata,
                                 PRUint32* aMetaOrigLen)
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
    PRUint32 metaOffset = woff->metaOffset;
    PRUint32 metaCompLen = woff->metaCompLen;
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
gfxUserFontSet::OnLoadComplete(gfxProxyFontEntry *aProxy,
                               const PRUint8 *aFontData, PRUint32 aLength,
                               nsresult aDownloadStatus)
{
    // forget about the loader, as we no longer potentially need to cancel it
    // if the entry is obsoleted
    aProxy->mLoader = nsnull;

    // download successful, make platform font using font data
    if (NS_SUCCEEDED(aDownloadStatus)) {
        gfxFontEntry *fe = LoadFont(aProxy, aFontData, aLength);
        aFontData = nsnull;

        if (fe) {
            IncrementGeneration();
            return true;
        }

    } else {
        // download failed
        LogMessage(aProxy, "download failed", nsIScriptError::errorFlag,
                   aDownloadStatus);
    }

    if (aFontData) {
        NS_Free((void*)aFontData);
    }

    // error occurred, load next src
    (void)LoadNext(aProxy);

    // We ignore the status returned by LoadNext();
    // even if loading failed, we need to bump the font-set generation
    // and return true in order to trigger reflow, so that fallback
    // will be used where the text was "masked" by the pending download
    IncrementGeneration();
    return true;
}


gfxUserFontSet::LoadStatus
gfxUserFontSet::LoadNext(gfxProxyFontEntry *aProxyEntry)
{
    PRUint32 numSrc = aProxyEntry->mSrcList.Length();

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
            if (fe) {
                LOG(("userfonts (%p) [src %d] loaded local: (%s) for (%s) gen: %8.8x\n",
                     this, aProxyEntry->mSrcIndex,
                     NS_ConvertUTF16toUTF8(currSrc.mLocalName).get(),
                     NS_ConvertUTF16toUTF8(aProxyEntry->mFamily->Name()).get(),
                     PRUint32(mGeneration)));
                fe->mFeatureSettings.AppendElements(aProxyEntry->mFeatureSettings);
                fe->mLanguageOverride = aProxyEntry->mLanguageOverride;
                StoreUserFontData(fe, aProxyEntry, nsString(), nsnull, 0);
                ReplaceFontEntry(aProxyEntry, fe);
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

                nsresult rv;
                bool loadDoesntSpin = false;
                rv = NS_URIChainHasFlags(currSrc.mURI,
                       nsIProtocolHandler::URI_SYNC_LOAD_IS_OK,
                       &loadDoesntSpin);

                if (NS_SUCCEEDED(rv) && loadDoesntSpin) {
                    PRUint8 *buffer = nsnull;
                    PRUint32 bufferLength = 0;

                    // sync load font immediately
                    rv = SyncLoadFontData(aProxyEntry, &currSrc, buffer,
                                          bufferLength);

                    if (NS_SUCCEEDED(rv) &&
                        LoadFont(aProxyEntry, buffer, bufferLength)) {
                        return STATUS_LOADED;
                    } else {
                        LogMessage(aProxyEntry, "font load failed",
                                   nsIScriptError::errorFlag, rv);
                    }

                } else {
                    // otherwise load font async
                    rv = StartLoad(aProxyEntry, &currSrc);
                    bool loadOK = NS_SUCCEEDED(rv);

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
                        LogMessage(aProxyEntry, "download failed",
                                   nsIScriptError::errorFlag, rv);
                    }
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
        LogMessage(aProxyEntry, "no supported format found",
                   nsIScriptError::warningFlag);
    }

    // all src's failed; mark this entry as unusable (so fallback will occur)
    LOG(("userfonts (%p) failed all src for (%s)\n",
        this, NS_ConvertUTF16toUTF8(aProxyEntry->mFamily->Name()).get()));
    aProxyEntry->mLoadingState = gfxProxyFontEntry::LOADING_FAILED;

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


gfxFontEntry*
gfxUserFontSet::LoadFont(gfxProxyFontEntry *aProxy,
                         const PRUint8 *aFontData, PRUint32 &aLength)
{
    // if the proxy doesn't belong to a family, we just bail as it won't be
    // accessible/usable anyhow (maybe the font set got modified right as
    // the load was completing?)
    if (!aProxy->Family()) {
        NS_Free(const_cast<PRUint8*>(aFontData));
        return nsnull;
    }

    gfxFontEntry *fe = nsnull;

    gfxUserFontType fontType =
        gfxFontUtils::DetermineFontDataType(aFontData, aLength);

    // Save a copy of the metadata block (if present) for nsIDOMFontFace
    // to use if required. Ownership of the metadata block will be passed
    // to the gfxUserFontData record below.
    // NOTE: after the non-OTS codepath using PrepareOpenTypeData is
    // removed, we should defer this until after we've created the new
    // fontEntry.
    nsTArray<PRUint8> metadata;
    PRUint32 metaOrigLen = 0;
    if (fontType == GFX_USERFONT_WOFF) {
        CopyWOFFMetadata(aFontData, aLength, &metadata, &metaOrigLen);
    }

    // Unwrap/decompress/sanitize or otherwise munge the downloaded data
    // to make a usable sfnt structure.

    // Because platform font activation code may replace the name table
    // in the font with a synthetic one, we save the original name so that
    // it can be reported via the nsIDOMFontFace API.
    nsAutoString originalFullName;

    if (gfxPlatform::GetPlatform()->SanitizeDownloadedFonts()) {
       // Call the OTS sanitizer; this will also decode WOFF to sfnt
        // if necessary. The original data in aFontData is left unchanged.
        PRUint32 saneLen;
        const PRUint8* saneData =
            SanitizeOpenTypeData(aFontData, aLength, saneLen,
                                 fontType == GFX_USERFONT_WOFF);
        if (!saneData) {
            LogMessage(aProxy, "rejected by sanitizer");
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
                LogMessage(aProxy, "not usable by platform");
            }
        }
    } else {
        // FIXME: this code can be removed once we remove the pref to
        // disable the sanitizer; the PrepareOpenTypeData and
        // ValidateSFNTHeaders functions will then be obsolete.
        aFontData = PrepareOpenTypeData(aFontData, &aLength);

        if (aFontData) {
            if (gfxFontUtils::ValidateSFNTHeaders(aFontData, aLength)) {
                // ValidateSFNTHeaders has checked that we have a valid
                // sfnt structure and a usable 'name' table
                gfxFontUtils::GetFullNameFromSFNT(aFontData, aLength,
                                                  originalFullName);
                // Here ownership of aFontData is passed to the platform,
                // which will delete it when no longer required
                fe = gfxPlatform::GetPlatform()->MakePlatformFont(aProxy,
                                                                  aFontData,
                                                                  aLength);
                if (!fe) {
                    LogMessage(aProxy, "not usable by platform");
                }
                aFontData = nsnull; // we must NOT free this!
            } else {
                // the data was unusable, so just discard it
                // (error will be reported below, if logging is enabled)
                LogMessage(aProxy, "SFNT header or tables invalid");
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
        fe->mFeatureSettings.AppendElements(aProxy->mFeatureSettings);
        fe->mLanguageOverride = aProxy->mLanguageOverride;
        StoreUserFontData(fe, aProxy, originalFullName,
                          &metadata, metaOrigLen);
#ifdef PR_LOGGING
        // must do this before ReplaceFontEntry() because that will
        // clear the proxy's mFamily pointer!
        if (LOG_ENABLED()) {
            nsCAutoString fontURI;
            aProxy->mSrcList[aProxy->mSrcIndex].mURI->GetSpec(fontURI);
            LOG(("userfonts (%p) [src %d] loaded uri: (%s) for (%s) gen: %8.8x\n",
                 this, aProxy->mSrcIndex, fontURI.get(),
                 NS_ConvertUTF16toUTF8(aProxy->mFamily->Name()).get(),
                 PRUint32(mGeneration)));
        }
#endif
        ReplaceFontEntry(aProxy, fe);
    } else {
#ifdef PR_LOGGING
        if (LOG_ENABLED()) {
            nsCAutoString fontURI;
            aProxy->mSrcList[aProxy->mSrcIndex].mURI->GetSpec(fontURI);
            LOG(("userfonts (%p) [src %d] failed uri: (%s) for (%s)"
                 " error making platform font\n",
                 this, aProxy->mSrcIndex, fontURI.get(),
                 NS_ConvertUTF16toUTF8(aProxy->mFamily->Name()).get()));
        }
#endif
    }

    return fe;
}

gfxMixedFontFamily*
gfxUserFontSet::GetFamily(const nsAString& aFamilyName) const
{
    nsAutoString key(aFamilyName);
    ToLowerCase(key);

    return mFontFamilies.GetWeak(key);
}

