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
#pragma once

#include "inc/Main.h"
#include "inc/GlyphFace.h"
#include "inc/Silf.h"
#include "inc/TtfUtil.h"
#include "inc/Main.h"
#include "graphite2/Font.h"
#include "inc/FeatureMap.h"
#include "inc/GlyphFaceCache.h"

#ifndef GRAPHITE2_NFILEFACE
#include <cstdio>
#include <cassert>
#include "inc/TtfTypes.h"
#endif      //!GRAPHITE2_NFILEFACE

namespace graphite2 {

class Segment;
class FeatureVal;
class NameTable;
class Cmap;

using TtfUtil::Tag;

// These are the actual tags, as distinct from the consecutive IDs in TtfUtil.h

#ifndef GRAPHITE2_NFILEFACE
class TableCacheItem
{
public:
    TableCacheItem(char * theData, size_t theSize) : m_data(theData), m_size(theSize) {}
    TableCacheItem() : m_data(0), m_size(0) {}
    ~TableCacheItem()
    {
        if (m_size) free(m_data);
    }
    void set(char * theData, size_t theSize) { m_data = theData; m_size = theSize; }
    const void * data() const { return m_data; }
    size_t size() const { return m_size; }
private:
    char * m_data;
    size_t m_size;
};
#endif      //!GRAPHITE2_NFILEFACE




class FileFace
{
#ifndef GRAPHITE2_NFILEFACE
public:
    static const void *table_fn(const void* appFaceHandle, unsigned int name, size_t *len);
  
    FileFace(const char *filename);
    ~FileFace();
//    virtual const void *getTable(unsigned int name, size_t *len) const;
    bool isValid() const { return m_pfile && m_pHeader && m_pTableDir; }

    CLASS_NEW_DELETE
public:     //for local convenience    
    FILE* m_pfile;
    unsigned int m_lfile;
    mutable TableCacheItem m_tables[18];
    TtfUtil::Sfnt::OffsetSubTable* m_pHeader;
    TtfUtil::Sfnt::OffsetSubTable::Entry* m_pTableDir;       //[] number of elements is determined by m_pHeader->num_tables
#endif      //!GRAPHITE2_NFILEFACE
   
private:        //defensive
    FileFace(const FileFace&);
    FileFace& operator=(const FileFace&);
};

class Face
{
public:
    const byte *getTable(const Tag name, size_t  * len = 0) const {
    	size_t tbl_len=0;
    	const byte * const tbl = reinterpret_cast<const byte *>((*m_getTable)(m_appFaceHandle, name, &tbl_len));
    	if (len) *len = tbl_len;
    	return TtfUtil::CheckTable(name, tbl, tbl_len) ? tbl : 0;
    }
    float advance(unsigned short id) const { return m_pGlyphFaceCache->glyph(id)->theAdvance().x; }
    const Silf *silf(int i) const { return ((i < m_numSilf) ? m_silfs + i : (const Silf *)NULL); }
    virtual bool runGraphite(Segment *seg, const Silf *silf) const;
    uint16 findPseudo(uint32 uid) const { return (m_numSilf) ? m_silfs[0].findPseudo(uid) : 0; }

public:
    Face(const void* appFaceHandle/*non-NULL*/, gr_get_table_fn getTable2) : 
        m_appFaceHandle(appFaceHandle), m_getTable(getTable2), m_pGlyphFaceCache(NULL),
        m_cmap(NULL), m_numSilf(0), m_silfs(NULL), m_pFileFace(NULL),
        m_pNames(NULL) {}
    virtual ~Face();
public:
    float getAdvance(unsigned short glyphid, float scale) const { return advance(glyphid) * scale; }
    const Rect &theBBoxTemporary(uint16 gid) const { return m_pGlyphFaceCache->glyph(gid)->theBBox(); }   //warning value may become invalid when another glyph is accessed
    unsigned short upem() const { return m_upem; }
    uint16 glyphAttr(uint16 gid, uint8 gattr) const { return m_pGlyphFaceCache->glyphAttr(gid, gattr); }

private:
    friend class Font;
    unsigned short numGlyphs() const { return m_pGlyphFaceCache->m_nGlyphs; }

public:
    bool readGlyphs(uint32 faceOptions);
    bool readGraphite();
    bool readFeatures() { return m_Sill.readFace(*this); }
    const Silf *chooseSilf(uint32 script) const;
    const SillMap& theSill() const { return m_Sill; }
    uint16 numFeatures() const { return m_Sill.m_FeatureMap.numFeats(); }
    const FeatureRef *featureById(uint32 id) const { return m_Sill.m_FeatureMap.findFeatureRef(id); }
    const FeatureRef *feature(uint16 index) const { return m_Sill.m_FeatureMap.feature(index); }
    uint16 getGlyphMetric(uint16 gid, uint8 metric) const;

    const GlyphFaceCache* getGlyphFaceCache() const { return m_pGlyphFaceCache; }      //never NULL
    void takeFileFace(FileFace* pFileFace/*takes ownership*/);
    Cmap & cmap() const { return *m_cmap; };
    NameTable * nameTable() const;
    uint16 languageForLocale(const char * locale) const;

    CLASS_NEW_DELETE
private:
    const void* m_appFaceHandle/*non-NULL*/;
    gr_get_table_fn m_getTable;
    uint16 m_ascent;
    uint16 m_descent;
    // unsigned short *m_glyphidx;     // index for each glyph id in the font
    // unsigned short m_readglyphs;    // how many glyphs have we in m_glyphs?
    // unsigned short m_capacity;      // how big is m_glyphs
    mutable GlyphFaceCache* m_pGlyphFaceCache;      //owned - never NULL
    mutable Cmap * m_cmap; // cmap cache if available
    unsigned short m_upem;          // design units per em
protected:
    unsigned short m_numSilf;       // number of silf subtables in the silf table
    Silf *m_silfs;                   // silf subtables.
private:
    SillMap m_Sill;
    FileFace* m_pFileFace;      //owned
    mutable NameTable* m_pNames;
    
private:        //defensive on m_pGlyphFaceCache, m_pFileFace and m_silfs
    Face(const Face&);
    Face& operator=(const Face&);
};

} // namespace graphite2

struct gr_face : public graphite2::Face {};
