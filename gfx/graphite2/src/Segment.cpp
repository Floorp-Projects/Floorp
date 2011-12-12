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
#include "UtfCodec.h"
#include <cstring>
#include <cstdlib>

#include "Segment.h"
#include "graphite2/Font.h"
#include "CharInfo.h"
#include "Slot.h"
#include "Main.h"
#include "XmlTraceLog.h"
#include "CmapCache.h"
#include "graphite2/Segment.h"


using namespace graphite2;

Segment::Segment(unsigned int numchars, const Face* face, uint32 script, int textDir) :
        m_freeSlots(NULL),
        m_first(NULL),
        m_last(NULL),
        m_numGlyphs(numchars),
        m_numCharinfo(numchars),
        m_defaultOriginal(0),
        m_charinfo(new CharInfo[numchars]),
        m_face(face),
        m_silf(face->chooseSilf(script)),
        m_bbox(Rect(Position(0, 0), Position(0, 0))),
        m_dir(textDir)
{
    unsigned int i, j;
    m_bufSize = numchars + 10;
    freeSlot(newSlot());
    for (i = 0, j = 1; j < numchars; i++, j <<= 1) {}
    if (!i) i = 1;
    m_bufSize = i;                  // log2(numchars)
}

Segment::~Segment()
{
    for (SlotRope::iterator i = m_slots.begin(); i != m_slots.end(); ++i)
        free(*i);
    for (AttributeRope::iterator j = m_userAttrs.begin(); j != m_userAttrs.end(); ++j)
        free(*j);
    delete[] m_charinfo;
}

#ifndef DISABLE_SEGCACHE
SegmentScopeState Segment::setScope(Slot * firstSlot, Slot * lastSlot, size_t subLength)
{
    SegmentScopeState state;
    state.numGlyphsOutsideScope = m_numGlyphs - subLength;
    state.realFirstSlot = m_first;
    state.slotBeforeScope = firstSlot->prev();
    state.slotAfterScope = lastSlot->next();
    state.realLastSlot = m_last;
    firstSlot->prev(NULL);
    lastSlot->next(NULL);
    assert(m_defaultOriginal == 0);
    m_defaultOriginal = firstSlot->original();
    m_numGlyphs = subLength;
    m_first = firstSlot;
    m_last = lastSlot;
    return state;
}

void Segment::removeScope(SegmentScopeState & state)
{
    m_numGlyphs = state.numGlyphsOutsideScope + m_numGlyphs;
    if (state.slotBeforeScope)
    {
        state.slotBeforeScope->next(m_first);
        m_first->prev(state.slotBeforeScope);
        m_first = state.realFirstSlot;
    }
    if (state.slotAfterScope)
    {
        state.slotAfterScope->prev(m_last);
        m_last->next(state.slotAfterScope);
        m_last = state.realLastSlot;
    }
    m_defaultOriginal = 0;
}


void Segment::append(const Segment &other)
{
    Rect bbox = other.m_bbox + m_advance;

    m_slots.insert(m_slots.end(), other.m_slots.begin(), other.m_slots.end());
    CharInfo* pNewCharInfo = new CharInfo[m_numCharinfo+other.m_numCharinfo];		//since CharInfo has no constructor, this doesn't do much
    for (unsigned int i=0 ; i<m_numCharinfo ; ++i)
	pNewCharInfo[i] = m_charinfo[i];
    m_last->next(other.m_first);
    other.m_last->prev(m_last);
    m_userAttrs.insert(m_userAttrs.end(), other.m_userAttrs.begin(), other.m_userAttrs.end());
    
    delete[] m_charinfo;
    m_charinfo = pNewCharInfo;
    pNewCharInfo += m_numCharinfo ;
    for (unsigned int i=0 ; i<m_numCharinfo ; ++i)
        pNewCharInfo[i] = other.m_charinfo[i];
 
    m_numCharinfo += other.m_numCharinfo;
    m_numGlyphs += other.m_numGlyphs;
    m_advance = m_advance + other.m_advance;
    m_bbox = m_bbox.widen(bbox);
}
#endif // DISABLE_SEGCACHE

void Segment::appendSlot(int id, int cid, int gid, int iFeats, size_t coffset)
{
    Slot *aSlot = newSlot();
    
    m_charinfo[id].init(cid);
    m_charinfo[id].feats(iFeats);
    m_charinfo[id].base(coffset);
    const GlyphFace * theGlyph = m_face->getGlyphFaceCache()->glyphSafe(gid);
    if (theGlyph)
    {
        m_charinfo[id].breakWeight(theGlyph->getAttr(m_silf->aBreak()));
    }
    else
    {
        m_charinfo[id].breakWeight(0);
    }
    
    aSlot->child(NULL);
    aSlot->setGlyph(this, gid, theGlyph);
    aSlot->originate(id);
    aSlot->before(id);
    aSlot->after(id);
    if (m_last) m_last->next(aSlot);
    aSlot->prev(m_last);
    m_last = aSlot;
    if (!m_first) m_first = aSlot;
}

Slot *Segment::newSlot()
{
    if (!m_freeSlots)
    {
        int numUser = m_silf->numUser();
        Slot *newSlots = grzeroalloc<Slot>(m_bufSize);
        uint16 *newAttrs = grzeroalloc<uint16>(numUser * m_bufSize);
        newSlots[0].userAttrs(newAttrs);
        for (size_t i = 1; i < m_bufSize - 1; i++)
        {
            newSlots[i].next(newSlots + i + 1);
            newSlots[i].userAttrs(newAttrs + i * numUser);
        }
        newSlots[m_bufSize - 1].userAttrs(newAttrs + (m_bufSize - 1) * numUser);
        newSlots[m_bufSize - 1].next(NULL);
        m_slots.push_back(newSlots);
        m_userAttrs.push_back(newAttrs);
        m_freeSlots = (m_bufSize > 1)? newSlots + 1 : NULL;
        return newSlots;
    }
    Slot *res = m_freeSlots;
    m_freeSlots = m_freeSlots->next();
    res->next(NULL);
    return res;
}

void Segment::freeSlot(Slot *aSlot)
{
    if (m_last == aSlot) m_last = aSlot->prev();
    if (m_first == aSlot) m_first = aSlot->next();
    // reset the slot incase it is reused
    ::new (aSlot) Slot;
    memset(aSlot->userAttrs(), 0, m_silf->numUser() * sizeof(uint16));
    // update next pointer
    if (!m_freeSlots)
        aSlot->next(NULL);
    else
        aSlot->next(m_freeSlots);
    m_freeSlots = aSlot;
}

#ifndef DISABLE_SEGCACHE
void Segment::splice(size_t offset, size_t length, Slot * startSlot,
                       Slot * endSlot, const Slot * firstSpliceSlot,
                       size_t numGlyphs)
{
    const Slot * replacement = firstSpliceSlot;
    Slot * slot = startSlot;
    extendLength(numGlyphs - length);
    // insert extra slots if needed
    while (numGlyphs > length)
    {
        Slot * extra = newSlot();
        extra->prev(endSlot);
        extra->next(endSlot->next());
        endSlot->next(extra);
        if (extra->next())
            extra->next()->prev(extra);
        if (m_last == endSlot)
            m_last = extra;
        endSlot = extra;
        ++length;
    }
    // remove any extra
    if (numGlyphs < length)
    {
        Slot * afterSplice = endSlot->next();
        do
        {
            endSlot = endSlot->prev();
            freeSlot(endSlot->next());
            --length;
        } while (numGlyphs < length);
        endSlot->next(afterSplice);
        if (afterSplice)
            afterSplice->prev(endSlot);
    }
    assert(numGlyphs == length);
    // keep a record of consecutive slots wrt start of splice to minimize
    // iterative next/prev calls
    Slot * slotArray[eMaxSpliceSize];
    uint16 slotPosition = 0;
    for (uint16 i = 0; i < numGlyphs; i++)
    {
        if (slotPosition <= i)
        {
            slotArray[i] = slot;
            slotPosition = i;
        }
        slot->set(*replacement, offset, m_silf->numUser());
        if (replacement->attachedTo())
        {
            uint16 parentPos = replacement->attachedTo() - firstSpliceSlot;
            while (slotPosition < parentPos)
            {
                slotArray[slotPosition+1] = slotArray[slotPosition]->next();
                ++slotPosition;
            }
            slot->attachTo(slotArray[parentPos]);
        }
        if (replacement->nextSibling())
        {
            uint16 pos = replacement->nextSibling() - firstSpliceSlot;
            while (slotPosition < pos)
            {
                slotArray[slotPosition+1] = slotArray[slotPosition]->next();
                ++slotPosition;
            }
            slot->sibling(slotArray[pos]);
        }
        if (replacement->firstChild())
        {
            uint16 pos = replacement->firstChild() - firstSpliceSlot;
            while (slotPosition < pos)
            {
                slotArray[slotPosition+1] = slotArray[slotPosition]->next();
                ++slotPosition;
            }
            slot->child(slotArray[pos]);
        }
        slot = slot->next();
        replacement = replacement->next();
    }
}
#endif // DISABLE_SEGCACHE
        
void Segment::positionSlots(const Font *font, Slot *iStart, Slot *iEnd)
{
    Position currpos(0., 0.);
    Slot *s, *ls = NULL;
    int iSlot = 0;
    float cMin = 0.;
    float clusterMin = 0.;
    Rect bbox;

    if (!iStart) iStart = m_first;
    if (!iEnd) iEnd = m_last;
    
    if (m_dir & 1)
    {
        for (s = iEnd, iSlot = m_numGlyphs - 1; s && s != iStart->prev(); s = s->prev(), --iSlot)
        {
            int j = s->before();
            if (j >= 0)
            {
                for ( ; j <= s->after(); j++)
                {
                    CharInfo *c = charinfo(j);
                    if (c->before() == -1 || iSlot < c->before()) c->before(iSlot);
                    if (c->after() < iSlot) c->after(iSlot);
                }
            }
            s->index(iSlot);

            if (s->isBase())
            {
                clusterMin = currpos.x;
                currpos = s->finalise(this, font, &currpos, &bbox, &cMin, 0, &clusterMin);
                if (ls)
                    ls->sibling(s);
                ls = s;
            }
        }
    }
    else
    {
        for (s = iStart, iSlot = 0; s && s != iEnd->next(); s = s->next(), ++iSlot)
        {
            int j = s->before();
            if (j >= 0)
            {
                for ( ; j <= s->after(); j++)
                {
                    CharInfo *c = charinfo(j);
                    if (c->before() == -1 || iSlot < c->before()) c->before(iSlot);
                    if (c->after() < iSlot) c->after(iSlot);
                }
            }
            s->index(iSlot);

            if (s->isBase())
            {
                clusterMin = currpos.x;
                currpos = s->finalise(this, font, &currpos, &bbox, &cMin, 0, &clusterMin);
                if (ls)
                    ls->sibling(s);
                ls = s;
            }
        }
    }
    if (iStart == m_first && iEnd == m_last) m_advance = currpos;
}

#ifndef DISABLE_TRACING
void Segment::logSegment(gr_encform enc, const void* pStart, size_t nChars) const
{
    if (XmlTraceLog::get().active())
    {
        if (pStart)
        {
            XmlTraceLog::get().openElement(ElementText);
            XmlTraceLog::get().addAttribute(AttrEncoding, enc);
            XmlTraceLog::get().addAttribute(AttrLength, nChars);
            switch (enc)
            {
            case gr_utf8:
                XmlTraceLog::get().writeText(
                    reinterpret_cast<const char *>(pStart));
                break;
            case gr_utf16:
                for (size_t j = 0; j < nChars; ++j)
                {
                    uint32 code = reinterpret_cast<const uint16 *>(pStart)[j];
                    if (code >= 0xD800 && code <= 0xDBFF) // high surrogate
                    {
                        j++;
                        // append low surrogate
                        code = (code << 16) + reinterpret_cast<const uint16 *>(pStart)[j];
                    }
                    else if (code >= 0xDC00 && code <= 0xDFFF)
                    {
                        XmlTraceLog::get().warning("Unexpected low surrogate %x at %d", code, j);
                    }
                    XmlTraceLog::get().writeUnicode(code);
                }
                break;
            case gr_utf32:
                for (size_t j = 0; j < nChars; ++j)
                {
                    XmlTraceLog::get().writeUnicode(
                        reinterpret_cast<const uint32 *>(pStart)[j]);
                }
                break;
            }
            XmlTraceLog::get().closeElement(ElementText);
        }
        logSegment();
    }
}

void Segment::logSegment() const
{
    if (XmlTraceLog::get().active())
    {
        XmlTraceLog::get().openElement(ElementSegment);
        XmlTraceLog::get().addAttribute(AttrLength, slotCount());
        XmlTraceLog::get().addAttribute(AttrAdvanceX, advance().x);
        XmlTraceLog::get().addAttribute(AttrAdvanceY, advance().y);
        for (Slot *i = m_first; i; i = i->next())
        {
            XmlTraceLog::get().openElement(ElementSlot);
            XmlTraceLog::get().addAttribute(AttrGlyphId, i->gid());
            XmlTraceLog::get().addAttribute(AttrX, i->origin().x);
            XmlTraceLog::get().addAttribute(AttrY, i->origin().y);
            XmlTraceLog::get().addAttribute(AttrBefore, i->before());
            XmlTraceLog::get().addAttribute(AttrAfter, i->after());
            XmlTraceLog::get().closeElement(ElementSlot);
        }
        XmlTraceLog::get().closeElement(ElementSegment);
    }
}

#endif

template <typename utf_iter>
inline void process_utf_data(Segment & seg, const Face & face, const int fid, utf_iter c, size_t n_chars)
{
	const Cmap    & cmap = face.cmap();
	int slotid = 0;

	const typename utf_iter::codeunit_type * const base = c;
	for (; n_chars; --n_chars, ++c, ++slotid)
	{
		const uint32 usv = *c;
		uint16 gid = cmap[usv];
		if (!gid)	gid = face.findPseudo(usv);
		seg.appendSlot(slotid, usv, gid, fid, c - base);
	}
}

void Segment::read_text(const Face *face, const Features* pFeats/*must not be NULL*/, gr_encform enc, const void* pStart, size_t nChars)
{
	assert(face);
	assert(pFeats);

	switch (enc)
	{
	case gr_utf8:	process_utf_data(*this, *face, addFeatures(*pFeats), utf8::const_iterator(pStart), nChars); break;
	case gr_utf16:	process_utf_data(*this, *face, addFeatures(*pFeats), utf16::const_iterator(pStart), nChars); break;
	case gr_utf32:	process_utf_data(*this, *face, addFeatures(*pFeats), utf32::const_iterator(pStart), nChars); break;
	}
}

void Segment::prepare_pos(const Font * /*font*/)
{
    // copy key changeable metrics into slot (if any);
}

void Segment::finalise(const Font *font)
{
    positionSlots(font);
}

void Segment::justify(Slot *pSlot, const Font *font, float width, GR_MAYBE_UNUSED justFlags flags, Slot *pFirst, Slot *pLast)
{
    Slot *pEnd = pSlot;
    Slot *s, *end;
    int numBase = 0;
    float currWidth = 0.;
    float scale = font ? font->scale() : 1.0;
    float base;

    if (!pFirst) pFirst = pSlot;
    base = pFirst->origin().x / scale;
    width = width / scale;
    end = pLast ? pLast->next() : NULL;

    for (s = pFirst; s != end; s=s->next())
    {
        float w = s->origin().x / scale + s->advance() - base;
        if (w > currWidth) currWidth = w;
        pEnd = s;
        if (!s->attachedTo())       // what about trailing whitespace?
            numBase++;
    }
    if (pLast)
        while (s)
        {
            pEnd = s;
            s = s->next();
        }
    else
        pLast = pEnd;
        
    if (!numBase) return;

    Slot *oldFirst = m_first;
    Slot *oldLast = m_last;
    // add line end contextuals to linked list
    m_first = pSlot;
    m_last = pEnd;
    // process the various silf justification stuff returning updated currwidth

    // now fallback to spreading the remaining space among all the bases
    float nShift = (width - currWidth) / (numBase - 1);
    for (s = pFirst->nextSibling(); s != end; s = s->nextSibling())
        s->just(nShift + s->just());
    positionSlots(font, pSlot, pEnd);

    m_first = oldFirst;
    m_last = oldLast;
    // dump line end contextual markers
}

Slot *resolveExplicit(int level, int dir, Slot *s, int nNest = 0);
void resolveWeak(int baseLevel, Slot *s);
void resolveNeutrals(int baseLevel, Slot *s);
void resolveImplicit(Slot *s, Segment *seg, uint8 aMirror);
void resolveWhitespace(int baseLevel, Segment *seg, uint8 aBidi, Slot *s);
Slot *resolveOrder(Slot * & s, const bool reordered, const int level = 0);

void Segment::bidiPass(uint8 aBidi, int paradir, uint8 aMirror)
{
	if (slotCount() == 0)
		return;

    Slot *s;
    int baseLevel = paradir ? 1 : 0;
    unsigned int bmask = 0;
    for (s = first(); s; s = s->next())
    {
        unsigned int bAttr = glyphAttr(s->gid(), aBidi);
        s->setBidiClass((bAttr <= 16) * bAttr);
        bmask |= (1 << s->getBidiClass());
        s->setBidiLevel(baseLevel);
    }
    if (bmask & (paradir ? 0x92 : 0x9C))
    {
        if (bmask & 0xF800)
            resolveExplicit(baseLevel, 0, first(), 0);
        if (bmask & 0x10178)
            resolveWeak(baseLevel, first());
        if (bmask & 0x161)
            resolveNeutrals(baseLevel, first());
        resolveImplicit(first(), this, aMirror);
        resolveWhitespace(baseLevel, this, aBidi, last());
        s = resolveOrder(s = first(), baseLevel);
        first(s); last(s->prev());
        s->prev()->next(0); s->prev(0);
    }
    else if (!(dir() & 4) && baseLevel && aMirror)
    {
        for (s = first(); s; s = s->next())
        {
            unsigned short g = glyphAttr(s->gid(), aMirror);
            if (g) s->setGlyph(this, g);
        }
    }
}

