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

#ifndef GRAPHITE2_NSEGCACHE

#include <graphite2/Segment.h>
#include "inc/CachedFace.h"
#include "inc/SegCacheStore.h"


using namespace graphite2;

CachedFace::CachedFace(const void* appFaceHandle/*non-NULL*/, const gr_face_ops & ops)
: Face(appFaceHandle, ops), m_cacheStore(0)
{
}

CachedFace::~CachedFace()
{
    delete m_cacheStore;
}

bool CachedFace::setupCache(unsigned int cacheSize)
{
    m_cacheStore = new SegCacheStore(*this, m_numSilf, cacheSize);
    return bool(m_cacheStore);
}


bool CachedFace::runGraphite(Segment *seg, const Silf *pSilf) const
{
    assert(pSilf);
    pSilf->runGraphite(seg, 0, pSilf->substitutionPass());

    unsigned int silfIndex = 0;
    for (; silfIndex < m_numSilf && &(m_silfs[silfIndex]) != pSilf; ++silfIndex);
    if (silfIndex == m_numSilf)  return false;
    SegCache * const segCache = m_cacheStore->getOrCreate(silfIndex, seg->getFeatures(0));
    if (!segCache)
        return false;

    assert(m_cacheStore);
    // find where the segment can be broken
    Slot * subSegStartSlot = seg->first();
    Slot * subSegEndSlot = subSegStartSlot;
    uint16 cmapGlyphs[eMaxSpliceSize];
    int subSegStart = 0;
    for (unsigned int i = 0; i < seg->charInfoCount(); ++i)
    {
        const unsigned int length = i - subSegStart + 1;
        if (length < eMaxSpliceSize)
            cmapGlyphs[length-1] = subSegEndSlot->gid();
        else return false;
        const bool spaceOnly = m_cacheStore->isSpaceGlyph(subSegEndSlot->gid());
        // at this stage the character to slot mapping is still 1 to 1
        const int   breakWeight = seg->charinfo(i)->breakWeight(),
                    nextBreakWeight = (i + 1 < seg->charInfoCount())?
                            seg->charinfo(i+1)->breakWeight() : 0;
        const uint8 f = seg->charinfo(i)->flags();
        if (((spaceOnly
                || (breakWeight > 0 && breakWeight <= gr_breakWord)
                || i + 1 == seg->charInfoCount()
                || ((nextBreakWeight < 0 && nextBreakWeight >= gr_breakBeforeWord)
                    || (subSegEndSlot->next() && m_cacheStore->isSpaceGlyph(subSegEndSlot->next()->gid()))))
                && f != 1)
            || f == 2)
        {
            // record the next slot before any splicing
            Slot * nextSlot = subSegEndSlot->next();
            // spaces should be left untouched by graphite rules in any sane font
            if (!spaceOnly)
            {
                // found a break position, check for a cache of the sub sequence
                const SegCacheEntry * entry = segCache->find(cmapGlyphs, length);
                // TODO disable cache for words at start/end of line with contextuals
                if (!entry)
                {
                    SegmentScopeState scopeState = seg->setScope(subSegStartSlot, subSegEndSlot, length);
                    pSilf->runGraphite(seg, pSilf->substitutionPass(), pSilf->numPasses());
                    if (length < eMaxSpliceSize)
                    {
                        seg->associateChars(subSegStart, length);
                        segCache->cache(m_cacheStore, cmapGlyphs, length, seg, subSegStart);
                    }
                    seg->removeScope(scopeState);
                }
                else
                    seg->splice(subSegStart, length, subSegStartSlot, subSegEndSlot,
                        entry->first(), entry->glyphLength());
            }
            subSegStartSlot = subSegEndSlot = nextSlot;
            subSegStart = i + 1;
        }
        else
        {
            subSegEndSlot = subSegEndSlot->next();
        }
    }
    return true;
}

#endif

