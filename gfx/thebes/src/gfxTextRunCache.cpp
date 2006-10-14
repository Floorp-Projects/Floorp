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

gfxTextRunCache*
gfxTextRunCache::GetCache()
{
    if (!mGlobalCache)
        mGlobalCache = new gfxTextRunCache();

    return mGlobalCache;
}

gfxTextRun*
gfxTextRunCache::GetOrMakeTextRun (gfxFontGroup *aFontGroup, const nsAString& aString)
{
    if (gDisableCache)
        return aFontGroup->MakeTextRun(aString);

    // Evict first, to make sure that the textrun we return is live.
    EvictUTF16();

    gfxTextRun *tr;
    TextRunEntry *entry;
    FontGroupAndString key(aFontGroup, &aString);

    if (mHashTableUTF16.Get(key, &entry)) {
        entry->Used();
        tr = entry->textRun.get();
    } else {
        key.Realize();
        tr = aFontGroup->MakeTextRun(key.GetString());
        entry = new TextRunEntry(tr);
        mHashTableUTF16.Put(key, entry);
    }

    return tr;
}

gfxTextRun*
gfxTextRunCache::GetOrMakeTextRun (gfxFontGroup *aFontGroup, const nsACString& aString)
{
    if (gDisableCache)
        return aFontGroup->MakeTextRun(aString);

    // Evict first, to make sure that the textrun we return is live.
    EvictASCII();

    gfxTextRun *tr;
    TextRunEntry *entry;
    FontGroupAndCString key(aFontGroup, &aString);

    if (mHashTableASCII.Get(key, &entry)) {
        entry->Used();
        tr = entry->textRun.get();
    } else {
        key.Realize();
        tr = aFontGroup->MakeTextRun(key.GetString());
        entry = new TextRunEntry(tr);
        mHashTableASCII.Put(key, entry);
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
