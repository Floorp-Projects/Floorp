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

#ifdef DEBUG
#include <stdio.h>
#endif

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

class TextRunWordCache {
public:
    TextRunWordCache() {
        mCache.Init(100);
    }
    ~TextRunWordCache() {
        NS_WARN_IF_FALSE(mCache.Count() == 0, "Textrun cache not empty!");
    }

    /**
     * Create a textrun using cached words.
     * Invalid characters (see gfxFontGroup::IsInvalidChar) will be automatically
     * treated as invisible missing.
     * @param aFlags the flags TEXT_IS_ASCII and TEXT_HAS_SURROGATES must be set
     * by the caller, if applicable
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

#ifdef DEBUG
    void Dump();
#endif

protected:
    struct CacheHashKey {
        void        *mFontOrGroup;
        const void  *mString;
        PRUint32     mLength;
        PRUint32     mAppUnitsPerDevUnit;
        PRUint32     mStringHash;
        PRPackedBool mIsDoubleByteText;
        PRPackedBool mIsRTL;
        PRPackedBool mEnabledOptionalLigatures;
        PRPackedBool mOptimizeSpeed;
        
        CacheHashKey(gfxTextRun *aBaseTextRun, void *aFontOrGroup,
                     PRUint32 aStart, PRUint32 aLength, PRUint32 aHash)
            : mFontOrGroup(aFontOrGroup), mString(aBaseTextRun->GetTextAt(aStart)),
              mLength(aLength),
              mAppUnitsPerDevUnit(aBaseTextRun->GetAppUnitsPerDevUnit()),
              mStringHash(aHash),
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

        PRBool KeyEquals(const KeyTypePointer aKey) const;
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
    
    PRBool LookupWord(gfxTextRun *aTextRun, gfxFont *aFirstFont,
                      PRUint32 aStart, PRUint32 aEnd, PRUint32 aHash);
    void AddWord(gfxTextRun *aTextRun, PRUint32 aDestOffset,
                 gfxTextRun *aNewRun, PRUint32 aSourceOffset, PRUint32 aLength,
                 PRUint32 aHash, const gfxFontGroup::Parameters *aParams);
    void RemoveWord(gfxTextRun *aTextRun, PRUint32 aStart,
                    PRUint32 aEnd, PRUint32 aHash);    

    nsTHashtable<CacheHashEntry> mCache;
    
#ifdef DEBUG
    static PLDHashOperator PR_CALLBACK CacheDumpEntry(CacheHashEntry* aEntry, void* userArg);
#endif
};

static PRLogModuleInfo *gWordCacheLog = PR_NewLogModule("wordCache");

static inline PRUint32
HashMix(PRUint32 aHash, PRUnichar aCh)
{
    return (aHash >> 28) ^ (aHash << 4) ^ aCh;
}

// If the substring of the textrun is rendered entirely in the first font
// of the textrun's fontgroup, then return that font. Otherwise return the
// fontgroup.
static void *GetWordFontOrGroup(gfxTextRun *aTextRun, PRUint32 aOffset,
                                PRUint32 aLength)
{
    PRUint32 glyphRunCount;
    const gfxTextRun::GlyphRun *glyphRuns = aTextRun->GetGlyphRuns(&glyphRunCount);
    PRUint32 glyphRunIndex = aTextRun->FindFirstGlyphRunContaining(aOffset);
    gfxFontGroup *fontGroup = aTextRun->GetFontGroup();
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
static PRBool
IsBoundarySpace(PRUnichar aChar)
{
    return aChar == ' ' || aChar == UNICODE_NBSP;
}

static PRBool
IsWordBoundary(PRUnichar aChar)
{
    return IsBoundarySpace(aChar) || gfxFontGroup::IsInvalidChar(aChar);
}

/**
 * Looks up a word in the cache. If the word is found in the cache
 * (which could be from an existing textrun or an earlier word in the same
 * textrun), we copy the glyphs from the word into the textrun.
 * 
 * @return true if the word was found in the cache, false otherwise.
 */
PRBool
TextRunWordCache::LookupWord(gfxTextRun *aTextRun, gfxFont *aFirstFont,
                             PRUint32 aStart, PRUint32 aEnd, PRUint32 aHash)
{
    if (aEnd <= aStart)
        return PR_TRUE;

    // Look for an entry keyed by font or fontgroup
    CacheHashKey key(aTextRun, aFirstFont, aStart, aEnd - aStart, aHash);
    CacheHashEntry *existingEntry = mCache.GetEntry(key);
    if (!existingEntry) {
        key.mFontOrGroup = aTextRun->GetFontGroup();
        existingEntry = mCache.GetEntry(key);
    }

    if (!existingEntry)
        return PR_FALSE;

    // Copy the glyphs from the word into the text run
    aTextRun->CopyGlyphDataFrom(existingEntry->mTextRun,
                                existingEntry->mWordOffset,
                                aEnd - aStart, aStart, PR_FALSE);
    return PR_TRUE;
}

/**
 * Processes a word that doesn't yet exist in the cache.  The word is copied
 * from the source textrun to the output textrun. (The source may be an
 * earlier word in the output textrun.)
 */
void
TextRunWordCache::AddWord(gfxTextRun *aTextRun, PRUint32 aDestOffset,
                          gfxTextRun *aNewRun, PRUint32 aSourceOffset,
                          PRUint32 aLength, PRUint32 aHash,
                          const gfxFontGroup::Parameters *aParams)
{
    gfxTextRun *source = aNewRun;
    PRUint32 sourceOffset = aSourceOffset;
    PRUint32 destOffset = aDestOffset;
    PRUint32 length = aLength;
    nsAutoPtr<gfxTextRun> tmpTextRun;

    // If the word starts inside a cluster we don't want this word
    // in the cache.
    PRBool wordStartsInsideCluster =
        aNewRun && !aNewRun->IsClusterStart(aSourceOffset);

    if (!wordStartsInsideCluster) {
        CacheHashKey key(aTextRun,
                         GetWordFontOrGroup(aNewRun, aSourceOffset, aLength),
                         aDestOffset, aLength, aHash);
        CacheHashEntry *entry = mCache.PutEntry(key);
        NS_ASSERTION(!entry->mTextRun, "Entry shouldn't have existed!");

        if (entry) {
            aTextRun->SetFlagBits(gfxTextRunWordCache::TEXT_IN_CACHE);

            entry->mTextRun = aTextRun;
            entry->mWordOffset = aDestOffset;
            gfxFontGroup *fontGroup = aTextRun->GetFontGroup();
            entry->mHashedByFont = key.mFontOrGroup != fontGroup;
#ifdef DEBUG
            ++aTextRun->mCachedWords;
            PR_LOG(gWordCacheLog, PR_LOG_DEBUG,
                   ("%p(%d-%d,%d): added using %s",
                    aTextRun, aDestOffset, aLength, aHash,
                    entry->mHashedByFont ? "font" :"fontgroup"));
            NS_ASSERTION(mCache.GetEntry(key),
                         "We should find the thing we just added!");
#endif
        }
    }
    else { // wordStartsInsideCluster
        NS_ASSERTION(sourceOffset > 0, "How can the first character be inside a cluster?");
        if (destOffset > 0 && IsBoundarySpace(aTextRun->GetChar(destOffset - 1))) {
            // We should copy over data for the preceding space
            // as well. The glyphs have probably been attached
            // to that character.
            --sourceOffset;
            --destOffset;
            ++length;
        } else {
            // URK! This case sucks! We have combining marks
            // at the start of the text. We had to prepend a space
            // just so we could detect these are combining marks
            // (so we can keep this "word" out of the cache).
            // But now the data in aNewRun is no use to us. We
            // need to find out what the platform would do
            // if the marks were at the start of the text.
            tmpTextRun = aNewRun->GetFontGroup()->
                MakeTextRun(aTextRun->GetTextUnicode(), length, aParams,
                            aNewRun->GetFlags());
            source = tmpTextRun;
            sourceOffset = 0;
        }
    }
    // Copy the word.
    // Allow CopyGlyphDataFrom to steal the internal data of
    // aNewRun or tmpTextRun since they are only temporary anyway.
    aTextRun->CopyGlyphDataFrom(source, sourceOffset, length,
                                destOffset, PR_TRUE);
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
    textRun->AddGlyphRun(font, 0);
    return textRun.forget();
}

gfxTextRun *
TextRunWordCache::MakeTextRun(const PRUnichar *aText, PRUint32 aLength,
                              gfxFontGroup *aFontGroup,
                              const gfxFontGroup::Parameters *aParams,
                              PRUint32 aFlags)
{
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
#endif

    gfxFont *font = aFontGroup->GetFontAt(0);
    nsresult rv = textRun->AddGlyphRun(font, 0);
    NS_ENSURE_SUCCESS(rv, nsnull);

    gfxTextRunFactory::Parameters params =
        { aParams->mContext, nsnull, nsnull, nsnull, 0, aParams->mAppUnitsPerDevUnit };
    nsAutoTArray<PRUnichar,200> tempString;
    PRUint32 i;
    PRUint32 wordStart = 0;
    PRUint32 hash = 0;
    for (i = 0; i <= aLength; ++i) {
        PRUnichar ch = i < aLength ? aText[i] : ' ';
        if (IsWordBoundary(ch)) {
            PRBool hit = LookupWord(textRun, font, wordStart, i, hash);
            if (!hit) {
                // Always put a space before the word so we can detect
                // combining characters at the start of a word
                tempString.AppendElement(' ');
                PRUint32 offset = tempString.Length();
                PRUint32 length = i - wordStart;

                PRUnichar *chars = tempString.AppendElements(length);
                if (!chars)
                    return nsnull;
                memcpy(chars, aText + wordStart, length*sizeof(PRUnichar));

                // create textrun for this unknown word
                nsAutoPtr<gfxTextRun> newRun;
                newRun = aFontGroup->
                    MakeTextRun(tempString.Elements(), tempString.Length(),
                                &params,
                                aFlags | gfxTextRunFactory::TEXT_IS_PERSISTENT);

                if (newRun) {
                    AddWord(textRun, wordStart, newRun, offset, length, hash,
                            aParams);
                }
                tempString.Clear();
            }
            
            if (IsBoundarySpace(ch) && i < aLength) {
                textRun->SetSpaceGlyph(font, aParams->mContext, i);
            } // else we should set this character to be invisible missing,
              // but it already is because the textrun is blank!

            hash = 0;
            wordStart = i + 1;
        } else {
            hash = HashMix(hash, ch);
        }
    }

    return textRun.forget();
}

gfxTextRun *
TextRunWordCache::MakeTextRun(const PRUint8 *aText, PRUint32 aLength,
                              gfxFontGroup *aFontGroup,
                              const gfxFontGroup::Parameters *aParams,
                              PRUint32 aFlags)
{
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
#endif

    gfxFont *font = aFontGroup->GetFontAt(0);
    nsresult rv = textRun->AddGlyphRun(font, 0);
    NS_ENSURE_SUCCESS(rv, nsnull);

    gfxTextRunFactory::Parameters params =
        { aParams->mContext, nsnull, nsnull, nsnull, 0, aParams->mAppUnitsPerDevUnit };
    PRUint32 i;
    PRUint32 wordStart = 0;
    PRUint32 hash = 0;
    for (i = 0; i <= aLength; ++i) {
        PRUint8 ch = i < aLength ? aText[i] : ' ';
        if (IsWordBoundary(ch)) {
            PRBool hit = LookupWord(textRun, font, wordStart, i, hash);
            if (!hit) {
                PRUint32 length = i - wordStart;

                // create textrun for this unknown word
                nsAutoPtr<gfxTextRun> newRun;
                newRun = aFontGroup->
                    MakeTextRun(aText + wordStart, length, &params,
                                aFlags | gfxTextRunFactory::TEXT_IS_PERSISTENT);
                if (newRun) {
                    AddWord(textRun, wordStart, newRun, 0, length, hash,
                            aParams);
                }
            }
            
            if (IsBoundarySpace(ch) && i < aLength) {
                textRun->SetSpaceGlyph(font, aParams->mContext, i);
            } // else we should set this character to be invisible missing,
              // but it already is because the textrun is blank!

            hash = 0;
            wordStart = i + 1;
        } else {
            hash = HashMix(hash, ch);
        }
    }

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

static PRBool
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

static PRBool
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
GetFontOrGroup(gfxFontGroup *aFontGroup, PRBool aUseFont)
{
    return aUseFont
        ? static_cast<void *>(aFontGroup->GetFontAt(0))
        : static_cast<void *>(aFontGroup);
}

PRBool
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
        aKey->mOptimizeSpeed != ((mTextRun->GetFlags() & gfxTextRunFactory::TEXT_OPTIMIZE_SPEED) != 0))
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
    return aKey->mStringHash + (long)aKey->mFontOrGroup + aKey->mAppUnitsPerDevUnit +
        aKey->mIsDoubleByteText + aKey->mIsRTL*2 + aKey->mEnabledOptionalLigatures*4 +
        aKey->mOptimizeSpeed*8;
}

#ifdef DEBUG
PLDHashOperator PR_CALLBACK
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

static TextRunWordCache *gTextRunWordCache = nsnull;

nsresult
gfxTextRunWordCache::Init()
{
    gTextRunWordCache = new TextRunWordCache();
    return gTextRunWordCache ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

void
gfxTextRunWordCache::Shutdown()
{
    delete gTextRunWordCache;
    gTextRunWordCache = nsnull;
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
