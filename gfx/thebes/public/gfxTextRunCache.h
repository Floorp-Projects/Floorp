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

#include "gfxFont.h"
#include "nsCheapSets.h"

/**
 * A textrun cache object. A general textrun caching solution. If you use
 * this class to create a textrun cache, you are responsible for managing
 * textrun lifetimes. The full power of textrun creation is exposed; you can
 * set all textrun creation flags and parameters.
 */
class THEBES_API gfxTextRunCache {
public:
    gfxTextRunCache() {
        mCache.Init(100);
    }
    ~gfxTextRunCache() {
        NS_ASSERTION(mCache.Count() == 0, "Textrun cache not empty!");
    }

    /**
     * Get a textrun from the cache, create one if necessary.
     * @param aFlags the flags TEXT_IS_ASCII, TEXT_IS_8BIT and TEXT_HAS_SURROGATES
     * are ignored; the cache sets them based on the string.
     * @param aCallerOwns if this is null, the cache always creates a new
     * textrun owned by the caller. If non-null, the cache may return a textrun
     * that was previously created and is owned by some previous caller
     * to GetOrMakeTextRun on this cache. If so, *aCallerOwns will be set
     * to false.
     */
    gfxTextRun *GetOrMakeTextRun(const PRUnichar *aText, PRUint32 aLength,
                                 gfxFontGroup *aFontGroup,
                                 const gfxFontGroup::Parameters *aParams,
                                 PRUint32 aFlags, PRBool *aCallerOwns = nsnull);
    /**
     * Get a textrun from the cache, create one if necessary.
     * @param aFlags the flags TEXT_IS_ASCII, TEXT_IS_8BIT and TEXT_HAS_SURROGATES
     * are ignored; the cache sets them based on the string.
     * @param aCallerOwns if this is null, the cache always creates a new
     * textrun owned by the caller. If non-null, the cache may return a textrun
     * that was previously created and is owned by some previous caller
     * to GetOrMakeTextRun on this cache. If so, *aCallerOwns will be set
     * to false.
     */
    gfxTextRun *GetOrMakeTextRun(const PRUint8 *aText, PRUint32 aLength,
                                 gfxFontGroup *aFontGroup,
                                 const gfxFontGroup::Parameters *aParams,
                                 PRUint32 aFlags, PRBool *aCallerOwns = nsnull);

    /**
     * Notify that a text run was hit in the cache, a new one created, and
     * that the new one has replaced the old one in the cache.
     */
    virtual void NotifyRemovedFromCache(gfxTextRun *aTextRun) {}

    /**
     * Remove a textrun from the cache. This must be called before aTextRun
     * is deleted!
     */
    void RemoveTextRun(gfxTextRun *aTextRun);

    /** The following flags are part of the cache key: */
    enum { FLAG_MASK =
        gfxTextRunFactory::TEXT_IS_RTL |
        gfxTextRunFactory::TEXT_ENABLE_SPACING |
        gfxTextRunFactory::TEXT_ABSOLUTE_SPACING |
        gfxTextRunFactory::TEXT_ENABLE_NEGATIVE_SPACING |
        gfxTextRunFactory::TEXT_ENABLE_HYPHEN_BREAKS |
        gfxTextRunFactory::TEXT_NEED_BOUNDING_BOX
    };

protected:
    struct THEBES_API CacheHashKey {
        void       *mFontOrGroup;
        const void *mString;
        PRUint32    mLength;
        PRUint32    mAppUnitsPerDevUnit;
        PRUint32    mFlags;
        PRUint32    mStringHash;

        CacheHashKey(void *aFontOrGroup, const void *aString, PRUint32 aLength,
                     PRUint32 aAppUnitsPerDevUnit, PRUint32 aFlags, PRUint32 aStringHash)
            : mFontOrGroup(aFontOrGroup), mString(aString), mLength(aLength),
              mAppUnitsPerDevUnit(aAppUnitsPerDevUnit), mFlags(aFlags),
              mStringHash(aStringHash) {}
    };

    class THEBES_API CacheHashEntry : public PLDHashEntryHdr {
    public:
        typedef const CacheHashKey &KeyType;
        typedef const CacheHashKey *KeyTypePointer;

        // When constructing a new entry in the hashtable, mTextRuns will be
        // blank. The caller of Put() will fill it in.
        CacheHashEntry(KeyTypePointer aKey)  { }
        CacheHashEntry(const CacheHashEntry& toCopy) { NS_ERROR("Should not be called"); }
        ~CacheHashEntry() { }

        PRBool KeyEquals(const KeyTypePointer aKey) const;
        static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
        static PLDHashNumber HashKey(const KeyTypePointer aKey);
        enum { ALLOW_MEMMOVE = PR_TRUE };

        gfxTextRun *mTextRun;
    };

    CacheHashKey GetKeyForTextRun(gfxTextRun *aTextRun);

    nsTHashtable<CacheHashEntry> mCache;
};

/**
 * A simple global textrun cache for textruns that do not carry state
 * (e.g., actual or potential linebreaks) and do not need complex initialization.
 * The lifetimes of these textruns are managed by the cache (they are auto-expired
 * after a certain period of time).
 */
class THEBES_API gfxGlobalTextRunCache {
public:
    /**
     * Get a textrun for the given text, using a global cache. The returned
     * textrun is valid until the next event loop. We own it, the caller
     * must not free it.
     * Do not set any state in the textrun (e.g. actual or potential linebreaks).
     * Flags IS_8BIT, IS_ASCII and HAS_SURROGATES are automatically set
     * appropriately.
     * Flag IS_PERSISTENT must NOT be set unless aText is guaranteed to live
     * forever.
     */
    static gfxTextRun *GetTextRun(const PRUnichar *aText, PRUint32 aLength,
                                  gfxFontGroup *aFontGroup,
                                  gfxContext *aRefContext,
                                  PRUint32 aAppUnitsPerDevUnit,
                                  PRUint32 aFlags);

    /**
     * Get a textrun for the given text, using a global cache. The returned
     * textrun is valid until the next event loop. We own it, the caller
     * must not free it.
     * Do not set any state in the textrun (e.g. actual or potential linebreaks).
     * Flags IS_8BIT, IS_ASCII and HAS_SURROGATES are automatically set
     * appropriately.
     * Flag IS_PERSISTENT must NOT be set unless aText is guaranteed to live
     * forever.
     */
    static gfxTextRun *GetTextRun(const PRUint8 *aText, PRUint32 aLength,
                                  gfxFontGroup *aFontGroup,
                                  gfxContext *aRefContext,
                                  PRUint32 aAppUnitsPerDevUnit,
                                  PRUint32 aFlags);

protected:
    friend class gfxPlatform;

    static nsresult Init();
    static void Shutdown();
};

#endif /* GFX_TEXT_RUN_CACHE_H */
