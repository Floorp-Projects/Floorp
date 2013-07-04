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
#include "inc/Segment.h"
#include "inc/Slot.h"
#include "inc/Silf.h"
#include "inc/CharInfo.h"
#include "inc/Rule.h"


using namespace graphite2;

Slot::Slot() :
    m_next(NULL), m_prev(NULL),
    m_glyphid(0), m_realglyphid(0), m_original(0), m_before(0), m_after(0),
    m_index(0), m_parent(NULL), m_child(NULL), m_sibling(NULL),
    m_position(0, 0), m_shift(0, 0), m_advance(-1, -1),
    m_attach(0, 0), m_with(0, 0), m_just(0.),
    m_flags(0), m_attLevel(0), m_bidiCls(0), m_bidiLevel(0), m_justs(NULL)
    // Do not set m_userAttr since it is set *before* new is called since this
    // is used as a positional new to reset the GrSlot
{
}

// take care, this does not copy any of the GrSlot pointer fields
void Slot::set(const Slot & orig, int charOffset, size_t numUserAttr, size_t justLevels)
{
    // leave m_next and m_prev unchanged
    m_glyphid = orig.m_glyphid;
    m_realglyphid = orig.m_realglyphid;
    m_original = orig.m_original + charOffset;
    m_before = orig.m_before + charOffset;
    m_after = orig.m_after + charOffset;
    m_parent = NULL;
    m_child = NULL;
    m_sibling = NULL;
    m_position = orig.m_position;
    m_shift = orig.m_shift;
    m_advance = orig.m_advance;
    m_attach = orig.m_attach;
    m_with = orig.m_with;
    m_flags = orig.m_flags;
    m_attLevel = orig.m_attLevel;
    m_bidiCls = orig.m_bidiCls;
    m_bidiLevel = orig.m_bidiLevel;
    if (m_userAttr && orig.m_userAttr)
    {
        memcpy(m_userAttr, orig.m_userAttr, numUserAttr * sizeof(*m_userAttr));
    }
    if (m_justs && orig.m_justs)
    {
        memcpy(m_justs, orig.m_justs, SlotJustify::size_of(justLevels));
    }
}

void Slot::update(int /*numGrSlots*/, int numCharInfo, Position &relpos)
{
    m_before += numCharInfo;
    m_after += numCharInfo;
    m_position = m_position + relpos;
}

Position Slot::finalise(const Segment *seg, const Font *font, Position & base, Rect & bbox, uint8 attrLevel, float & clusterMin)
{
    if (attrLevel && m_attLevel > attrLevel) return Position(0, 0);
    float scale = 1.0;
    Position shift(m_shift.x * ((seg->dir() & 1) * -2 + 1) + m_just, m_shift.y);
    float tAdvance = m_advance.x + m_just;
    const GlyphFace * glyphFace = seg->getFace()->glyphs().glyphSafe(glyph());
    if (font)
    {
        scale = font->scale();
        shift *= scale;
        if (font->isHinted() && glyphFace)
            tAdvance = (m_advance.x - glyphFace->theAdvance().x + m_just) * scale + font->advance(m_glyphid);
        else
            tAdvance *= scale;
    }    
    Position res;

    m_position = base + shift;
    if (!m_parent)
    {
        res = base + Position(tAdvance, m_advance.y * scale);
        clusterMin = base.x;
    }
    else
    {
        float tAdv;
        m_position += (m_attach - m_with) * scale;
        tAdv = m_advance.x >= 0.5 ? m_position.x + tAdvance - shift.x : 0.f;
        res = Position(tAdv, 0);
        if ((m_advance.x >= 0.5 || m_position.x < 0) && m_position.x < clusterMin) clusterMin = m_position.x;
    }

    if (glyphFace)
    {
        Rect ourBbox = glyphFace->theBBox() * scale + m_position;
        bbox = bbox.widen(ourBbox);
    }

    if (m_child && m_child != this && m_child->attachedTo() == this)
    {
        Position tRes = m_child->finalise(seg, font, m_position, bbox, attrLevel, clusterMin);
        if ((!m_parent || m_advance.x >= 0.5) && tRes.x > res.x) res = tRes;
    }

    if (m_parent && m_sibling && m_sibling != this && m_sibling->attachedTo() == m_parent)
    {
        Position tRes = m_sibling->finalise(seg, font, base, bbox, attrLevel, clusterMin);
        if (tRes.x > res.x) res = tRes;
    }
    
    if (!m_parent && clusterMin < base.x)
    {
        Position adj = Position(base.x - clusterMin, 0.);
        res += adj;
        m_position += adj;
        if (m_child) m_child->floodShift(adj);
    }
    return res;
}

int32 Slot::clusterMetric(const Segment *seg, uint8 metric, uint8 attrLevel)
{
    Position base;
    Rect bbox = seg->theGlyphBBoxTemporary(glyph());
    float clusterMin = 0.;
    Position res = finalise(seg, NULL, base, bbox, attrLevel, clusterMin);

    switch (metrics(metric))
    {
    case kgmetLsb :
        return static_cast<uint32>(bbox.bl.x);
    case kgmetRsb :
        return static_cast<uint32>(res.x - bbox.tr.x);
    case kgmetBbTop :
        return static_cast<uint32>(bbox.tr.y);
    case kgmetBbBottom :
        return static_cast<uint32>(bbox.bl.y);
    case kgmetBbLeft :
        return static_cast<uint32>(bbox.bl.x);
    case kgmetBbRight :
        return static_cast<uint32>(bbox.tr.x);
    case kgmetBbWidth :
        return static_cast<uint32>(bbox.tr.x - bbox.bl.x);
    case kgmetBbHeight :
        return static_cast<uint32>(bbox.tr.y - bbox.bl.y);
    case kgmetAdvWidth :
        return static_cast<uint32>(res.x);
    case kgmetAdvHeight :
        return static_cast<uint32>(res.y);
    default :
        return 0;
    }
}

int Slot::getAttr(const Segment *seg, attrCode ind, uint8 subindex) const
{
    if (!this) return 0;
    if (ind == gr_slatUserDefnV1)
    {
        ind = gr_slatUserDefn;
        subindex = 0;
    }
    else if (ind >= gr_slatJStretch && ind < gr_slatJStretch + 20 && ind != gr_slatJWidth)
    {
        int indx = ind - gr_slatJStretch;
        return getJustify(seg, indx / 5, indx % 5);
    }

    switch (ind)
    {
    case gr_slatAdvX :		return int(m_advance.x);
    case gr_slatAdvY :		return int(m_advance.y);
    case gr_slatAttTo :		return m_parent ? 1 : 0;
    case gr_slatAttX :		return int(m_attach.x);
    case gr_slatAttY :  	return int(m_attach.y);
    case gr_slatAttXOff :
    case gr_slatAttYOff :	return 0;
    case gr_slatAttWithX :  return int(m_with.x);
    case gr_slatAttWithY :  return int(m_with.y);
    case gr_slatAttWithXOff:
    case gr_slatAttWithYOff:return 0;
    case gr_slatAttLevel :	return m_attLevel;
    case gr_slatBreak :		return seg->charinfo(m_original)->breakWeight();
    case gr_slatCompRef : 	return 0;
    case gr_slatDir :		return seg->dir();
    case gr_slatInsert :	return isInsertBefore();
    case gr_slatPosX :		return int(m_position.x); // but need to calculate it
    case gr_slatPosY :		return int(m_position.y);
    case gr_slatShiftX :	return int(m_shift.x);
    case gr_slatShiftY :	return int(m_shift.y);
    case gr_slatMeasureSol:	return -1; // err what's this?
    case gr_slatMeasureEol: return -1;
    case gr_slatJWidth:     return m_just;
    case gr_slatUserDefn :	return m_userAttr[subindex];
    case gr_slatSegSplit :  return seg->charinfo(m_original)->flags() & 3;
    default :				return 0;
    }
}

void Slot::setAttr(Segment *seg, attrCode ind, uint8 subindex, int16 value, const SlotMap & map)
{
    if (!this) return;
    if (ind == gr_slatUserDefnV1)
    {
        ind = gr_slatUserDefn;
        subindex = 0;
    }
    else if (ind >= gr_slatJStretch && ind < gr_slatJStretch + 20 && ind != gr_slatJWidth)
    {
        int indx = ind - gr_slatJStretch;
        return setJustify(seg, indx / 5, indx % 5, value);
    }

    switch (ind)
    {
    case gr_slatAdvX :	m_advance.x = value; break;
    case gr_slatAdvY :	m_advance.y = value; break;
    case gr_slatAttTo :
    {
        const uint16 idx = uint16(value);
        if (idx < map.size() && map[idx])
        {
            Slot *other = map[idx];
            if (other == this) break;
            if (m_parent) m_parent->removeChild(this);
            if (other->child(this))
            {
                attachTo(other);
                if (((seg->dir() & 1) != 0) ^ (idx > subindex))
                    m_with = Position(advance(), 0);
                else        // normal match to previous root
                    m_attach = Position(other->advance(), 0);
            }
        }
        break;
    }
    case gr_slatAttX :			m_attach.x = value; break;
    case gr_slatAttY :			m_attach.y = value; break;
    case gr_slatAttXOff :
    case gr_slatAttYOff :		break;
    case gr_slatAttWithX :		m_with.x = value; break;
    case gr_slatAttWithY :		m_with.y = value; break;
    case gr_slatAttWithXOff :
    case gr_slatAttWithYOff :	break;
    case gr_slatAttLevel :
        m_attLevel = byte(value);
        break;
    case gr_slatBreak :
        seg->charinfo(m_original)->breakWeight(value);
        break;
    case gr_slatCompRef :	break;      // not sure what to do here
    case gr_slatDir :		break;  // read only
    case gr_slatInsert :
        markInsertBefore(value? true : false);
        break;
    case gr_slatPosX :		break; // can't set these here
    case gr_slatPosY :		break;
    case gr_slatShiftX :	m_shift.x = value; break;
    case gr_slatShiftY :    m_shift.y = value; break;
    case gr_slatMeasureSol :	break;
    case gr_slatMeasureEol :	break;
    case gr_slatJWidth :	just(value); break;
    case gr_slatSegSplit :  seg->charinfo(m_original)->addflags(value & 3); break;
    case gr_slatUserDefn :  m_userAttr[subindex] = value; break;
    default :
    	break;
    }
}

int Slot::getJustify(const Segment *seg, uint8 level, uint8 subindex) const
{
    if (level && level >= seg->silf()->numJustLevels()) return 0;

    if (m_justs)
        return m_justs->values[level * SlotJustify::NUMJUSTPARAMS + subindex];

    if (level >= seg->silf()->numJustLevels()) return 0;
    Justinfo *jAttrs = seg->silf()->justAttrs() + level;

    switch (subindex) {
        case 0 : return seg->glyphAttr(gid(), jAttrs->attrStretch());
        case 1 : return seg->glyphAttr(gid(), jAttrs->attrShrink());
        case 2 : return seg->glyphAttr(gid(), jAttrs->attrStep());
        case 3 : return seg->glyphAttr(gid(), jAttrs->attrWeight());
        case 4 : return 0;      // not been set yet, so clearly 0
        default: return 0;
    }
}

void Slot::setJustify(Segment *seg, uint8 level, uint8 subindex, int16 value)
{
    if (level && level >= seg->silf()->numJustLevels()) return;
    if (!m_justs)
    {
        SlotJustify *j = seg->newJustify();
        j->LoadSlot(this, seg);
        m_justs = j;
    }
    m_justs->values[level * SlotJustify::NUMJUSTPARAMS + subindex] = value;
}

bool Slot::child(Slot *ap)
{
    if (this == ap) return false;
    else if (ap == m_child) return true;
    else if (!m_child)
        m_child = ap;
    else
        return m_child->sibling(ap);
    return true;
}

bool Slot::sibling(Slot *ap)
{
    if (this == ap) return false;
    else if (ap == m_sibling) return true;
    else if (!m_sibling || !ap)
        m_sibling = ap;
    else
        return m_sibling->sibling(ap);
    return true;
}

bool Slot::removeChild(Slot *ap)
{
    if (this == ap || !m_child) return false;
    else if (ap == m_child)
    {
        Slot *nSibling = m_child->nextSibling();
        m_child->sibling(NULL);
        m_child = nSibling;
        return true;
    }
    else
        return m_child->removeSibling(ap);
    return true;
}

bool Slot::removeSibling(Slot *ap)
{
    if (this == ap || !m_sibling) return false;
    else if (ap == m_sibling)
    {
        m_sibling = m_sibling->nextSibling();
        return true;
    }
    else
        return m_sibling->removeSibling(ap);
    return true;
}

void Slot::setGlyph(Segment *seg, uint16 glyphid, const GlyphFace * theGlyph)
{
    m_glyphid = glyphid;
    if (!theGlyph)
    {
        theGlyph = seg->getFace()->glyphs().glyphSafe(glyphid);
        if (!theGlyph)
        {
            m_realglyphid = 0;
            m_advance = Position(0.,0.);
            return;
        }
    }
    m_realglyphid = theGlyph->attrs()[seg->silf()->aPseudo()];
    const GlyphFace *aGlyph = theGlyph;
    if (m_realglyphid)
    {
        aGlyph = seg->getFace()->glyphs().glyphSafe(m_realglyphid);
        if (!aGlyph) aGlyph = theGlyph;
    }
    m_advance = Position(aGlyph->theAdvance().x, 0.);
    if (seg->silf()->aPassBits())
        seg->mergePassBits(theGlyph->attrs()[seg->silf()->aPassBits()]);
}

void Slot::floodShift(Position adj)
{
    m_position += adj;
    if (m_child) m_child->floodShift(adj);
    if (m_sibling) m_sibling->floodShift(adj);
}

void SlotJustify::LoadSlot(const Slot *s, const Segment *seg)
{
    for (int i = seg->silf()->numJustLevels() - 1; i >= 0; --i)
    {
        Justinfo *justs = seg->silf()->justAttrs() + i;
        int16 *v = values + i * NUMJUSTPARAMS;
        v[0] = seg->glyphAttr(s->gid(), justs->attrStretch());
        v[1] = seg->glyphAttr(s->gid(), justs->attrShrink());
        v[2] = seg->glyphAttr(s->gid(), justs->attrStep());
        v[3] = seg->glyphAttr(s->gid(), justs->attrWeight());
    }
}
