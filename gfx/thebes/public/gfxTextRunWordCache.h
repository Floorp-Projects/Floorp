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

#ifndef GFX_TEXT_RUN_WORD_CACHE_H
#define GFX_TEXT_RUN_WORD_CACHE_H

#include "gfxFont.h"

/**
 * Cache individual "words" (strings delimited by white-space or white-space-like
 * characters that don't involve kerning or ligatures) in textruns.
  */
class THEBES_API gfxTextRunWordCache {
public:
    gfxTextRunWordCache() {
        mCache.Init(100);
    }
    ~gfxTextRunWordCache() {
        NS_ASSERTION(mCache.Count() == 0, "Textrun cache not empty!");
    }

    /**
     * Create a textrun using cached words.
     * @param aFlags the flags TEXT_IS_ASCII and TEXT_HAS_SURROGATES must be set
     * by the caller, if applicable
     * @param aIsInCache if true is returned, then RemoveTextRun must be called
     * before the textrun changes or dies.
     */
    gfxTextRun *MakeTextRun(const PRUnichar *aText, PRUint32 aLength,
                            gfxFontGroup *aFontGroup,
                            const gfxFontGroup::Parameters *aParams,
                            PRUint32 aFlags, PRBool *aIsInCache);
    /**
     * Create a textrun using cached words
     * @param aFlags the flag TEXT_IS_ASCII must be set by the caller,
     * if applicable
     * @param aIsInCache if true is returned, then RemoveTextRun must be called
     * before the textrun changes or dies.
     */
    gfxTextRun *MakeTextRun(const PRUint8 *aText, PRUint32 aLength,
                            gfxFontGroup *aFontGroup,
                            const gfxFontGroup::Parameters *aParams,
                            PRUint32 aFlags, PRBool *aIsInCache);

    /**
     * Remove a textrun from the cache. This must be called before aTextRun
     * is deleted! The text in the textrun must still be valid.
     */
    void RemoveTextRun(gfxTextRun *aTextRun);

protected:
    struct THEBES_API CacheHashKey {
        void        *mFontOrGroup;
        const void  *mString;
        PRUint32     mLength;
        PRUint32     mAppUnitsPerDevUnit;
        PRUint32     mStringHash;
        PRPackedBool mIsDoubleByteText;
    };

    class THEBES_API CacheHashEntry : public PLDHashEntryHdr {
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
    
    // Used to track words that should be copied from one textrun to
    // another during the textrun construction process
    struct DeferredWord {
        gfxTextRun *mSourceTextRun;
        PRUint32    mSourceOffset;
        PRUint32    mDestOffset;
        PRUint32    mLength;
        PRUint32    mHash;
    };
    
    PRBool LookupWord(gfxTextRun *aTextRun, gfxFont *aFirstFont,
                      PRUint32 aStart, PRUint32 aEnd, PRUint32 aHash,
                      nsTArray<DeferredWord>* aDeferredWords);
    void FinishTextRun(gfxTextRun *aTextRun, gfxTextRun *aNewRun,
                       const nsTArray<DeferredWord>& aDeferredWords,
                       PRBool aSuccessful);
    void RemoveWord(gfxTextRun *aTextRun, PRUint32 aStart,
                    PRUint32 aEnd, PRUint32 aHash);    

    nsTHashtable<CacheHashEntry> mCache;
};

#endif /* GFX_TEXT_RUN_WORD_CACHE_H */
