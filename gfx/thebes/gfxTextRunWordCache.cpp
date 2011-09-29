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
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com>
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

#include "gfxTextRunWordCache.h"

#include "nsWeakReference.h"
#include "nsCRT.h"
#include "nsIObserver.h"

#include "nsBidiUtils.h"
#include "mozilla/Preferences.h"

#if defined(XP_UNIX)
#include <stdint.h>
#endif

#ifdef DEBUG
#include <stdio.h>
#endif

using namespace mozilla;

/**
 * Cache individual "words" (strings delimited by white-space or white-space-like
 * characters that don't involve kerning or ligatures) in textruns.
 *  
 * The characters treated as word boundaries are defined by IsWordBoundary
 * below. The characters are: space, NBSP, and all the characters
 * defined by gfxFontGroup::IsInvalidChar. The latter are all converted
 * to invisible missing glyphs in this code. Thus, this class ensures
 * that none of those invalid characters are ever passed to platform
 * textrun implementations.
 * 
 * Some platforms support marks combining with spaces to form clusters.
 * In such cases we treat "before the space" as a word boundary but
 * "after the space" is not a word boundary; words with a leading space
 * are kept out of the cache. Also, words at the start of text, which start
 * with combining marks that would combine with a space if there was one,
 * are also kept out of the cache.
 */

class TextRunWordCache :
    public nsIObserver,
    public nsSupportsWeakReference {
public:
    TextRunWordCache() :
        mBidiNumeral(0) {
        mCache.Init(100);
    }
    ~TextRunWordCache() {
        Uninit();
        NS_WARN_IF_FALSE(mCache.Count() == 0, "Textrun cache not empty!");
    }
    void Init();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER

    /**
     * Create a textrun using cached words.
     * Invalid characters (see gfxFontGroup::IsInvalidChar) will be automatically
     * treated as invisible missing.
     * @param aFlags the flag TEXT_IS_ASCII must be set by the caller,
     * if applicable
     * @param aIsInCache if true is returned, then RemoveTextRun must be called
     * before the textrun changes or dies.
     */
    gfxTextRun *MakeTextRun(const PRUnichar *aText, PRUint32 aLength,
                            gfxFontGroup *aFontGroup,
                            const gfxFontGroup::Parameters *aParams,
                            PRUint32 aFlags);
    /**
     * Create a textrun using cached words.
     * Invalid characters (see gfxFontGroup::IsInvalidChar) will be automatically
     * treated as invisible missing.
     * @param aFlags the flag TEXT_IS_ASCII must be set by the caller,
     * if applicable
     * @param aIsInCache if true is returned, then RemoveTextRun must be called
     * before the textrun changes or dies.
     */
    gfxTextRun *MakeTextRun(const PRUint8 *aText, PRUint32 aLength,
                            gfxFontGroup *aFontGroup,
                            const gfxFontGroup::Parameters *aParams,
                            PRUint32 aFlags);

    /**
     * Remove a textrun from the cache. This must be called before aTextRun
     * is deleted! The text in the textrun must still be valid.
     */
    void RemoveTextRun(gfxTextRun *aTextRun);

    /**
     * Flush all cached runs. Use when a setting change makes them obsolete.
     */
    void Flush() {
        mCache.Clear(); 
#ifdef DEBUG
        mGeneration++;
#endif
    }

#ifdef DEBUG
    PRUint32 mGeneration;
    void Dump();
#endif

protected:
    struct CacheHashKey {
        void        *mFontOrGroup;
        const void  *mString;
        PRUint32     mLength;
        PRUint32     mAppUnitsPerDevUnit;
        PRUint32     mStringHash;
        PRUint64     mUserFontSetGeneration;
        bool mIsDoubleByteText;
        bool mIsRTL;
        bool mEnabledOptionalLigatures;
        bool mOptimizeSpeed;
        
        CacheHashKey(gfxTextRun *aBaseTextRun, void *aFontOrGroup,
                     PRUint32 aStart, PRUint32 aLength, PRUint32 aHash)
            : mFontOrGroup(aFontOrGroup), mString(aBaseTextRun->GetTextAt(aStart)),
              mLength(aLength),
              mAppUnitsPerDevUnit(aBaseTextRun->GetAppUnitsPerDevUnit()),
              mStringHash(aHash),
              mUserFontSetGeneration(aBaseTextRun->GetUserFontSetGeneration()),
              mIsDoubleByteText((aBaseTextRun->GetFlags() & gfxTextRunFactory::TEXT_IS_8BIT) == 0),
              mIsRTL(aBaseTextRun->IsRightToLeft()),
              mEnabledOptionalLigatures((aBaseTextRun->GetFlags() & gfxTextRunFactory::TEXT_DISABLE_OPTIONAL_LIGATURES) == 0),
              mOptimizeSpeed((aBaseTextRun->GetFlags() & gfxTextRunFactory::TEXT_OPTIMIZE_SPEED) != 0)
        {
        }
    };

    class CacheHashEntry : public PLDHashEntryHdr {
    public:
        typedef const CacheHashKey &KeyType;
        typedef const CacheHashKey *KeyTypePointer;

        // When constructing a new entry in the hashtable, the caller of Put()
        // will fill us in.
        CacheHashEntry(KeyTypePointer aKey) : mTextRun(nsnull), mWordOffset(0),
            mHashedByFont(PR_FALSE) { }
        CacheHashEntry(const CacheHashEntry& toCopy) { NS_ERROR("Should not be called"); }
        ~CacheHashEntry() { }

        bool KeyEquals(const KeyTypePointer aKey) const;
        static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
        static PLDHashNumber HashKey(const KeyTypePointer aKey);
        enum { ALLOW_MEMMOVE = PR_TRUE };

        gfxTextRun *mTextRun;
        // The offset of the start of the word in the textrun. The length of
        // the word is not stored here because we can figure it out by
        // looking at the textrun's text.
        PRUint32    mWordOffset:31;
        // This is set to true when the cache entry was hashed by the first
        // font in mTextRun's fontgroup; it's false when the cache entry
        // was hashed by the fontgroup itself.
        PRUint32    mHashedByFont:1;
    };
    
    // Used to track words that should be copied from one textrun to
    // another during the textrun construction process
    struct DeferredWord {
        gfxTextRun *mSourceTextRun;
        PRUint32    mSourceOffset;
        PRUint32    mDestOffset;
        PRUint32    mLength;
        PRUint32    mHash;
    };
    
    bool LookupWord(gfxTextRun *aTextRun, gfxFont *aFirstFont,
                      PRUint32 aStart, PRUint32 aEnd, PRUint32 aHash,
                      nsTArray<DeferredWord>* aDeferredWords);
    void FinishTextRun(gfxTextRun *aTextRun, gfxTextRun *aNewRun,
                       const gfxFontGroup::Parameters *aParams,
                       const nsTArray<DeferredWord>& aDeferredWords,
                       bool aSuccessful);
    void RemoveWord(gfxTextRun *aTextRun, PRUint32 aStart,
                    PRUint32 aEnd, PRUint32 aHash);
    void Uninit();

    nsTHashtable<CacheHashEntry> mCache;

    PRInt32 mBidiNumeral;

#ifdef DEBUG
    static PLDHashOperator CacheDumpEntry(CacheHashEntry* aEntry, void* userArg);
#endif
};

NS_IMPL_ISUPPORTS2(TextRunWordCache, nsIObserver, nsISupportsWeakReference)

static TextRunWordCache *gTextRunWordCache = nsnull;

static PRLogModuleInfo *gWordCacheLog = PR_NewLogModule("wordCache");

static const char* kObservedPrefs[] = {
    "bidi.",
    "font.",
    nsnull
};

void
TextRunWordCache::Init()
{
#ifdef DEBUG
    mGeneration = 0;
#endif

    Preferences::AddWeakObservers(this, kObservedPrefs);
    mBidiNumeral = Preferences::GetInt("bidi.numeral", mBidiNumeral);
}

void
TextRunWordCache::Uninit()
{
    Preferences::RemoveObservers(this, kObservedPrefs);
}

NS_IMETHODIMP
TextRunWordCache::Observe(nsISupports     *aSubject,
                          const char      *aTopic,
                          const PRUnichar *aData)
{
    if (!nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
        if (!nsCRT::strcmp(aData, NS_LITERAL_STRING("bidi.numeral").get())) {
          mBidiNumeral = Preferences::GetInt("bidi.numeral", mBidiNumeral);
        }
        mCache.Clear();
        PR_LOG(gWordCacheLog, PR_LOG_DEBUG, ("flushing the textrun cache"));
#ifdef DEBUG
        mGeneration++;
#endif
    }

    return NS_OK;
}

static inline PRUint32
HashMix(PRUint32 aHash, PRUnichar aCh)
{
    return (aHash >> 28) ^ (aHash << 4) ^ aCh;
}

// If the substring of the textrun is rendered entirely in the first font
// of the textrun's fontgroup, then return that font. Otherwise return the
// fontgroup.  When a user font set is in use, always return the font group.
static void *GetWordFontOrGroup(gfxTextRun *aTextRun, PRUint32 aOffset,
                                PRUint32 aLength)
{
    gfxFontGroup *fontGroup = aTextRun->GetFontGroup();
    if (fontGroup->GetUserFontSet() != nsnull)
        return fontGroup;
        
    PRUint32 glyphRunCount;
    const gfxTextRun::GlyphRun *glyphRuns = aTextRun->GetGlyphRuns(&glyphRunCount);
    PRUint32 glyphRunIndex = aTextRun->FindFirstGlyphRunContaining(aOffset);
    gfxFont *firstFont = fontGroup->GetFontAt(0);
    if (glyphRuns[glyphRunIndex].mFont != firstFont)
        return fontGroup;

    PRUint32 glyphRunEnd = glyphRunIndex == glyphRunCount - 1
        ? aTextRun->GetLength() : glyphRuns[glyphRunIndex + 1].mCharacterOffset;
    if (aOffset + aLength <= glyphRunEnd)
        return firstFont;
    return fontGroup;
}

#define UNICODE_NBSP 0x00A0

// XXX should we treat NBSP or SPACE combined with other characters as a word
// boundary? Currently this does.
static bool
IsBoundarySpace(PRUnichar aChar)
{
    return aChar == ' ' || aChar == UNICODE_NBSP;
}

static bool
IsWordBoundary(PRUnichar aChar)
{
    return IsBoundarySpace(aChar) || gfxFontGroup::IsInvalidChar(aChar);
}

/**
 * Looks up a word in the cache. If the word is found in the cache
 * (which could be from an existing textrun or an earlier word in the same
 * textrun), we copy the glyphs from the word into the textrun, unless
 * aDeferredWords is non-null (meaning that all words from now on must be
 * copied later instead of now), in which case we add the word to be copied
 * to the list.
 * 
 * If the word is not found in the cache then we add it to the cache with
 * aFirstFont as the key, on the assumption that the word will be matched
 * by aFirstFont. If this assumption is false we fix up the cache later in
 * FinishTextRun. We make this assumption for two reasons:
 * 1) it's usually true so it saves an extra cache lookup if we had to insert
 * the entry later
 * 2) we need to record words that appear in the string in some kind
 * of hash table so we can detect and use them if they appear later in the
 * (in general the string might be huge and contain many repeated words).
 * We might as well use the master hash table for this.
 * 
 * @return true if the word was found in the cache, false otherwise.
 */
bool
TextRunWordCache::LookupWord(gfxTextRun *aTextRun, gfxFont *aFirstFont,
                             PRUint32 aStart, PRUint32 aEnd, PRUint32 aHash,
                             nsTArray<DeferredWord>* aDeferredWords)
{
    if (aEnd <= aStart)
        return PR_TRUE;
        
    gfxFontGroup *fontGroup = aTextRun->GetFontGroup();

    bool useFontGroup = (fontGroup->GetUserFontSet() != nsnull);
    CacheHashKey key(aTextRun, (useFontGroup ? (void*)fontGroup : (void*)aFirstFont), aStart, aEnd - aStart, aHash);
    CacheHashEntry *fontEntry = mCache.PutEntry(key);
    if (!fontEntry)
        return PR_FALSE;
    CacheHashEntry *existingEntry = nsnull;

    if (fontEntry->mTextRun) {
        existingEntry = fontEntry;
    } else if (useFontGroup) {
        PR_LOG(gWordCacheLog, PR_LOG_DEBUG, ("%p(%d-%d,%d): added using font group", aTextRun, aStart, aEnd - aStart, aHash));
    } else {
        // test to see if this can be found using the font group instead
        PR_LOG(gWordCacheLog, PR_LOG_DEBUG, ("%p(%d-%d,%d): added using font", aTextRun, aStart, aEnd - aStart, aHash));
        key.mFontOrGroup = aTextRun->GetFontGroup();
        CacheHashEntry *groupEntry = mCache.GetEntry(key);
        if (groupEntry) {
            existingEntry = groupEntry;
            mCache.RawRemoveEntry(fontEntry);
            PR_LOG(gWordCacheLog, PR_LOG_DEBUG, ("%p(%d-%d,%d): removed using font", aTextRun, aStart, aEnd - aStart, aHash));
            fontEntry = nsnull;
        }
    }
    // At this point, either existingEntry is non-null and points to (surprise!)
    // an entry for a word in an existing textrun, or fontEntry points
    // to a cache entry for this word with aFirstFont, which needs to be
    // filled in or removed.

    if (existingEntry) {
        if (aDeferredWords) {
            DeferredWord word = { existingEntry->mTextRun,
                  existingEntry->mWordOffset, aStart, aEnd - aStart, aHash };
            aDeferredWords->AppendElement(word);
        } else {
            aTextRun->CopyGlyphDataFrom(existingEntry->mTextRun,
                existingEntry->mWordOffset, aEnd - aStart, aStart);
        }
        return PR_TRUE;
    }

#ifdef DEBUG
    ++aTextRun->mCachedWords;
#endif
    // Set up the cache entry so that if later in this textrun we hit this
    // entry, we'll copy within our own textrun
    fontEntry->mTextRun = aTextRun;
    fontEntry->mWordOffset = aStart;
    if (!useFontGroup)
        fontEntry->mHashedByFont = PR_TRUE;
    return PR_FALSE;
}

/**
 * Processes all deferred words. Each word is copied from the source
 * textrun to the output textrun. (The source may be an earlier word in the
 * output textrun.) If the word was not matched by the textrun's fontgroup's
 * first font, then we remove the entry we optimistically added to the cache
 * with that font in the key, and add a new entry keyed with the fontgroup
 * instead.
 * 
 * @param aSuccessful if false, then we don't do any word copies and we don't
 * add anything to the cache, but we still remove all the optimistic cache
 * entries.
 */
void
TextRunWordCache::FinishTextRun(gfxTextRun *aTextRun, gfxTextRun *aNewRun,
                                const gfxFontGroup::Parameters *aParams,
                                const nsTArray<DeferredWord>& aDeferredWords,
                                bool aSuccessful)
{
    aTextRun->SetFlagBits(gfxTextRunWordCache::TEXT_IN_CACHE);

    PRUint32 i;
    gfxFontGroup *fontGroup = aTextRun->GetFontGroup();
    gfxFont *font = fontGroup->GetFontAt(0);
    
    // need to use the font group when user font set is around, since
    // the first font may change as the result of a font download
    bool useFontGroup = (fontGroup->GetUserFontSet() != nsnull);

    // copy deferred words from various sources into destination textrun
    for (i = 0; i < aDeferredWords.Length(); ++i) {
        const DeferredWord *word = &aDeferredWords[i];
        gfxTextRun *source = word->mSourceTextRun;
        if (!source) {
            source = aNewRun;
        }
        // If the word starts inside a cluster we don't want this word
        // in the cache, so we'll remove the associated cache entry
        bool wordStartsInsideCluster;
        bool wordStartsInsideLigature;
        if (aSuccessful) {
            wordStartsInsideCluster =
                !source->IsClusterStart(word->mSourceOffset);
            wordStartsInsideLigature =
                !source->IsLigatureGroupStart(word->mSourceOffset);
        }
        if (source == aNewRun) {
            // We created a cache entry for this word based on the assumption
            // that the word matches GetFontAt(0). If this assumption is false,
            // we need to remove that cache entry and replace it with an entry
            // keyed off the fontgroup.
            bool removeFontKey = !aSuccessful ||
                wordStartsInsideCluster || wordStartsInsideLigature ||
                (!useFontGroup && font != GetWordFontOrGroup(aNewRun,
                                                             word->mSourceOffset,
                                                             word->mLength));
            if (removeFontKey) {
                // We need to remove the current placeholder cache entry
                CacheHashKey key(aTextRun,
                                 (useFontGroup ? (void*)fontGroup : (void*)font),
                                 word->mDestOffset, word->mLength, word->mHash);
                NS_ASSERTION(mCache.GetEntry(key),
                             "This entry should have been added previously!");
                mCache.RemoveEntry(key);
#ifdef DEBUG
                --aTextRun->mCachedWords;
#endif
                PR_LOG(gWordCacheLog, PR_LOG_DEBUG, ("%p(%d-%d,%d): removed using font", aTextRun, word->mDestOffset, word->mLength, word->mHash));
                
                if (aSuccessful && !wordStartsInsideCluster && !wordStartsInsideLigature) {
                    key.mFontOrGroup = fontGroup;
                    CacheHashEntry *groupEntry = mCache.PutEntry(key);
                    if (groupEntry) {
#ifdef DEBUG
                        ++aTextRun->mCachedWords;
#endif
                        PR_LOG(gWordCacheLog, PR_LOG_DEBUG, ("%p(%d-%d,%d): added using fontgroup", aTextRun, word->mDestOffset, word->mLength, word->mHash));
                        groupEntry->mTextRun = aTextRun;
                        groupEntry->mWordOffset = word->mDestOffset;
                        groupEntry->mHashedByFont = PR_FALSE;
                        NS_ASSERTION(mCache.GetEntry(key),
                                     "We should find the thing we just added!");
                    }
                }
            }
        }
        if (aSuccessful) {
            // Copy the word.
            PRUint32 sourceOffset = word->mSourceOffset;
            PRUint32 destOffset = word->mDestOffset;
            PRUint32 length = word->mLength;
            nsAutoPtr<gfxTextRun> tmpTextRun;
            if (wordStartsInsideCluster || wordStartsInsideLigature) {
                NS_ASSERTION(sourceOffset > 0, "How can the first character be inside a cluster?");
                if (wordStartsInsideCluster && destOffset > 0 &&
                    IsBoundarySpace(aTextRun->GetChar(destOffset - 1))) {
                    // The first character of the word formed a cluster
                    // with the preceding space.
                    // We should copy over data for the preceding space
                    // as well. The glyphs have probably been attached
                    // to that character.
                    --sourceOffset;
                    --destOffset;
                    ++length;
                } else {
                    // URK! This case sucks! We have combining marks or
                    // part of a ligature at the start of the text. We
                    // had to prepend a space just so we could detect this
                    // situation (so we can keep this "word" out of the
                    // cache). But now the data in aNewRun is no use to us.
                    // We need to find out what the platform would do
                    // if the characters were at the start of the text.
                    if (source->GetFlags() & gfxFontGroup::TEXT_IS_8BIT) {
                        tmpTextRun = fontGroup->
                            MakeTextRun(source->GetText8Bit() + sourceOffset,
                                        length, aParams, source->GetFlags());
                    } else {
                        tmpTextRun = fontGroup->
                            MakeTextRun(source->GetTextUnicode() + sourceOffset,
                                        length, aParams, source->GetFlags());
                    }
                    if (tmpTextRun) {
                        source = tmpTextRun;
                        sourceOffset = 0;
                    } else {
                        // If we failed to create the temporary run (OOM),
                        // skip the word, as if aSuccessful had been FALSE.
                        // (In practice this is only likely to occur if
                        // we're on the verge of an OOM crash anyhow.
                        // But ignoring gfxFontGroup::MakeTextRun() failure
                        // is bad because it means we'd be using an invalid
                        // source pointer.)
                        continue;
                    }
                }
            }
            aTextRun->CopyGlyphDataFrom(source, sourceOffset, length,
                                        destOffset);
            // Fill in additional spaces
            PRUint32 endCharIndex;
            if (i + 1 < aDeferredWords.Length()) {
                endCharIndex = aDeferredWords[i + 1].mDestOffset;
            } else {
                endCharIndex = aTextRun->GetLength();
            }
            PRUint32 charIndex;
            for (charIndex = word->mDestOffset + word->mLength;
                 charIndex < endCharIndex; ++charIndex) {
                if (IsBoundarySpace(aTextRun->GetChar(charIndex))) {
                    aTextRun->SetSpaceGlyph(font, aParams->mContext, charIndex);
                }
            }
        }
    }
}

static gfxTextRun *
MakeBlankTextRun(const void* aText, PRUint32 aLength,
                         gfxFontGroup *aFontGroup,
                         const gfxFontGroup::Parameters *aParams,
                         PRUint32 aFlags)
{
    nsAutoPtr<gfxTextRun> textRun;
    textRun = gfxTextRun::Create(aParams, aText, aLength, aFontGroup, aFlags);
    if (!textRun || !textRun->GetCharacterGlyphs())
        return nsnull;
    gfxFont *font = aFontGroup->GetFontAt(0);
    textRun->AddGlyphRun(font, gfxTextRange::kFontGroup, 0, PR_FALSE);
#ifdef DEBUG
    textRun->mCachedWords = 0;
    textRun->mCacheGeneration = gTextRunWordCache ? gTextRunWordCache->mGeneration : 0;
#endif
    return textRun.forget();
}

gfxTextRun *
TextRunWordCache::MakeTextRun(const PRUnichar *aText, PRUint32 aLength,
                              gfxFontGroup *aFontGroup,
                              const gfxFontGroup::Parameters *aParams,
                              PRUint32 aFlags)
{
    // update font list when using user fonts (assures generation is current)
    aFontGroup->UpdateFontList();

    if (aFontGroup->GetStyle()->size == 0) {
        // Short-circuit for size-0 fonts, as Windows and ATSUI can't handle
        // them, and always create at least size 1 fonts, i.e. they still
        // render something for size 0 fonts.
        return MakeBlankTextRun(aText, aLength, aFontGroup, aParams, aFlags);
    }

    nsAutoPtr<gfxTextRun> textRun;
    textRun = gfxTextRun::Create(aParams, aText, aLength, aFontGroup, aFlags);
    if (!textRun || !textRun->GetCharacterGlyphs())
        return nsnull;
#ifdef DEBUG
    textRun->mCachedWords = 0;
    textRun->mCacheGeneration = mGeneration;
#endif

    gfxFont *font = aFontGroup->GetFontAt(0);
    nsresult rv =
        textRun->AddGlyphRun(font, gfxTextRange::kFontGroup, 0, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, nsnull);

    nsAutoTArray<PRUnichar,200> tempString;
    nsAutoTArray<DeferredWord,50> deferredWords;
    nsAutoTArray<nsAutoPtr<gfxTextRun>,10> transientRuns;
    PRUint32 i;
    PRUint32 wordStart = 0;
    PRUint32 hash = 0;
    bool seenDigitToModify = false;
    bool needsNumeralProcessing =
        mBidiNumeral != IBMBIDI_NUMERAL_NOMINAL;
    for (i = 0; i <= aLength; ++i) {
        PRUnichar ch = i < aLength ? aText[i] : ' ';
        if (!seenDigitToModify && needsNumeralProcessing) {
            // check if there is a digit that needs to be transformed
            if (HandleNumberInChar(ch, !!(i > 0 ?
                                       IS_ARABIC_CHAR(aText[i-1]) :
                                       (aFlags & gfxTextRunWordCache::TEXT_INCOMING_ARABICCHAR)),
                                   mBidiNumeral) != ch)
                seenDigitToModify = PR_TRUE;
        }
        if (IsWordBoundary(ch)) {
            if (seenDigitToModify) {
                // the word included at least one digit that is modified by the current
                // bidi.numerals setting, so we must not use the cache for this word;
                // instead, we'll create a new textRun and a DeferredWord entry pointing to it
                PRUint32 length = i - wordStart;
                nsAutoArrayPtr<PRUnichar> numString;
                numString = new PRUnichar[length];
                for (PRUint32 j = 0; j < length; ++j) {
                    numString[j] = HandleNumberInChar(aText[wordStart+j],
                                                      !!(j > 0 ?
                                                          IS_ARABIC_CHAR(numString[j-1]) :
                                                          (aFlags & gfxTextRunWordCache::TEXT_INCOMING_ARABICCHAR)),
                                                      mBidiNumeral);
                }
                // now we make a transient textRun for the transformed word; this will not be cached
                gfxTextRun *numRun;
                numRun =
                    aFontGroup->MakeTextRun(numString.get(), length, aParams,
                                            aFlags & ~(gfxTextRunFactory::TEXT_IS_PERSISTENT |
                                                       gfxTextRunFactory::TEXT_IS_8BIT));
                // If MakeTextRun failed, numRun will be null, which is bad...
                // we'll just pretend there wasn't a digit to process.
                // This means we won't have the correct numerals, but at least
                // we're not trying to copy glyph data from an invalid source.
                // In practice it's unlikely to happen unless we're very close
                // to crashing due to OOM.
                if (numRun) {
                    DeferredWord word = { numRun, 0, wordStart, length, hash };
                    deferredWords.AppendElement(word);
                    transientRuns.AppendElement(numRun);
                } else {
                    seenDigitToModify = PR_FALSE;
                }
            }

            if (!seenDigitToModify) {
                // didn't need to modify digits (or failed to do so)
                bool hit = LookupWord(textRun, font, wordStart, i, hash,
                                        deferredWords.Length() == 0 ? nsnull : &deferredWords);
                if (!hit) {
                    // Always put a space before the word so we can detect
                    // combining characters at the start of a word
                    tempString.AppendElement(' ');
                    PRUint32 offset = tempString.Length();
                    PRUint32 length = i - wordStart;
                    PRUnichar *chars = tempString.AppendElements(length);
                    if (!chars) {
                        FinishTextRun(textRun, nsnull, nsnull, deferredWords, PR_FALSE);
                        return nsnull;
                    }
                    memcpy(chars, aText + wordStart, length*sizeof(PRUnichar));
                    DeferredWord word = { nsnull, offset, wordStart, length, hash };
                    deferredWords.AppendElement(word);
                }

                if (deferredWords.Length() == 0) {
                    if (IsBoundarySpace(ch) && i < aLength) {
                        textRun->SetSpaceGlyph(font, aParams->mContext, i);
                    } // else we should set this character to be invisible missing,
                      // but it already is because the textrun is blank!
                }
            } else {
                seenDigitToModify = PR_FALSE;
            }

            hash = 0;
            wordStart = i + 1;
        } else {
            hash = HashMix(hash, ch);
        }
    }

    if (deferredWords.Length() == 0) {
        // We got everything from the cache, so we're done. No point in calling
        // FinishTextRun.
        // This textrun is not referenced by the cache.
        return textRun.forget();
    }

    // create textrun for unknown words
    gfxTextRunFactory::Parameters params =
        { aParams->mContext, nsnull, nsnull, nsnull, 0, aParams->mAppUnitsPerDevUnit };
    nsAutoPtr<gfxTextRun> newRun;
    if (tempString.Length() == 0) {
        newRun = aFontGroup->MakeEmptyTextRun(&params, aFlags);
    } else {
        newRun = aFontGroup->MakeTextRun(tempString.Elements(), tempString.Length(),
                                         &params, aFlags | gfxTextRunFactory::TEXT_IS_PERSISTENT);
    }
    FinishTextRun(textRun, newRun, aParams, deferredWords, newRun != nsnull);
    return textRun.forget();
}

gfxTextRun *
TextRunWordCache::MakeTextRun(const PRUint8 *aText, PRUint32 aLength,
                              gfxFontGroup *aFontGroup,
                              const gfxFontGroup::Parameters *aParams,
                              PRUint32 aFlags)
{
    // update font list when using user fonts (assures generation is current)
    aFontGroup->UpdateFontList();

    if (aFontGroup->GetStyle()->size == 0) {
        // Short-circuit for size-0 fonts, as Windows and ATSUI can't handle
        // them, and always create at least size 1 fonts, i.e. they still
        // render something for size 0 fonts.
        return MakeBlankTextRun(aText, aLength, aFontGroup, aParams, aFlags);
    }

    aFlags |= gfxTextRunFactory::TEXT_IS_8BIT;
    nsAutoPtr<gfxTextRun> textRun;
    textRun = gfxTextRun::Create(aParams, aText, aLength, aFontGroup, aFlags);
    if (!textRun || !textRun->GetCharacterGlyphs())
        return nsnull;
#ifdef DEBUG
    textRun->mCachedWords = 0;
    textRun->mCacheGeneration = mGeneration;
#endif

    gfxFont *font = aFontGroup->GetFontAt(0);
    nsresult rv =
        textRun->AddGlyphRun(font, gfxTextRange::kFontGroup, 0, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, nsnull);

    nsAutoTArray<PRUint8,200> tempString;
    nsAutoTArray<DeferredWord,50> deferredWords;
    nsAutoTArray<nsAutoPtr<gfxTextRun>,10> transientRuns;
    PRUint32 i;
    PRUint32 wordStart = 0;
    PRUint32 hash = 0;
    bool seenDigitToModify = false;
    bool needsNumeralProcessing =
        mBidiNumeral != IBMBIDI_NUMERAL_NOMINAL;
    for (i = 0; i <= aLength; ++i) {
        PRUint8 ch = i < aLength ? aText[i] : ' ';
        if (!seenDigitToModify && needsNumeralProcessing) {
            // check if there is a digit that needs to be transformed
            if (HandleNumberInChar(ch, i == 0 && (aFlags & gfxTextRunWordCache::TEXT_INCOMING_ARABICCHAR),
                                   mBidiNumeral) != ch)
                seenDigitToModify = PR_TRUE;
        }
        if (IsWordBoundary(ch)) {
            if (seenDigitToModify) {
                // see parallel code in the 16-bit method above
                PRUint32 length = i - wordStart;
                nsAutoArrayPtr<PRUnichar> numString;
                numString = new PRUnichar[length];
                for (PRUint32 j = 0; j < length; ++j) {
                    numString[j] = HandleNumberInChar(aText[wordStart+j],
                                                      !!(j > 0 ?
                                                          IS_ARABIC_CHAR(numString[j-1]) :
                                                          (aFlags & gfxTextRunWordCache::TEXT_INCOMING_ARABICCHAR)),
                                                      mBidiNumeral);
                }
                // now we make a transient textRun for the transformed word; this will not be cached
                gfxTextRun *numRun;
                numRun =
                    aFontGroup->MakeTextRun(numString.get(), length, aParams,
                                            aFlags & ~(gfxTextRunFactory::TEXT_IS_PERSISTENT |
                                                       gfxTextRunFactory::TEXT_IS_8BIT));
                if (numRun) {
                    DeferredWord word = { numRun, 0, wordStart, length, hash };
                    deferredWords.AppendElement(word);
                    transientRuns.AppendElement(numRun);
                } else {
                    seenDigitToModify = PR_FALSE;
                }
            }

            if (!seenDigitToModify) {
                bool hit = LookupWord(textRun, font, wordStart, i, hash,
                                        deferredWords.Length() == 0 ? nsnull : &deferredWords);
                if (!hit) {
                    if (tempString.Length() > 0) {
                        tempString.AppendElement(' ');
                    }
                    PRUint32 offset = tempString.Length();
                    PRUint32 length = i - wordStart;
                    PRUint8 *chars = tempString.AppendElements(length);
                    if (!chars) {
                        FinishTextRun(textRun, nsnull, nsnull, deferredWords, PR_FALSE);
                        return nsnull;
                    }
                    memcpy(chars, aText + wordStart, length*sizeof(PRUint8));
                    DeferredWord word = { nsnull, offset, wordStart, length, hash };
                    deferredWords.AppendElement(word);
                }

                if (deferredWords.Length() == 0) {
                    if (IsBoundarySpace(ch) && i < aLength) {
                        textRun->SetSpaceGlyph(font, aParams->mContext, i);
                    } // else we should set this character to be invisible missing,
                      // but it already is because the textrun is blank!
                }
            } else {
                seenDigitToModify = PR_FALSE;
            }

            hash = 0;
            wordStart = i + 1;
        } else {
            hash = HashMix(hash, ch);
        }
    }

    if (deferredWords.Length() == 0) {
        // We got everything from the cache, so we're done. No point in calling
        // FinishTextRun.
        // This textrun is not referenced by the cache.
        return textRun.forget();
    }

    // create textrun for unknown words
    gfxTextRunFactory::Parameters params =
        { aParams->mContext, nsnull, nsnull, nsnull, 0, aParams->mAppUnitsPerDevUnit };
    nsAutoPtr<gfxTextRun> newRun;
    if (tempString.Length() == 0) {
        newRun = aFontGroup->MakeEmptyTextRun(&params, aFlags);
    } else {
        newRun = aFontGroup->MakeTextRun(tempString.Elements(), tempString.Length(),
                                         &params, aFlags | gfxTextRunFactory::TEXT_IS_PERSISTENT);
    }
    FinishTextRun(textRun, newRun, aParams, deferredWords, newRun != nsnull);
    return textRun.forget();
}

void
TextRunWordCache::RemoveWord(gfxTextRun *aTextRun, PRUint32 aStart,
                             PRUint32 aEnd, PRUint32 aHash)
{
    if (aEnd <= aStart)
        return;

    PRUint32 length = aEnd - aStart;
    CacheHashKey key(aTextRun, GetWordFontOrGroup(aTextRun, aStart, length),
                     aStart, length, aHash);
    CacheHashEntry *entry = mCache.GetEntry(key);
    if (entry && entry->mTextRun == aTextRun) {
        // XXX would like to use RawRemoveEntry here plus some extra method
        // that conditionally shrinks the hashtable
        mCache.RemoveEntry(key);
#ifdef DEBUG
        --aTextRun->mCachedWords;
#endif
        PR_LOG(gWordCacheLog, PR_LOG_DEBUG, ("%p(%d-%d,%d): removed using %s",
            aTextRun, aStart, length, aHash,
            key.mFontOrGroup == aTextRun->GetFontGroup() ? "fontgroup" : "font"));
    }
}

// Remove a textrun from the cache by looking up each word and removing it
void
TextRunWordCache::RemoveTextRun(gfxTextRun *aTextRun)
{
#ifdef DEBUG
    if (aTextRun->mCacheGeneration != mGeneration) {
        PR_LOG(gWordCacheLog, PR_LOG_DEBUG, ("cache generation changed (aTextRun %p)", aTextRun));
        return;
    }
#endif
    PRUint32 i;
    PRUint32 wordStart = 0;
    PRUint32 hash = 0;
    for (i = 0; i < aTextRun->GetLength(); ++i) {
        PRUnichar ch = aTextRun->GetChar(i);
        if (IsWordBoundary(ch)) {
            RemoveWord(aTextRun, wordStart, i, hash);
            hash = 0;
            wordStart = i + 1;
        } else {
            hash = HashMix(hash, ch);
        }
    }
    RemoveWord(aTextRun, wordStart, i, hash);
#ifdef DEBUG
    NS_ASSERTION(aTextRun->mCachedWords == 0,
                 "Textrun was not completely removed from the cache!");
#endif
}

static bool
CompareDifferentWidthStrings(const PRUint8 *aStr1, const PRUnichar *aStr2,
                             PRUint32 aLength)
{
    PRUint32 i;
    for (i = 0; i < aLength; ++i) {
        if (aStr1[i] != aStr2[i])
            return PR_FALSE;
    }
    return PR_TRUE;
}

static bool
IsWordEnd(gfxTextRun *aTextRun, PRUint32 aOffset)
{
    PRUint32 runLength = aTextRun->GetLength();
    if (aOffset == runLength)
        return PR_TRUE;
    if (aOffset > runLength)
        return PR_FALSE;
    return IsWordBoundary(aTextRun->GetChar(aOffset));
}

static void *
GetFontOrGroup(gfxFontGroup *aFontGroup, bool aUseFont)
{
    return aUseFont
        ? static_cast<void *>(aFontGroup->GetFontAt(0))
        : static_cast<void *>(aFontGroup);
}

bool
TextRunWordCache::CacheHashEntry::KeyEquals(const KeyTypePointer aKey) const
{
    if (!mTextRun)
        return PR_FALSE;

    PRUint32 length = aKey->mLength;
    gfxFontGroup *fontGroup = mTextRun->GetFontGroup();
    if (!IsWordEnd(mTextRun, mWordOffset + length) ||
        GetFontOrGroup(fontGroup, mHashedByFont) != aKey->mFontOrGroup ||
        aKey->mAppUnitsPerDevUnit != mTextRun->GetAppUnitsPerDevUnit() ||
        aKey->mIsRTL != mTextRun->IsRightToLeft() ||
        aKey->mEnabledOptionalLigatures != ((mTextRun->GetFlags() & gfxTextRunFactory::TEXT_DISABLE_OPTIONAL_LIGATURES) == 0) ||
        aKey->mOptimizeSpeed != ((mTextRun->GetFlags() & gfxTextRunFactory::TEXT_OPTIMIZE_SPEED) != 0) ||
        aKey->mUserFontSetGeneration != (mTextRun->GetUserFontSetGeneration()))
        return PR_FALSE;

    if (mTextRun->GetFlags() & gfxFontGroup::TEXT_IS_8BIT) {
        const PRUint8 *text = mTextRun->GetText8Bit() + mWordOffset;
        if (!aKey->mIsDoubleByteText)
            return memcmp(text, aKey->mString, length) == 0;
        return CompareDifferentWidthStrings(text,
                                            static_cast<const PRUnichar *>(aKey->mString), length);
    } else {
        const PRUnichar *text = mTextRun->GetTextUnicode() + mWordOffset;
        if (aKey->mIsDoubleByteText)
            return memcmp(text, aKey->mString, length*sizeof(PRUnichar)) == 0;
        return CompareDifferentWidthStrings(static_cast<const PRUint8 *>(aKey->mString),
                                            text, length);
    }
}

PLDHashNumber
TextRunWordCache::CacheHashEntry::HashKey(const KeyTypePointer aKey)
{
    // only use lower 32 bits of generation counter in hash key, 
    // since these vary the most
    PRUint32 fontSetGen;
    LL_L2UI(fontSetGen, aKey->mUserFontSetGeneration);

    return aKey->mStringHash + fontSetGen + (PRUint32)(intptr_t)aKey->mFontOrGroup + aKey->mAppUnitsPerDevUnit +
        aKey->mIsDoubleByteText + aKey->mIsRTL*2 + aKey->mEnabledOptionalLigatures*4 +
        aKey->mOptimizeSpeed*8;
}

#ifdef DEBUG
PLDHashOperator
TextRunWordCache::CacheDumpEntry(CacheHashEntry* aEntry, void* userArg)
{
    FILE* output = static_cast<FILE*>(userArg);
    if (!aEntry->mTextRun) {
        fprintf(output, "<EMPTY>\n");
        return PL_DHASH_NEXT;
    }
    fprintf(output, "Word at %p:%d => ", static_cast<void*>(aEntry->mTextRun), aEntry->mWordOffset);
    aEntry->mTextRun->Dump(output);
    fprintf(output, " (hashed by %s)\n", aEntry->mHashedByFont ? "font" : "fontgroup");
    return PL_DHASH_NEXT;
}

void
TextRunWordCache::Dump()
{
    mCache.EnumerateEntries(CacheDumpEntry, stdout);
}
#endif

nsresult
gfxTextRunWordCache::Init()
{
    gTextRunWordCache = new TextRunWordCache();
    if (gTextRunWordCache) {
        // ensure there is a reference before the AddObserver calls;
        // this will be Release()'d in gfxTextRunWordCache::Shutdown()
        NS_ADDREF(gTextRunWordCache);
        gTextRunWordCache->Init();
    }
    return gTextRunWordCache ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

void
gfxTextRunWordCache::Shutdown()
{
    NS_IF_RELEASE(gTextRunWordCache);
}

gfxTextRun *
gfxTextRunWordCache::MakeTextRun(const PRUnichar *aText, PRUint32 aLength,
                                 gfxFontGroup *aFontGroup,
                                 const gfxFontGroup::Parameters *aParams,
                                 PRUint32 aFlags)
{
    if (!gTextRunWordCache)
        return nsnull;
    return gTextRunWordCache->MakeTextRun(aText, aLength, aFontGroup, aParams, aFlags);
}

gfxTextRun *
gfxTextRunWordCache::MakeTextRun(const PRUint8 *aText, PRUint32 aLength,
                                 gfxFontGroup *aFontGroup,
                                 const gfxFontGroup::Parameters *aParams,
                                 PRUint32 aFlags)
{
    if (!gTextRunWordCache)
        return nsnull;
    return gTextRunWordCache->MakeTextRun(aText, aLength, aFontGroup, aParams, aFlags);
}

void
gfxTextRunWordCache::RemoveTextRun(gfxTextRun *aTextRun)
{
    if (!gTextRunWordCache)
        return;
    gTextRunWordCache->RemoveTextRun(aTextRun);
}

void
gfxTextRunWordCache::Flush()
{
    if (!gTextRunWordCache)
        return;
    gTextRunWordCache->Flush();
}
