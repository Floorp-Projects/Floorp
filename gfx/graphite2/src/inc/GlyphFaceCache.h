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

#include "inc/GlyphFace.h"
#include "graphite2/Font.h"

namespace graphite2 {

class Segment;
class Face;
class FeatureVal;


class GlyphFaceCacheHeader
{
public:
    bool initialize(const Face & face, const bool dumb_font);    //return result indicates success. Do not use if failed.
    unsigned short numGlyphs() const { return m_nGlyphs; }
    unsigned short numAttrs() const { return m_numAttrs; }

private:
friend class Face;
friend class GlyphFace;
    const byte* m_pHead,
    		  * m_pHHea,
    		  * m_pHmtx,
    		  * m_pGlat,
    		  * m_pGloc,
    		  * m_pGlyf,
    		  * m_pLoca;
    size_t		m_lHmtx,
    			m_lGlat,
    			m_lGlyf,
    			m_lLoca;

    uint32			m_fGlat;
    unsigned short 	m_numAttrs,					// number of glyph attributes per glyph
    				m_nGlyphsWithGraphics,		//i.e. boundary box and advance
    				m_nGlyphsWithAttributes,
    				m_nGlyphs;					// number of glyphs in the font. Max of the above 2.
    bool 			m_locFlagsUse32Bit;
};

class GlyphFaceCache : public GlyphFaceCacheHeader
{
public:
    static GlyphFaceCache* makeCache(const GlyphFaceCacheHeader& hdr /*, EGlyphCacheStrategy requested */);

    GlyphFaceCache(const GlyphFaceCacheHeader& hdr);
    ~GlyphFaceCache();

    const GlyphFace *glyphSafe(unsigned short glyphid) const { return glyphid<numGlyphs()?glyph(glyphid):NULL; }
    uint16 glyphAttr(uint16 gid, uint8 gattr) const { if (gattr>=numAttrs()) return 0; const GlyphFace*p=glyphSafe(gid); return p?p->getAttr(gattr):0; }

    void * operator new (size_t s, const GlyphFaceCacheHeader& hdr)
    {
        return malloc(s + sizeof(GlyphFace*)*hdr.numGlyphs());
    }
    // delete in case an exception is thrown in constructor
    void operator delete(void* p, const GlyphFaceCacheHeader& ) throw()
    {
        free(p);
    }

    const GlyphFace *glyph(unsigned short glyphid) const;      //result may be changed by subsequent call with a different glyphid
    void loadAllGlyphs();

    CLASS_NEW_DELETE
    
private:
    GlyphFace **glyphPtrDirect(unsigned short glyphid) const { return (GlyphFace **)((const char*)(this)+sizeof(GlyphFaceCache)+sizeof(GlyphFace*)*glyphid);}

private:      //defensive
    GlyphFaceCache(const GlyphFaceCache&);
    GlyphFaceCache& operator=(const GlyphFaceCache&);
};

} // namespace graphite2
