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
 * Portions created by the Initial Developer are Copyright (C) 2005-2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <stuart@mozilla.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
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

#include "nsIPrefService.h"
#include "nsServiceManagerUtils.h"
#include "nsReadableUtils.h"
#include "nsExpirationTracker.h"
#include "nsILanguageAtomService.h"

#include "gfxFont.h"
#include "gfxPlatform.h"
#include "gfxAtoms.h"

#include "prtypes.h"
#include "gfxTypes.h"
#include "gfxContext.h"
#include "gfxFontMissingGlyphs.h"
#include "gfxUserFontSet.h"
#include "gfxPlatformFontList.h"
#include "gfxScriptItemizer.h"
#include "gfxUnicodeProperties.h"
#include "nsMathUtils.h"
#include "nsBidiUtils.h"
#include "nsUnicodeRange.h"
#include "nsCompressedCharMap.h"

#include "cairo.h"
#include "gfxFontTest.h"

#include "harfbuzz/hb-blob.h"

#include "nsCRT.h"

using namespace mozilla;

gfxFontCache *gfxFontCache::gGlobalCache = nsnull;

static PRLogModuleInfo *gFontSelection = PR_NewLogModule("fontSelectionLog");

#ifdef DEBUG_roc
#define DEBUG_TEXT_RUN_STORAGE_METRICS
#endif

#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
static PRUint32 gTextRunStorageHighWaterMark = 0;
static PRUint32 gTextRunStorage = 0;
static PRUint32 gFontCount = 0;
static PRUint32 gGlyphExtentsCount = 0;
static PRUint32 gGlyphExtentsWidthsTotalSize = 0;
static PRUint32 gGlyphExtentsSetupEagerSimple = 0;
static PRUint32 gGlyphExtentsSetupEagerTight = 0;
static PRUint32 gGlyphExtentsSetupLazyTight = 0;
static PRUint32 gGlyphExtentsSetupFallBackToTight = 0;
#endif

gfxFontEntry::~gfxFontEntry() 
{
    if (mUserFontData) {
        delete mUserFontData;
    }
    if (mFeatureSettings) {
        delete mFeatureSettings;
    }
}

PRBool gfxFontEntry::TestCharacterMap(PRUint32 aCh)
{
    if (!mCmapInitialized) {
        ReadCMAP();
    }
    return mCharacterMap.test(aCh);
}

nsresult gfxFontEntry::InitializeUVSMap()
{
    // mUVSOffset will not be initialized
    // until cmap is initialized.
    if (!mCmapInitialized) {
        ReadCMAP();
    }

    if (!mUVSOffset) {
        return NS_ERROR_FAILURE;
    }

    if (!mUVSData) {
        const PRUint32 kCmapTag = TRUETYPE_TAG('c','m','a','p');
        nsAutoTArray<PRUint8,16384> buffer;
        if (GetFontTable(kCmapTag, buffer) != NS_OK) {
            mUVSOffset = 0; // don't bother to read the table again
            return NS_ERROR_FAILURE;
        }

        PRUint8* uvsData;
        nsresult rv = gfxFontUtils::ReadCMAPTableFormat14(
                          buffer.Elements() + mUVSOffset,
                          buffer.Length() - mUVSOffset,
                          uvsData);
        if (NS_FAILED(rv)) {
            mUVSOffset = 0; // don't bother to read the table again
            return rv;
        }

        mUVSData = uvsData;
    }

    return NS_OK;
}

PRUint16 gfxFontEntry::GetUVSGlyph(PRUint32 aCh, PRUint32 aVS)
{
    InitializeUVSMap();

    if (mUVSData) {
        return gfxFontUtils::MapUVSToGlyphFormat14(mUVSData, aCh, aVS);
    }

    return 0;
}

nsresult gfxFontEntry::ReadCMAP()
{
    mCmapInitialized = PR_TRUE;
    return NS_OK;
}

const nsString& gfxFontEntry::FamilyName() const
{
    NS_ASSERTION(mFamily, "gfxFontEntry is not a member of a family");
    return mFamily->Name();
}

already_AddRefed<gfxFont>
gfxFontEntry::FindOrMakeFont(const gfxFontStyle *aStyle, PRBool aNeedsBold)
{
    // the font entry name is the psname, not the family name
    nsRefPtr<gfxFont> font = gfxFontCache::GetCache()->Lookup(this, aStyle);

    if (!font) {
        gfxFont *newFont = CreateFontInstance(aStyle, aNeedsBold);
        if (!newFont)
            return nsnull;
        if (!newFont->Valid()) {
            delete newFont;
            return nsnull;
        }
        font = newFont;
        gfxFontCache::GetCache()->AddNew(font);
    }
    gfxFont *f = nsnull;
    font.swap(f);
    return f;
}

/**
 * FontTableBlobData
 *
 * See FontTableHashEntry for the general strategy.
 */

class gfxFontEntry::FontTableBlobData {
public:
    // Adopts the content of aBuffer.
    // Pass a non-null aHashEntry only if it should be cleared if/when this
    // FontTableBlobData is deleted.
    FontTableBlobData(nsTArray<PRUint8>& aBuffer,
                      FontTableHashEntry *aHashEntry)
        : mHashEntry(aHashEntry), mHashtable()
    {
        MOZ_COUNT_CTOR(FontTableBlobData);
        mTableData.SwapElements(aBuffer);
    }

    ~FontTableBlobData() {
        MOZ_COUNT_DTOR(FontTableBlobData);
        if (mHashEntry) {
            if (mHashtable) {
                mHashtable->RemoveEntry(mHashEntry->GetKey());
            } else {
                mHashEntry->Clear();
            }
        }
    }

    // Useful for creating blobs
    const char *GetTable() const
    {
        return reinterpret_cast<const char*>(mTableData.Elements());
    }
    PRUint32 GetTableLength() const { return mTableData.Length(); }

    // Tell this FontTableBlobData to remove the HashEntry when this is
    // destroyed.
    void ManageHashEntry(nsTHashtable<FontTableHashEntry> *aHashtable)
    {
        mHashtable = aHashtable;
    }

    // Disconnect from the HashEntry (because the blob has already been
    // removed from the hashtable).
    void ForgetHashEntry()
    {
        mHashEntry = nsnull;
    }

private:
    // The font table data block, owned (via adoption)
    nsTArray<PRUint8> mTableData;
    // The blob destroy function needs to know the hashtable entry,
    FontTableHashEntry *mHashEntry;
    // and the owning hashtable, so that it can remove the entry.
    nsTHashtable<FontTableHashEntry> *mHashtable;

    // not implemented
    FontTableBlobData(const FontTableBlobData&);
};

void
gfxFontEntry::FontTableHashEntry::SaveTable(nsTArray<PRUint8>& aTable)
{
    Clear();
    // adopts elements of aTable
    FontTableBlobData *data = new FontTableBlobData(aTable, nsnull);
    mBlob = hb_blob_create(data->GetTable(), data->GetTableLength(),
                           HB_MEMORY_MODE_READONLY,
                           DeleteFontTableBlobData, data);    
}

hb_blob_t *
gfxFontEntry::FontTableHashEntry::
ShareTableAndGetBlob(nsTArray<PRUint8>& aTable,
                     nsTHashtable<FontTableHashEntry> *aHashtable)
{
    Clear();
    // adopts elements of aTable
    mSharedBlobData = new FontTableBlobData(aTable, this);
    mBlob = hb_blob_create(mSharedBlobData->GetTable(),
                           mSharedBlobData->GetTableLength(),
                           HB_MEMORY_MODE_READONLY,
                           DeleteFontTableBlobData, mSharedBlobData);
    if (!mSharedBlobData) {
        // The FontTableBlobData was destroyed during hb_blob_create().
        // The (empty) blob is still be held in the hashtable with a strong
        // reference.
        return hb_blob_reference(mBlob);
    }

    // Tell the FontTableBlobData to remove this hash entry when destroyed.
    // The hashtable does not keep a strong reference.
    mSharedBlobData->ManageHashEntry(aHashtable);
    return mBlob;
}

void
gfxFontEntry::FontTableHashEntry::Clear()
{
    // If the FontTableBlobData is managing the hash entry, then the blob is
    // not owned by this HashEntry; otherwise there is strong reference to the
    // blob that must be removed.
    if (mSharedBlobData) {
        mSharedBlobData->ForgetHashEntry();
        mSharedBlobData = nsnull;
    } else if (mBlob) {
        hb_blob_destroy(mBlob);
    }
    mBlob = nsnull;
}

// a hb_destroy_func for hb_blob_create

/* static */ void
gfxFontEntry::FontTableHashEntry::DeleteFontTableBlobData(void *aBlobData)
{
    delete static_cast<FontTableBlobData*>(aBlobData);
}

hb_blob_t *
gfxFontEntry::FontTableHashEntry::GetBlob() const
{
    return hb_blob_reference(mBlob);
}

hb_blob_t *
gfxFontEntry::GetFontTable(PRUint32 aTag)
{
    if (!mFontTableCache.IsInitialized()) {
        // we do this here rather than on fontEntry construction
        // because not all shapers will access the table cache at all
        mFontTableCache.Init(10);
    }

    FontTableHashEntry *entry = mFontTableCache.GetEntry(aTag);
    if (entry) {
        return entry->GetBlob();
    }

    entry = mFontTableCache.PutEntry(aTag);
    if (NS_UNLIKELY(!entry)) { // OOM
        return nsnull;
    }

    nsTArray<PRUint8> buffer;
    if (NS_FAILED(GetFontTable(aTag, buffer))) {
        return nsnull; // leaves the null entry cached in the hashtable
    }

    return entry->ShareTableAndGetBlob(buffer, &mFontTableCache);
}

void
gfxFontEntry::PreloadFontTable(PRUint32 aTag, nsTArray<PRUint8>& aTable)
{
    if (!mFontTableCache.IsInitialized()) {
        // This is intended for use with downloaded fonts, to cache the layout
        // tables for harfbuzz, so initialize the cache for 3 entries to allow
        // for GDEF/GSUB/GPOS.
        mFontTableCache.Init(3);
    }

    FontTableHashEntry *entry = mFontTableCache.PutEntry(aTag);
    if (NS_UNLIKELY(!entry)) { // OOM
        return;
    }

    // adopts elements of aTable
    entry->SaveTable(aTable);
}

//////////////////////////////////////////////////////////////////////////////
//
// class gfxFontFamily
//
//////////////////////////////////////////////////////////////////////////////

// we consider faces with mStandardFace == PR_TRUE to be "greater than" those with PR_FALSE,
// because during style matching, later entries will replace earlier ones
class FontEntryStandardFaceComparator {
  public:
    PRBool Equals(const nsRefPtr<gfxFontEntry>& a, const nsRefPtr<gfxFontEntry>& b) const {
        return a->mStandardFace == b->mStandardFace;
    }
    PRBool LessThan(const nsRefPtr<gfxFontEntry>& a, const nsRefPtr<gfxFontEntry>& b) const {
        return (a->mStandardFace == PR_FALSE && b->mStandardFace == PR_TRUE);
    }
};

void
gfxFontFamily::SortAvailableFonts()
{
    mAvailableFonts.Sort(FontEntryStandardFaceComparator());
}

PRBool
gfxFontFamily::HasOtherFamilyNames()
{
    // need to read in other family names to determine this
    if (!mOtherFamilyNamesInitialized) {
        ReadOtherFamilyNames(gfxPlatformFontList::PlatformFontList());  // sets mHasOtherFamilyNames
    }
    return mHasOtherFamilyNames;
}

gfxFontEntry*
gfxFontFamily::FindFontForStyle(const gfxFontStyle& aFontStyle, 
                                PRBool& aNeedsSyntheticBold)
{
    if (!mHasStyles)
        FindStyleVariations(); // collect faces for the family, if not already done

    NS_ASSERTION(mAvailableFonts.Length() > 0, "font family with no faces!");

    aNeedsSyntheticBold = PR_FALSE;

    PRInt8 baseWeight = aFontStyle.ComputeWeight();
    PRBool wantBold = baseWeight >= 6;

    // If the family has only one face, we simply return it; no further checking needed
    if (mAvailableFonts.Length() == 1) {
        gfxFontEntry *fe = mAvailableFonts[0];
        aNeedsSyntheticBold = wantBold && !fe->IsBold();
        return fe;
    }

    PRBool wantItalic = (aFontStyle.style & (FONT_STYLE_ITALIC | FONT_STYLE_OBLIQUE)) != 0;

    // Most families are "simple", having just Regular/Bold/Italic/BoldItalic,
    // or some subset of these. In this case, we have exactly 4 entries in mAvailableFonts,
    // stored in the above order; note that some of the entries may be NULL.
    // We can then pick the required entry based on whether the request is for
    // bold or non-bold, italic or non-italic, without running the more complex
    // matching algorithm used for larger families with many weights and/or widths.

    if (mIsSimpleFamily) {
        // Family has no more than the "standard" 4 faces, at fixed indexes;
        // calculate which one we want.
        // Note that we cannot simply return it as not all 4 faces are necessarily present.
        PRUint8 faceIndex = (wantItalic ? kItalicMask : 0) |
                            (wantBold ? kBoldMask : 0);

        // if the desired style is available, return it directly
        gfxFontEntry *fe = mAvailableFonts[faceIndex];
        if (fe) {
            // no need to set aNeedsSyntheticBold here as we matched the boldness request
            return fe;
        }

        // order to check fallback faces in a simple family, depending on requested style
        static const PRUint8 simpleFallbacks[4][3] = {
            { kBoldFaceIndex, kItalicFaceIndex, kBoldItalicFaceIndex },   // fallbacks for Regular
            { kRegularFaceIndex, kBoldItalicFaceIndex, kItalicFaceIndex },// Bold
            { kBoldItalicFaceIndex, kRegularFaceIndex, kBoldFaceIndex },  // Italic
            { kItalicFaceIndex, kBoldFaceIndex, kRegularFaceIndex }       // BoldItalic
        };
        const PRUint8 *order = simpleFallbacks[faceIndex];

        for (PRUint8 trial = 0; trial < 3; ++trial) {
            // check remaining faces in order of preference to find the first that actually exists
            fe = mAvailableFonts[order[trial]];
            if (fe) {
                PR_LOG(gFontSelection, PR_LOG_DEBUG,
                       ("(FindFontForStyle) name: %s, sty: %02x, wt: %d, sz: %.1f -> %s (trial %d)\n", 
                        NS_ConvertUTF16toUTF8(mName).get(),
                        aFontStyle.style, aFontStyle.weight, aFontStyle.size,
                        NS_ConvertUTF16toUTF8(fe->Name()).get(), trial));
                aNeedsSyntheticBold = wantBold && !fe->IsBold();
                return fe;
            }
        }

        // this can't happen unless we have totally broken the font-list manager!
        NS_NOTREACHED("no face found in simple font family!");
        return nsnull;
    }

    // This is a large/rich font family, so we do full style- and weight-matching:
    // first collect a list of weights that are the best match for the requested
    // font-stretch and font-style, then pick the best weight match among those
    // available.

    gfxFontEntry *weightList[10] = { 0 };
    PRBool foundWeights = FindWeightsForStyle(weightList, wantItalic, aFontStyle.stretch);
    if (!foundWeights) {
        PR_LOG(gFontSelection, PR_LOG_DEBUG,
               ("(FindFontForStyle) name: %s, sty: %02x, wt: %d, sz: %.1f -> null\n", 
                NS_ConvertUTF16toUTF8(mName).get(),
                aFontStyle.style, aFontStyle.weight, aFontStyle.size));
        return nsnull;
    }

    // First find a match for the best weight
    PRInt8 matchBaseWeight = 0;
    PRInt8 i = baseWeight;

    // Need to special case when normal face doesn't exist but medium does.
    // In that case, use medium otherwise weights < 400
    if (baseWeight == 4 && !weightList[4]) {
        i = 5; // medium
    }

    // Loop through weights, since one exists loop will terminate
    PRInt8 direction = (baseWeight > 5) ? 1 : -1;
    for (; ; i += direction) {
        if (weightList[i]) {
            matchBaseWeight = i;
            break;
        }

        // If we've reached one side without finding a font,
        // start over and go the other direction until we find a match
        if (i == 1 || i == 9) {
            i = baseWeight;
            direction = -direction;
        }
    }

    NS_ASSERTION(matchBaseWeight != 0, 
                 "weight mapping should always find at least one font in a family");

    gfxFontEntry *matchFE = weightList[matchBaseWeight];

    NS_ASSERTION(matchFE,
                 "weight mapping should always find at least one font in a family");

    if (!matchFE->IsBold() && baseWeight >= 6)
    {
        aNeedsSyntheticBold = PR_TRUE;
    }

    PR_LOG(gFontSelection, PR_LOG_DEBUG,
           ("(FindFontForStyle) name: %s, sty: %02x, wt: %d, sz: %.1f -> %s\n", 
            NS_ConvertUTF16toUTF8(mName).get(),
            aFontStyle.style, aFontStyle.weight, aFontStyle.size,
            NS_ConvertUTF16toUTF8(matchFE->Name()).get()));
    return matchFE;
}

void
gfxFontFamily::CheckForSimpleFamily()
{
    if (mAvailableFonts.Length() > 4 || mAvailableFonts.Length() == 0) {
        return; // can't be "simple" if there are >4 faces;
                // if none then the family is unusable anyway
    }

    PRInt16 firstStretch = mAvailableFonts[0]->Stretch();

    gfxFontEntry *faces[4] = { 0 };
    for (PRUint8 i = 0; i < mAvailableFonts.Length(); ++i) {
        gfxFontEntry *fe = mAvailableFonts[i];
        if (fe->Stretch() != firstStretch) {
            return; // font-stretch doesn't match, don't treat as simple family
        }
        PRUint8 faceIndex = (fe->IsItalic() ? kItalicMask : 0) |
                            (fe->Weight() >= 600 ? kBoldMask : 0);
        if (faces[faceIndex]) {
            return; // two faces resolve to the same slot; family isn't "simple"
        }
        faces[faceIndex] = fe;
    }

    // we have successfully slotted the available faces into the standard
    // 4-face framework
    mAvailableFonts.SetLength(4);
    for (PRUint8 i = 0; i < 4; ++i) {
        if (mAvailableFonts[i].get() != faces[i]) {
            mAvailableFonts[i].swap(faces[i]);
        }
    }

    mIsSimpleFamily = PR_TRUE;
}

static inline PRUint32
StyleDistance(gfxFontEntry *aFontEntry,
              PRBool anItalic, PRInt16 aStretch)
{
    // Compute a measure of the "distance" between the requested style
    // and the given fontEntry,
    // considering italicness and font-stretch but not weight.

    // TODO (refine CSS spec...): discuss priority of italic vs stretch;
    // whether penalty for stretch mismatch should depend on actual difference in values;
    // whether a sign mismatch in stretch should increase the effective distance

    return (aFontEntry->IsItalic() != anItalic ? 1 : 0) +
           (aFontEntry->mStretch != aStretch ? 10 : 0);
}

PRBool
gfxFontFamily::FindWeightsForStyle(gfxFontEntry* aFontsForWeights[],
                                   PRBool anItalic, PRInt16 aStretch)
{
    PRUint32 foundWeights = 0;
    PRUint32 bestMatchDistance = 0xffffffff;

    for (PRUint32 i = 0; i < mAvailableFonts.Length(); i++) {
        // this is not called for "simple" families, and therefore it does not
        // need to check the mAvailableFonts entries for NULL
        gfxFontEntry *fe = mAvailableFonts[i];
        PRUint32 distance = StyleDistance(fe, anItalic, aStretch);
        if (distance <= bestMatchDistance) {
            PRInt8 wt = fe->mWeight / 100;
            NS_ASSERTION(wt >= 1 && wt < 10, "invalid weight in fontEntry");
            if (!aFontsForWeights[wt]) {
                // record this as a possible candidate for weight matching
                aFontsForWeights[wt] = fe;
                ++foundWeights;
            } else {
                PRUint32 prevDistance = StyleDistance(aFontsForWeights[wt], anItalic, aStretch);
                if (prevDistance >= distance) {
                    // replacing a weight we already found, so don't increment foundWeights
                    aFontsForWeights[wt] = fe;
                }
            }
            bestMatchDistance = distance;
        }
    }

    NS_ASSERTION(foundWeights > 0, "Font family containing no faces?");

    if (foundWeights == 1) {
        // no need to cull entries if we only found one weight
        return PR_TRUE;
    }

    // we might have recorded some faces that were a partial style match, but later found
    // others that were closer; in this case, we need to cull the poorer matches from the
    // weight list we'll return
    for (PRUint32 i = 0; i < 10; ++i) {
        if (aFontsForWeights[i] &&
            StyleDistance(aFontsForWeights[i], anItalic, aStretch) > bestMatchDistance)
        {
            aFontsForWeights[i] = 0;
        }
    }

    return (foundWeights > 0);
}


void gfxFontFamily::LocalizedName(nsAString& aLocalizedName)
{
    // just return the primary name; subclasses should override
    aLocalizedName = mName;
}


void
gfxFontFamily::FindFontForChar(FontSearch *aMatchData)
{
    if (!mHasStyles)
        FindStyleVariations();

    // xxx - optimization point - keep a bit vector with the union of supported unicode ranges
    // by all fonts for this family and bail immediately if the character is not in any of
    // this family's cmaps

    // iterate over fonts
    PRUint32 numFonts = mAvailableFonts.Length();
    for (PRUint32 i = 0; i < numFonts; i++) {
        gfxFontEntry *fe = mAvailableFonts[i];

        // skip certain fonts during system fallback
        if (!fe || fe->SkipDuringSystemFallback())
            continue;

        PRInt32 rank = 0;

        if (fe->TestCharacterMap(aMatchData->mCh)) {
            rank += 20;
        }

        // if we didn't match any characters don't bother wasting more time with this face.
        if (rank == 0)
            continue;
            
        // omitting from original windows code -- family name, lang group, pitch
        // not available in current FontEntry implementation

        if (aMatchData->mFontToMatch) { 
            const gfxFontStyle *style = aMatchData->mFontToMatch->GetStyle();
            
            // italics
            PRBool wantItalic =
                ((style->style & (FONT_STYLE_ITALIC | FONT_STYLE_OBLIQUE)) != 0);
            if (fe->IsItalic() == wantItalic) {
                rank += 5;
            }
            
            // weight
            PRInt32 targetWeight = style->ComputeWeight() * 100;

            PRInt32 entryWeight = fe->Weight();
            if (entryWeight == targetWeight) {
                rank += 5;
            } else {
                PRUint32 diffWeight = abs(entryWeight - targetWeight);
                if (diffWeight <= 100)  // favor faces close in weight
                    rank += 2;
            }
        } else {
            // if no font to match, prefer non-bold, non-italic fonts
            if (!fe->IsItalic()) {
                rank += 3;
            }
            if (!fe->IsBold()) {
                rank += 2;
            }
        }
        
        // xxx - add whether AAT font with morphing info for specific lang groups
        
        if (rank > aMatchData->mMatchRank
            || (rank == aMatchData->mMatchRank &&
                Compare(fe->Name(), aMatchData->mBestMatch->Name()) > 0)) 
        {
            aMatchData->mBestMatch = fe;
            aMatchData->mMatchRank = rank;
        }
    }
}

// returns true if other names were found, false otherwise
PRBool
gfxFontFamily::ReadOtherFamilyNamesForFace(gfxPlatformFontList *aPlatformFontList,
                                           nsTArray<PRUint8>& aNameTable,
                                           PRBool useFullName)
{
    const PRUint8 *nameData = aNameTable.Elements();
    PRUint32 dataLength = aNameTable.Length();
    const gfxFontUtils::NameHeader *nameHeader =
        reinterpret_cast<const gfxFontUtils::NameHeader*>(nameData);

    PRUint32 nameCount = nameHeader->count;
    if (nameCount * sizeof(gfxFontUtils::NameRecord) > dataLength) {
        NS_WARNING("invalid font (name records)");
        return PR_FALSE;
    }
    
    const gfxFontUtils::NameRecord *nameRecord =
        reinterpret_cast<const gfxFontUtils::NameRecord*>(nameData + sizeof(gfxFontUtils::NameHeader));
    PRUint32 stringsBase = PRUint32(nameHeader->stringOffset);

    PRBool foundNames = PR_FALSE;
    for (PRUint32 i = 0; i < nameCount; i++, nameRecord++) {
        PRUint32 nameLen = nameRecord->length;
        PRUint32 nameOff = nameRecord->offset;  // offset from base of string storage

        if (stringsBase + nameOff + nameLen > dataLength) {
            NS_WARNING("invalid font (name table strings)");
            return PR_FALSE;
        }

        PRUint16 nameID = nameRecord->nameID;
        if ((useFullName && nameID == gfxFontUtils::NAME_ID_FULL) ||
            (!useFullName && (nameID == gfxFontUtils::NAME_ID_FAMILY ||
                              nameID == gfxFontUtils::NAME_ID_PREFERRED_FAMILY))) {
            nsAutoString otherFamilyName;
            PRBool ok = gfxFontUtils::DecodeFontName(nameData + stringsBase + nameOff,
                                                     nameLen,
                                                     PRUint32(nameRecord->platformID),
                                                     PRUint32(nameRecord->encodingID),
                                                     PRUint32(nameRecord->languageID),
                                                     otherFamilyName);
            // add if not same as canonical family name
            if (ok && otherFamilyName != mName) {
                aPlatformFontList->AddOtherFamilyName(this, otherFamilyName);
                foundNames = PR_TRUE;
            }
        }
    }

    return foundNames;
}


void
gfxFontFamily::ReadOtherFamilyNames(gfxPlatformFontList *aPlatformFontList)
{
    if (mOtherFamilyNamesInitialized) 
        return;
    mOtherFamilyNamesInitialized = PR_TRUE;

    FindStyleVariations();

    // read in other family names for the first face in the list
    PRUint32 i, numFonts = mAvailableFonts.Length();
    const PRUint32 kNAME = TRUETYPE_TAG('n','a','m','e');
    nsAutoTArray<PRUint8,8192> buffer;

    for (i = 0; i < numFonts; ++i) {
        gfxFontEntry *fe = mAvailableFonts[i];
        if (!fe)
            continue;

        if (fe->GetFontTable(kNAME, buffer) != NS_OK)
            continue;

        mHasOtherFamilyNames = ReadOtherFamilyNamesForFace(aPlatformFontList,
                                                           buffer);
        break;
    }

    // read in other names for the first face in the list with the assumption
    // that if extra names don't exist in that face then they don't exist in
    // other faces for the same font
    if (!mHasOtherFamilyNames) 
        return;

    // read in names for all faces, needed to catch cases where fonts have
    // family names for individual weights (e.g. Hiragino Kaku Gothic Pro W6)
    for ( ; i < numFonts; i++) {
        gfxFontEntry *fe = mAvailableFonts[i];
        if (!fe)
            continue;

        if (fe->GetFontTable(kNAME, buffer) != NS_OK)
            continue;

        ReadOtherFamilyNamesForFace(aPlatformFontList, buffer);
    }
}

void
gfxFontFamily::ReadFaceNames(gfxPlatformFontList *aPlatformFontList, 
                             PRBool aNeedFullnamePostscriptNames)
{
    // if all needed names have already been read, skip
    if (mOtherFamilyNamesInitialized &&
        (mFaceNamesInitialized || !aNeedFullnamePostscriptNames))
        return;

    FindStyleVariations();

    PRUint32 i, numFonts = mAvailableFonts.Length();
    const PRUint32 kNAME = TRUETYPE_TAG('n','a','m','e');
    nsAutoTArray<PRUint8,8192> buffer;
    nsAutoString fullname, psname;

    PRBool firstTime = PR_TRUE, readAllFaces = PR_FALSE;
    for (i = 0; i < numFonts; ++i) {
        gfxFontEntry *fe = mAvailableFonts[i];
        if (!fe)
            continue;

        if (fe->GetFontTable(kNAME, buffer) != NS_OK)
            continue;

        if (aNeedFullnamePostscriptNames) {
            if (gfxFontUtils::ReadCanonicalName(
                    buffer, gfxFontUtils::NAME_ID_FULL, fullname) == NS_OK)
            {
                aPlatformFontList->AddFullname(fe, fullname);
            }

            if (gfxFontUtils::ReadCanonicalName(
                    buffer, gfxFontUtils::NAME_ID_POSTSCRIPT, psname) == NS_OK)
            {
                aPlatformFontList->AddPostscriptName(fe, psname);
            }
        }

       if (!mOtherFamilyNamesInitialized && (firstTime || readAllFaces)) {
           PRBool foundOtherName = ReadOtherFamilyNamesForFace(aPlatformFontList,
                                                               buffer);

           // if the first face has a different name, scan all faces, otherwise
           // assume the family doesn't have other names
           if (firstTime && foundOtherName) {
               mHasOtherFamilyNames = PR_TRUE;
               readAllFaces = PR_TRUE;
           }
           firstTime = PR_FALSE;
       }

       // if not reading in any more names, skip other faces
       if (!readAllFaces && !aNeedFullnamePostscriptNames)
           break;
    }

    mFaceNamesInitialized = PR_TRUE;
    mOtherFamilyNamesInitialized = PR_TRUE;
}


gfxFontEntry*
gfxFontFamily::FindFont(const nsAString& aPostscriptName)
{
    // find the font using a simple linear search
    PRUint32 numFonts = mAvailableFonts.Length();
    for (PRUint32 i = 0; i < numFonts; i++) {
        gfxFontEntry *fe = mAvailableFonts[i].get();
        if (fe && fe->Name() == aPostscriptName)
            return fe;
    }
    return nsnull;
}


nsresult
gfxFontCache::Init()
{
    NS_ASSERTION(!gGlobalCache, "Where did this come from?");
    gGlobalCache = new gfxFontCache();
    return gGlobalCache ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

void
gfxFontCache::Shutdown()
{
    delete gGlobalCache;
    gGlobalCache = nsnull;

#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
    printf("Textrun storage high water mark=%d\n", gTextRunStorageHighWaterMark);
    printf("Total number of fonts=%d\n", gFontCount);
    printf("Total glyph extents allocated=%d (size %d)\n", gGlyphExtentsCount,
            int(gGlyphExtentsCount*sizeof(gfxGlyphExtents)));
    printf("Total glyph extents width-storage size allocated=%d\n", gGlyphExtentsWidthsTotalSize);
    printf("Number of simple glyph extents eagerly requested=%d\n", gGlyphExtentsSetupEagerSimple);
    printf("Number of tight glyph extents eagerly requested=%d\n", gGlyphExtentsSetupEagerTight);
    printf("Number of tight glyph extents lazily requested=%d\n", gGlyphExtentsSetupLazyTight);
    printf("Number of simple glyph extent setups that fell back to tight=%d\n", gGlyphExtentsSetupFallBackToTight);
#endif
}

PRBool
gfxFontCache::HashEntry::KeyEquals(const KeyTypePointer aKey) const
{
    return aKey->mFontEntry == mFont->GetFontEntry() &&
           aKey->mStyle->Equals(*mFont->GetStyle());
}

already_AddRefed<gfxFont>
gfxFontCache::Lookup(const gfxFontEntry *aFontEntry,
                     const gfxFontStyle *aStyle)
{
    Key key(aFontEntry, aStyle);
    HashEntry *entry = mFonts.GetEntry(key);
    if (!entry)
        return nsnull;

    gfxFont *font = entry->mFont;
    NS_ADDREF(font);
    return font;
}

void
gfxFontCache::AddNew(gfxFont *aFont)
{
    Key key(aFont->GetFontEntry(), aFont->GetStyle());
    HashEntry *entry = mFonts.PutEntry(key);
    if (!entry)
        return;
    gfxFont *oldFont = entry->mFont;
    entry->mFont = aFont;
    // If someone's asked us to replace an existing font entry, then that's a
    // bit weird, but let it happen, and expire the old font if it's not used.
    if (oldFont && oldFont->GetExpirationState()->IsTracked()) {
        // if oldFont == aFont, recount should be > 0,
        // so we shouldn't be here.
        NS_ASSERTION(aFont != oldFont, "new font is tracked for expiry!");
        NotifyExpired(oldFont);
    }
}

void
gfxFontCache::NotifyReleased(gfxFont *aFont)
{
    nsresult rv = AddObject(aFont);
    if (NS_FAILED(rv)) {
        // We couldn't track it for some reason. Kill it now.
        DestroyFont(aFont);
    }
    // Note that we might have fonts that aren't in the hashtable, perhaps because
    // of OOM adding to the hashtable or because someone did an AddNew where
    // we already had a font. These fonts are added to the expiration tracker
    // anyway, even though Lookup can't resurrect them. Eventually they will
    // expire and be deleted.
}

void
gfxFontCache::NotifyExpired(gfxFont *aFont)
{
    RemoveObject(aFont);
    DestroyFont(aFont);
}

void
gfxFontCache::DestroyFont(gfxFont *aFont)
{
    Key key(aFont->GetFontEntry(), aFont->GetStyle());
    HashEntry *entry = mFonts.GetEntry(key);
    if (entry && entry->mFont == aFont)
        mFonts.RemoveEntry(key);
    NS_ASSERTION(aFont->GetRefCount() == 0,
                 "Destroying with non-zero ref count!");
    delete aFont;
}

void
gfxFont::RunMetrics::CombineWith(const RunMetrics& aOther, PRBool aOtherIsOnLeft)
{
    mAscent = PR_MAX(mAscent, aOther.mAscent);
    mDescent = PR_MAX(mDescent, aOther.mDescent);
    if (aOtherIsOnLeft) {
        mBoundingBox =
            (mBoundingBox + gfxPoint(aOther.mAdvanceWidth, 0)).Union(aOther.mBoundingBox);
    } else {
        mBoundingBox =
            mBoundingBox.Union(aOther.mBoundingBox + gfxPoint(mAdvanceWidth, 0));
    }
    mAdvanceWidth += aOther.mAdvanceWidth;
}

gfxFont::gfxFont(gfxFontEntry *aFontEntry, const gfxFontStyle *aFontStyle,
                 AntialiasOption anAAOption) :
    mFontEntry(aFontEntry), mIsValid(PR_TRUE),
    mStyle(*aFontStyle),
    mAdjustedSize(0.0),
    mFUnitsConvFactor(0.0f),
    mSyntheticBoldOffset(0),
    mAntialiasOption(anAAOption),
    mPlatformShaper(nsnull),
    mHarfBuzzShaper(nsnull)
{
#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
    ++gFontCount;
#endif
}

gfxFont::~gfxFont()
{
    PRUint32 i;
    // We destroy the contents of mGlyphExtentsArray explicitly instead of
    // using nsAutoPtr because VC++ can't deal with nsTArrays of nsAutoPtrs
    // of classes that lack a proper copy constructor
    for (i = 0; i < mGlyphExtentsArray.Length(); ++i) {
        delete mGlyphExtentsArray[i];
    }
}

/**
 * A helper function in case we need to do any rounding or other
 * processing here.
 */
#define ToDeviceUnits(aAppUnits, aDevUnitsPerAppUnit) \
    (double(aAppUnits)*double(aDevUnitsPerAppUnit))

struct GlyphBuffer {
#define GLYPH_BUFFER_SIZE (2048/sizeof(cairo_glyph_t))
    cairo_glyph_t mGlyphBuffer[GLYPH_BUFFER_SIZE];
    unsigned int mNumGlyphs;

    GlyphBuffer()
        : mNumGlyphs(0) { }

    cairo_glyph_t *AppendGlyph() {
        return &mGlyphBuffer[mNumGlyphs++];
    }

    void Flush(cairo_t *aCR, PRBool aDrawToPath, PRBool aReverse,
               PRBool aFinish = PR_FALSE) {
        // Ensure there's enough room for at least two glyphs in the
        // buffer (because we may allocate two glyphs between flushes)
        if (!aFinish && mNumGlyphs + 2 <= GLYPH_BUFFER_SIZE)
            return;

        if (aReverse) {
            for (PRUint32 i = 0; i < mNumGlyphs/2; ++i) {
                cairo_glyph_t tmp = mGlyphBuffer[i];
                mGlyphBuffer[i] = mGlyphBuffer[mNumGlyphs - 1 - i];
                mGlyphBuffer[mNumGlyphs - 1 - i] = tmp;
            }
        }
        if (aDrawToPath)
            cairo_glyph_path(aCR, mGlyphBuffer, mNumGlyphs);
        else
            cairo_show_glyphs(aCR, mGlyphBuffer, mNumGlyphs);

        mNumGlyphs = 0;
    }
#undef GLYPH_BUFFER_SIZE
};

void
gfxFont::Draw(gfxTextRun *aTextRun, PRUint32 aStart, PRUint32 aEnd,
              gfxContext *aContext, PRBool aDrawToPath, gfxPoint *aPt,
              Spacing *aSpacing)
{
    if (aStart >= aEnd)
        return;

    const gfxTextRun::CompressedGlyph *charGlyphs = aTextRun->GetCharacterGlyphs();
    const PRUint32 appUnitsPerDevUnit = aTextRun->GetAppUnitsPerDevUnit();
    const double devUnitsPerAppUnit = 1.0/double(appUnitsPerDevUnit);
    PRBool isRTL = aTextRun->IsRightToLeft();
    double direction = aTextRun->GetDirection();
    // double-strike in direction of run
    double synBoldDevUnitOffsetAppUnits =
      direction * (double) mSyntheticBoldOffset * appUnitsPerDevUnit;
    PRUint32 i;
    // Current position in appunits
    double x = aPt->x;
    double y = aPt->y;

    PRBool success = SetupCairoFont(aContext);
    if (NS_UNLIKELY(!success))
        return;

    GlyphBuffer glyphs;
    cairo_glyph_t *glyph;
    cairo_t *cr = aContext->GetCairo();

    if (aSpacing) {
        x += direction*aSpacing[0].mBefore;
    }
    for (i = aStart; i < aEnd; ++i) {
        const gfxTextRun::CompressedGlyph *glyphData = &charGlyphs[i];
        if (glyphData->IsSimpleGlyph()) {
            glyph = glyphs.AppendGlyph();
            glyph->index = glyphData->GetSimpleGlyph();
            double advance = glyphData->GetSimpleAdvance();
            // Perhaps we should put a scale in the cairo context instead of
            // doing this scaling here...
            // Multiplying by the reciprocal may introduce tiny error here,
            // but we assume cairo is going to round coordinates at some stage
            // and this is faster
            double glyphX;
            if (isRTL) {
                x -= advance;
                glyphX = x;
            } else {
                glyphX = x;
                x += advance;
            }
            glyph->x = ToDeviceUnits(glyphX, devUnitsPerAppUnit);
            glyph->y = ToDeviceUnits(y, devUnitsPerAppUnit);
            
            // synthetic bolding by drawing with a one-pixel offset
            if (mSyntheticBoldOffset) {
                cairo_glyph_t *doubleglyph;
                doubleglyph = glyphs.AppendGlyph();
                doubleglyph->index = glyph->index;
                doubleglyph->x =
                  ToDeviceUnits(glyphX + synBoldDevUnitOffsetAppUnits,
                                devUnitsPerAppUnit);
                doubleglyph->y = glyph->y;
            }
            
            glyphs.Flush(cr, aDrawToPath, isRTL);
        } else {
            PRUint32 j;
            PRUint32 glyphCount = glyphData->GetGlyphCount();
            const gfxTextRun::DetailedGlyph *details = aTextRun->GetDetailedGlyphs(i);
            for (j = 0; j < glyphCount; ++j, ++details) {
                double advance = details->mAdvance;
                if (glyphData->IsMissing()) {
                    // default ignorable characters will have zero advance width.
                    // we don't have to draw the hexbox for them
                    if (!aDrawToPath && advance > 0) {
                        double glyphX = x;
                        if (isRTL) {
                            glyphX -= advance;
                        }
                        gfxPoint pt(ToDeviceUnits(glyphX, devUnitsPerAppUnit),
                                    ToDeviceUnits(y, devUnitsPerAppUnit));
                        gfxFloat advanceDevUnits = ToDeviceUnits(advance, devUnitsPerAppUnit);
                        gfxFloat height = GetMetrics().maxAscent;
                        gfxRect glyphRect(pt.x, pt.y - height, advanceDevUnits, height);
                        gfxFontMissingGlyphs::DrawMissingGlyph(aContext, glyphRect, details->mGlyphID);
                    }
                } else {
                    glyph = glyphs.AppendGlyph();
                    glyph->index = details->mGlyphID;
                    double glyphX = x + details->mXOffset;
                    if (isRTL) {
                        glyphX -= advance;
                    }
                    glyph->x = ToDeviceUnits(glyphX, devUnitsPerAppUnit);
                    glyph->y = ToDeviceUnits(y + details->mYOffset, devUnitsPerAppUnit);

                    // synthetic bolding by drawing with a one-pixel offset
                    if (mSyntheticBoldOffset) {
                        cairo_glyph_t *doubleglyph;
                        doubleglyph = glyphs.AppendGlyph();
                        doubleglyph->index = glyph->index;
                        doubleglyph->x =
                            ToDeviceUnits(glyphX + synBoldDevUnitOffsetAppUnits,
                                          devUnitsPerAppUnit);
                        doubleglyph->y = glyph->y;
                    }

                    glyphs.Flush(cr, aDrawToPath, isRTL);
                }
                x += direction*advance;
            }
        }

        if (aSpacing) {
            double space = aSpacing[i - aStart].mAfter;
            if (i + 1 < aEnd) {
                space += aSpacing[i + 1 - aStart].mBefore;
            }
            x += direction*space;
        }
    }

    if (gfxFontTestStore::CurrentStore()) {
        /* This assumes that the tests won't have anything that results
         * in more than GLYPH_BUFFER_SIZE glyphs.  Do this before we
         * flush, since that'll blow away the num_glyphs.
         */
        gfxFontTestStore::CurrentStore()->AddItem(GetUniqueName(),
                                                  glyphs.mGlyphBuffer, glyphs.mNumGlyphs);
    }

    // draw any remaining glyphs
    glyphs.Flush(cr, aDrawToPath, isRTL, PR_TRUE);

    *aPt = gfxPoint(x, y);
}

static PRInt32
GetAdvanceForGlyphs(gfxTextRun *aTextRun, PRUint32 aStart, PRUint32 aEnd)
{
    const gfxTextRun::CompressedGlyph *glyphData = aTextRun->GetCharacterGlyphs() + aStart;
    PRInt32 advance = 0;
    PRUint32 i;
    for (i = aStart; i < aEnd; ++i, ++glyphData) {
        if (glyphData->IsSimpleGlyph()) {
            advance += glyphData->GetSimpleAdvance();   
        } else {
            PRUint32 glyphCount = glyphData->GetGlyphCount();
            const gfxTextRun::DetailedGlyph *details = aTextRun->GetDetailedGlyphs(i);
            PRUint32 j;
            for (j = 0; j < glyphCount; ++j, ++details) {
                advance += details->mAdvance;
            }
        }
    }
    return advance;
}

static void
UnionRange(gfxFloat aX, gfxFloat* aDestMin, gfxFloat* aDestMax)
{
    *aDestMin = PR_MIN(*aDestMin, aX);
    *aDestMax = PR_MAX(*aDestMax, aX);
}

// We get precise glyph extents if the textrun creator requested them, or
// if the font is a user font --- in which case the author may be relying
// on overflowing glyphs.
static PRBool
NeedsGlyphExtents(gfxFont *aFont, gfxTextRun *aTextRun)
{
    return (aTextRun->GetFlags() & gfxTextRunFactory::TEXT_NEED_BOUNDING_BOX) ||
        aFont->GetFontEntry()->IsUserFont();
}

static PRBool
NeedsGlyphExtents(gfxTextRun *aTextRun)
{
    if (aTextRun->GetFlags() & gfxTextRunFactory::TEXT_NEED_BOUNDING_BOX)
        return PR_TRUE;
    PRUint32 numRuns;
    const gfxTextRun::GlyphRun *glyphRuns = aTextRun->GetGlyphRuns(&numRuns);
    for (PRUint32 i = 0; i < numRuns; ++i) {
        if (glyphRuns[i].mFont->GetFontEntry()->IsUserFont())
            return PR_TRUE;
    }
    return PR_FALSE;
}

gfxFont::RunMetrics
gfxFont::Measure(gfxTextRun *aTextRun,
                 PRUint32 aStart, PRUint32 aEnd,
                 BoundingBoxType aBoundingBoxType,
                 gfxContext *aRefContext,
                 Spacing *aSpacing)
{
    // If aBoundingBoxType is TIGHT_HINTED_OUTLINE_EXTENTS
    // and the underlying cairo font may be antialiased,
    // we need to create a copy in order to avoid getting cached extents.
    // This is only used by MathML layout at present.
    if (aBoundingBoxType == TIGHT_HINTED_OUTLINE_EXTENTS &&
        mAntialiasOption != kAntialiasNone) {
        if (!mNonAAFont) {
            mNonAAFont = CopyWithAntialiasOption(kAntialiasNone);
        }
        // if font subclass doesn't implement CopyWithAntialiasOption(),
        // it will return null and we'll proceed to use the existing font
        if (mNonAAFont) {
            return mNonAAFont->Measure(aTextRun, aStart, aEnd,
                                       TIGHT_HINTED_OUTLINE_EXTENTS,
                                       aRefContext, aSpacing);
        }
    }

    const PRUint32 appUnitsPerDevUnit = aTextRun->GetAppUnitsPerDevUnit();
    // Current position in appunits
    const gfxFont::Metrics& fontMetrics = GetMetrics();

    RunMetrics metrics;
    metrics.mAscent = fontMetrics.maxAscent*appUnitsPerDevUnit;
    metrics.mDescent = fontMetrics.maxDescent*appUnitsPerDevUnit;
    if (aStart == aEnd) {
        // exit now before we look at aSpacing[0], which is undefined
        metrics.mBoundingBox = gfxRect(0, -metrics.mAscent, 0, metrics.mAscent + metrics.mDescent);
        return metrics;
    }

    gfxFloat advanceMin = 0, advanceMax = 0;
    const gfxTextRun::CompressedGlyph *charGlyphs = aTextRun->GetCharacterGlyphs();
    PRBool isRTL = aTextRun->IsRightToLeft();
    double direction = aTextRun->GetDirection();
    PRBool needsGlyphExtents = NeedsGlyphExtents(this, aTextRun);
    gfxGlyphExtents *extents =
        (aBoundingBoxType == LOOSE_INK_EXTENTS &&
            !needsGlyphExtents &&
            !aTextRun->HasDetailedGlyphs()) ? nsnull
        : GetOrCreateGlyphExtents(aTextRun->GetAppUnitsPerDevUnit());
    double x = 0;
    if (aSpacing) {
        x += direction*aSpacing[0].mBefore;
    }
    PRUint32 i;
    for (i = aStart; i < aEnd; ++i) {
        const gfxTextRun::CompressedGlyph *glyphData = &charGlyphs[i];
        if (glyphData->IsSimpleGlyph()) {
            double advance = glyphData->GetSimpleAdvance();
            // Only get the real glyph horizontal extent if we were asked
            // for the tight bounding box or we're in quality mode
            if ((aBoundingBoxType != LOOSE_INK_EXTENTS || needsGlyphExtents) &&
                extents) {
                PRUint32 glyphIndex = glyphData->GetSimpleGlyph();
                PRUint16 extentsWidth = extents->GetContainedGlyphWidthAppUnits(glyphIndex);
                if (extentsWidth != gfxGlyphExtents::INVALID_WIDTH &&
                    aBoundingBoxType == LOOSE_INK_EXTENTS) {
                    UnionRange(x, &advanceMin, &advanceMax);
                    UnionRange(x + direction*extentsWidth, &advanceMin, &advanceMax);
                } else {
                    gfxRect glyphRect;
                    if (!extents->GetTightGlyphExtentsAppUnits(this,
                            aRefContext, glyphIndex, &glyphRect)) {
                        glyphRect = gfxRect(0, metrics.mBoundingBox.Y(),
                            advance, metrics.mBoundingBox.Height());
                    }
                    if (isRTL) {
                        glyphRect.pos.x -= advance;
                    }
                    glyphRect.pos.x += x;
                    metrics.mBoundingBox = metrics.mBoundingBox.Union(glyphRect);
                }
            }
            x += direction*advance;
        } else {
            PRUint32 glyphCount = glyphData->GetGlyphCount();
            const gfxTextRun::DetailedGlyph *details = aTextRun->GetDetailedGlyphs(i);
            PRUint32 j;
            for (j = 0; j < glyphCount; ++j, ++details) {
                PRUint32 glyphIndex = details->mGlyphID;
                gfxPoint glyphPt(x + details->mXOffset, details->mYOffset);
                double advance = details->mAdvance;
                gfxRect glyphRect;
                if (glyphData->IsMissing() || !extents ||
                    !extents->GetTightGlyphExtentsAppUnits(this,
                            aRefContext, glyphIndex, &glyphRect)) {
                    // We might have failed to get glyph extents due to
                    // OOM or something
                    glyphRect = gfxRect(0, -metrics.mAscent,
                        advance, metrics.mAscent + metrics.mDescent);
                }
                if (isRTL) {
                    glyphRect.pos.x -= advance;
                }
                glyphRect.pos.x += x;
                metrics.mBoundingBox = metrics.mBoundingBox.Union(glyphRect);
                x += direction*advance;
            }
        }
        // Every other glyph type is ignored
        if (aSpacing) {
            double space = aSpacing[i - aStart].mAfter;
            if (i + 1 < aEnd) {
                space += aSpacing[i + 1 - aStart].mBefore;
            }
            x += direction*space;
        }
    }

    if (aBoundingBoxType == LOOSE_INK_EXTENTS) {
        UnionRange(x, &advanceMin, &advanceMax);
        gfxRect fontBox(advanceMin, -metrics.mAscent,
                        advanceMax - advanceMin, metrics.mAscent + metrics.mDescent);
        metrics.mBoundingBox = metrics.mBoundingBox.Union(fontBox);
    }
    if (isRTL) {
        metrics.mBoundingBox.pos.x -= x;
    }

    metrics.mAdvanceWidth = x*direction;
    return metrics;
}

#define MAX_SHAPING_LENGTH  32760 // slightly less than 32K, trying to avoid
                                  // over-stressing platform shapers

#define BACKTRACK_LIMIT  1024 // If we can't find a space or a cluster start
                              // within 1K chars, just chop arbitrarily.
                              // Limiting backtrack here avoids pathological
                              // behavior on long runs with no whitespace.

PRBool
gfxFont::InitTextRun(gfxContext *aContext,
                     gfxTextRun *aTextRun,
                     const PRUnichar *aString,
                     PRUint32 aRunStart,
                     PRUint32 aRunLength,
                     PRInt32 aRunScript,
                     PRBool aPreferPlatformShaping)
{
    PRBool ok;

    do {
        // Because various shaping backends struggle with very long runs,
        // we look for appropriate break locations (preferring whitespace),
        // and shape sub-runs of no more than 32K characters at a time.
        // See bug 606714 (CoreText), and similar Uniscribe issues.
        // This loop always executes at least once, and "processes" up to
        // MAX_RUN_LENGTH_FOR_SHAPING characters, updating aRunStart and
        // aRunLength accordingly. It terminates when the entire run has
        // been processed, or when shaping fails.

        PRUint32 thisRunLength;
        ok = PR_FALSE;

        if (aRunLength <= MAX_SHAPING_LENGTH) {
            thisRunLength = aRunLength;
        } else {
            // We're splitting this font run because it's very long
            PRUint32 offset = aRunStart + MAX_SHAPING_LENGTH;
            PRUint32 clusterStart = 0;
            while (offset > aRunStart + MAX_SHAPING_LENGTH - BACKTRACK_LIMIT) {
                if (aTextRun->IsClusterStart(offset)) {
                    if (!clusterStart) {
                        clusterStart = offset;
                    }
                    if (aString[offset] == ' ' || aString[offset - 1] == ' ') {
                        break;
                    }
                }
                --offset;
            }
            
            if (offset > MAX_SHAPING_LENGTH - BACKTRACK_LIMIT) {
                // we found a space, so break the run there
                thisRunLength = offset - aRunStart;
            } else if (clusterStart != 0) {
                // didn't find a space, but we found a cluster start
                thisRunLength = clusterStart - aRunStart;
            } else {
                // otherwise we'll simply break at MAX_SHAPING_LENGTH chars,
                // which may interfere with shaping behavior (but in practice
                // only pathological cases will lack ANY whitespace or cluster
                // boundaries, so we don't really care; it won't affect any
                // "real" text)
                thisRunLength = MAX_SHAPING_LENGTH;
            }
        }

        if (mHarfBuzzShaper && !aPreferPlatformShaping) {
            if (gfxPlatform::GetPlatform()->UseHarfBuzzLevel() >=
                gfxUnicodeProperties::ScriptShapingLevel(aRunScript)) {
                ok = mHarfBuzzShaper->InitTextRun(aContext, aTextRun, aString,
                                                  aRunStart, thisRunLength,
                                                  aRunScript);
            }
        }

        if (!ok) {
            if (!mPlatformShaper) {
                CreatePlatformShaper();
                NS_ASSERTION(mPlatformShaper, "no platform shaper available!");
            }
            if (mPlatformShaper) {
                ok = mPlatformShaper->InitTextRun(aContext, aTextRun, aString,
                                                  aRunStart, thisRunLength,
                                                  aRunScript);
            }
        }
        
        aRunStart += thisRunLength;
        aRunLength -= thisRunLength;
    } while (ok && aRunLength > 0);

    NS_WARN_IF_FALSE(ok, "shaper failed, expect scrambled or missing text");
    return ok;
}

gfxGlyphExtents *
gfxFont::GetOrCreateGlyphExtents(PRUint32 aAppUnitsPerDevUnit) {
    PRUint32 i;
    for (i = 0; i < mGlyphExtentsArray.Length(); ++i) {
        if (mGlyphExtentsArray[i]->GetAppUnitsPerDevUnit() == aAppUnitsPerDevUnit)
            return mGlyphExtentsArray[i];
    }
    gfxGlyphExtents *glyphExtents = new gfxGlyphExtents(aAppUnitsPerDevUnit);
    if (glyphExtents) {
        mGlyphExtentsArray.AppendElement(glyphExtents);
        // Initialize the extents of a space glyph, assuming that spaces don't
        // render anything!
        glyphExtents->SetContainedGlyphWidthAppUnits(GetSpaceGlyph(), 0);
    }
    return glyphExtents;
}

void
gfxFont::SetupGlyphExtents(gfxContext *aContext, PRUint32 aGlyphID, PRBool aNeedTight,
                           gfxGlyphExtents *aExtents)
{
    gfxMatrix matrix = aContext->CurrentMatrix();
    aContext->IdentityMatrix();
    cairo_glyph_t glyph;
    glyph.index = aGlyphID;
    glyph.x = 0;
    glyph.y = 0;
    cairo_text_extents_t extents;
    cairo_glyph_extents(aContext->GetCairo(), &glyph, 1, &extents);
    aContext->SetMatrix(matrix);

    const Metrics& fontMetrics = GetMetrics();
    PRUint32 appUnitsPerDevUnit = aExtents->GetAppUnitsPerDevUnit();
    if (!aNeedTight && extents.x_bearing >= 0 &&
        extents.y_bearing >= -fontMetrics.maxAscent &&
        extents.height + extents.y_bearing <= fontMetrics.maxDescent) {
        PRUint32 appUnitsWidth =
            PRUint32(NS_ceil((extents.x_bearing + extents.width)*appUnitsPerDevUnit));
        if (appUnitsWidth < gfxGlyphExtents::INVALID_WIDTH) {
            aExtents->SetContainedGlyphWidthAppUnits(aGlyphID, PRUint16(appUnitsWidth));
            return;
        }
    }
#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
    if (!aNeedTight) {
        ++gGlyphExtentsSetupFallBackToTight;
    }
#endif

    double d2a = appUnitsPerDevUnit;
    gfxRect bounds(extents.x_bearing*d2a, extents.y_bearing*d2a,
                   extents.width*d2a, extents.height*d2a);
    aExtents->SetTightGlyphExtents(aGlyphID, bounds);
}

// Try to initialize font metrics by reading sfnt tables directly;
// set mIsValid=TRUE and return TRUE on success.
// Return FALSE if the gfxFontEntry subclass does not
// implement GetFontTable(), or for non-sfnt fonts where tables are
// not available.
PRBool
gfxFont::InitMetricsFromSfntTables(Metrics& aMetrics)
{
    mIsValid = PR_FALSE; // font is NOT valid in case of early return

    const PRUint32 kHeadTableTag = TRUETYPE_TAG('h','e','a','d');
    const PRUint32 kHheaTableTag = TRUETYPE_TAG('h','h','e','a');
    const PRUint32 kPostTableTag = TRUETYPE_TAG('p','o','s','t');
    const PRUint32 kOS_2TableTag = TRUETYPE_TAG('O','S','/','2');

    if (mFUnitsConvFactor == 0.0) {
        // If the conversion factor from FUnits is not yet set,
        // 'head' table is required; otherwise we cannot read any metrics
        // because we don't know unitsPerEm
        nsAutoTArray<PRUint8,sizeof(HeadTable)> headData;
        if (NS_FAILED(mFontEntry->GetFontTable(kHeadTableTag, headData)) ||
            headData.Length() < sizeof(HeadTable)) {
            return PR_FALSE; // no 'head' table -> not an sfnt
        }
        HeadTable *head = reinterpret_cast<HeadTable*>(headData.Elements());
        PRUint32 unitsPerEm = head->unitsPerEm;
        if (!unitsPerEm) {
            return PR_TRUE; // is an sfnt, but not valid
        }
        mFUnitsConvFactor = mAdjustedSize / unitsPerEm;
    }

    // 'hhea' table is required to get vertical extents
    nsAutoTArray<PRUint8,sizeof(HheaTable)> hheaData;
    if (NS_FAILED(mFontEntry->GetFontTable(kHheaTableTag, hheaData)) ||
        hheaData.Length() < sizeof(HheaTable)) {
        return PR_FALSE; // no 'hhea' table -> not an sfnt
    }
    HheaTable *hhea = reinterpret_cast<HheaTable*>(hheaData.Elements());

#define SET_UNSIGNED(field,src) aMetrics.field = PRUint16(src) * mFUnitsConvFactor
#define SET_SIGNED(field,src)   aMetrics.field = PRInt16(src) * mFUnitsConvFactor

    SET_UNSIGNED(maxAdvance, hhea->advanceWidthMax);
    SET_SIGNED(maxAscent, hhea->ascender);
    SET_SIGNED(maxDescent, -PRInt16(hhea->descender));
    SET_SIGNED(externalLeading, hhea->lineGap);

    // 'post' table is required for underline metrics
    nsAutoTArray<PRUint8,sizeof(PostTable)> postData;
    if (NS_FAILED(mFontEntry->GetFontTable(kPostTableTag, postData))) {
        return PR_TRUE; // no 'post' table -> sfnt is not valid
    }
    if (postData.Length() <
        offsetof(PostTable, underlineThickness) + sizeof(PRUint16)) {
        return PR_TRUE; // bad post table -> sfnt is not valid
    }
    PostTable *post = reinterpret_cast<PostTable*>(postData.Elements());

    SET_SIGNED(underlineOffset, post->underlinePosition);
    SET_UNSIGNED(underlineSize, post->underlineThickness);

    // 'OS/2' table is optional, if not found we'll estimate xHeight
    // and aveCharWidth by measuring glyphs
    nsAutoTArray<PRUint8,sizeof(OS2Table)> os2data;
    if (NS_SUCCEEDED(mFontEntry->GetFontTable(kOS_2TableTag, os2data))) {
        OS2Table *os2 = reinterpret_cast<OS2Table*>(os2data.Elements());

        if (os2data.Length() >= offsetof(OS2Table, sxHeight) +
                                sizeof(PRInt16) &&
            PRUint16(os2->version) >= 2) {
            // version 2 and later includes the x-height field
            SET_SIGNED(xHeight, os2->sxHeight);
            // PR_ABS because of negative xHeight seen in Kokonor (Tibetan) font
            aMetrics.xHeight = PR_ABS(aMetrics.xHeight);
        }
        // this should always be present
        if (os2data.Length() >= offsetof(OS2Table, yStrikeoutPosition) +
                                sizeof(PRInt16)) {
            SET_SIGNED(aveCharWidth, os2->xAvgCharWidth);
            SET_SIGNED(subscriptOffset, os2->ySubscriptYOffset);
            SET_SIGNED(superscriptOffset, os2->ySuperscriptYOffset);
            SET_SIGNED(strikeoutSize, os2->yStrikeoutSize);
            SET_SIGNED(strikeoutOffset, os2->yStrikeoutPosition);
        }
    }

    mIsValid = PR_TRUE;

    return PR_TRUE;
}

static double
RoundToNearestMultiple(double aValue, double aFraction)
{
    return floor(aValue/aFraction + 0.5) * aFraction;
}

void gfxFont::CalculateDerivedMetrics(Metrics& aMetrics)
{
    aMetrics.maxAscent =
        NS_ceil(RoundToNearestMultiple(aMetrics.maxAscent, 1/1024.0));
    aMetrics.maxDescent =
        NS_ceil(RoundToNearestMultiple(aMetrics.maxDescent, 1/1024.0));

    if (aMetrics.xHeight <= 0) {
        // only happens if we couldn't find either font metrics
        // or a char to measure;
        // pick an arbitrary value that's better than zero
        aMetrics.xHeight = aMetrics.maxAscent * DEFAULT_XHEIGHT_FACTOR;
    }

    aMetrics.maxHeight = aMetrics.maxAscent + aMetrics.maxDescent;

    if (aMetrics.maxHeight - aMetrics.emHeight > 0.0) {
        aMetrics.internalLeading = aMetrics.maxHeight - aMetrics.emHeight;
    } else {
        aMetrics.internalLeading = 0.0;
    }

    aMetrics.emAscent = aMetrics.maxAscent * aMetrics.emHeight
                            / aMetrics.maxHeight;
    aMetrics.emDescent = aMetrics.emHeight - aMetrics.emAscent;

    if (GetFontEntry()->IsFixedPitch()) {
        // Some Quartz fonts are fixed pitch, but there's some glyph with a bigger
        // advance than the average character width... this forces
        // those fonts to be recognized like fixed pitch fonts by layout.
        aMetrics.maxAdvance = aMetrics.aveCharWidth;
    }

    if (!aMetrics.subscriptOffset) {
        aMetrics.subscriptOffset = aMetrics.xHeight;
    }
    if (!aMetrics.superscriptOffset) {
        aMetrics.superscriptOffset = aMetrics.xHeight;
    }

    if (!aMetrics.strikeoutOffset) {
        aMetrics.strikeoutOffset = aMetrics.xHeight * 0.5;
    }
    if (!aMetrics.strikeoutSize) {
        aMetrics.strikeoutSize = aMetrics.underlineSize;
    }
}

void
gfxFont::SanitizeMetrics(gfxFont::Metrics *aMetrics, PRBool aIsBadUnderlineFont)
{
    // Even if this font size is zero, this font is created with non-zero size.
    // However, for layout and others, we should return the metrics of zero size font.
    if (mStyle.size == 0) {
        memset(aMetrics, 0, sizeof(gfxFont::Metrics));
        return;
    }

    // MS (P)Gothic and MS (P)Mincho are not having suitable values in their super script offset.
    // If the values are not suitable, we should use x-height instead of them.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=353632
    if (aMetrics->superscriptOffset <= 0 ||
        aMetrics->superscriptOffset >= aMetrics->maxAscent) {
        aMetrics->superscriptOffset = aMetrics->xHeight;
    }
    // And also checking the case of sub script offset. The old gfx for win has checked this too.
    if (aMetrics->subscriptOffset <= 0 ||
        aMetrics->subscriptOffset >= aMetrics->maxAscent) {
        aMetrics->subscriptOffset = aMetrics->xHeight;
    }

    aMetrics->underlineSize = PR_MAX(1.0, aMetrics->underlineSize);
    aMetrics->strikeoutSize = PR_MAX(1.0, aMetrics->strikeoutSize);

    aMetrics->underlineOffset = PR_MIN(aMetrics->underlineOffset, -1.0);

    if (aMetrics->maxAscent < 1.0) {
        // We cannot draw strikeout line and overline in the ascent...
        aMetrics->underlineSize = 0;
        aMetrics->underlineOffset = 0;
        aMetrics->strikeoutSize = 0;
        aMetrics->strikeoutOffset = 0;
        return;
    }

    /**
     * Some CJK fonts have bad underline offset. Therefore, if this is such font,
     * we need to lower the underline offset to bottom of *em* descent.
     * However, if this is system font, we should not do this for the rendering compatibility with
     * another application's UI on the platform.
     * XXX Should not use this hack if the font size is too small?
     *     Such text cannot be read, this might be used for tight CSS rendering? (E.g., Acid2)
     */
    if (!mStyle.systemFont && aIsBadUnderlineFont) {
        // First, we need 2 pixels between baseline and underline at least. Because many CJK characters
        // put their glyphs on the baseline, so, 1 pixel is too close for CJK characters.
        aMetrics->underlineOffset = PR_MIN(aMetrics->underlineOffset, -2.0);

        // Next, we put the underline to bottom of below of the descent space.
        if (aMetrics->internalLeading + aMetrics->externalLeading > aMetrics->underlineSize) {
            aMetrics->underlineOffset = PR_MIN(aMetrics->underlineOffset, -aMetrics->emDescent);
        } else {
            aMetrics->underlineOffset = PR_MIN(aMetrics->underlineOffset,
                                               aMetrics->underlineSize - aMetrics->emDescent);
        }
    }
    // If underline positioned is too far from the text, descent position is preferred so that underline
    // will stay within the boundary.
    else if (aMetrics->underlineSize - aMetrics->underlineOffset > aMetrics->maxDescent) {
        if (aMetrics->underlineSize > aMetrics->maxDescent)
            aMetrics->underlineSize = PR_MAX(aMetrics->maxDescent, 1.0);
        // The max underlineOffset is 1px (the min underlineSize is 1px, and min maxDescent is 0px.)
        aMetrics->underlineOffset = aMetrics->underlineSize - aMetrics->maxDescent;
    }

    // If strikeout line is overflowed from the ascent, the line should be resized and moved for
    // that being in the ascent space.
    // Note that the strikeoutOffset is *middle* of the strikeout line position.
    gfxFloat halfOfStrikeoutSize = NS_floor(aMetrics->strikeoutSize / 2.0 + 0.5);
    if (halfOfStrikeoutSize + aMetrics->strikeoutOffset > aMetrics->maxAscent) {
        if (aMetrics->strikeoutSize > aMetrics->maxAscent) {
            aMetrics->strikeoutSize = PR_MAX(aMetrics->maxAscent, 1.0);
            halfOfStrikeoutSize = NS_floor(aMetrics->strikeoutSize / 2.0 + 0.5);
        }
        gfxFloat ascent = NS_floor(aMetrics->maxAscent + 0.5);
        aMetrics->strikeoutOffset = PR_MAX(halfOfStrikeoutSize, ascent / 2.0);
    }

    // If overline is larger than the ascent, the line should be resized.
    if (aMetrics->underlineSize > aMetrics->maxAscent) {
        aMetrics->underlineSize = aMetrics->maxAscent;
    }
}

gfxGlyphExtents::~gfxGlyphExtents()
{
#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
    gGlyphExtentsWidthsTotalSize += mContainedGlyphWidths.ComputeSize();
    gGlyphExtentsCount++;
#endif
    MOZ_COUNT_DTOR(gfxGlyphExtents);
}

PRBool
gfxGlyphExtents::GetTightGlyphExtentsAppUnits(gfxFont *aFont,
    gfxContext *aContext, PRUint32 aGlyphID, gfxRect *aExtents)
{
    HashEntry *entry = mTightGlyphExtents.GetEntry(aGlyphID);
    if (!entry) {
        if (!aContext) {
            NS_WARNING("Could not get glyph extents (no aContext)");
            return PR_FALSE;
        }

        aFont->SetupCairoFont(aContext);
#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
        ++gGlyphExtentsSetupLazyTight;
#endif
        aFont->SetupGlyphExtents(aContext, aGlyphID, PR_TRUE, this);
        entry = mTightGlyphExtents.GetEntry(aGlyphID);
        if (!entry) {
            NS_WARNING("Could not get glyph extents");
            return PR_FALSE;
        }
    }

    *aExtents = gfxRect(entry->x, entry->y, entry->width, entry->height);
    return PR_TRUE;
}

gfxGlyphExtents::GlyphWidths::~GlyphWidths()
{
    PRUint32 i;
    for (i = 0; i < mBlocks.Length(); ++i) {
        PtrBits bits = mBlocks[i];
        if (bits && !(bits & 0x1)) {
            delete[] reinterpret_cast<PRUint16 *>(bits);
        }
    }
}

#ifdef DEBUG
PRUint32
gfxGlyphExtents::GlyphWidths::ComputeSize()
{
    PRUint32 i;
    PRUint32 size = mBlocks.Capacity()*sizeof(PtrBits);
    for (i = 0; i < mBlocks.Length(); ++i) {
        PtrBits bits = mBlocks[i];
        if (bits && !(bits & 0x1)) {
            size += BLOCK_SIZE*sizeof(PRUint16);
        }
    }
    return size;
}
#endif

void
gfxGlyphExtents::GlyphWidths::Set(PRUint32 aGlyphID, PRUint16 aWidth)
{
    PRUint32 block = aGlyphID >> BLOCK_SIZE_BITS;
    PRUint32 len = mBlocks.Length();
    if (block >= len) {
        PtrBits *elems = mBlocks.AppendElements(block + 1 - len);
        if (!elems)
            return;
        memset(elems, 0, sizeof(PtrBits)*(block + 1 - len));
    }

    PtrBits bits = mBlocks[block];
    PRUint32 glyphOffset = aGlyphID & (BLOCK_SIZE - 1);
    if (!bits) {
        mBlocks[block] = MakeSingle(glyphOffset, aWidth);
        return;
    }

    PRUint16 *newBlock;
    if (bits & 0x1) {
        // Expand the block to a real block. We could avoid this by checking
        // glyphOffset == GetGlyphOffset(bits), but that never happens so don't bother
        newBlock = new PRUint16[BLOCK_SIZE];
        if (!newBlock)
            return;
        PRUint32 i;
        for (i = 0; i < BLOCK_SIZE; ++i) {
            newBlock[i] = INVALID_WIDTH;
        }
        newBlock[GetGlyphOffset(bits)] = GetWidth(bits);
        mBlocks[block] = reinterpret_cast<PtrBits>(newBlock);
    } else {
        newBlock = reinterpret_cast<PRUint16 *>(bits);
    }
    newBlock[glyphOffset] = aWidth;
}

void
gfxGlyphExtents::SetTightGlyphExtents(PRUint32 aGlyphID, const gfxRect& aExtentsAppUnits)
{
    HashEntry *entry = mTightGlyphExtents.PutEntry(aGlyphID);
    if (!entry)
        return;
    entry->x = aExtentsAppUnits.pos.x;
    entry->y = aExtentsAppUnits.pos.y;
    entry->width = aExtentsAppUnits.size.width;
    entry->height = aExtentsAppUnits.size.height;
}

gfxFontGroup::gfxFontGroup(const nsAString& aFamilies, const gfxFontStyle *aStyle, gfxUserFontSet *aUserFontSet)
    : mFamilies(aFamilies), mStyle(*aStyle), mUnderlineOffset(UNDERLINE_OFFSET_NOT_SET)
{
    mUserFontSet = nsnull;
    SetUserFontSet(aUserFontSet);

    mPageLang = gfxPlatform::GetFontPrefLangFor(mStyle.language);
    BuildFontList();
}

void
gfxFontGroup::BuildFontList()
{
// "#if" to be removed once all platforms are moved to gfxPlatformFontList interface
// and subclasses of gfxFontGroup eliminated
#if defined(XP_MACOSX) || (defined(XP_WIN) && !defined(WINCE)) || defined(ANDROID)
    ForEachFont(FindPlatformFont, this);

    if (mFonts.Length() == 0) {
        PRBool needsBold;
        gfxPlatformFontList *pfl = gfxPlatformFontList::PlatformFontList();
        gfxFontEntry *defaultFont = pfl->GetDefaultFont(&mStyle, needsBold);
        NS_ASSERTION(defaultFont, "invalid default font returned by GetDefaultFont");

        if (defaultFont) {
            nsRefPtr<gfxFont> font = defaultFont->FindOrMakeFont(&mStyle,
                                                                 needsBold);
            if (font) {
                mFonts.AppendElement(font);
            }
        }

        if (mFonts.Length() == 0) {
            // Try for a "font of last resort...."
            // Because an empty font list would be Really Bad for later code
            // that assumes it will be able to get valid metrics for layout,
            // just look for the first usable font and put in the list.
            // (see bug 554544)
            nsAutoTArray<nsRefPtr<gfxFontFamily>,200> families;
            pfl->GetFontFamilyList(families);
            for (PRUint32 i = 0; i < families.Length(); ++i) {
                gfxFontEntry *fe = families[i]->FindFontForStyle(mStyle,
                                                                 needsBold);
                if (fe) {
                    nsRefPtr<gfxFont> font = fe->FindOrMakeFont(&mStyle,
                                                                needsBold);
                    if (font) {
                        mFonts.AppendElement(font);
                        break;
                    }
                }
            }
        }

        if (mFonts.Length() == 0) {
            // an empty font list at this point is fatal; we're not going to
            // be able to do even the most basic layout operations
            char msg[256]; // CHECK buffer length if revising message below
            sprintf(msg, "unable to find a usable font (%.220s)",
                    NS_ConvertUTF16toUTF8(mFamilies).get());
            NS_RUNTIMEABORT(msg);
        }
    }

    if (!mStyle.systemFont) {
        for (PRUint32 i = 0; i < mFonts.Length(); ++i) {
            gfxFont* font = mFonts[i];
            if (font->GetFontEntry()->mIsBadUnderlineFont) {
                gfxFloat first = mFonts[0]->GetMetrics().underlineOffset;
                gfxFloat bad = font->GetMetrics().underlineOffset;
                mUnderlineOffset = PR_MIN(first, bad);
                break;
            }
        }
    }
#endif
}

PRBool
gfxFontGroup::FindPlatformFont(const nsAString& aName,
                               const nsACString& aGenericName,
                               void *aClosure)
{
    gfxFontGroup *fontGroup = static_cast<gfxFontGroup*>(aClosure);
    const gfxFontStyle *fontStyle = fontGroup->GetStyle();


    PRBool needsBold;
    gfxFontEntry *fe = nsnull;

    // first, look up in the user font set
    gfxUserFontSet *fs = fontGroup->GetUserFontSet();
    if (fs) {
        fe = fs->FindFontEntry(aName, *fontStyle, needsBold);
    }

    // nothing in the user font set ==> check system fonts
    if (!fe) {
        fe = gfxPlatformFontList::PlatformFontList()->
            FindFontForFamily(aName, fontStyle, needsBold);
    }

    // add to the font group, unless it's already there
    if (fe && !fontGroup->HasFont(fe)) {
        nsRefPtr<gfxFont> font = fe->FindOrMakeFont(fontStyle, needsBold);
        if (font) {
            fontGroup->mFonts.AppendElement(font);
        }
    }

    return PR_TRUE;
}

PRBool
gfxFontGroup::HasFont(const gfxFontEntry *aFontEntry)
{
    for (PRUint32 i = 0; i < mFonts.Length(); ++i) {
        if (mFonts.ElementAt(i)->GetFontEntry() == aFontEntry)
            return PR_TRUE;
    }
    return PR_FALSE;
}

gfxFontGroup::~gfxFontGroup() {
    mFonts.Clear();
    SetUserFontSet(nsnull);
}

gfxFontGroup *
gfxFontGroup::Copy(const gfxFontStyle *aStyle)
{
    return new gfxFontGroup(mFamilies, aStyle, mUserFontSet);
}

PRBool 
gfxFontGroup::IsInvalidChar(PRUnichar ch) {
    if (ch >= 32) {
        return ch == 0x0085/*NEL*/ ||
            ((ch & 0xFF00) == 0x2000 /* Unicode control character */ &&
             (ch == 0x200B/*ZWSP*/ || ch == 0x2028/*LSEP*/ || ch == 0x2029/*PSEP*/ ||
              IS_BIDI_CONTROL_CHAR(ch)));
    }
    // We could just blacklist all control characters, but it seems better
    // to only blacklist the ones we know cause problems for native font
    // engines.
    return ch == 0x0B || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\f' ||
        (ch >= 0x1c && ch <= 0x1f);
}

PRBool
gfxFontGroup::ForEachFont(FontCreationCallback fc,
                          void *closure)
{
    return ForEachFontInternal(mFamilies, mStyle.language,
                               PR_TRUE, PR_TRUE, fc, closure);
}

PRBool
gfxFontGroup::ForEachFont(const nsAString& aFamilies,
                          nsIAtom *aLanguage,
                          FontCreationCallback fc,
                          void *closure)
{
    return ForEachFontInternal(aFamilies, aLanguage,
                               PR_FALSE, PR_TRUE, fc, closure);
}

struct ResolveData {
    ResolveData(gfxFontGroup::FontCreationCallback aCallback,
                nsACString& aGenericFamily,
                void *aClosure) :
        mCallback(aCallback),
        mGenericFamily(aGenericFamily),
        mClosure(aClosure) {
    }
    gfxFontGroup::FontCreationCallback mCallback;
    nsCString mGenericFamily;
    void *mClosure;
};

PRBool
gfxFontGroup::ForEachFontInternal(const nsAString& aFamilies,
                                  nsIAtom *aLanguage,
                                  PRBool aResolveGeneric,
                                  PRBool aResolveFontName,
                                  FontCreationCallback fc,
                                  void *closure)
{
    const PRUnichar kSingleQuote  = PRUnichar('\'');
    const PRUnichar kDoubleQuote  = PRUnichar('\"');
    const PRUnichar kComma        = PRUnichar(',');

    nsIAtom *groupAtom = nsnull;
    nsCAutoString groupString;
    if (aLanguage) {
        if (!gLangService) {
            CallGetService(NS_LANGUAGEATOMSERVICE_CONTRACTID, &gLangService);
        }
        if (gLangService) {
            nsresult rv;
            groupAtom = gLangService->GetLanguageGroup(aLanguage, &rv);
        }
    }
    if (!groupAtom) {
        groupAtom = gfxAtoms::x_unicode;
    }
    groupAtom->ToUTF8String(groupString);

    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);

    nsPromiseFlatString families(aFamilies);
    const PRUnichar *p, *p_end;
    families.BeginReading(p);
    families.EndReading(p_end);
    nsAutoString family;
    nsCAutoString lcFamily;
    nsAutoString genericFamily;
    nsXPIDLCString value;

    while (p < p_end) {
        while (nsCRT::IsAsciiSpace(*p) || *p == kComma)
            if (++p == p_end)
                return PR_TRUE;

        PRBool generic;
        if (*p == kSingleQuote || *p == kDoubleQuote) {
            // quoted font family
            PRUnichar quoteMark = *p;
            if (++p == p_end)
                return PR_TRUE;
            const PRUnichar *nameStart = p;

            // XXX What about CSS character escapes?
            while (*p != quoteMark)
                if (++p == p_end)
                    return PR_TRUE;

            family = Substring(nameStart, p);
            generic = PR_FALSE;
            genericFamily.SetIsVoid(PR_TRUE);

            while (++p != p_end && *p != kComma)
                /* nothing */ ;

        } else {
            // unquoted font family
            const PRUnichar *nameStart = p;
            while (++p != p_end && *p != kComma)
                /* nothing */ ;

            family = Substring(nameStart, p);
            family.CompressWhitespace(PR_FALSE, PR_TRUE);

            if (aResolveGeneric &&
                (family.LowerCaseEqualsLiteral("serif") ||
                 family.LowerCaseEqualsLiteral("sans-serif") ||
                 family.LowerCaseEqualsLiteral("monospace") ||
                 family.LowerCaseEqualsLiteral("cursive") ||
                 family.LowerCaseEqualsLiteral("fantasy")))
            {
                generic = PR_TRUE;

                ToLowerCase(NS_LossyConvertUTF16toASCII(family), lcFamily);

                nsCAutoString prefName("font.name.");
                prefName.Append(lcFamily);
                prefName.AppendLiteral(".");
                prefName.Append(groupString);

                // prefs file always uses (must use) UTF-8 so that we can use
                // |GetCharPref| and treat the result as a UTF-8 string.
                nsresult rv = prefs->GetCharPref(prefName.get(), getter_Copies(value));
                if (NS_SUCCEEDED(rv)) {
                    CopyASCIItoUTF16(lcFamily, genericFamily);
                    CopyUTF8toUTF16(value, family);
                }
            } else {
                generic = PR_FALSE;
                genericFamily.SetIsVoid(PR_TRUE);
            }
        }

        if (generic) {
            ForEachFontInternal(family, groupAtom, PR_FALSE,
                                aResolveFontName, fc, closure);
        } else if (!family.IsEmpty()) {
            NS_LossyConvertUTF16toASCII gf(genericFamily);
            if (aResolveFontName) {
                ResolveData data(fc, gf, closure);
                PRBool aborted = PR_FALSE, needsBold;
                nsresult rv;

                if (mUserFontSet && mUserFontSet->FindFontEntry(family, mStyle, needsBold)) {
                    gfxFontGroup::FontResolverProc(family, &data);
                    rv = NS_OK;
                } else {
                    gfxPlatform *pf = gfxPlatform::GetPlatform();
                    rv = pf->ResolveFontName(family,
                                                  gfxFontGroup::FontResolverProc,
                                                  &data, aborted);
                }
                if (NS_FAILED(rv) || aborted)
                    return PR_FALSE;
            }
            else {
                if (!fc(family, gf, closure))
                    return PR_FALSE;
            }
        }

        if (generic && aResolveGeneric) {
            nsCAutoString prefName("font.name-list.");
            prefName.Append(lcFamily);
            prefName.AppendLiteral(".");
            prefName.Append(groupString);
            nsresult rv = prefs->GetCharPref(prefName.get(), getter_Copies(value));
            if (NS_SUCCEEDED(rv)) {
                ForEachFontInternal(NS_ConvertUTF8toUTF16(value),
                                    groupAtom, PR_FALSE, aResolveFontName,
                                    fc, closure);
            }
        }

        ++p; // may advance past p_end
    }

    return PR_TRUE;
}

PRBool
gfxFontGroup::FontResolverProc(const nsAString& aName, void *aClosure)
{
    ResolveData *data = reinterpret_cast<ResolveData*>(aClosure);
    return (data->mCallback)(aName, data->mGenericFamily, data->mClosure);
}

gfxTextRun *
gfxFontGroup::MakeEmptyTextRun(const Parameters *aParams, PRUint32 aFlags)
{
    aFlags |= TEXT_IS_8BIT | TEXT_IS_ASCII | TEXT_IS_PERSISTENT;
    return gfxTextRun::Create(aParams, nsnull, 0, this, aFlags);
}

gfxTextRun *
gfxFontGroup::MakeSpaceTextRun(const Parameters *aParams, PRUint32 aFlags)
{
    aFlags |= TEXT_IS_8BIT | TEXT_IS_ASCII | TEXT_IS_PERSISTENT;
    static const PRUint8 space = ' ';

    nsAutoPtr<gfxTextRun> textRun;
    textRun = gfxTextRun::Create(aParams, &space, 1, this, aFlags);
    if (!textRun)
        return nsnull;

    gfxFont *font = GetFontAt(0);
    if (NS_UNLIKELY(GetStyle()->size == 0)) {
        // Short-circuit for size-0 fonts, as Windows and ATSUI can't handle
        // them, and always create at least size 1 fonts, i.e. they still
        // render something for size 0 fonts.
        textRun->AddGlyphRun(font, 0);
    }
    else {
        textRun->SetSpaceGlyph(font, aParams->mContext, 0);
    }
    // Note that the gfxGlyphExtents glyph bounds storage for the font will
    // always contain an entry for the font's space glyph, so we don't have
    // to call FetchGlyphExtents here.
    return textRun.forget();
}

#define UNICODE_LRO 0x202d
#define UNICODE_RLO 0x202e
#define UNICODE_PDF 0x202c

inline void
AppendDirectionalIndicatorStart(PRUint32 aFlags, nsAString& aString)
{
    static const PRUnichar overrides[2] = { UNICODE_LRO, UNICODE_RLO };
    aString.Append(overrides[(aFlags & gfxTextRunFactory::TEXT_IS_RTL) != 0]);    
    aString.Append(' ');
}

inline void
AppendDirectionalIndicatorEnd(PRBool aNeedDirection, nsAString& aString)
{
    // append a space (always, for consistent treatment of last char,
    // and a direction control if required (we skip this for 8-bit text,
    // which is known to be unidirectional LTR, unless the direction was
    // forced RTL via overrides)
    aString.Append(' ');
    if (!aNeedDirection)
        return;

    aString.Append('.');
    aString.Append(UNICODE_PDF);
}

gfxTextRun *
gfxFontGroup::MakeTextRun(const PRUint8 *aString, PRUint32 aLength,
                          const Parameters *aParams, PRUint32 aFlags)
{
    NS_ASSERTION(aLength > 0, "should use MakeEmptyTextRun for zero-length text");
    NS_ASSERTION(aFlags & TEXT_IS_8BIT, "should be marked 8bit");
    gfxTextRun *textRun = gfxTextRun::Create(aParams, aString, aLength, this, aFlags);
    if (!textRun)
        return nsnull;

    nsDependentCSubstring cString(reinterpret_cast<const char*>(aString),
                                  reinterpret_cast<const char*>(aString) + aLength);

    nsAutoString utf16;
    AppendASCIItoUTF16(cString, utf16);

    InitTextRun(aParams->mContext, textRun, utf16.get(), utf16.Length());

    textRun->FetchGlyphExtents(aParams->mContext);

    return textRun;
}

gfxTextRun *
gfxFontGroup::MakeTextRun(const PRUnichar *aString, PRUint32 aLength,
                          const Parameters *aParams, PRUint32 aFlags)
{
    NS_ASSERTION(aLength > 0, "should use MakeEmptyTextRun for zero-length text");
    gfxTextRun *textRun = gfxTextRun::Create(aParams, aString, aLength, this, aFlags);
    if (!textRun)
        return nsnull;

    gfxPlatform::GetPlatform()->SetupClusterBoundaries(textRun, aString);

    InitTextRun(aParams->mContext, textRun, aString, aLength);

    textRun->FetchGlyphExtents(aParams->mContext);

    return textRun;
}

#define SMALL_GLYPH_RUN 128 // preallocated size of our auto arrays for per-glyph data;
                            // some testing indicates that 90%+ of glyph runs will fit
                            // without requiring a separate allocation

void
gfxFontGroup::InitTextRun(gfxContext *aContext,
                          gfxTextRun *aTextRun,
                          const PRUnichar *aString,
                          PRUint32 aLength)
{
    // split into script runs so that script can potentially influence
    // the font matching process below
    gfxScriptItemizer scriptRuns(aString, aLength);

    PRUint32 runStart = 0, runLimit = aLength;
    PRInt32 runScript = HB_SCRIPT_LATIN;
    while (scriptRuns.Next(runStart, runLimit, runScript)) {
        InitTextRun(aContext, aTextRun, aString, aLength,
                    runStart, runLimit, runScript);
    }
}

void
gfxFontGroup::InitTextRun(gfxContext *aContext,
                          gfxTextRun *aTextRun,
                          const PRUnichar *aString,
                          PRUint32 aTotalLength,
                          PRUint32 aScriptRunStart,
                          PRUint32 aScriptRunEnd,
                          PRInt32 aRunScript)
{
    gfxFont *mainFont = mFonts[0].get();

    PRUint32 runStart = aScriptRunStart;
    nsAutoTArray<gfxTextRange,3> fontRanges;
    ComputeRanges(fontRanges, aString,
                  aScriptRunStart, aScriptRunEnd, aRunScript);
    PRUint32 numRanges = fontRanges.Length();

    for (PRUint32 r = 0; r < numRanges; r++) {
        const gfxTextRange& range = fontRanges[r];
        PRUint32 matchedLength = range.Length();
        gfxFont *matchedFont = (range.font ? range.font.get() : nsnull);

        // create the glyph run for this range
        aTextRun->AddGlyphRun(matchedFont ? matchedFont : mainFont,
                              runStart, (matchedLength > 0));
        if (matchedFont) {
            // do glyph layout and record the resulting positioned glyphs
            if (!matchedFont->InitTextRun(aContext, aTextRun, aString,
                                          runStart, matchedLength,
                                          aRunScript)) {
                // glyph layout failed! treat as missing glyphs
                matchedFont = nsnull;
            }
        }
        if (!matchedFont) {
            for (PRUint32 index = runStart; index < runStart + matchedLength; index++) {
                // Record the char code so we can draw a box with the Unicode value
                if (NS_IS_HIGH_SURROGATE(aString[index]) &&
                    index + 1 < aScriptRunEnd &&
                    NS_IS_LOW_SURROGATE(aString[index+1])) {
                    aTextRun->SetMissingGlyph(index,
                                              SURROGATE_TO_UCS4(aString[index],
                                                                aString[index+1]));
                    index++;
                } else {
                    aTextRun->SetMissingGlyph(index, aString[index]);
                }
            }
        }

        runStart += matchedLength;
    }

    // It's possible for CoreText to omit glyph runs if it decides they contain
    // only invisibles (e.g., U+FEFF, see reftest 474417-1). In this case, we
    // need to eliminate them from the glyph run array to avoid drawing "partial
    // ligatures" with the wrong font.
    aTextRun->SanitizeGlyphRuns();

    // Is this actually necessary? Without it, gfxTextRun::CopyGlyphDataFrom may assert
    // "Glyphruns not coalesced", but does that matter?
    aTextRun->SortGlyphRuns();

#ifdef DUMP_TEXT_RUNS
    nsCAutoString lang;
    style->language->ToUTF8String(lang);
    PR_LOG(gFontSelection, PR_LOG_DEBUG,\
           ("InitTextRun %p fontgroup %p (%s) lang: %s len %d features: %s "
            "TEXTRUN \"%s\" ENDTEXTRUN\n",
            aTextRun, this,
            NS_ConvertUTF16toUTF8(mFamilies).get(),
            lang.get(), aLength,
            NS_ConvertUTF16toUTF8(mStyle.featureSettings).get(),
            NS_ConvertUTF16toUTF8(aString, aLength).get()) );
#endif
}


already_AddRefed<gfxFont>
gfxFontGroup::FindFontForChar(PRUint32 aCh, PRUint32 aPrevCh,
                              PRInt32 aRunScript, gfxFont *aPrevMatchedFont)
{
    nsRefPtr<gfxFont>    selectedFont;

    // if this character or the previous one is a join-causer,
    // use the same font as the previous range if we can
    if (gfxFontUtils::IsJoinCauser(aCh) || gfxFontUtils::IsJoinCauser(aPrevCh)) {
        if (aPrevMatchedFont && aPrevMatchedFont->HasCharacter(aCh)) {
            selectedFont = aPrevMatchedFont;
            return selectedFont.forget();
        }
    }

    // if this character is a variation selector,
    // use the previous font regardless of whether it supports VS or not.
    // otherwise the text run will be divided.
    if (gfxFontUtils::IsVarSelector(aCh)) {
        if (aPrevMatchedFont) {
            selectedFont = aPrevMatchedFont;
            return selectedFont.forget();
        }
        // VS alone. it's meaningless to search different fonts
        return nsnull;
    }

    // 1. check fonts in the font group
    for (PRUint32 i = 0; i < FontListLength(); i++) {
        nsRefPtr<gfxFont> font = GetFontAt(i);
        if (font->HasCharacter(aCh))
            return font.forget();
    }

    // if character is in Private Use Area, don't do matching against pref or system fonts
    if ((aCh >= 0xE000  && aCh <= 0xF8FF) || (aCh >= 0xF0000 && aCh <= 0x10FFFD))
        return nsnull;

    // 2. search pref fonts
    if ((selectedFont = WhichPrefFontSupportsChar(aCh))) {
        return selectedFont.forget();
    }

    // 3. use fallback fonts
    // -- before searching for something else check the font used for the previous character
    if (!selectedFont && aPrevMatchedFont && aPrevMatchedFont->HasCharacter(aCh)) {
        selectedFont = aPrevMatchedFont;
        return selectedFont.forget();
    }

    // -- otherwise look for other stuff
    if (!selectedFont) {
        selectedFont = WhichSystemFontSupportsChar(aCh);
        return selectedFont.forget();
    }

    return nsnull;
}


void gfxFontGroup::ComputeRanges(nsTArray<gfxTextRange>& aRanges,
                                 const PRUnichar *aString,
                                 PRUint32 begin, PRUint32 end,
                                 PRInt32 aRunScript)
{
    const PRUnichar *str = aString + begin;
    PRUint32 len = end - begin;

    aRanges.Clear();

    if (len == 0) {
        return;
    }

    PRUint32 prevCh = 0;
    for (PRUint32 i = 0; i < len; i++) {

        const PRUint32 origI = i; // save off in case we increase for surrogate

        // set up current ch
        PRUint32 ch = str[i];
        if ((i+1 < len) && NS_IS_HIGH_SURROGATE(ch) && NS_IS_LOW_SURROGATE(str[i+1])) {
            i++;
            ch = SURROGATE_TO_UCS4(ch, str[i]);
        }

        // find the font for this char
        nsRefPtr<gfxFont> font =
            FindFontForChar(ch, prevCh, aRunScript,
                            (aRanges.Length() == 0) ?
                            nsnull : aRanges[aRanges.Length() - 1].font.get());

        prevCh = ch;

        if (aRanges.Length() == 0) {
            // first char ==> make a new range
            gfxTextRange r(0,1);
            r.font = font;
            aRanges.AppendElement(r);
        } else {
            // if font has changed, make a new range
            gfxTextRange& prevRange = aRanges[aRanges.Length() - 1];
            if (prevRange.font != font) {
                // close out the previous range
                prevRange.end = origI;

                gfxTextRange r(origI, i+1);
                r.font = font;
                aRanges.AppendElement(r);
            }
        }
    }
    aRanges[aRanges.Length()-1].end = len;
}

gfxUserFontSet* 
gfxFontGroup::GetUserFontSet()
{
    return mUserFontSet;
}

void 
gfxFontGroup::SetUserFontSet(gfxUserFontSet *aUserFontSet)
{
    NS_IF_RELEASE(mUserFontSet);
    mUserFontSet = aUserFontSet;
    NS_IF_ADDREF(mUserFontSet);
    mCurrGeneration = GetGeneration();
}

PRUint64
gfxFontGroup::GetGeneration()
{
    if (!mUserFontSet)
        return 0;
    return mUserFontSet->GetGeneration();
}

void
gfxFontGroup::UpdateFontList()
{
    // if user font set is set, check to see if font list needs updating
    if (mUserFontSet && mCurrGeneration != GetGeneration()) {
        // xxx - can probably improve this to detect when all fonts were found, so no need to update list
        mFonts.Clear();
        mUnderlineOffset = UNDERLINE_OFFSET_NOT_SET;

        // bug 548184 - need to clean up FT2, OS/2 platform code to use BuildFontList
#if defined(XP_MACOSX) || defined(XP_WIN)
        BuildFontList();
#else
        ForEachFont(FindPlatformFont, this);
#endif
        mCurrGeneration = GetGeneration();
    }
}

struct PrefFontCallbackData {
    PrefFontCallbackData(nsTArray<nsRefPtr<gfxFontFamily> >& aFamiliesArray)
        : mPrefFamilies(aFamiliesArray)
    {}

    nsTArray<nsRefPtr<gfxFontFamily> >& mPrefFamilies;

    static PRBool AddFontFamilyEntry(eFontPrefLang aLang, const nsAString& aName, void *aClosure)
    {
        PrefFontCallbackData *prefFontData = static_cast<PrefFontCallbackData*>(aClosure);

        gfxFontFamily *family = gfxPlatformFontList::PlatformFontList()->FindFamily(aName);
        if (family) {
            prefFontData->mPrefFamilies.AppendElement(family);
        }
        return PR_TRUE;
    }
};

already_AddRefed<gfxFont>
gfxFontGroup::WhichPrefFontSupportsChar(PRUint32 aCh)
{
    gfxFont *font;

    // FindCharUnicodeRange only supports BMP character points and there are no non-BMP fonts in prefs
    if (aCh > 0xFFFF)
        return nsnull;

    // get the pref font list if it hasn't been set up already
    PRUint32 unicodeRange = FindCharUnicodeRange(aCh);
    eFontPrefLang charLang = gfxPlatform::GetPlatform()->GetFontPrefLangFor(unicodeRange);

    // if the last pref font was the first family in the pref list, no need to recheck through a list of families
    if (mLastPrefFont && charLang == mLastPrefLang &&
        mLastPrefFirstFont && mLastPrefFont->HasCharacter(aCh)) {
        font = mLastPrefFont;
        NS_ADDREF(font);
        return font;
    }

    // based on char lang and page lang, set up list of pref lang fonts to check
    eFontPrefLang prefLangs[kMaxLenPrefLangList];
    PRUint32 i, numLangs = 0;

    gfxPlatform::GetPlatform()->GetLangPrefs(prefLangs, numLangs, charLang, mPageLang);

    for (i = 0; i < numLangs; i++) {
        nsAutoTArray<nsRefPtr<gfxFontFamily>, 5> families;
        eFontPrefLang currentLang = prefLangs[i];

        gfxPlatformFontList *fontList = gfxPlatformFontList::PlatformFontList();

        // get the pref families for a single pref lang
        if (!fontList->GetPrefFontFamilyEntries(currentLang, &families)) {
            eFontPrefLang prefLangsToSearch[1] = { currentLang };
            PrefFontCallbackData prefFontData(families);
            gfxPlatform::ForEachPrefFont(prefLangsToSearch, 1, PrefFontCallbackData::AddFontFamilyEntry,
                                           &prefFontData);
            fontList->SetPrefFontFamilyEntries(currentLang, families);
        }

        // find the first pref font that includes the character
        PRUint32  i, numPrefs;
        numPrefs = families.Length();
        for (i = 0; i < numPrefs; i++) {
            // look up the appropriate face
            gfxFontFamily *family = families[i];
            if (!family) continue;

            // if a pref font is used, it's likely to be used again in the same text run.
            // the style doesn't change so the face lookup can be cached rather than calling
            // FindOrMakeFont repeatedly.  speeds up FindFontForChar lookup times for subsequent
            // pref font lookups
            if (family == mLastPrefFamily && mLastPrefFont->HasCharacter(aCh)) {
                font = mLastPrefFont;
                NS_ADDREF(font);
                return font;
            }

            PRBool needsBold;
            gfxFontEntry *fe = family->FindFontForStyle(mStyle, needsBold);
            // if ch in cmap, create and return a gfxFont
            if (fe && fe->TestCharacterMap(aCh)) {
                nsRefPtr<gfxFont> prefFont = fe->FindOrMakeFont(&mStyle, needsBold);
                if (!prefFont) continue;
                mLastPrefFamily = family;
                mLastPrefFont = prefFont;
                mLastPrefLang = charLang;
                mLastPrefFirstFont = (i == 0);
                return prefFont.forget();
            }

        }
    }

    return nsnull;
}

already_AddRefed<gfxFont>
gfxFontGroup::WhichSystemFontSupportsChar(PRUint32 aCh)
{
    gfxFontEntry *fe = 
        gfxPlatformFontList::PlatformFontList()->FindFontForChar(aCh, GetFontAt(0));
    if (fe) {
        nsRefPtr<gfxFont> font = fe->FindOrMakeFont(&mStyle, PR_FALSE); // ignore bolder considerations in system fallback case...
        return font.forget();
    }

    return nsnull;
}

/*static*/ void
gfxFontGroup::Shutdown()
{
    NS_IF_RELEASE(gLangService);
}

nsILanguageAtomService* gfxFontGroup::gLangService = nsnull;


#define DEFAULT_PIXEL_FONT_SIZE 16.0f

/*static*/ void
gfxFontStyle::ParseFontFeatureSettings(const nsString& aFeatureString,
                                       nsTArray<gfxFontFeature>& aFeatures)
{
  aFeatures.Clear();
  PRUint32 offset = 0;
  while (offset < aFeatureString.Length()) {
    // skip whitespace
    while (offset < aFeatureString.Length() &&
           nsCRT::IsAsciiSpace(aFeatureString[offset])) {
      ++offset;
    }
    PRInt32 limit = aFeatureString.FindChar(',', offset);
    if (limit < 0) {
      limit = aFeatureString.Length();
    }
    // check that we have enough text for a 4-char tag,
    // the '=' sign, and at least one digit
    if (offset + 6 <= PRUint32(limit) &&
      aFeatureString[offset+4] == '=') {
      gfxFontFeature setting;
      setting.mTag =
        ((aFeatureString[offset] & 0xff) << 24) +
        ((aFeatureString[offset+1] & 0xff) << 16) +
        ((aFeatureString[offset+2] & 0xff) << 8) +
         (aFeatureString[offset+3] & 0xff);
      nsString valString;
      aFeatureString.Mid(valString, offset+5, limit-offset-5);
      PRInt32 rv;
      setting.mValue = valString.ToInteger(&rv);
      if (rv == NS_OK) {
        // we keep the features array sorted so that we can
        // use nsTArray<>::Equals() to compare feature lists
        aFeatures.InsertElementSorted(setting);
      }
    }
    offset = limit + 1;
  }
}

/*static*/ PRUint32
gfxFontStyle::ParseFontLanguageOverride(const nsString& aLangTag)
{
  if (!aLangTag.Length() || aLangTag.Length() > 4) {
    return NO_FONT_LANGUAGE_OVERRIDE;
  }
  PRUint32 index, result = 0;
  for (index = 0; index < aLangTag.Length(); ++index) {
    PRUnichar ch = aLangTag[index];
    if (!nsCRT::IsAscii(ch)) { // valid tags are pure ASCII
      return NO_FONT_LANGUAGE_OVERRIDE;
    }
    result = (result << 8) + ch;
  }
  while (index++ < 4) {
    result = (result << 8) + 0x20;
  }
  return result;
}

gfxFontStyle::gfxFontStyle() :
    style(FONT_STYLE_NORMAL), systemFont(PR_TRUE), printerFont(PR_FALSE), 
    familyNameQuirks(PR_FALSE), weight(FONT_WEIGHT_NORMAL),
    stretch(NS_FONT_STRETCH_NORMAL), size(DEFAULT_PIXEL_FONT_SIZE),
    sizeAdjust(0.0f),
    language(gfxAtoms::x_western),
    languageOverride(NO_FONT_LANGUAGE_OVERRIDE),
    featureSettings(nsnull)
{
}

gfxFontStyle::gfxFontStyle(PRUint8 aStyle, PRUint16 aWeight, PRInt16 aStretch,
                           gfxFloat aSize, nsIAtom *aLanguage,
                           float aSizeAdjust, PRPackedBool aSystemFont,
                           PRPackedBool aFamilyNameQuirks,
                           PRPackedBool aPrinterFont,
                           const nsString& aFeatureSettings,
                           const nsString& aLanguageOverride):
    style(aStyle), systemFont(aSystemFont), printerFont(aPrinterFont),
    familyNameQuirks(aFamilyNameQuirks), weight(aWeight), stretch(aStretch),
    size(aSize), sizeAdjust(aSizeAdjust),
    language(aLanguage),
    languageOverride(ParseFontLanguageOverride(aLanguageOverride)),
    featureSettings(nsnull)
{
    if (!aFeatureSettings.IsEmpty()) {
        featureSettings = new nsTArray<gfxFontFeature>;
        ParseFontFeatureSettings(aFeatureSettings, *featureSettings);
        if (featureSettings->Length() == 0) {
            delete featureSettings;
            featureSettings = nsnull;
        }
    }

    if (weight > 900)
        weight = 900;
    if (weight < 100)
        weight = 100;

    if (size >= FONT_MAX_SIZE) {
        size = FONT_MAX_SIZE;
        sizeAdjust = 0.0;
    } else if (size < 0.0) {
        NS_WARNING("negative font size");
        size = 0.0;
    }

    if (!language) {
        NS_WARNING("null language");
        language = gfxAtoms::x_western;
    }
}

gfxFontStyle::gfxFontStyle(const gfxFontStyle& aStyle) :
    style(aStyle.style), systemFont(aStyle.systemFont), printerFont(aStyle.printerFont),
    familyNameQuirks(aStyle.familyNameQuirks), weight(aStyle.weight),
    stretch(aStyle.stretch), size(aStyle.size),
    sizeAdjust(aStyle.sizeAdjust),
    language(aStyle.language),
    languageOverride(aStyle.languageOverride),
    featureSettings(nsnull)
{
    if (aStyle.featureSettings) {
        featureSettings = new nsTArray<gfxFontFeature>;
        featureSettings->AppendElements(*aStyle.featureSettings);
    }
}

PRInt8
gfxFontStyle::ComputeWeight() const
{
    PRInt8 baseWeight = (weight + 50) / 100;

    if (baseWeight < 0)
        baseWeight = 0;
    if (baseWeight > 9)
        baseWeight = 9;

    return baseWeight;
}

PRBool
gfxTextRun::GlyphRunIterator::NextRun()  {
    if (mNextIndex >= mTextRun->mGlyphRuns.Length())
        return PR_FALSE;
    mGlyphRun = &mTextRun->mGlyphRuns[mNextIndex];
    if (mGlyphRun->mCharacterOffset >= mEndOffset)
        return PR_FALSE;

    mStringStart = PR_MAX(mStartOffset, mGlyphRun->mCharacterOffset);
    PRUint32 last = mNextIndex + 1 < mTextRun->mGlyphRuns.Length()
        ? mTextRun->mGlyphRuns[mNextIndex + 1].mCharacterOffset : mTextRun->mCharacterCount;
    mStringEnd = PR_MIN(mEndOffset, last);

    ++mNextIndex;
    return PR_TRUE;
}

#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
static void
AccountStorageForTextRun(gfxTextRun *aTextRun, PRInt32 aSign)
{
    // Ignores detailed glyphs... we don't know when those have been constructed
    // Also ignores gfxSkipChars dynamic storage (which won't be anything
    // for preformatted text)
    // Also ignores GlyphRun array, again because it hasn't been constructed
    // by the time this gets called. If there's only one glyphrun that's stored
    // directly in the textrun anyway so no additional overhead.
    PRUint32 length = aTextRun->GetLength();
    PRInt32 bytes = length * sizeof(gfxTextRun::CompressedGlyph);
    if (aTextRun->GetFlags() & gfxTextRunFactory::TEXT_IS_PERSISTENT) {
      bytes += length * ((aTextRun->GetFlags() & gfxTextRunFactory::TEXT_IS_8BIT) ? 1 : 2);
      bytes += sizeof(gfxTextRun::CompressedGlyph) - 1;
      bytes &= ~(sizeof(gfxTextRun::CompressedGlyph) - 1);
    }
    bytes += sizeof(gfxTextRun);
    gTextRunStorage += bytes*aSign;
    gTextRunStorageHighWaterMark = PR_MAX(gTextRunStorageHighWaterMark, gTextRunStorage);
}
#endif

// Helper for textRun creation to preallocate storage for glyphs and text;
// this function returns a pointer to the newly-allocated glyph storage,
// AND modifies the aText parameter if TEXT_IS_PERSISTENT was not set.
// In that case, the text is appended to the glyph storage, so a single
// delete[] operation in the textRun destructor will free both.
// Returns nsnull if allocation fails.
gfxTextRun::CompressedGlyph *
gfxTextRun::AllocateStorage(const void*& aText, PRUint32 aLength, PRUint32 aFlags)
{
    // Here, we rely on CompressedGlyph being the largest unit we care about for
    // allocation/alignment of either glyph data or text, so we allocate an array
    // of CompressedGlyphs, then take the last chunk of that and cast a pointer to
    // PRUint8* or PRUnichar* for text storage.

    // always need to allocate storage for the glyph data
    PRUint64 allocCount = aLength;

    // if the text is not persistent, we also need space for a copy
    if (!(aFlags & gfxTextRunFactory::TEXT_IS_PERSISTENT)) {
        // figure out number of extra CompressedGlyph elements we need to
        // get sufficient space for the text
        if (aFlags & gfxTextRunFactory::TEXT_IS_8BIT) {
            allocCount += (aLength + sizeof(CompressedGlyph)-1)
                          / sizeof(CompressedGlyph);
        } else {
            allocCount += (aLength*sizeof(PRUnichar) + sizeof(CompressedGlyph)-1)
                          / sizeof(CompressedGlyph);
        }
    }

    // allocate the storage we need, returning nsnull on failure rather than
    // throwing an exception (because web content can create huge runs)
    CompressedGlyph *storage = new (std::nothrow) CompressedGlyph[allocCount];
    if (!storage) {
        NS_WARNING("failed to allocate glyph/text storage for text run!");
        return nsnull;
    }

    // copy the text if we need to keep a copy in the textrun
    if (!(aFlags & gfxTextRunFactory::TEXT_IS_PERSISTENT)) {
        if (aFlags & gfxTextRunFactory::TEXT_IS_8BIT) {
            PRUint8 *newText = reinterpret_cast<PRUint8*>(storage + aLength);
            memcpy(newText, aText, aLength);
            aText = newText;
        } else {
            PRUnichar *newText = reinterpret_cast<PRUnichar*>(storage + aLength);
            memcpy(newText, aText, aLength*sizeof(PRUnichar));
            aText = newText;
        }
    }

    return storage;
}

gfxTextRun *
gfxTextRun::Create(const gfxTextRunFactory::Parameters *aParams, const void *aText,
                   PRUint32 aLength, gfxFontGroup *aFontGroup, PRUint32 aFlags)
{
    CompressedGlyph *glyphStorage = AllocateStorage(aText, aLength, aFlags);
    if (!glyphStorage) {
        return nsnull;
    }

    return new gfxTextRun(aParams, aText, aLength, aFontGroup, aFlags, glyphStorage);
}

gfxTextRun::gfxTextRun(const gfxTextRunFactory::Parameters *aParams, const void *aText,
                       PRUint32 aLength, gfxFontGroup *aFontGroup, PRUint32 aFlags,
                       CompressedGlyph *aGlyphStorage)
  : mCharacterGlyphs(aGlyphStorage),
    mUserData(aParams->mUserData),
    mFontGroup(aFontGroup),
    mAppUnitsPerDevUnit(aParams->mAppUnitsPerDevUnit),
    mFlags(aFlags), mCharacterCount(aLength), mHashCode(0)
{
    NS_ASSERTION(mAppUnitsPerDevUnit != 0, "Invalid app unit scale");
    MOZ_COUNT_CTOR(gfxTextRun);
    NS_ADDREF(mFontGroup);
    if (aParams->mSkipChars) {
        mSkipChars.TakeFrom(aParams->mSkipChars);
    }

    if (mFlags & gfxTextRunFactory::TEXT_IS_8BIT) {
        mText.mSingle = static_cast<const PRUint8 *>(aText);
    } else {
        mText.mDouble = static_cast<const PRUnichar *>(aText);
    }
#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
    AccountStorageForTextRun(this, 1);
#endif

    mUserFontSetGeneration = mFontGroup->GetGeneration();
}

gfxTextRun::~gfxTextRun()
{
#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
    AccountStorageForTextRun(this, -1);
#endif
#ifdef DEBUG
    // Make it easy to detect a dead text run
    mFlags = 0xFFFFFFFF;
#endif

    // this will also delete the text, if it is owned by the run,
    // because we merge the storage allocations
    delete [] mCharacterGlyphs;

    NS_RELEASE(mFontGroup);
    MOZ_COUNT_DTOR(gfxTextRun);
}

gfxTextRun *
gfxTextRun::Clone(const gfxTextRunFactory::Parameters *aParams, const void *aText,
                  PRUint32 aLength, gfxFontGroup *aFontGroup, PRUint32 aFlags)
{
    if (!mCharacterGlyphs)
        return nsnull;

    nsAutoPtr<gfxTextRun> textRun;
    textRun = gfxTextRun::Create(aParams, aText, aLength, aFontGroup, aFlags);
    if (!textRun)
        return nsnull;

    textRun->CopyGlyphDataFrom(this, 0, mCharacterCount, 0, PR_FALSE);
    return textRun.forget();
}

PRBool
gfxTextRun::SetPotentialLineBreaks(PRUint32 aStart, PRUint32 aLength,
                                   PRPackedBool *aBreakBefore,
                                   gfxContext *aRefContext)
{
    NS_ASSERTION(aStart + aLength <= mCharacterCount, "Overflow");

    if (!mCharacterGlyphs)
        return PR_TRUE;
    PRUint32 changed = 0;
    PRUint32 i;
    for (i = 0; i < aLength; ++i) {
        PRBool canBreak = aBreakBefore[i];
        if (canBreak && !mCharacterGlyphs[aStart + i].IsClusterStart()) {
            // This can happen ... there is no guarantee that our linebreaking rules
            // align with the platform's idea of what constitutes a cluster.
            NS_WARNING("Break suggested inside cluster!");
            canBreak = PR_FALSE;
        }
        changed |= mCharacterGlyphs[aStart + i].SetCanBreakBefore(canBreak);
    }
    return changed != 0;
}

gfxTextRun::LigatureData
gfxTextRun::ComputeLigatureData(PRUint32 aPartStart, PRUint32 aPartEnd,
                                PropertyProvider *aProvider)
{
    NS_ASSERTION(aPartStart < aPartEnd, "Computing ligature data for empty range");
    NS_ASSERTION(aPartEnd <= mCharacterCount, "Character length overflow");
  
    LigatureData result;
    CompressedGlyph *charGlyphs = mCharacterGlyphs;

    PRUint32 i;
    for (i = aPartStart; !charGlyphs[i].IsLigatureGroupStart(); --i) {
        NS_ASSERTION(i > 0, "Ligature at the start of the run??");
    }
    result.mLigatureStart = i;
    for (i = aPartStart + 1; i < mCharacterCount && !charGlyphs[i].IsLigatureGroupStart(); ++i) {
    }
    result.mLigatureEnd = i;

    PRInt32 ligatureWidth =
        GetAdvanceForGlyphs(this, result.mLigatureStart, result.mLigatureEnd);
    // Count the number of started clusters we have seen
    PRUint32 totalClusterCount = 0;
    PRUint32 partClusterIndex = 0;
    PRUint32 partClusterCount = 0;
    for (i = result.mLigatureStart; i < result.mLigatureEnd; ++i) {
        // Treat the first character of the ligature as the start of a
        // cluster for our purposes of allocating ligature width to its
        // characters.
        if (i == result.mLigatureStart || charGlyphs[i].IsClusterStart()) {
            ++totalClusterCount;
            if (i < aPartStart) {
                ++partClusterIndex;
            } else if (i < aPartEnd) {
                ++partClusterCount;
            }
        }
    }
    NS_ASSERTION(totalClusterCount > 0, "Ligature involving no clusters??");
    result.mPartAdvance = ligatureWidth*partClusterIndex/totalClusterCount;
    result.mPartWidth = ligatureWidth*partClusterCount/totalClusterCount;

    if (partClusterCount == 0) {
        // nothing to draw
        result.mClipBeforePart = result.mClipAfterPart = PR_TRUE;
    } else {
        // Determine whether we should clip before or after this part when
        // drawing its slice of the ligature.
        // We need to clip before the part if any cluster is drawn before
        // this part.
        result.mClipBeforePart = partClusterIndex > 0;
        // We need to clip after the part if any cluster is drawn after
        // this part.
        result.mClipAfterPart = partClusterIndex + partClusterCount < totalClusterCount;
    }

    if (aProvider && (mFlags & gfxTextRunFactory::TEXT_ENABLE_SPACING)) {
        gfxFont::Spacing spacing;
        if (aPartStart == result.mLigatureStart) {
            aProvider->GetSpacing(aPartStart, 1, &spacing);
            result.mPartWidth += spacing.mBefore;
        }
        if (aPartEnd == result.mLigatureEnd) {
            aProvider->GetSpacing(aPartEnd - 1, 1, &spacing);
            result.mPartWidth += spacing.mAfter;
        }
    }

    return result;
}

gfxFloat
gfxTextRun::ComputePartialLigatureWidth(PRUint32 aPartStart, PRUint32 aPartEnd,
                                        PropertyProvider *aProvider)
{
    if (aPartStart >= aPartEnd)
        return 0;
    LigatureData data = ComputeLigatureData(aPartStart, aPartEnd, aProvider);
    return data.mPartWidth;
}

static void
GetAdjustedSpacing(gfxTextRun *aTextRun, PRUint32 aStart, PRUint32 aEnd,
                   gfxTextRun::PropertyProvider *aProvider,
                   gfxTextRun::PropertyProvider::Spacing *aSpacing)
{
    if (aStart >= aEnd)
        return;

    aProvider->GetSpacing(aStart, aEnd - aStart, aSpacing);

#ifdef DEBUG
    // Check to see if we have spacing inside ligatures

    const gfxTextRun::CompressedGlyph *charGlyphs = aTextRun->GetCharacterGlyphs();
    PRUint32 i;

    for (i = aStart; i < aEnd; ++i) {
        if (!charGlyphs[i].IsLigatureGroupStart()) {
            NS_ASSERTION(i == aStart || aSpacing[i - aStart].mBefore == 0,
                         "Before-spacing inside a ligature!");
            NS_ASSERTION(i - 1 <= aStart || aSpacing[i - 1 - aStart].mAfter == 0,
                         "After-spacing inside a ligature!");
        }
    }
#endif
}

PRBool
gfxTextRun::GetAdjustedSpacingArray(PRUint32 aStart, PRUint32 aEnd,
                                    PropertyProvider *aProvider,
                                    PRUint32 aSpacingStart, PRUint32 aSpacingEnd,
                                    nsTArray<PropertyProvider::Spacing> *aSpacing)
{
    if (!aProvider || !(mFlags & gfxTextRunFactory::TEXT_ENABLE_SPACING))
        return PR_FALSE;
    if (!aSpacing->AppendElements(aEnd - aStart))
        return PR_FALSE;
    memset(aSpacing->Elements(), 0, sizeof(gfxFont::Spacing)*(aSpacingStart - aStart));
    GetAdjustedSpacing(this, aSpacingStart, aSpacingEnd, aProvider,
                       aSpacing->Elements() + aSpacingStart - aStart);
    memset(aSpacing->Elements() + aSpacingEnd - aStart, 0, sizeof(gfxFont::Spacing)*(aEnd - aSpacingEnd));
    return PR_TRUE;
}

void
gfxTextRun::ShrinkToLigatureBoundaries(PRUint32 *aStart, PRUint32 *aEnd)
{
    if (*aStart >= *aEnd)
        return;
  
    CompressedGlyph *charGlyphs = mCharacterGlyphs;

    while (*aStart < *aEnd && !charGlyphs[*aStart].IsLigatureGroupStart()) {
        ++(*aStart);
    }
    if (*aEnd < mCharacterCount) {
        while (*aEnd > *aStart && !charGlyphs[*aEnd].IsLigatureGroupStart()) {
            --(*aEnd);
        }
    }
}

void
gfxTextRun::DrawGlyphs(gfxFont *aFont, gfxContext *aContext,
                       PRBool aDrawToPath, gfxPoint *aPt,
                       PRUint32 aStart, PRUint32 aEnd,
                       PropertyProvider *aProvider,
                       PRUint32 aSpacingStart, PRUint32 aSpacingEnd)
{
    nsAutoTArray<PropertyProvider::Spacing,200> spacingBuffer;
    PRBool haveSpacing = GetAdjustedSpacingArray(aStart, aEnd, aProvider,
        aSpacingStart, aSpacingEnd, &spacingBuffer);
    aFont->Draw(this, aStart, aEnd, aContext, aDrawToPath, aPt,
                haveSpacing ? spacingBuffer.Elements() : nsnull);
}

static void
ClipPartialLigature(gfxTextRun *aTextRun, gfxFloat *aLeft, gfxFloat *aRight,
                    gfxFloat aXOrigin, gfxTextRun::LigatureData *aLigature)
{
    if (aLigature->mClipBeforePart) {
        if (aTextRun->IsRightToLeft()) {
            *aRight = PR_MIN(*aRight, aXOrigin);
        } else {
            *aLeft = PR_MAX(*aLeft, aXOrigin);
        }
    }
    if (aLigature->mClipAfterPart) {
        gfxFloat endEdge = aXOrigin + aTextRun->GetDirection()*aLigature->mPartWidth;
        if (aTextRun->IsRightToLeft()) {
            *aLeft = PR_MAX(*aLeft, endEdge);
        } else {
            *aRight = PR_MIN(*aRight, endEdge);
        }
    }    
}

void
gfxTextRun::DrawPartialLigature(gfxFont *aFont, gfxContext *aCtx, PRUint32 aStart,
                                PRUint32 aEnd,
                                const gfxRect *aDirtyRect, gfxPoint *aPt,
                                PropertyProvider *aProvider)
{
    if (aStart >= aEnd)
        return;
    if (!aDirtyRect) {
        NS_ERROR("Cannot draw partial ligatures without a dirty rect");
        return;
    }

    // Draw partial ligature. We hack this by clipping the ligature.
    LigatureData data = ComputeLigatureData(aStart, aEnd, aProvider);
    gfxFloat left = aDirtyRect->X();
    gfxFloat right = aDirtyRect->XMost();
    ClipPartialLigature(this, &left, &right, aPt->x, &data);

    aCtx->Save();
    aCtx->NewPath();
    // use division here to ensure that when the rect is aligned on multiples
    // of mAppUnitsPerDevUnit, we clip to true device unit boundaries.
    // Also, make sure we snap the rectangle to device pixels.
    aCtx->Rectangle(gfxRect(left/mAppUnitsPerDevUnit,
                            aDirtyRect->Y()/mAppUnitsPerDevUnit,
                            (right - left)/mAppUnitsPerDevUnit,
                            aDirtyRect->Height()/mAppUnitsPerDevUnit), PR_TRUE);
    aCtx->Clip();
    gfxFloat direction = GetDirection();
    gfxPoint pt(aPt->x - direction*data.mPartAdvance, aPt->y);
    DrawGlyphs(aFont, aCtx, PR_FALSE, &pt, data.mLigatureStart,
               data.mLigatureEnd, aProvider, aStart, aEnd);
    aCtx->Restore();

    aPt->x += direction*data.mPartWidth;
}

// returns true if a glyph run is using a font with synthetic bolding enabled, false otherwise
static PRBool
HasSyntheticBold(gfxTextRun *aRun, PRUint32 aStart, PRUint32 aLength)
{
    gfxTextRun::GlyphRunIterator iter(aRun, aStart, aLength);
    while (iter.NextRun()) {
        gfxFont *font = iter.GetGlyphRun()->mFont;
        if (font && font->IsSyntheticBold()) {
            return PR_TRUE;
        }
    }

    return PR_FALSE;
}

// returns true if color is non-opaque (i.e. alpha != 1.0) or completely transparent, false otherwise
// if true, color is set on output
static PRBool
HasNonOpaqueColor(gfxContext *aContext, gfxRGBA& aCurrentColor)
{
    if (aContext->GetDeviceColor(aCurrentColor)) {
        if (aCurrentColor.a < 1.0 && aCurrentColor.a > 0.0) {
            return PR_TRUE;
        }
    }
        
    return PR_FALSE;
}

// helper class for double-buffering drawing with non-opaque color
struct BufferAlphaColor {
    BufferAlphaColor(gfxContext *aContext)
        : mContext(aContext)
    {

    }

    ~BufferAlphaColor() {}

    void PushSolidColor(const gfxRect& aBounds, const gfxRGBA& aAlphaColor, PRUint32 appsPerDevUnit)
    {
        mContext->Save();
        mContext->NewPath();
        mContext->Rectangle(gfxRect(aBounds.X() / appsPerDevUnit,
                    aBounds.Y() / appsPerDevUnit,
                    aBounds.Width() / appsPerDevUnit,
                    aBounds.Height() / appsPerDevUnit), PR_TRUE);
        mContext->Clip();
        mContext->SetColor(gfxRGBA(aAlphaColor.r, aAlphaColor.g, aAlphaColor.b));
        mContext->PushGroup(gfxASurface::CONTENT_COLOR_ALPHA);
        mAlpha = aAlphaColor.a;
    }

    void PopAlpha()
    {
        // pop the text, using the color alpha as the opacity
        mContext->PopGroupToSource();
        mContext->SetOperator(gfxContext::OPERATOR_OVER);
        mContext->Paint(mAlpha);
        mContext->Restore();
    }

    gfxContext *mContext;
    gfxFloat mAlpha;
};

void
gfxTextRun::AdjustAdvancesForSyntheticBold(PRUint32 aStart, PRUint32 aLength)
{
    const PRUint32 appUnitsPerDevUnit = GetAppUnitsPerDevUnit();
    PRBool isRTL = IsRightToLeft();

    GlyphRunIterator iter(this, aStart, aLength);
    while (iter.NextRun()) {
        gfxFont *font = iter.GetGlyphRun()->mFont;
        if (font->IsSyntheticBold()) {
            PRUint32 synAppUnitOffset = font->GetSyntheticBoldOffset() * appUnitsPerDevUnit;
            PRUint32 start = iter.GetStringStart();
            PRUint32 end = iter.GetStringEnd();
            PRUint32 i;
            
            // iterate over glyphs, start to end
            for (i = start; i < end; ++i) {
                gfxTextRun::CompressedGlyph *glyphData = &mCharacterGlyphs[i];
                
                if (glyphData->IsSimpleGlyph()) {
                    // simple glyphs ==> just add the advance
                    PRUint32 advance = glyphData->GetSimpleAdvance() + synAppUnitOffset;
                    if (CompressedGlyph::IsSimpleAdvance(advance)) {
                        glyphData->SetSimpleGlyph(advance, glyphData->GetSimpleGlyph());
                    } else {
                        // rare case, tested by making this the default
                        PRUint32 glyphIndex = glyphData->GetSimpleGlyph();
                        glyphData->SetComplex(PR_TRUE, PR_TRUE, 1);
                        DetailedGlyph detail = {glyphIndex, advance, 0, 0};
                        SetGlyphs(i, *glyphData, &detail);
                    }
                } else {
                    // complex glyphs ==> add offset at cluster/ligature boundaries
                    PRUint32 detailedLength = glyphData->GetGlyphCount();
                    if (detailedLength && mDetailedGlyphs) {
                        gfxTextRun::DetailedGlyph *details = mDetailedGlyphs[i].get();
                        if (!details) continue;
                        if (isRTL)
                            details[0].mAdvance += synAppUnitOffset;
                        else
                            details[detailedLength - 1].mAdvance += synAppUnitOffset;
                    }
                }
            }
        }
    }
}

void
gfxTextRun::Draw(gfxContext *aContext, gfxPoint aPt,
                 PRUint32 aStart, PRUint32 aLength, const gfxRect *aDirtyRect,
                 PropertyProvider *aProvider, gfxFloat *aAdvanceWidth)
{
    NS_ASSERTION(aStart + aLength <= mCharacterCount, "Substring out of range");

    gfxFloat direction = GetDirection();
    gfxPoint pt = aPt;

    // synthetic bolding draws glyphs twice ==> colors with opacity won't draw correctly unless first drawn without alpha
    BufferAlphaColor syntheticBoldBuffer(aContext);
    gfxRGBA currentColor;
    PRBool needToRestore = PR_FALSE;

    if (HasNonOpaqueColor(aContext, currentColor) && HasSyntheticBold(this, aStart, aLength)) {
        needToRestore = PR_TRUE;
        // measure text, use the bounding box
        gfxTextRun::Metrics metrics = MeasureText(aStart, aLength, gfxFont::LOOSE_INK_EXTENTS,
                                                  aContext, aProvider);
        metrics.mBoundingBox.MoveBy(aPt);
        syntheticBoldBuffer.PushSolidColor(metrics.mBoundingBox, currentColor, GetAppUnitsPerDevUnit());
    }

    GlyphRunIterator iter(this, aStart, aLength);
    while (iter.NextRun()) {
        gfxFont *font = iter.GetGlyphRun()->mFont;
        PRUint32 start = iter.GetStringStart();
        PRUint32 end = iter.GetStringEnd();
        PRUint32 ligatureRunStart = start;
        PRUint32 ligatureRunEnd = end;
        ShrinkToLigatureBoundaries(&ligatureRunStart, &ligatureRunEnd);
        
        DrawPartialLigature(font, aContext, start, ligatureRunStart, aDirtyRect, &pt, aProvider);
        DrawGlyphs(font, aContext, PR_FALSE, &pt, ligatureRunStart,
                   ligatureRunEnd, aProvider, ligatureRunStart, ligatureRunEnd);
        DrawPartialLigature(font, aContext, ligatureRunEnd, end, aDirtyRect, &pt, aProvider);
    }

    // composite result when synthetic bolding used
    if (needToRestore) {
        syntheticBoldBuffer.PopAlpha();
    }

    if (aAdvanceWidth) {
        *aAdvanceWidth = (pt.x - aPt.x)*direction;
    }
}

void
gfxTextRun::DrawToPath(gfxContext *aContext, gfxPoint aPt,
                       PRUint32 aStart, PRUint32 aLength,
                       PropertyProvider *aProvider, gfxFloat *aAdvanceWidth)
{
    NS_ASSERTION(aStart + aLength <= mCharacterCount, "Substring out of range");

    gfxFloat direction = GetDirection();
    gfxPoint pt = aPt;

    GlyphRunIterator iter(this, aStart, aLength);
    while (iter.NextRun()) {
        gfxFont *font = iter.GetGlyphRun()->mFont;
        PRUint32 start = iter.GetStringStart();
        PRUint32 end = iter.GetStringEnd();
        PRUint32 ligatureRunStart = start;
        PRUint32 ligatureRunEnd = end;
        ShrinkToLigatureBoundaries(&ligatureRunStart, &ligatureRunEnd);
        NS_ASSERTION(ligatureRunStart == start,
                     "Can't draw path starting inside ligature");
        NS_ASSERTION(ligatureRunEnd == end,
                     "Can't end drawing path inside ligature");

        DrawGlyphs(font, aContext, PR_TRUE, &pt, ligatureRunStart, ligatureRunEnd, aProvider,
            ligatureRunStart, ligatureRunEnd);
    }

    if (aAdvanceWidth) {
        *aAdvanceWidth = (pt.x - aPt.x)*direction;
    }
}

void
gfxTextRun::AccumulateMetricsForRun(gfxFont *aFont,
                                    PRUint32 aStart, PRUint32 aEnd,
                                    gfxFont::BoundingBoxType aBoundingBoxType,
                                    gfxContext *aRefContext,
                                    PropertyProvider *aProvider,
                                    PRUint32 aSpacingStart, PRUint32 aSpacingEnd,
                                    Metrics *aMetrics)
{
    nsAutoTArray<PropertyProvider::Spacing,200> spacingBuffer;
    PRBool haveSpacing = GetAdjustedSpacingArray(aStart, aEnd, aProvider,
        aSpacingStart, aSpacingEnd, &spacingBuffer);
    Metrics metrics = aFont->Measure(this, aStart, aEnd, aBoundingBoxType, aRefContext,
                                     haveSpacing ? spacingBuffer.Elements() : nsnull);
    aMetrics->CombineWith(metrics, IsRightToLeft());
}

void
gfxTextRun::AccumulatePartialLigatureMetrics(gfxFont *aFont,
    PRUint32 aStart, PRUint32 aEnd,
    gfxFont::BoundingBoxType aBoundingBoxType, gfxContext *aRefContext,
    PropertyProvider *aProvider, Metrics *aMetrics)
{
    if (aStart >= aEnd)
        return;

    // Measure partial ligature. We hack this by clipping the metrics in the
    // same way we clip the drawing.
    LigatureData data = ComputeLigatureData(aStart, aEnd, aProvider);

    // First measure the complete ligature
    Metrics metrics;
    AccumulateMetricsForRun(aFont, data.mLigatureStart, data.mLigatureEnd,
                            aBoundingBoxType, aRefContext,
                            aProvider, aStart, aEnd, &metrics);

    // Clip the bounding box to the ligature part
    gfxFloat bboxLeft = metrics.mBoundingBox.X();
    gfxFloat bboxRight = metrics.mBoundingBox.XMost();
    // Where we are going to start "drawing" relative to our left baseline origin
    gfxFloat origin = IsRightToLeft() ? metrics.mAdvanceWidth - data.mPartAdvance : 0;
    ClipPartialLigature(this, &bboxLeft, &bboxRight, origin, &data);
    metrics.mBoundingBox.pos.x = bboxLeft;
    metrics.mBoundingBox.size.width = bboxRight - bboxLeft;

    // mBoundingBox is now relative to the left baseline origin for the entire
    // ligature. Shift it left.
    metrics.mBoundingBox.pos.x -=
        IsRightToLeft() ? metrics.mAdvanceWidth - (data.mPartAdvance + data.mPartWidth)
            : data.mPartAdvance;    
    metrics.mAdvanceWidth = data.mPartWidth;

    aMetrics->CombineWith(metrics, IsRightToLeft());
}

gfxTextRun::Metrics
gfxTextRun::MeasureText(PRUint32 aStart, PRUint32 aLength,
                        gfxFont::BoundingBoxType aBoundingBoxType,
                        gfxContext *aRefContext,
                        PropertyProvider *aProvider)
{
    NS_ASSERTION(aStart + aLength <= mCharacterCount, "Substring out of range");

    Metrics accumulatedMetrics;
    GlyphRunIterator iter(this, aStart, aLength);
    while (iter.NextRun()) {
        gfxFont *font = iter.GetGlyphRun()->mFont;
        PRUint32 start = iter.GetStringStart();
        PRUint32 end = iter.GetStringEnd();
        PRUint32 ligatureRunStart = start;
        PRUint32 ligatureRunEnd = end;
        ShrinkToLigatureBoundaries(&ligatureRunStart, &ligatureRunEnd);

        AccumulatePartialLigatureMetrics(font, start, ligatureRunStart,
            aBoundingBoxType, aRefContext, aProvider, &accumulatedMetrics);

        // XXX This sucks. We have to get glyph extents just so we can detect
        // glyphs outside the font box, even when aBoundingBoxType is LOOSE,
        // even though in almost all cases we could get correct results just
        // by getting some ascent/descent from the font and using our stored
        // advance widths.
        AccumulateMetricsForRun(font,
            ligatureRunStart, ligatureRunEnd, aBoundingBoxType,
            aRefContext, aProvider, ligatureRunStart, ligatureRunEnd,
            &accumulatedMetrics);

        AccumulatePartialLigatureMetrics(font, ligatureRunEnd, end,
            aBoundingBoxType, aRefContext, aProvider, &accumulatedMetrics);
    }

    return accumulatedMetrics;
}

#define MEASUREMENT_BUFFER_SIZE 100

PRUint32
gfxTextRun::BreakAndMeasureText(PRUint32 aStart, PRUint32 aMaxLength,
                                PRBool aLineBreakBefore, gfxFloat aWidth,
                                PropertyProvider *aProvider,
                                PRBool aSuppressInitialBreak,
                                gfxFloat *aTrimWhitespace,
                                Metrics *aMetrics,
                                gfxFont::BoundingBoxType aBoundingBoxType,
                                gfxContext *aRefContext,
                                PRBool *aUsedHyphenation,
                                PRUint32 *aLastBreak,
                                PRBool aCanWordWrap,
                                gfxBreakPriority *aBreakPriority)
{
    aMaxLength = PR_MIN(aMaxLength, mCharacterCount - aStart);

    NS_ASSERTION(aStart + aMaxLength <= mCharacterCount, "Substring out of range");

    PRUint32 bufferStart = aStart;
    PRUint32 bufferLength = PR_MIN(aMaxLength, MEASUREMENT_BUFFER_SIZE);
    PropertyProvider::Spacing spacingBuffer[MEASUREMENT_BUFFER_SIZE];
    PRBool haveSpacing = aProvider && (mFlags & gfxTextRunFactory::TEXT_ENABLE_SPACING) != 0;
    if (haveSpacing) {
        GetAdjustedSpacing(this, bufferStart, bufferStart + bufferLength, aProvider,
                           spacingBuffer);
    }
    PRPackedBool hyphenBuffer[MEASUREMENT_BUFFER_SIZE];
    PRBool haveHyphenation = (mFlags & gfxTextRunFactory::TEXT_ENABLE_HYPHEN_BREAKS) != 0;
    if (haveHyphenation) {
        aProvider->GetHyphenationBreaks(bufferStart, bufferLength,
                                        hyphenBuffer);
    }

    gfxFloat width = 0;
    gfxFloat advance = 0;
    // The number of space characters that can be trimmed
    PRUint32 trimmableChars = 0;
    // The amount of space removed by ignoring trimmableChars
    gfxFloat trimmableAdvance = 0;
    PRInt32 lastBreak = -1;
    PRInt32 lastBreakTrimmableChars = -1;
    gfxFloat lastBreakTrimmableAdvance = -1;
    PRBool aborted = PR_FALSE;
    PRUint32 end = aStart + aMaxLength;
    PRBool lastBreakUsedHyphenation = PR_FALSE;

    PRUint32 ligatureRunStart = aStart;
    PRUint32 ligatureRunEnd = end;
    ShrinkToLigatureBoundaries(&ligatureRunStart, &ligatureRunEnd);

    PRUint32 i;
    for (i = aStart; i < end; ++i) {
        if (i >= bufferStart + bufferLength) {
            // Fetch more spacing and hyphenation data
            bufferStart = i;
            bufferLength = PR_MIN(aStart + aMaxLength, i + MEASUREMENT_BUFFER_SIZE) - i;
            if (haveSpacing) {
                GetAdjustedSpacing(this, bufferStart, bufferStart + bufferLength, aProvider,
                                   spacingBuffer);
            }
            if (haveHyphenation) {
                aProvider->GetHyphenationBreaks(bufferStart, bufferLength,
                                                hyphenBuffer);
            }
        }

        // There can't be a word-wrap break opportunity at the beginning of the
        // line: if the width is too small for even one character to fit, it 
        // could be the first and last break opportunity on the line, and that
        // would trigger an infinite loop.
        if (!aSuppressInitialBreak || i > aStart) {
            PRBool lineBreakHere = mCharacterGlyphs[i].CanBreakBefore();
            PRBool hyphenation = haveHyphenation && hyphenBuffer[i - bufferStart];
            PRBool wordWrapping = aCanWordWrap && *aBreakPriority <= eWordWrapBreak;

            if (lineBreakHere || hyphenation || wordWrapping) {
                gfxFloat hyphenatedAdvance = advance;
                if (!lineBreakHere && !wordWrapping) {
                    hyphenatedAdvance += aProvider->GetHyphenWidth();
                }
            
                if (lastBreak < 0 || width + hyphenatedAdvance - trimmableAdvance <= aWidth) {
                    // We can break here.
                    lastBreak = i;
                    lastBreakTrimmableChars = trimmableChars;
                    lastBreakTrimmableAdvance = trimmableAdvance;
                    lastBreakUsedHyphenation = !lineBreakHere && !wordWrapping;
                    *aBreakPriority = hyphenation || lineBreakHere ?
                        eNormalBreak : eWordWrapBreak;
                }

                width += advance;
                advance = 0;
                if (width - trimmableAdvance > aWidth) {
                    // No more text fits. Abort
                    aborted = PR_TRUE;
                    break;
                }
            }
        }
        
        gfxFloat charAdvance;
        if (i >= ligatureRunStart && i < ligatureRunEnd) {
            charAdvance = GetAdvanceForGlyphs(this, i, i + 1);
            if (haveSpacing) {
                PropertyProvider::Spacing *space = &spacingBuffer[i - bufferStart];
                charAdvance += space->mBefore + space->mAfter;
            }
        } else {
            charAdvance = ComputePartialLigatureWidth(i, i + 1, aProvider);
        }
        
        advance += charAdvance;
        if (aTrimWhitespace) {
            if (GetChar(i) == ' ') {
                ++trimmableChars;
                trimmableAdvance += charAdvance;
            } else {
                trimmableAdvance = 0;
                trimmableChars = 0;
            }
        }
    }

    if (!aborted) {
        width += advance;
    }

    // There are three possibilities:
    // 1) all the text fit (width <= aWidth)
    // 2) some of the text fit up to a break opportunity (width > aWidth && lastBreak >= 0)
    // 3) none of the text fits before a break opportunity (width > aWidth && lastBreak < 0)
    PRUint32 charsFit;
    PRBool usedHyphenation = PR_FALSE;
    if (width - trimmableAdvance <= aWidth) {
        charsFit = aMaxLength;
    } else if (lastBreak >= 0) {
        charsFit = lastBreak - aStart;
        trimmableChars = lastBreakTrimmableChars;
        trimmableAdvance = lastBreakTrimmableAdvance;
        usedHyphenation = lastBreakUsedHyphenation;
    } else {
        charsFit = aMaxLength;
    }

    if (aMetrics) {
        *aMetrics = MeasureText(aStart, charsFit - trimmableChars,
            aBoundingBoxType, aRefContext, aProvider);
    }
    if (aTrimWhitespace) {
        *aTrimWhitespace = trimmableAdvance;
    }
    if (aUsedHyphenation) {
        *aUsedHyphenation = usedHyphenation;
    }
    if (aLastBreak && charsFit == aMaxLength) {
        if (lastBreak < 0) {
            *aLastBreak = PR_UINT32_MAX;
        } else {
            *aLastBreak = lastBreak - aStart;
        }
    }

    return charsFit;
}

gfxFloat
gfxTextRun::GetAdvanceWidth(PRUint32 aStart, PRUint32 aLength,
                            PropertyProvider *aProvider)
{
    NS_ASSERTION(aStart + aLength <= mCharacterCount, "Substring out of range");

    PRUint32 ligatureRunStart = aStart;
    PRUint32 ligatureRunEnd = aStart + aLength;
    ShrinkToLigatureBoundaries(&ligatureRunStart, &ligatureRunEnd);

    gfxFloat result = ComputePartialLigatureWidth(aStart, ligatureRunStart, aProvider) +
                      ComputePartialLigatureWidth(ligatureRunEnd, aStart + aLength, aProvider);

    // Account for all remaining spacing here. This is more efficient than
    // processing it along with the glyphs.
    if (aProvider && (mFlags & gfxTextRunFactory::TEXT_ENABLE_SPACING)) {
        PRUint32 i;
        nsAutoTArray<PropertyProvider::Spacing,200> spacingBuffer;
        if (spacingBuffer.AppendElements(aLength)) {
            GetAdjustedSpacing(this, ligatureRunStart, ligatureRunEnd, aProvider,
                               spacingBuffer.Elements());
            for (i = 0; i < ligatureRunEnd - ligatureRunStart; ++i) {
                PropertyProvider::Spacing *space = &spacingBuffer[i];
                result += space->mBefore + space->mAfter;
            }
        }
    }

    return result + GetAdvanceForGlyphs(this, ligatureRunStart, ligatureRunEnd);
}

PRBool
gfxTextRun::SetLineBreaks(PRUint32 aStart, PRUint32 aLength,
                          PRBool aLineBreakBefore, PRBool aLineBreakAfter,
                          gfxFloat *aAdvanceWidthDelta,
                          gfxContext *aRefContext)
{
    // Do nothing because our shaping does not currently take linebreaks into
    // account. There is no change in advance width.
    if (aAdvanceWidthDelta) {
        *aAdvanceWidthDelta = 0;
    }
    return PR_FALSE;
}

PRUint32
gfxTextRun::FindFirstGlyphRunContaining(PRUint32 aOffset)
{
    NS_ASSERTION(aOffset <= mCharacterCount, "Bad offset looking for glyphrun");
    NS_ASSERTION(mCharacterCount == 0 || mGlyphRuns.Length() > 0,
                 "non-empty text but no glyph runs present!");
    if (aOffset == mCharacterCount)
        return mGlyphRuns.Length();
    PRUint32 start = 0;
    PRUint32 end = mGlyphRuns.Length();
    while (end - start > 1) {
        PRUint32 mid = (start + end)/2;
        if (mGlyphRuns[mid].mCharacterOffset <= aOffset) {
            start = mid;
        } else {
            end = mid;
        }
    }
    NS_ASSERTION(mGlyphRuns[start].mCharacterOffset <= aOffset,
                 "Hmm, something went wrong, aOffset should have been found");
    return start;
}

nsresult
gfxTextRun::AddGlyphRun(gfxFont *aFont, PRUint32 aUTF16Offset, PRBool aForceNewRun)
{
    PRUint32 numGlyphRuns = mGlyphRuns.Length();
    if (!aForceNewRun &&
        numGlyphRuns > 0)
    {
        GlyphRun *lastGlyphRun = &mGlyphRuns[numGlyphRuns - 1];

        NS_ASSERTION(lastGlyphRun->mCharacterOffset <= aUTF16Offset,
                     "Glyph runs out of order (and run not forced)");

        if (lastGlyphRun->mFont == aFont)
            return NS_OK;
        if (lastGlyphRun->mCharacterOffset == aUTF16Offset) {
            lastGlyphRun->mFont = aFont;
            return NS_OK;
        }
    }

    NS_ASSERTION(aForceNewRun || numGlyphRuns > 0 || aUTF16Offset == 0,
                 "First run doesn't cover the first character (and run not forced)?");

    GlyphRun *glyphRun = mGlyphRuns.AppendElement();
    if (!glyphRun)
        return NS_ERROR_OUT_OF_MEMORY;
    glyphRun->mFont = aFont;
    glyphRun->mCharacterOffset = aUTF16Offset;
    return NS_OK;
}

void
gfxTextRun::SortGlyphRuns()
{
    if (mGlyphRuns.Length() <= 1)
        return;

    nsTArray<GlyphRun> runs(mGlyphRuns);
    GlyphRunOffsetComparator comp;
    runs.Sort(comp);

    // Now copy back, coalescing adjacent glyph runs that have the same font
    mGlyphRuns.Clear();
    PRUint32 i;
    for (i = 0; i < runs.Length(); ++i) {
        // a GlyphRun with the same font as the previous GlyphRun can just
        // be skipped; the last GlyphRun will cover its character range.
        if (i == 0 || runs[i].mFont != runs[i - 1].mFont) {
            mGlyphRuns.AppendElement(runs[i]);
            // If two fonts have the same character offset, Sort() will have
            // randomized the order.
            NS_ASSERTION(i == 0 ||
                         runs[i].mCharacterOffset !=
                         runs[i - 1].mCharacterOffset,
                         "Two fonts for the same run, glyph indices may not match the font");
        }
    }
}

void
gfxTextRun::SanitizeGlyphRuns()
{
    if (mGlyphRuns.Length() <= 1)
        return;

    // If any glyph run starts with ligature-continuation characters, we need to advance it
    // to the first "real" character to avoid drawing partial ligature glyphs from wrong font
    // (seen with U+FEFF in reftest 474417-1, as Core Text eliminates the glyph, which makes
    // it appear as if a ligature has been formed)
    PRInt32 i, lastRunIndex = mGlyphRuns.Length() - 1;
    for (i = lastRunIndex; i >= 0; --i) {
        GlyphRun& run = mGlyphRuns[i];
        while (mCharacterGlyphs[run.mCharacterOffset].IsLigatureContinuation() &&
               run.mCharacterOffset < mCharacterCount) {
            run.mCharacterOffset++;
        }
        // if the run has become empty, eliminate it
        if ((i < lastRunIndex &&
             run.mCharacterOffset >= mGlyphRuns[i+1].mCharacterOffset) ||
            (i == lastRunIndex && run.mCharacterOffset == mCharacterCount)) {
            mGlyphRuns.RemoveElementAt(i);
            --lastRunIndex;
        }
    }
}

PRUint32
gfxTextRun::CountMissingGlyphs()
{
    PRUint32 i;
    PRUint32 count = 0;
    for (i = 0; i < mCharacterCount; ++i) {
        if (mCharacterGlyphs[i].IsMissing()) {
            ++count;
        }
    }
    return count;
}

gfxTextRun::DetailedGlyph *
gfxTextRun::AllocateDetailedGlyphs(PRUint32 aIndex, PRUint32 aCount)
{
    NS_ASSERTION(aIndex < mCharacterCount, "Index out of range");

    if (!mCharacterGlyphs)
        return nsnull;

    if (!mDetailedGlyphs) {
        mDetailedGlyphs = new nsAutoArrayPtr<DetailedGlyph>[mCharacterCount];
        if (!mDetailedGlyphs) {
            mCharacterGlyphs[aIndex].SetMissing(0);
            return nsnull;
        }
    }
    DetailedGlyph *details = new DetailedGlyph[aCount];
    if (!details) {
        mCharacterGlyphs[aIndex].SetMissing(0);
        return nsnull;
    }
    mDetailedGlyphs[aIndex] = details;
    return details;
}

void
gfxTextRun::SetGlyphs(PRUint32 aIndex, CompressedGlyph aGlyph,
                      const DetailedGlyph *aGlyphs)
{
    NS_ASSERTION(!aGlyph.IsSimpleGlyph(), "Simple glyphs not handled here");
    NS_ASSERTION(aIndex > 0 ||
                 (aGlyph.IsClusterStart() && aGlyph.IsLigatureGroupStart()),
                 "First character must be the start of a cluster and can't be a ligature continuation!");

    PRUint32 glyphCount = aGlyph.GetGlyphCount();
    if (glyphCount > 0) {
        DetailedGlyph *details = AllocateDetailedGlyphs(aIndex, glyphCount);
        if (!details)
            return;
        memcpy(details, aGlyphs, sizeof(DetailedGlyph)*glyphCount);
    }
    mCharacterGlyphs[aIndex] = aGlyph;
}

#include "ignorable.x-ccmap"
DEFINE_X_CCMAP(gIgnorableCCMapExt, const);

static inline PRBool
IsDefaultIgnorable(PRUint32 aChar)
{
    return CCMAP_HAS_CHAR_EXT(gIgnorableCCMapExt, aChar);
}

void
gfxTextRun::SetMissingGlyph(PRUint32 aIndex, PRUint32 aChar)
{
    DetailedGlyph *details = AllocateDetailedGlyphs(aIndex, 1);
    if (!details)
        return;

    details->mGlyphID = aChar;
    GlyphRun *glyphRun = &mGlyphRuns[FindFirstGlyphRunContaining(aIndex)];
    if (IsDefaultIgnorable(aChar)) {
        // Setting advance width to zero will prevent drawing the hexbox
        details->mAdvance = 0;
    } else {
        gfxFloat width = PR_MAX(glyphRun->mFont->GetMetrics().aveCharWidth,
                                gfxFontMissingGlyphs::GetDesiredMinWidth(aChar));
        details->mAdvance = PRUint32(width*GetAppUnitsPerDevUnit());
    }
    details->mXOffset = 0;
    details->mYOffset = 0;
    mCharacterGlyphs[aIndex].SetMissing(1);
}

PRBool
gfxTextRun::FilterIfIgnorable(PRUint32 aIndex)
{
    PRUint32 ch = GetChar(aIndex);
    if (IsDefaultIgnorable(ch)) {
        DetailedGlyph *details = AllocateDetailedGlyphs(aIndex, 1);
        if (details) {
            details->mGlyphID = ch;
            details->mAdvance = 0;
            details->mXOffset = 0;
            details->mYOffset = 0;
            mCharacterGlyphs[aIndex].SetMissing(1);
            return PR_TRUE;
        }
    }
    return PR_FALSE;
}

static void
ClearCharacters(gfxTextRun::CompressedGlyph *aGlyphs, PRUint32 aLength)
{
    memset(aGlyphs, 0, sizeof(gfxTextRun::CompressedGlyph)*aLength);
}

void
gfxTextRun::CopyGlyphDataFrom(gfxTextRun *aSource, PRUint32 aStart,
                              PRUint32 aLength, PRUint32 aDest,
                              PRBool aStealData)
{
    NS_ASSERTION(aStart + aLength <= aSource->GetLength(),
                 "Source substring out of range");
    NS_ASSERTION(aDest + aLength <= GetLength(),
                 "Destination substring out of range");

    PRUint32 i;
    // Copy base character data
    for (i = 0; i < aLength; ++i) {
        CompressedGlyph g = aSource->mCharacterGlyphs[i + aStart];
        g.SetCanBreakBefore(mCharacterGlyphs[i + aDest].CanBreakBefore());
        mCharacterGlyphs[i + aDest] = g;
        if (aStealData) {
            aSource->mCharacterGlyphs[i + aStart].SetMissing(0);
        }
    }

    // Copy detailed glyphs
    if (aSource->mDetailedGlyphs) {
        for (i = 0; i < aLength; ++i) {
            DetailedGlyph *details = aSource->mDetailedGlyphs[i + aStart];
            if (details) {
                if (aStealData) {
                    if (!mDetailedGlyphs) {
                        mDetailedGlyphs = new nsAutoArrayPtr<DetailedGlyph>[mCharacterCount];
                        if (!mDetailedGlyphs) {
                            ClearCharacters(&mCharacterGlyphs[aDest], aLength);
                            return;
                        }
                    }        
                    mDetailedGlyphs[i + aDest] = details;
                    aSource->mDetailedGlyphs[i + aStart].forget();
                } else {
                    PRUint32 glyphCount = mCharacterGlyphs[i + aDest].GetGlyphCount();
                    DetailedGlyph *dest = AllocateDetailedGlyphs(i + aDest, glyphCount);
                    if (!dest) {
                        ClearCharacters(&mCharacterGlyphs[aDest], aLength);
                        return;
                    }
                    memcpy(dest, details, sizeof(DetailedGlyph)*glyphCount);
                }
            } else if (mDetailedGlyphs) {
                mDetailedGlyphs[i + aDest] = nsnull;
            }
        }
    } else if (mDetailedGlyphs) {
        for (i = 0; i < aLength; ++i) {
            mDetailedGlyphs[i + aDest] = nsnull;
        }
    }

    // Copy glyph runs
    GlyphRunIterator iter(aSource, aStart, aLength);
#ifdef DEBUG
    gfxFont *lastFont = nsnull;
#endif
    while (iter.NextRun()) {
        gfxFont *font = iter.GetGlyphRun()->mFont;
        NS_ASSERTION(font != lastFont, "Glyphruns not coalesced?");
#ifdef DEBUG
        lastFont = font;
        PRUint32 end = iter.GetStringEnd();
#endif
        PRUint32 start = iter.GetStringStart();

        // These used to be NS_ASSERTION()s, but WARNING is more appropriate.
        // Although it's unusual (and not desirable), it's possible for us to assign
        // different fonts to a base character and a following diacritic.
        // Example on OSX 10.5/10.6 with default fonts installed:
        //     data:text/html,<p style="font-family:helvetica, arial, sans-serif;">
        //                    &#x043E;&#x0486;&#x20;&#x043E;&#x0486;
        // This means the rendering of the cluster will probably not be very good,
        // but it's the best we can do for now if the specified font only covered the
        // initial base character and not its applied marks.
        NS_WARN_IF_FALSE(aSource->IsClusterStart(start),
                         "Started font run in the middle of a cluster");
        NS_WARN_IF_FALSE(end == aSource->GetLength() || aSource->IsClusterStart(end),
                         "Ended font run in the middle of a cluster");

        nsresult rv = AddGlyphRun(font, start - aStart + aDest);
        if (NS_FAILED(rv))
            return;
    }
}

void
gfxTextRun::SetSpaceGlyph(gfxFont *aFont, gfxContext *aContext, PRUint32 aCharIndex)
{
    PRUint32 spaceGlyph = aFont->GetSpaceGlyph();
    float spaceWidth = aFont->GetMetrics().spaceWidth;
    PRUint32 spaceWidthAppUnits = NS_lroundf(spaceWidth*mAppUnitsPerDevUnit);
    if (!spaceGlyph ||
        !CompressedGlyph::IsSimpleGlyphID(spaceGlyph) ||
        !CompressedGlyph::IsSimpleAdvance(spaceWidthAppUnits)) {
        gfxTextRunFactory::Parameters params = {
            aContext, nsnull, nsnull, nsnull, 0, mAppUnitsPerDevUnit
        };
        static const PRUint8 space = ' ';
        nsAutoPtr<gfxTextRun> textRun;
        textRun = mFontGroup->MakeTextRun(&space, 1, &params,
            gfxTextRunFactory::TEXT_IS_8BIT | gfxTextRunFactory::TEXT_IS_ASCII |
            gfxTextRunFactory::TEXT_IS_PERSISTENT);
        if (!textRun || !textRun->mCharacterGlyphs)
            return;
        CopyGlyphDataFrom(textRun, 0, 1, aCharIndex, PR_TRUE);
        return;
    }

    AddGlyphRun(aFont, aCharIndex);
    CompressedGlyph g;
    g.SetSimpleGlyph(spaceWidthAppUnits, spaceGlyph);
    SetSimpleGlyph(aCharIndex, g);
}

void
gfxTextRun::FetchGlyphExtents(gfxContext *aRefContext)
{
    PRBool needsGlyphExtents = NeedsGlyphExtents(this);
    if (!needsGlyphExtents && !mDetailedGlyphs)
        return;

    PRUint32 i;
    CompressedGlyph *charGlyphs = mCharacterGlyphs;
    for (i = 0; i < mGlyphRuns.Length(); ++i) {
        gfxFont *font = mGlyphRuns[i].mFont;
        PRUint32 start = mGlyphRuns[i].mCharacterOffset;
        PRUint32 end = i + 1 < mGlyphRuns.Length()
            ? mGlyphRuns[i + 1].mCharacterOffset : GetLength();
        PRBool fontIsSetup = PR_FALSE;
        PRUint32 j;
        gfxGlyphExtents *extents = font->GetOrCreateGlyphExtents(mAppUnitsPerDevUnit);
  
        for (j = start; j < end; ++j) {
            const gfxTextRun::CompressedGlyph *glyphData = &charGlyphs[j];
            if (glyphData->IsSimpleGlyph()) {
                // If we're in speed mode, don't set up glyph extents here; we'll
                // just return "optimistic" glyph bounds later
                if (needsGlyphExtents) {
                    PRUint32 glyphIndex = glyphData->GetSimpleGlyph();
                    if (!extents->IsGlyphKnown(glyphIndex)) {
                        if (!fontIsSetup) {
                            font->SetupCairoFont(aRefContext);
                             fontIsSetup = PR_TRUE;
                        }
#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
                        ++gGlyphExtentsSetupEagerSimple;
#endif
                        font->SetupGlyphExtents(aRefContext, glyphIndex, PR_FALSE, extents);
                    }
                }
            } else if (!glyphData->IsMissing()) {
                PRUint32 k;
                PRUint32 glyphCount = glyphData->GetGlyphCount();
                const gfxTextRun::DetailedGlyph *details = GetDetailedGlyphs(j);
                for (k = 0; k < glyphCount; ++k, ++details) {
                    PRUint32 glyphIndex = details->mGlyphID;
                    if (!extents->IsGlyphKnownWithTightExtents(glyphIndex)) {
                        if (!fontIsSetup) {
                            font->SetupCairoFont(aRefContext);
                            fontIsSetup = PR_TRUE;
                        }
#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
                        ++gGlyphExtentsSetupEagerTight;
#endif
                        font->SetupGlyphExtents(aRefContext, glyphIndex, PR_TRUE, extents);
                    }
                }
            }
        }
    }
}

#ifdef DEBUG
void
gfxTextRun::Dump(FILE* aOutput) {
    if (!aOutput) {
        aOutput = stdout;
    }

    PRUint32 i;
    fputc('"', aOutput);
    for (i = 0; i < mCharacterCount; ++i) {
        PRUnichar ch = GetChar(i);
        if (ch >= 32 && ch < 128) {
            fputc(ch, aOutput);
        } else {
            fprintf(aOutput, "\\u%4x", ch);
        }
    }
    fputs("\" [", aOutput);
    for (i = 0; i < mGlyphRuns.Length(); ++i) {
        if (i > 0) {
            fputc(',', aOutput);
        }
        gfxFont* font = mGlyphRuns[i].mFont;
        const gfxFontStyle* style = font->GetStyle();
        NS_ConvertUTF16toUTF8 fontName(font->GetName());
        nsCAutoString lang;
        style->language->ToUTF8String(lang);
        fprintf(aOutput, "%d: %s %f/%d/%d/%s", mGlyphRuns[i].mCharacterOffset,
                fontName.get(), style->size,
                style->weight, style->style, lang.get());
    }
    fputc(']', aOutput);
}
#endif
