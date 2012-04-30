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
#include <cstring>
#include "graphite2/Segment.h"
#include "inc/Face.h"
#include "inc/Endian.h"
#include "inc/Segment.h"
#include "inc/CmapCache.h"
#include "inc/NameTable.h"
#include "inc/SegCacheStore.h"


using namespace graphite2;

Face::~Face()
{
    delete m_pGlyphFaceCache;
    delete m_cmap;
    delete[] m_silfs;
    m_pGlyphFaceCache = NULL;
    m_cmap = NULL;
    m_silfs = NULL;
    delete m_pFileFace;
    delete m_pNames;
    m_pFileFace = NULL;
}


bool Face::readGlyphs(uint32 faceOptions)
{
    GlyphFaceCacheHeader hdr;
    if (!hdr.initialize(*this, faceOptions & gr_face_dumbRendering)) return false;

    m_pGlyphFaceCache = GlyphFaceCache::makeCache(hdr);
    if (!m_pGlyphFaceCache) return false;

    size_t length = 0;
    const byte * table = getTable(Tag::cmap, &length);
    if (!table) return false;

    if (faceOptions & gr_face_cacheCmap)
    	m_cmap = new CmapCache(table, length);
    else
    	m_cmap = new DirectCmap(table, length);

    if (!m_cmap || !*m_cmap) return false;

    if (faceOptions & gr_face_preloadGlyphs)
    {
        m_pGlyphFaceCache->loadAllGlyphs();
        nameTable();        // preload the name table along with the glyphs, heh.
    }
    m_upem = TtfUtil::DesignUnits(m_pGlyphFaceCache->m_pHead);
    if (!m_upem) return false;
    // m_glyphidx = new unsigned short[m_numGlyphs];        // only need this if doing occasional glyph reads
    
    return true;
}

bool Face::readGraphite()
{
    size_t lSilf;
    const byte * const pSilf = getTable(Tag::Silf, &lSilf),
    		   *           p = pSilf;
    if (!p) return false;

    const uint32 version = be::read<uint32>(p);
    if (version < 0x00020000) return false;
    if (version >= 0x00030000)
    	be::skip<uint32>(p);		// compilerVersion
    m_numSilf = be::read<uint16>(p);
    be::skip<uint16>(p);			// reserved

    bool havePasses = false;
    m_silfs = new Silf[m_numSilf];
    for (int i = 0; i < m_numSilf; i++)
    {
        const uint32 offset = be::read<uint32>(p),
        			 next   = i == m_numSilf - 1 ? lSilf : be::peek<uint32>(p);
        if (next > lSilf || offset >= next)
            return false;

        if (!m_silfs[i].readGraphite(pSilf + offset, next - offset, *this, version))
            return false;

        if (m_silfs[i].numPasses())
            havePasses = true;
    }

    return havePasses;
}

bool Face::runGraphite(Segment *seg, const Silf *aSilf) const
{
    return aSilf->runGraphite(seg, 0, aSilf->numPasses());
}

const Silf *Face::chooseSilf(uint32 script) const
{
    if (m_numSilf == 0)
        return NULL;
    else if (m_numSilf == 1 || script == 0)
        return m_silfs;
    else // do more work here
        return m_silfs;
}

uint16 Face::getGlyphMetric(uint16 gid, uint8 metric) const
{
    switch (metrics(metric))
    {
        case kgmetAscent : return m_ascent;
        case kgmetDescent : return m_descent;
        default: return m_pGlyphFaceCache->glyph(gid)->getMetric(metric);
    }
}

void Face::takeFileFace(FileFace* pFileFace/*takes ownership*/)
{
    if (m_pFileFace==pFileFace)
      return;
    
    delete m_pFileFace;
    m_pFileFace = pFileFace;
}

NameTable * Face::nameTable() const
{
    if (m_pNames) return m_pNames;
    size_t tableLength = 0;
    const byte * table = getTable(Tag::name, &tableLength);
    if (table)
        m_pNames = new NameTable(table, tableLength);
    return m_pNames;
}

uint16 Face::languageForLocale(const char * locale) const
{
    nameTable();
    if (m_pNames)
        return m_pNames->getLanguageId(locale);
    return 0;
}


#ifndef GRAPHITE2_NFILEFACE

FileFace::FileFace(const char *filename) :
    m_pHeader(NULL),
    m_pTableDir(NULL)
{
    if (!(m_pfile = fopen(filename, "rb"))) return;
    if (fseek(m_pfile, 0, SEEK_END)) return;
    m_lfile = ftell(m_pfile);
    if (fseek(m_pfile, 0, SEEK_SET)) return;
    size_t lOffset, lSize;
    if (!TtfUtil::GetHeaderInfo(lOffset, lSize)) return;
    m_pHeader = (TtfUtil::Sfnt::OffsetSubTable*)gralloc<char>(lSize);
    if (fseek(m_pfile, lOffset, SEEK_SET)) return;
    if (fread(m_pHeader, 1, lSize, m_pfile) != lSize) return;
    if (!TtfUtil::CheckHeader(m_pHeader)) return;
    if (!TtfUtil::GetTableDirInfo(m_pHeader, lOffset, lSize)) return;
    m_pTableDir = (TtfUtil::Sfnt::OffsetSubTable::Entry*)gralloc<char>(lSize);
    if (fseek(m_pfile, lOffset, SEEK_SET)) return;
    if (fread(m_pTableDir, 1, lSize, m_pfile) != lSize) return;
}

FileFace::~FileFace()
{
    free(m_pTableDir);
    free(m_pHeader);
    if (m_pfile)
        fclose(m_pfile);
    m_pTableDir = NULL;
    m_pfile = NULL;
    m_pHeader = NULL;
}


const void *FileFace::table_fn(const void* appFaceHandle, unsigned int name, size_t *len)
{
    const FileFace* ttfFaceHandle = (const FileFace*)appFaceHandle;
    TableCacheItem * tci = ttfFaceHandle->m_tables;

    switch (name)
    {
		case Tag::Feat:	tci += 0; break;
		case Tag::Glat:	tci += 1; break;
		case Tag::Gloc:	tci += 2; break;
		case Tag::OS_2:	tci += 3; break;
		case Tag::Sile:	tci += 4; break;
		case Tag::Silf:	tci += 5; break;
		case Tag::Sill:	tci += 6; break;
    	case Tag::cmap:	tci += 7; break;
    	case Tag::glyf:	tci += 8; break;
    	case Tag::hdmx:	tci += 9; break;
    	case Tag::head:	tci += 10; break;
    	case Tag::hhea:	tci += 11; break;
    	case Tag::hmtx:	tci += 12; break;
    	case Tag::kern:	tci += 13; break;
    	case Tag::loca:	tci += 14; break;
    	case Tag::maxp:	tci += 15; break;
    	case Tag::name:	tci += 16; break;
    	case Tag::post:	tci += 17; break;
    	default:					tci = 0; break;
    }

    assert(tci); // don't expect any other table types
    if (!tci) return NULL;
    if (tci->data() == NULL)
    {
        char *tptr;
        size_t tlen, lOffset;
        if (!TtfUtil::GetTableInfo(name, ttfFaceHandle->m_pHeader, ttfFaceHandle->m_pTableDir, lOffset, tlen)) return NULL;
        if (fseek(ttfFaceHandle->m_pfile, lOffset, SEEK_SET)) return NULL;
        if (lOffset + tlen > ttfFaceHandle->m_lfile) return NULL;
        tptr = gralloc<char>(tlen);
        if (fread(tptr, 1, tlen, ttfFaceHandle->m_pfile) != tlen) 
        {
            free(tptr);
            return NULL;
        }
        tci->set(tptr, tlen);
    }
    if (len) *len = tci->size();
    return tci->data();
}
#endif                  //!GRAPHITE2_NFILEFACE
