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

#ifndef GFX_TEXT_RUN_CACHE_H
#define GFX_TEXT_RUN_CACHE_H

#include "nsRefPtrHashtable.h"
#include "nsClassHashtable.h"

#include "gfxFont.h"

#include "prtime.h"

class THEBES_API gfxTextRunCache {
public:
    /*
     * Get the global gfxTextRunCache.  You must call Init() before
     * calling this method.
     */
    static gfxTextRunCache* GetCache() {
        return mGlobalCache;
    }

    static nsresult Init();
    static void Shutdown();

    /* Will return a pointer to a gfxTextRun, which may or may not be from
     * the cache. If aCallerOwns is set to true, the caller owns the textrun
     * and must delete it. Otherwise the returned textrun is only valid until
     * the next GetOrMakeTextRun call and the caller must not delete it.
     */
    gfxTextRun *GetOrMakeTextRun (gfxContext* aContext, gfxFontGroup *aFontGroup,
                                  const char *aString, PRUint32 aLength,
                                  PRUint32 aAppUnitsPerDevUnit, PRBool aIsRTL,
                                  PRBool aEnableSpacing, PRBool *aCallerOwns);
    gfxTextRun *GetOrMakeTextRun (gfxContext* aContext, gfxFontGroup *aFontGroup,
                                  const PRUnichar *aString, PRUint32 aLength,
                                  PRUint32 aAppUnitsPerDevUnit, PRBool aIsRTL,
                                  PRBool aEnableSpacing, PRBool *aCallerOwns);

protected:
    gfxTextRunCache();

    static gfxTextRunCache *mGlobalCache;
    static PRInt32 mGlobalCacheRefCount;

    /* A small container class to hold a gfxFontGroup ref and a string.
     * This is used as the key for the cache hash table; to avoid
     * copying a whole pile of strings every time we do a hash lookup.
     * we only create our own copy of the string when Realize() is called.
     * gfxTextRunCache calls Realize whenever it puts a new entry into
     * the hashtable.
     */
    template<class GenericString, class RealString>
    struct FontGroupAndStringT {
        FontGroupAndStringT(gfxFontGroup *fg, const GenericString* str)
            : mFontGroup(fg), mString(str) { }

        FontGroupAndStringT(const FontGroupAndStringT<GenericString,RealString>& other)
            : mFontGroup(other.mFontGroup), mString(&mRealString)
        {
            mRealString.Assign(*other.mString);
        }

        void Realize() {
            mRealString.Assign(*mString);
            mString = &mRealString;
        }

        nsRefPtr<gfxFontGroup> mFontGroup;
        RealString mRealString;
        const GenericString* mString;
    };

    static PRUint32 HashDouble(const double d) {
        if (d == 0.0)
            return 0;
        int exponent;
        double mantissa = frexp (d, &exponent);
        return (PRUint32) (2 * fabs(mantissa) - 1);
    }

    template<class T>
    struct FontGroupAndStringHashKeyT : public PLDHashEntryHdr {
        typedef const T& KeyType;
        typedef const T* KeyTypePointer;

        FontGroupAndStringHashKeyT(KeyTypePointer aObj) : mObj(*aObj) { }
        FontGroupAndStringHashKeyT(const FontGroupAndStringHashKeyT<T>& other) : mObj(other.mObj) { }
        ~FontGroupAndStringHashKeyT() { }

        KeyType GetKey() const { return mObj; }

        PRBool KeyEquals(KeyTypePointer aKey) const {
            return
                mObj.mString->Equals(*(aKey->mString)) &&
                mObj.mFontGroup->Equals(*(aKey->mFontGroup.get()));
        }

        static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
        static PLDHashNumber HashKey(KeyTypePointer aKey) {
            PRUint32 h1 = HashString(*(aKey->mString));
            PRUint32 h2 = HashString(aKey->mFontGroup->GetFamilies());
            PRUint32 h3 = HashDouble(aKey->mFontGroup->GetStyle()->size);

            return h1 ^ h2 ^ h3;
        }
        enum { ALLOW_MEMMOVE = PR_FALSE };

    private:
        const T mObj;
    };

    struct TextRunEntry {
        TextRunEntry(gfxTextRun *tr) : textRun(tr), lastUse(PR_Now()) { }
        void Used() { lastUse = PR_Now(); }

        gfxTextRun* textRun;
        PRTime      lastUse;
        
        ~TextRunEntry() { delete textRun; }
    };

    typedef FontGroupAndStringT<nsAString, nsString> FontGroupAndString;
    typedef FontGroupAndStringT<nsACString, nsCString> FontGroupAndCString;

    typedef FontGroupAndStringHashKeyT<FontGroupAndString> FontGroupAndStringHashKey;
    typedef FontGroupAndStringHashKeyT<FontGroupAndCString> FontGroupAndCStringHashKey;

    nsClassHashtable<FontGroupAndStringHashKey, TextRunEntry> mHashTableUTF16;
    nsClassHashtable<FontGroupAndCStringHashKey, TextRunEntry> mHashTableASCII;

    void EvictUTF16();
    void EvictASCII();

    PRTime mLastUTF16Eviction;
    PRTime mLastASCIIEviction;

    static PLDHashOperator UTF16EvictEnumerator(const FontGroupAndString& key,
                                                nsAutoPtr<TextRunEntry> &value,
                                                void *closure);

    static PLDHashOperator ASCIIEvictEnumerator(const FontGroupAndCString& key,
                                                nsAutoPtr<TextRunEntry> &value,
                                                void *closure);
};


#endif /* GFX_TEXT_RUN_CACHE_H */
