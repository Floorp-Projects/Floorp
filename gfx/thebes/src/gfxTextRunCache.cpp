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

#include "gfxTextRunCache.h"

#include "nsExpirationTracker.h"

static inline PRUint32
HashMix(PRUint32 aHash, PRUnichar aCh)
{
    return (aHash >> 28) ^ (aHash << 4) ^ aCh;
}

static PRUint32
HashString(const PRUnichar *aText, PRUint32 aLength, PRUint32 *aFlags)
{
    *aFlags &= ~(gfxFontGroup::TEXT_HAS_SURROGATES | gfxFontGroup::TEXT_IS_ASCII);
    PRUint32 i;
    PRUint32 hashCode = 0;
    PRUnichar allBits = 0;
    for (i = 0; i < aLength; ++i) {
        PRUnichar ch = aText[i];
        hashCode = HashMix(hashCode, ch);
        allBits |= ch;
        if (IS_SURROGATE(ch)) {
            *aFlags |= gfxFontGroup::TEXT_HAS_SURROGATES;
        }
    }
    if (!(allBits & ~0x7F)) {
        *aFlags |= gfxFontGroup::TEXT_IS_ASCII;
    }
    return hashCode;
}

static PRUint32
HashString(const PRUint8 *aText, PRUint32 aLength, PRUint32 *aFlags)
{
    *aFlags &= ~(gfxFontGroup::TEXT_HAS_SURROGATES | gfxFontGroup::TEXT_IS_ASCII);
    *aFlags |= gfxFontGroup::TEXT_IS_8BIT;
    PRUint32 i;
    PRUint32 hashCode = 0;
    PRUint8 allBits = 0;
    for (i = 0; i < aLength; ++i) {
        PRUint8 ch = aText[i];
        hashCode = HashMix(hashCode, ch);
        allBits |= ch;
    }
    if (!(allBits & ~0x7F)) {
        *aFlags |= gfxFontGroup::TEXT_IS_ASCII;
    }
    return hashCode;
}

static void *GetCacheKeyFontOrGroup(gfxTextRun *aTextRun)
{
    PRUint32 glyphRunCount;
    const gfxTextRun::GlyphRun *glyphRuns = aTextRun->GetGlyphRuns(&glyphRunCount);
    gfxFontGroup *fontGroup = aTextRun->GetFontGroup();
    gfxFont *firstFont = fontGroup->GetFontAt(0);
    return glyphRunCount == 1 && glyphRuns[0].mFont == firstFont
           ? NS_STATIC_CAST(void *, firstFont)
           : NS_STATIC_CAST(void *, fontGroup);
}

static const PRUnichar *
CloneText(const PRUnichar *aText, PRUint32 aLength,
          nsAutoArrayPtr<PRUnichar> *aBuffer, PRUint32 aFlags)
{
    if (*aBuffer == aText || (aFlags & gfxFontGroup::TEXT_IS_PERSISTENT))
        return aText;
    PRUnichar *newText = new PRUnichar[aLength];
    if (!newText)
        return nsnull;
    memcpy(newText, aText, aLength*sizeof(PRUnichar));
    *aBuffer = newText;
    return newText;
}

static const PRUint8 *
CloneText(const PRUint8 *aText, PRUint32 aLength,
          nsAutoArrayPtr<PRUint8> *aBuffer, PRUint32 aFlags)
{
    if (*aBuffer == aText || (aFlags & gfxFontGroup::TEXT_IS_PERSISTENT))
        return aText;
    PRUint8 *newText = new PRUint8[aLength];
    if (!newText)
        return nsnull;
    memcpy(newText, aText, aLength);
    *aBuffer = newText;
    return newText;
}

gfxTextRun *
gfxTextRunCache::GetOrMakeTextRun(const PRUnichar *aText, PRUint32 aLength,
                                  gfxFontGroup *aFontGroup,
                                  const gfxFontGroup::Parameters *aParams,
                                  PRUint32 aFlags, PRBool *aCallerOwns)
{
    if (aCallerOwns) {
        *aCallerOwns = PR_TRUE;
    }
    if (aLength == 0) {
        aFlags |= gfxFontGroup::TEXT_IS_PERSISTENT;
    } else if (aLength == 1 && aText[0] == ' ') {
        aFlags |= gfxFontGroup::TEXT_IS_PERSISTENT;
        static const PRUnichar space = ' ';
        aText = &space;
    }

    PRUint32 hashCode = HashString(aText, aLength, &aFlags);
    gfxFont *font = aFontGroup->GetFontAt(0);
    CacheHashKey key(font, aText, aLength, aParams->mAppUnitsPerDevUnit, aFlags, hashCode);
    CacheHashEntry *entry = nsnull;
    if (font) {
        entry = mCache.GetEntry(key);
    }
    if (!entry) {
        key.mFontOrGroup = aFontGroup;
        entry = mCache.GetEntry(key);
    }
    nsAutoArrayPtr<PRUnichar> text;
    if (entry) {
        gfxTextRun *textRun = entry->mTextRun;
        if (aCallerOwns) {
            *aCallerOwns = PR_FALSE;
            return textRun;
        }
        aText = CloneText(aText, aLength, &text, aFlags);
        if (!aText)
            return nsnull;
        gfxTextRun *newRun =
            textRun->Clone(aParams, aText, aLength, aFontGroup, aFlags);
        if (newRun) {
            entry->mTextRun = newRun;
            NotifyRemovedFromCache(textRun);
            text.forget();
            return newRun;
        }
    }

    aText = CloneText(aText, aLength, &text, aFlags);
    if (!aText)
        return nsnull;
    gfxTextRun *newRun =
        aFontGroup->MakeTextRun(aText, aLength, aParams, aFlags);
    if (newRun) {
        key.mFontOrGroup = GetCacheKeyFontOrGroup(newRun);
        entry = mCache.PutEntry(key);
        if (entry) {
            entry->mTextRun = newRun;
        }
        NS_ASSERTION(!entry || entry == mCache.GetEntry(GetKeyForTextRun(newRun)),
                     "Inconsistent hashing");
    }
    text.forget();
    return newRun;
}

gfxTextRun *
gfxTextRunCache::GetOrMakeTextRun(const PRUint8 *aText, PRUint32 aLength,
                                  gfxFontGroup *aFontGroup,
                                  const gfxFontGroup::Parameters *aParams,
                                  PRUint32 aFlags, PRBool *aCallerOwns)
{
    if (aCallerOwns) {
        *aCallerOwns = PR_TRUE;
    }
    if (aLength == 0) {
        aFlags |= gfxFontGroup::TEXT_IS_PERSISTENT;
    } else if (aLength == 1 && aText[0] == ' ') {
        aFlags |= gfxFontGroup::TEXT_IS_PERSISTENT;
        static const PRUint8 space = ' ';
        aText = &space;
    }

    PRUint32 hashCode = HashString(aText, aLength, &aFlags);
    gfxFont *font = aFontGroup->GetFontAt(0);
    CacheHashKey key(font, aText, aLength, aParams->mAppUnitsPerDevUnit, aFlags, hashCode);
    CacheHashEntry *entry = nsnull;
    if (font) {
        entry = mCache.GetEntry(key);
    }
    if (!entry) {
        key.mFontOrGroup = aFontGroup;
        entry = mCache.GetEntry(key);
    }
    nsAutoArrayPtr<PRUint8> text;
    if (entry) {
        gfxTextRun *textRun = entry->mTextRun;
        if (aCallerOwns) {
            *aCallerOwns = PR_FALSE;
            return textRun;
        }
        aText = CloneText(aText, aLength, &text, aFlags);
        if (!aText)
            return nsnull;
        gfxTextRun *newRun =
            textRun->Clone(aParams, aText, aLength,
                           aFontGroup, aFlags);
        if (newRun) {
            entry->mTextRun = newRun;
            NotifyRemovedFromCache(textRun);
            text.forget();
            return newRun;
        }
    }

    aText = CloneText(aText, aLength, &text, aFlags);
    if (!aText)
        return nsnull;
    gfxTextRun *newRun =
        aFontGroup->MakeTextRun(aText, aLength, aParams, aFlags);
    if (newRun) {
        key.mFontOrGroup = GetCacheKeyFontOrGroup(newRun);
        entry = mCache.PutEntry(key);
        if (entry) {
            entry->mTextRun = newRun;
        }
        NS_ASSERTION(!entry || entry == mCache.GetEntry(GetKeyForTextRun(newRun)),
                     "Inconsistent hashing");
    }
    text.forget();
    return newRun;
}

gfxTextRunCache::CacheHashKey
gfxTextRunCache::GetKeyForTextRun(gfxTextRun *aTextRun)
{
    PRUint32 hashCode;
    const void *text;
    PRUint32 length = aTextRun->GetLength();
    if (aTextRun->GetFlags() & gfxFontGroup::TEXT_IS_8BIT) {
        PRUint32 flags;
        text = aTextRun->GetText8Bit();
        hashCode = HashString(aTextRun->GetText8Bit(), length, &flags);
    } else {
        PRUint32 flags;
        text = aTextRun->GetTextUnicode();
        hashCode = HashString(aTextRun->GetTextUnicode(), length, &flags);
    }
    void *fontOrGroup = GetCacheKeyFontOrGroup(aTextRun);
    return CacheHashKey(fontOrGroup, text, length, aTextRun->GetAppUnitsPerDevUnit(),
                        aTextRun->GetFlags(), hashCode);
}

void
gfxTextRunCache::RemoveTextRun(gfxTextRun *aTextRun)
{
    CacheHashKey key = GetKeyForTextRun(aTextRun);
#ifdef DEBUG
    CacheHashEntry *entry = mCache.GetEntry(key);
    NS_ASSERTION(entry && entry->mTextRun == aTextRun,
                 "Failed to find textrun in cache");
#endif
    mCache.RemoveEntry(key);
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

PRBool
gfxTextRunCache::CacheHashEntry::KeyEquals(const KeyTypePointer aKey) const
{
    gfxTextRun *textRun = mTextRun;
    if (!textRun)
        return PR_FALSE;
    PRUint32 length = textRun->GetLength();
    if (aKey->mFontOrGroup != GetCacheKeyFontOrGroup(textRun) ||
        aKey->mLength != length ||
        aKey->mAppUnitsPerDevUnit != textRun->GetAppUnitsPerDevUnit() ||
        ((aKey->mFlags ^ textRun->GetFlags()) & FLAG_MASK))
        return PR_FALSE;

    if (textRun->GetFlags() & gfxFontGroup::TEXT_IS_8BIT) {
        if (aKey->mFlags & gfxFontGroup::TEXT_IS_8BIT)
            return memcmp(textRun->GetText8Bit(), aKey->mString, length) == 0;
        return CompareDifferentWidthStrings(textRun->GetText8Bit(),
                                            NS_STATIC_CAST(const PRUnichar *, aKey->mString), length);
    } else {
        if (!(aKey->mFlags & gfxFontGroup::TEXT_IS_8BIT))
            return memcmp(textRun->GetTextUnicode(), aKey->mString, length*sizeof(PRUnichar)) == 0;
        return CompareDifferentWidthStrings(NS_STATIC_CAST(const PRUint8 *, aKey->mString),
                                            textRun->GetTextUnicode(), length);
    }
}

PLDHashNumber
gfxTextRunCache::CacheHashEntry::HashKey(const KeyTypePointer aKey)
{
    return aKey->mStringHash + (long)aKey->mFontOrGroup + aKey->mAppUnitsPerDevUnit +
        (aKey->mFlags & FLAG_MASK);
}

/*
 * Cache textruns and expire them after 3*10 seconds of no use
 */
class TextRunCache : public nsExpirationTracker<gfxTextRun,3> {
public:
    enum { TIMEOUT_SECONDS = 10 };
    TextRunCache()
        : nsExpirationTracker<gfxTextRun,3>(TIMEOUT_SECONDS*1000) {}
    ~TextRunCache() {
        AgeAllGenerations();
    }

    // This gets called when the timeout has expired on a gfxTextRun
    virtual void NotifyExpired(gfxTextRun *aTextRun) {
        RemoveObject(aTextRun);
        mCache.RemoveTextRun(aTextRun);
        delete aTextRun;
    }

    gfxTextRunCache mCache;
};

static TextRunCache *gTextRunCache = nsnull;

static nsresult
UpdateOwnership(gfxTextRun *aTextRun, PRBool aOwned)
{
    if (!aTextRun)
        return nsnull;
    if (aOwned)
        return gTextRunCache->AddObject(aTextRun);
    if (!aTextRun->GetExpirationState()->IsTracked())
        return NS_OK;
    return gTextRunCache->MarkUsed(aTextRun);
}

gfxTextRun *
gfxGlobalTextRunCache::GetTextRun(const PRUnichar *aText, PRUint32 aLength,
                                  gfxFontGroup *aFontGroup,
                                  gfxContext *aRefContext,
                                  PRUint32 aAppUnitsPerDevUnit,
                                  PRUint32 aFlags)
{
    if (!gTextRunCache)
        return nsnull;
    PRBool owned;
    gfxTextRunFactory::Parameters params = {
        aRefContext, nsnull, nsnull, nsnull, 0, aAppUnitsPerDevUnit
    };
    nsAutoPtr<gfxTextRun> textRun;
    textRun = gTextRunCache->mCache.GetOrMakeTextRun(aText, aLength, aFontGroup, &params, aFlags, &owned);
    nsresult rv = UpdateOwnership(textRun, owned);
    if (NS_FAILED(rv))
        return nsnull;
    return textRun.forget();
}

gfxTextRun *
gfxGlobalTextRunCache::GetTextRun(const PRUint8 *aText, PRUint32 aLength,
                                  gfxFontGroup *aFontGroup,
                                  gfxContext *aRefContext,
                                  PRUint32 aAppUnitsPerDevUnit,
                                  PRUint32 aFlags)
{
    if (!gTextRunCache)
        return nsnull;
    PRBool owned;
    gfxTextRunFactory::Parameters params = {
        aRefContext, nsnull, nsnull, nsnull, 0, aAppUnitsPerDevUnit
    };     
    nsAutoPtr<gfxTextRun> textRun;
    textRun = gTextRunCache->mCache.GetOrMakeTextRun(aText, aLength, aFontGroup, &params, aFlags, &owned);
    nsresult rv = UpdateOwnership(textRun, owned);
    if (NS_FAILED(rv))
        return nsnull;
    return textRun.forget();
}

nsresult
gfxGlobalTextRunCache::Init()
{
    gTextRunCache = new TextRunCache();
    return gTextRunCache ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

void
gfxGlobalTextRunCache::Shutdown()
{
    delete gTextRunCache;
    gTextRunCache = nsnull;
}
