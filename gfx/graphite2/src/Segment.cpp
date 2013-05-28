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
#include "inc/UtfCodec.h"
#include <cstring>
#include <cstdlib>

#include "inc/bits.h"
#include "inc/Segment.h"
#include "graphite2/Font.h"
#include "inc/CharInfo.h"
#include "inc/debug.h"
#include "inc/Slot.h"
#include "inc/Main.h"
#include "inc/CmapCache.h"
#include "graphite2/Segment.h"


using namespace graphite2;

Segment::Segment(unsigned int numchars, const Face* face, uint32 script, int textDir)
: m_freeSlots(NULL),
  m_freeJustifies(NULL),
  m_charinfo(new CharInfo[numchars]),
  m_face(face),
  m_silf(face->chooseSilf(script)),
  m_first(NULL),
  m_last(NULL),
  m_bufSize(numchars + 10),
  m_numGlyphs(numchars),
  m_numCharinfo(numchars),
  m_passBits(m_silf->aPassBits() ? -1 : 0),
  m_defaultOriginal(0),
  m_dir(textDir)
{
    freeSlot(newSlot());
    m_bufSize = log_binary(numchars)+1;
}

Segment::~Segment()
{
    for (SlotRope::iterator i = m_slots.begin(); i != m_slots.end(); ++i)
        free(*i);
    for (AttributeRope::iterator j = m_userAttrs.begin(); j != m_userAttrs.end(); ++j)
        free(*j);
    delete[] m_charinfo;
}

#ifndef GRAPHITE2_NSEGCACHE
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
    m_passBits &= other.passBits();
}
#endif // GRAPHITE2_NSEGCACHE

void Segment::appendSlot(int id, int cid, int gid, int iFeats, size_t coffset)
{
    Slot *aSlot = newSlot();
    
    m_charinfo[id].init(cid);
    m_charinfo[id].feats(iFeats);
    m_charinfo[id].base(coffset);
    const GlyphFace * theGlyph = m_face->glyphs().glyphSafe(gid);
    m_charinfo[id].breakWeight(theGlyph ? theGlyph->attrs()[m_silf->aBreak()] : 0);
    
    aSlot->child(NULL);
    aSlot->setGlyph(this, gid, theGlyph);
    aSlot->originate(id);
    aSlot->before(id);
    aSlot->after(id);
    if (m_last) m_last->next(aSlot);
    aSlot->prev(m_last);
    m_last = aSlot;
    if (!m_first) m_first = aSlot;
    if (theGlyph && m_silf->aPassBits())
        m_passBits &= theGlyph->attrs()[m_silf->aPassBits()] 
                    | (m_silf->numPasses() > 16 ? (theGlyph->attrs()[m_silf->aPassBits() + 1] << 16) : 0);
}

Slot *Segment::newSlot()
{
    if (!m_freeSlots)
    {
        int numUser = m_silf->numUser();
#if !defined GRAPHITE2_NTRACING
        if (m_face->logger()) ++numUser;
#endif
        Slot *newSlots = grzeroalloc<Slot>(m_bufSize);
        int16 *newAttrs = grzeroalloc<int16>(numUser * m_bufSize);
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
    if (aSlot->attachedTo())
        aSlot->attachedTo()->removeChild(aSlot);
    while (aSlot->firstChild())
    {
        aSlot->firstChild()->attachTo(NULL);
        aSlot->removeChild(aSlot->firstChild());
    }
    // reset the slot incase it is reused
    ::new (aSlot) Slot;
    memset(aSlot->userAttrs(), 0, m_silf->numUser() * sizeof(int16));
    // Update generation counter for debug
#if !defined GRAPHITE2_NTRACING
    if (m_face->logger())
        ++aSlot->userAttrs()[m_silf->numUser()];
#endif
    // update next pointer
    if (!m_freeSlots)
        aSlot->next(NULL);
    else
        aSlot->next(m_freeSlots);
    m_freeSlots = aSlot;
}

SlotJustify *Segment::newJustify()
{
    if (!m_freeJustifies)
    {
        const size_t justSize = SlotJustify::size_of(m_silf->numJustLevels());
        byte *justs = grzeroalloc<byte>(justSize * m_bufSize);
        for (int i = m_bufSize - 2; i >= 0; --i)
        {
            SlotJustify *p = reinterpret_cast<SlotJustify *>(justs + justSize * i);
            SlotJustify *next = reinterpret_cast<SlotJustify *>(justs + justSize * (i + 1));
            p->next = next;
        }
        m_freeJustifies = (SlotJustify *)justs;
        m_justifies.push_back(m_freeJustifies);
    }
    SlotJustify *res = m_freeJustifies;
    m_freeJustifies = m_freeJustifies->next;
    res->next = NULL;
    return res;
}

void Segment::freeJustify(SlotJustify *aJustify)
{
    int numJust = m_silf->numJustLevels();
    if (m_silf->numJustLevels() <= 0) numJust = 1;
    aJustify->next = m_freeJustifies;
    memset(aJustify->values, 0, numJust*SlotJustify::NUMJUSTPARAMS*sizeof(int16));
    m_freeJustifies = aJustify;
}

#ifndef GRAPHITE2_NSEGCACHE
void Segment::splice(size_t offset, size_t length, Slot * const startSlot,
                       Slot * endSlot, const Slot * srcSlot,
                       const size_t numGlyphs)
{
    extendLength(numGlyphs - length);
    // remove any extra
    if (numGlyphs < length)
    {
        Slot * end = endSlot->next();
        do
        {
            endSlot = endSlot->prev();
            freeSlot(endSlot->next());
        } while (numGlyphs < --length);
        endSlot->next(end);
        if (end)
            end->prev(endSlot);
    }
    else
    {
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
    }

    endSlot = endSlot->next();
    assert(numGlyphs == length);
    Slot * indexmap[eMaxSpliceSize*3];
    assert(numGlyphs < sizeof indexmap/sizeof *indexmap);
    Slot * slot = startSlot;
    for (uint16 i=0; i < numGlyphs; slot = slot->next(), ++i)
    	indexmap[i] = slot;

    slot = startSlot;
    for (slot=startSlot; slot != endSlot; slot = slot->next(), srcSlot = srcSlot->next())
    {
        slot->set(*srcSlot, offset, m_silf->numUser(), m_silf->numJustLevels());
        if (srcSlot->attachedTo())	slot->attachTo(indexmap[srcSlot->attachedTo()->index()]);
        if (srcSlot->nextSibling())	slot->m_sibling = indexmap[srcSlot->nextSibling()->index()];
        if (srcSlot->firstChild())	slot->m_child = indexmap[srcSlot->firstChild()->index()];
    }
}
#endif // GRAPHITE2_NSEGCACHE

void Segment::linkClusters(Slot *s, Slot * end)
{
	end = end->next();

	for (; s != end && !s->isBase(); s = s->next());
	Slot * ls = s;

	if (m_dir & 1)
	{
		for (; s != end; s = s->next())
		{
			if (!s->isBase())	continue;

			s->sibling(ls);
			ls = s;
		}
	}
	else
	{
		for (; s != end; s = s->next())
		{
			if (!s->isBase())	continue;

			ls->sibling(s);
			ls = s;
		}
	}
}

Position Segment::positionSlots(const Font *font, Slot * iStart, Slot * iEnd)
{
    Position currpos(0., 0.);
    Rect bbox;
    float clusterMin = 0.;

    if (!iStart)	iStart = m_first;
    if (!iEnd)		iEnd   = m_last;

    if (m_dir & 1)
    {
        for (Slot * s = iEnd, * const end = iStart->prev(); s && s != end; s = s->prev())
        {
            if (s->isBase())
                currpos = s->finalise(this, font, currpos, bbox, 0, clusterMin = currpos.x);
        }
    }
    else
    {
        for (Slot * s = iStart, * const end = iEnd->next(); s && s != end; s = s->next())
        {
            if (s->isBase())
                currpos = s->finalise(this, font, currpos, bbox, 0, clusterMin = currpos.x);
        }
    }
    return currpos;
}


void Segment::associateChars()
{
    int i = 0;
    for (Slot * s = m_first; s; s->index(i++), s = s->next())
    {
        int j = s->before();
        if (j < 0)	continue;

        for (const int after = s->after(); j <= after; ++j)
		{
			CharInfo & c = *charinfo(j);
			if (c.before() == -1 || i < c.before()) 	c.before(i);
			if (c.after() < i) 							c.after(i);
		}
    }
}


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
        if (bmask & 0x361)
            resolveNeutrals(baseLevel, first());
        resolveImplicit(first(), this, aMirror);
        resolveWhitespace(baseLevel, this, aBidi, last());
        s = resolveOrder(s = first(), baseLevel != 0);
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

