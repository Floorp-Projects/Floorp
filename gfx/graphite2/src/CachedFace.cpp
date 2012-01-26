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

CachedFace::CachedFace(const void* appFaceHandle/*non-NULL*/, gr_get_table_fn getTable2)
: Face(appFaceHandle, getTable2), m_cacheStore(0) 
{
}

CachedFace::~CachedFace()
{
    delete m_cacheStore;
}

bool CachedFace::setupCache(unsigned int cacheSize)
{
    m_cacheStore = new SegCacheStore(this, m_numSilf, cacheSize);
    return (m_cacheStore != NULL);
}


bool CachedFace::runGraphite(Segment *seg, const Silf *pSilf) const
{
    assert(pSilf);
    pSilf->runGraphite(seg, 0, pSilf->substitutionPass());

    SegCache * segCache = NULL;
    unsigned int silfIndex = 0;

    for (unsigned int i = 0; i < m_numSilf; i++)
    {
        if (&(m_silfs[i]) == pSilf)
        {
            break;
        }
    }
    assert(silfIndex < m_numSilf);
    assert(m_cacheStore);
    segCache = m_cacheStore->getOrCreate(silfIndex, seg->getFeatures(0));
    // find where the segment can be broken
    Slot * subSegStartSlot = seg->first();
    Slot * subSegEndSlot = subSegStartSlot;
    uint16 cmapGlyphs[eMaxSpliceSize];
    int subSegStart = 0;
    bool spaceOnly = true;
    for (unsigned int i = 0; i < seg->charInfoCount(); i++)
    {
        if (i - subSegStart < eMaxSpliceSize)
        {
            cmapGlyphs[i-subSegStart] = subSegEndSlot->gid();
        }
        if (!m_cacheStore->isSpaceGlyph(subSegEndSlot->gid()))
        {
            spaceOnly = false;
        }
        // at this stage the character to slot mapping is still 1 to 1
        int breakWeight = seg->charinfo(i)->breakWeight();
        int nextBreakWeight = (i + 1 < seg->charInfoCount())?
            seg->charinfo(i+1)->breakWeight() : 0;
        if (((breakWeight > 0) &&
             (breakWeight <= gr_breakWord)) ||
            (i + 1 == seg->charInfoCount()) ||
             m_cacheStore->isSpaceGlyph(subSegEndSlot->gid()) ||
            ((i + 1 < seg->charInfoCount()) &&
             (((nextBreakWeight < 0) &&
              (nextBreakWeight >= gr_breakBeforeWord)) ||
              (subSegEndSlot->next() && m_cacheStore->isSpaceGlyph(subSegEndSlot->next()->gid())))))
        {
            // record the next slot before any splicing
            Slot * nextSlot = subSegEndSlot->next();
            if (spaceOnly)
            {
                // spaces should be left untouched by graphite rules in any sane font
            }
            else
            {
                // found a break position, check for a cache of the sub sequence
                const SegCacheEntry * entry = (segCache)?
                    segCache->find(cmapGlyphs, i - subSegStart + 1) : NULL;
                // TODO disable cache for words at start/end of line with contextuals
                if (!entry)
                {
                    unsigned int length = i - subSegStart + 1;
                    SegmentScopeState scopeState = seg->setScope(subSegStartSlot, subSegEndSlot, length);
                    pSilf->runGraphite(seg, pSilf->substitutionPass(), pSilf->numPasses());
                    //entry =runGraphiteOnSubSeg(segCache, seg, cmapGlyphs,
                    //                           subSegStartSlot, subSegEndSlot,
                    //                           subSegStart, i - subSegStart + 1);
                    if ((length < eMaxSpliceSize) && segCache)
                        entry = segCache->cache(m_cacheStore, cmapGlyphs, length, seg, subSegStart);
                    seg->removeScope(scopeState);
                }
                else
                {
                    //seg->splice(subSegStart, i - subSegStart + 1, subSegStartSlot, subSegEndSlot, entry);
                    seg->splice(subSegStart, i - subSegStart + 1, subSegStartSlot, subSegEndSlot,
                        entry->first(), entry->glyphLength());
                }
            }
            subSegEndSlot = nextSlot;
            subSegStartSlot = nextSlot;
            subSegStart = i + 1;
            spaceOnly = true;
        }
        else
        {
            subSegEndSlot = subSegEndSlot->next();
        }
    }
    return true;
}

#endif

