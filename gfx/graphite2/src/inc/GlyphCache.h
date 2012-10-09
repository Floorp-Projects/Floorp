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


#include "graphite2/Font.h"
#include "inc/Main.h"

namespace graphite2 {

class Face;
class FeatureVal;
class GlyphFace;
class Segment;

class GlyphCache
{
    class Loader;

	GlyphCache(const GlyphCache&);
    GlyphCache& operator=(const GlyphCache&);

public:
    GlyphCache(const Face & face, const uint32 face_options);
    ~GlyphCache();

    size_t numGlyphs() const throw();
    size_t numAttrs() const throw();
    size_t unitsPerEm() const throw();

    const GlyphFace *glyph(unsigned short glyphid) const;      //result may be changed by subsequent call with a different glyphid
    const GlyphFace *glyphSafe(unsigned short glyphid) const;
    uint16 glyphAttr(uint16 gid, uint16 gattr) const;

    CLASS_NEW_DELETE;
    
private:
    const Loader              * _glyph_loader;
    const GlyphFace *   *       _glyphs;
    unsigned short              _num_glyphs,
                                _num_attrs,
                                _upem;
};

inline
size_t GlyphCache::numGlyphs() const throw()
{
    return _num_glyphs;
}

inline
size_t GlyphCache::numAttrs() const throw()
{
    return _num_attrs;
}

inline
size_t GlyphCache::unitsPerEm() const throw()
{
    return _upem;
}

inline
const GlyphFace *GlyphCache::glyphSafe(unsigned short glyphid) const
{
    return glyphid < _num_glyphs ? glyph(glyphid) : NULL;
}

} // namespace graphite2
