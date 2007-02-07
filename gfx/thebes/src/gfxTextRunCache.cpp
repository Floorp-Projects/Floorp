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

gfxTextRunCache* gfxTextRunCache::mGlobalCache = nsnull;
PRInt32 gfxTextRunCache::mGlobalCacheRefCount = 0;

static int gDisableCache = -1;

gfxTextRunCache::gfxTextRunCache()
{
    if (getenv("MOZ_GFX_NO_TEXT_CACHE"))
        gDisableCache = 1;
    else
        gDisableCache = 0;
        
    mHashTableUTF16.Init();
    mHashTableASCII.Init();

    mLastUTF16Eviction = mLastASCIIEviction = PR_Now();
}

// static
nsresult
gfxTextRunCache::Init()
{
    // We only live on the UI thread, right?  ;)
    ++mGlobalCacheRefCount;

    if (mGlobalCacheRefCount == 1) {
        NS_ASSERTION(!mGlobalCache, "Why do we have an mGlobalCache?");
        mGlobalCache = new gfxTextRunCache();

        if (!mGlobalCache) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }

    return NS_OK;
}

// static
void
gfxTextRunCache::Shutdown()
{
    --mGlobalCacheRefCount;
    if (mGlobalCacheRefCount == 0) {
        delete mGlobalCache;
        mGlobalCache = nsnull;
    }
}    

static PRUint32 ComputeFlags(PRBool aIsRTL, PRBool aEnableSpacing)
{
    PRUint32 flags = gfxTextRunFactory::TEXT_HAS_SURROGATES;
    if (aIsRTL) {
        flags |= gfxTextRunFactory::TEXT_IS_RTL;
    }
    if (aEnableSpacing) {
        flags |= gfxTextRunFactory::TEXT_ENABLE_SPACING |
                 gfxTextRunFactory::TEXT_ABSOLUTE_SPACING |
                 gfxTextRunFactory::TEXT_ENABLE_NEGATIVE_SPACING;
    }
    return flags;
}

gfxTextRun*
gfxTextRunCache::GetOrMakeTextRun (gfxContext* aContext, gfxFontGroup *aFontGroup,
                                   const PRUnichar *aString, PRUint32 aLength,
                                   PRUint32 aAppUnitsPerDevUnit, PRBool aIsRTL,
                                   PRBool aEnableSpacing, PRBool *aCallerOwns)
{
    gfxSkipChars skipChars;
    // Choose pessimistic flags since we don't want to bother analyzing the string
    gfxTextRunFactory::Parameters params =
        { aContext, nsnull, nsnull, &skipChars, nsnull, 0, aAppUnitsPerDevUnit,
              ComputeFlags(aIsRTL, aEnableSpacing) };

    gfxTextRun* tr = nsnull;
    // Don't cache textruns that use spacing
    if (!gDisableCache && !aEnableSpacing) {
        // Evict first, to make sure that the textrun we return is live.
        EvictUTF16();
    
        TextRunEntry *entry;
        nsDependentSubstring keyStr(aString, aString + aLength);
        FontGroupAndString key(aFontGroup, &keyStr);

        if (mHashTableUTF16.Get(key, &entry)) {
            gfxTextRun *cachedTR = entry->textRun;
            // Check that this matches what we wanted. If it doesn't, we leave
            // this cache entry alone and return a fresh, caller-owned textrun
            // below.
            if (cachedTR->GetAppUnitsPerDevUnit() == aAppUnitsPerDevUnit &&
                cachedTR->IsRightToLeft() == aIsRTL) {
                entry->Used();
                tr = cachedTR;
                tr->SetContext(aContext);
            }
        } else {
            tr = aFontGroup->MakeTextRun(aString, aLength, &params);
            entry = new TextRunEntry(tr);
            key.Realize();
            mHashTableUTF16.Put(key, entry);
        }
    }

    if (tr) {
        *aCallerOwns = PR_FALSE;
    } else {
        // Textrun is not in the cache for some reason.
        *aCallerOwns = PR_TRUE;
        tr = aFontGroup->MakeTextRun(aString, aLength, &params);
    }
    if (tr) {
        // We don't want to have to reconstruct the string
        tr->RememberText(aString, aLength);
    }

    return tr;
}

gfxTextRun*
gfxTextRunCache::GetOrMakeTextRun (gfxContext* aContext, gfxFontGroup *aFontGroup,
                                   const char *aString, PRUint32 aLength,
                                   PRUint32 aAppUnitsPerDevUnit, PRBool aIsRTL,
                                   PRBool aEnableSpacing, PRBool *aCallerOwns)
{
    gfxSkipChars skipChars;
    // Choose pessimistic flags since we don't want to bother analyzing the string
    gfxTextRunFactory::Parameters params =
        { aContext, nsnull, nsnull, &skipChars, nsnull, 0, aAppUnitsPerDevUnit,
              ComputeFlags(aIsRTL, aEnableSpacing) };
    const PRUint8* str = NS_REINTERPRET_CAST(const PRUint8*, aString);

    gfxTextRun* tr = nsnull;
    // Don't cache textruns that use spacing
    if (!gDisableCache && !aEnableSpacing) {
        // Evict first, to make sure that the textrun we return is live.
        EvictASCII();
    
        TextRunEntry *entry;
        nsDependentCSubstring keyStr(aString, aString + aLength);
        FontGroupAndCString key(aFontGroup, &keyStr);

        if (mHashTableASCII.Get(key, &entry)) {
            gfxTextRun *cachedTR = entry->textRun;
            // Check that this matches what we wanted. If it doesn't, we leave
            // this cache entry alone and return a fresh, caller-owned textrun
            // below.
            if (cachedTR->GetAppUnitsPerDevUnit() == aAppUnitsPerDevUnit &&
                cachedTR->IsRightToLeft() == aIsRTL) {
                entry->Used();
                tr = cachedTR;
                tr->SetContext(aContext);
            }
        } else {
            tr = aFontGroup->MakeTextRun(str, aLength, &params);
            entry = new TextRunEntry(tr);
            key.Realize();
            mHashTableASCII.Put(key, entry);
        }
    }

    if (tr) {
        *aCallerOwns = PR_FALSE;
    } else {
        // Textrun is not in the cache for some reason.
        *aCallerOwns = PR_TRUE;
        tr = aFontGroup->MakeTextRun(str, aLength, &params);
    }
    if (tr) {
        // We don't want to have to reconstruct the string
        tr->RememberText(str, aLength);
    }

    return tr;
}

/*
 * Stupid eviction algorithm: every 3*EVICT_AGE (3*1s),
 * evict every run that hasn't been used for more than a second.
 *
 * Don't evict anything if we have less than EVICT_MIN_COUNT (1000)
 * entries in the cache.
 *
 * XXX todo Don't use PR_Now(); use RDTSC or something for the timing.
 * XXX todo Use a less-stupid eviction algorithm
 * XXX todo Tweak EVICT_MIN_COUNT based on actual browsing
 */

#define EVICT_MIN_COUNT 1000

// 1s, in PRTime units (microseconds)
#define EVICT_AGE 1000000

void
gfxTextRunCache::EvictUTF16()
{
    PRTime evictBarrier = PR_Now();

    if (mLastUTF16Eviction > (evictBarrier - (3*EVICT_AGE)))
        return;

    if (mHashTableUTF16.Count() < EVICT_MIN_COUNT)
        return;

    //fprintf (stderr, "Evicting UTF16\n");
    mLastUTF16Eviction = evictBarrier;
    evictBarrier -= EVICT_AGE;
    mHashTableUTF16.Enumerate(UTF16EvictEnumerator, &evictBarrier);
}

void
gfxTextRunCache::EvictASCII()
{
    PRTime evictBarrier = PR_Now();

    if (mLastASCIIEviction > (evictBarrier - (3*EVICT_AGE)))
        return;

    if (mHashTableASCII.Count() < EVICT_MIN_COUNT)
        return;

    //fprintf (stderr, "Evicting ASCII\n");
    mLastASCIIEviction = evictBarrier;
    evictBarrier -= EVICT_AGE;
    mHashTableASCII.Enumerate(ASCIIEvictEnumerator, &evictBarrier);
}

PLDHashOperator
gfxTextRunCache::UTF16EvictEnumerator(const FontGroupAndString& key,
                                      nsAutoPtr<TextRunEntry> &value,
                                      void *closure)
{
    PRTime t = *(PRTime *)closure;

    if (value->lastUse < t)
        return PL_DHASH_REMOVE;

    return PL_DHASH_NEXT;
}

PLDHashOperator
gfxTextRunCache::ASCIIEvictEnumerator(const FontGroupAndCString& key,
                                      nsAutoPtr<TextRunEntry> &value,
                                      void *closure)
{
    PRTime t = *(PRTime *)closure;

    if (value->lastUse < t)
        return PL_DHASH_REMOVE;

    return PL_DHASH_NEXT;
}
