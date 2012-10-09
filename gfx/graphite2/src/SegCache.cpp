/*  GRAPHITE2 LICENSING

    Copyright 2010, SIL International
    All rights reserved.

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 2.1 of License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should also have received a copy of the GNU Lesser General Public
    License along with this library in the file named "LICENSE".
    If not, write to the Free Software Foundation, 51 Franklin Street,
    Suite 500, Boston, MA 02110-1335, USA or visit their web page on the
    internet at http://www.fsf.org/licenses/lgpl.html.

Alternatively, the contents of this file may be used under the terms of the
Mozilla Public License (http://mozilla.org/MPL) or the GNU General Public
License, as published by the Free Software Foundation, either version 2
of the License or (at your option) any later version.
*/

#include "inc/Main.h"
#include "inc/TtfTypes.h"
#include "inc/TtfUtil.h"
#include "inc/SegCache.h"
#include "inc/SegCacheEntry.h"
#include "inc/SegCacheStore.h"
#include "inc/CmapCache.h"


using namespace graphite2;

#ifndef GRAPHITE2_NSEGCACHE

SegCache::SegCache(const SegCacheStore * store, const Features & feats)
: m_prefixLength(ePrefixLength),
  m_maxCachedSegLength(eMaxSpliceSize),
  m_segmentCount(0),
  m_features(feats),
  m_totalAccessCount(0l), m_totalMisses(0l),
  m_purgeFactor(1.0f / (ePurgeFactor * store->maxSegmentCount()))
{
    m_prefixes.raw = grzeroalloc<void*>(store->maxCmapGid() + 2);
    m_prefixes.range[SEG_CACHE_MIN_INDEX] = SEG_CACHE_UNSET_INDEX;
    m_prefixes.range[SEG_CACHE_MAX_INDEX] = SEG_CACHE_UNSET_INDEX;
}

void SegCache::freeLevel(SegCacheStore * store, SegCachePrefixArray prefixes, size_t level)
{
    for (size_t i = 0; i < store->maxCmapGid(); i++)
    {
        if (prefixes.array[i].raw)
        {
            if (level + 1 < ePrefixLength)
                freeLevel(store, prefixes.array[i], level + 1);
            else
            {
                SegCachePrefixEntry * prefixEntry = prefixes.prefixEntries[i];
                delete prefixEntry;
            }
        }
    }
    free(prefixes.raw);
}

void SegCache::clear(SegCacheStore * store)
{
    freeLevel(store, m_prefixes, 0);
    m_prefixes.raw = NULL;
}

SegCache::~SegCache()
{
    assert(m_prefixes.raw == NULL);
}

SegCacheEntry* SegCache::cache(SegCacheStore * store, const uint16* cmapGlyphs, size_t length, Segment * seg, size_t charOffset)
{
    uint16 pos = 0;
    if (!length) return NULL;
    assert(length < m_maxCachedSegLength);
    SegCachePrefixArray pArray = m_prefixes;
    while (pos + 1 < m_prefixLength)
    {
        uint16 gid = (pos < length)? cmapGlyphs[pos] : 0;
        if (!pArray.array[gid].raw)
        {
            pArray.array[gid].raw = grzeroalloc<void*>(store->maxCmapGid() + 2);
            if (!pArray.array[gid].raw)
                return NULL; // malloc failed
            if (pArray.range[SEG_CACHE_MIN_INDEX] == SEG_CACHE_UNSET_INDEX)
            {
                pArray.range[SEG_CACHE_MIN_INDEX] = gid;
                pArray.range[SEG_CACHE_MAX_INDEX] = gid;
            }
            else
            {
                if (gid < pArray.range[SEG_CACHE_MIN_INDEX])
                    pArray.range[SEG_CACHE_MIN_INDEX] = gid;
                else if (gid > pArray.range[SEG_CACHE_MAX_INDEX])
                    pArray.range[SEG_CACHE_MAX_INDEX] = gid;
            }
        }
        pArray = pArray.array[gid];
        ++pos;
    }
    uint16 gid = (pos < length)? cmapGlyphs[pos] : 0;
    SegCachePrefixEntry * prefixEntry = pArray.prefixEntries[gid];
    if (!prefixEntry)
    {
        prefixEntry = new SegCachePrefixEntry();
        pArray.prefixEntries[gid] = prefixEntry;
        if (pArray.range[SEG_CACHE_MIN_INDEX] == SEG_CACHE_UNSET_INDEX)
        {
            pArray.range[SEG_CACHE_MIN_INDEX] = gid;
            pArray.range[SEG_CACHE_MAX_INDEX] = gid;
        }
        else
        {
            if (gid < pArray.range[SEG_CACHE_MIN_INDEX])
                pArray.range[SEG_CACHE_MIN_INDEX] = gid;
            else if (gid > pArray.range[SEG_CACHE_MAX_INDEX])
                pArray.range[SEG_CACHE_MAX_INDEX] = gid;
        }
    }
    if (!prefixEntry) return NULL;
    // if the cache is full run a purge - this is slow, since it walks the tree
    if (m_segmentCount + 1 > store->maxSegmentCount())
    {
        purge(store);
        assert(m_segmentCount < store->maxSegmentCount());
    }
    SegCacheEntry * pEntry = prefixEntry->cache(cmapGlyphs, length, seg, charOffset, m_totalAccessCount);
    if (pEntry) ++m_segmentCount;
    return pEntry;
}

void SegCache::purge(SegCacheStore * store)
{
    unsigned long long minAccessCount = static_cast<unsigned long long>(m_totalAccessCount * m_purgeFactor + 1);
    if (minAccessCount < 2) minAccessCount = 2;
    unsigned long long oldAccessTime = m_totalAccessCount - store->maxSegmentCount() / eAgeFactor;
    purgeLevel(store, m_prefixes, 0, minAccessCount, oldAccessTime);
}

void SegCache::purgeLevel(SegCacheStore * store, SegCachePrefixArray prefixes, size_t level,
                          unsigned long long minAccessCount, unsigned long long oldAccessTime)
{
    if (prefixes.range[SEG_CACHE_MIN_INDEX] == SEG_CACHE_UNSET_INDEX) return;
    size_t maxGlyphCached = prefixes.range[SEG_CACHE_MAX_INDEX];
    for (size_t i = prefixes.range[SEG_CACHE_MIN_INDEX]; i <= maxGlyphCached; i++)
    {
        if (prefixes.array[i].raw)
        {
            if (level + 1 < ePrefixLength)
                purgeLevel(store, prefixes.array[i], level + 1, minAccessCount, oldAccessTime);
            else
            {
                SegCachePrefixEntry * prefixEntry = prefixes.prefixEntries[i];
                m_segmentCount -= prefixEntry->purge(minAccessCount,
                    oldAccessTime, m_totalAccessCount);
            }
        }
    }
}

uint32 SegCachePrefixEntry::purge(unsigned long long minAccessCount,
                                              unsigned long long oldAccessTime,
                                              unsigned long long currentTime)
{
    // ignore the purge request if another has been done recently
    //if (m_lastPurge > oldAccessTime)
    //    return 0;

    uint32 totalPurged = 0;
    // real length is length + 1 in this loop
    for (uint16 length = 0; length < eMaxSpliceSize; length++)
    {
        if (m_entryCounts[length] == 0)
            continue;
        uint16 purgeCount = 0;
        uint16 newIndex = 0;
        for (uint16 j = 0; j < m_entryCounts[length]; j++)
        {
            SegCacheEntry & tempEntry = m_entries[length][j];
            // purge entries with a low access count which haven't been
            // accessed recently
            if (tempEntry.accessCount() <= minAccessCount &&
                tempEntry.lastAccess() <= oldAccessTime)
            {
                tempEntry.clear();
                ++purgeCount;
            }
            else
            {
                memcpy(m_entries[length] + newIndex++, m_entries[length] + j, sizeof(SegCacheEntry));
            }
        }
        if (purgeCount == m_entryCounts[length])
        {
            assert(newIndex == 0);
            m_entryCounts[length] = 0;
            m_entryBSIndex[length] = 0;
            free(m_entries[length]);
            m_entries[length] = NULL;
        }
        else if (purgeCount > 0)
        {
            assert(m_entryCounts[length] == newIndex + purgeCount);
            m_entryCounts[length] = newIndex;
        }
        totalPurged += purgeCount;
    }
    m_lastPurge = currentTime;
    return totalPurged;
}

#endif
