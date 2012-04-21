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
#include "inc/GlyphFaceCache.h"
#include "graphite2/Font.h"
#include "inc/Face.h"     //for the tags
#include "inc/Endian.h"

using namespace graphite2;

/*virtual*/ bool GlyphFaceCacheHeader::initialize(const Face & face, const bool dumb_font)    //return result indicates success. Do not use if failed.
{
    if ((m_pLoca = face.getTable(Tag::loca, &m_lLoca)) == NULL) return false;
    if ((m_pHead = face.getTable(Tag::head)) == NULL) return false;
    if ((m_pGlyf = face.getTable(Tag::glyf, &m_lGlyf)) == NULL) return false;
    if ((m_pHmtx = face.getTable(Tag::hmtx, &m_lHmtx)) == NULL) return false;
    if ((m_pHHea = face.getTable(Tag::hhea)) == NULL) return false;

    const void* pMaxp = face.getTable(Tag::maxp);
    if (pMaxp == NULL) return false;
    m_nGlyphs = m_nGlyphsWithGraphics = (unsigned short)TtfUtil::GlyphCount(pMaxp);
    if (TtfUtil::LocaLookup(m_nGlyphs-1, m_pLoca, m_lLoca, m_pHead) == size_t(-1))
    	return false; // This will fail if m_nGlyphs is wildly out of range.

    if (!dumb_font)
    {
		if ((m_pGlat = face.getTable(Tag::Glat, &m_lGlat)) == NULL) return false;
		m_fGlat = be::peek<uint32>(m_pGlat);
		size_t lGloc;
		if ((m_pGloc = face.getTable(Tag::Gloc, &lGloc)) == NULL) return false;
		if (lGloc < 6) return false;
		int version = be::read<uint32>(m_pGloc);
		if (version != 0x00010000) return false;

		const uint16 locFlags = be::read<uint16>(m_pGloc);
		m_numAttrs = be::read<uint16>(m_pGloc);
		if (m_numAttrs > 0x1000) return false;                  // is this hard limit appropriate?

		if (locFlags & 1)
		{
			m_locFlagsUse32Bit = true;
			m_nGlyphsWithAttributes = (unsigned short)((lGloc - 12) / 4);
		}
		else
		{
			m_locFlagsUse32Bit = false;
			m_nGlyphsWithAttributes = (unsigned short)((lGloc - 10) / 2);
		}

		if (m_nGlyphsWithAttributes > m_nGlyphs)
	        m_nGlyphs = m_nGlyphsWithAttributes;
    }

    return true;
}

GlyphFaceCache* GlyphFaceCache::makeCache(const GlyphFaceCacheHeader& hdr)
{
    return new (hdr) GlyphFaceCache(hdr);
}

GlyphFaceCache::GlyphFaceCache(const GlyphFaceCacheHeader& hdr)
:   GlyphFaceCacheHeader(hdr)
{
    unsigned int nGlyphs = numGlyphs();
    
    for (unsigned int i = 0; i < nGlyphs; i++)
    {
         *glyphPtrDirect(i) = NULL;
    }
}

GlyphFaceCache::~GlyphFaceCache()
{
    unsigned int nGlyphs = numGlyphs();
    int deltaPointers = (*glyphPtrDirect(nGlyphs-1u) - *glyphPtrDirect(0u));
    if ((nGlyphs > 0u) && (deltaPointers == static_cast<int>(nGlyphs - 1)))
    {
        for (unsigned int i=0 ; i<nGlyphs; ++i)
        {
            GlyphFace *p = *glyphPtrDirect(i);
            assert (p);
            p->~GlyphFace();
        }
        free (*glyphPtrDirect(0));
    }
    else
    {
        for (unsigned int i=0 ; i<nGlyphs; ++i)
        {
            GlyphFace *p = *glyphPtrDirect(i);
            if (p)
            {
                p->~GlyphFace();
                free(p);
            }
        }
    }
}

void GlyphFaceCache::loadAllGlyphs()
{
    unsigned int nGlyphs = numGlyphs();
//    size_t sparse_size = 0;
    GlyphFace * glyphs = gralloc<GlyphFace>(nGlyphs);
    for (unsigned short glyphid = 0; glyphid < nGlyphs; glyphid++)
    {
        GlyphFace **p = glyphPtrDirect(glyphid);
        *p = &(glyphs[glyphid]);
        new(*p) GlyphFace(*this, glyphid);
//        sparse_size += (*p)->m_attrs._sizeof();
    }
//    const size_t flat_size = nGlyphs*(sizeof(uint16*) + sizeof(uint16)*numAttrs());
//    assert(sparse_size <= flat_size);
}

/*virtual*/ const GlyphFace *GlyphFaceCache::glyph(unsigned short glyphid) const      //result may be changed by subsequent call with a different glyphid
{ 
    GlyphFace **p = glyphPtrDirect(glyphid);
    if (*p)
        return *p;

    *p = (GlyphFace*)malloc(sizeof(GlyphFace));
    new(*p) GlyphFace(*this, glyphid);
    return *p;
}
