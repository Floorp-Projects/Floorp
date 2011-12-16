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
#include <cstdlib>
#include "graphite2/Segment.h"
#include "Endian.h"
#include "Silf.h"
#include "XmlTraceLog.h"
#include "Segment.h"
#include "Rule.h"


using namespace graphite2;

Silf::Silf() throw()
: m_passes(0), m_pseudos(0), m_classOffsets(0), m_classData(0), m_justs(0),
  m_numPasses(0), m_sPass(0), m_pPass(0), m_jPass(0), m_bPass(0), m_flags(0),
  m_aBreak(0), m_aUser(0), m_iMaxComp(0),
  m_aLig(0), m_numPseudo(0), m_nClass(0), m_nLinear(0)
{
}

Silf::~Silf() throw()
{
    releaseBuffers();
}

void Silf::releaseBuffers() throw()
{
    delete [] m_passes;
    delete [] m_pseudos;
    free(m_classOffsets);
    free(m_classData);
    free(m_justs);
    m_passes= 0;
    m_pseudos = 0;
    m_classOffsets = 0;
    m_classData = 0;
    m_justs = 0;
}


bool Silf::readGraphite(void* pSilf, size_t lSilf, const Face& face, uint32 version)
{
    const byte *p = (byte *)pSilf;
    const byte * const eSilf = p + lSilf;
    uint32 *pPasses;
#ifndef DISABLE_TRACING
    XmlTraceLog::get().openElement(ElementSilfSub);
#endif
    if (version >= 0x00030000)
    {
#ifndef DISABLE_TRACING
        if (XmlTraceLog::get().active())
        {
            XmlTraceLog::get().addAttribute(AttrMajor, be::peek<uint16>(p));
            XmlTraceLog::get().addAttribute(AttrMinor, be::peek<uint16>(p+sizeof(uint16)));
        }
#endif
        if (lSilf < 27) { releaseBuffers(); return false; }
        p += 8;
    }
    else if (lSilf < 19) { releaseBuffers(); return false; }
    p += 2;     // maxGlyphID
    p += 4;     // extra ascent/descent
    m_numPasses = uint8(*p++);
#ifndef DISABLE_TRACING
    XmlTraceLog::get().addAttribute(AttrNumPasses, m_numPasses);
#endif
    if (m_numPasses > 128)
        return false;
    m_passes = new Pass[m_numPasses];
    m_sPass = uint8(*p++);
#ifndef DISABLE_TRACING
    XmlTraceLog::get().addAttribute(AttrSubPass, m_sPass);
#endif
    m_pPass = uint8(*p++);
#ifndef DISABLE_TRACING
    XmlTraceLog::get().addAttribute(AttrPosPass, m_pPass);
#endif
    if (m_pPass < m_sPass) {
        releaseBuffers();
        return false;
    }
    m_jPass = uint8(*p++);
#ifndef DISABLE_TRACING
    XmlTraceLog::get().addAttribute(AttrJustPass, m_jPass);
#endif
    if (m_jPass < m_pPass) {
        releaseBuffers();
        return false;
    }
    m_bPass = uint8(*p++);     // when do we reorder?
#ifndef DISABLE_TRACING
    XmlTraceLog::get().addAttribute(AttrBidiPass, m_bPass);
#endif
    if (m_bPass != 0xFF && (m_bPass < m_jPass || m_bPass > m_numPasses)) {
        releaseBuffers();
        return false;
    }
    m_flags = uint8(*p++);
    p += 2;     // ignore line end contextuals for now
    m_aPseudo = uint8(*p++);
    m_aBreak = uint8(*p++);
#ifndef DISABLE_TRACING
    XmlTraceLog::get().addAttribute(AttrBreakWeight, m_aBreak);
    XmlTraceLog::get().addAttribute(AttrDirectionality, *p);
#endif
    m_aBidi = uint8(*p++);
    m_aMirror = uint8(*p++);
    p += 1;     // skip reserved stuff
#ifndef DISABLE_TRACING
    XmlTraceLog::get().addAttribute(AttrNumJustLevels, *p);
#endif
    m_numJusts = uint8(*p++);
    m_justs = gralloc<Justinfo>(m_numJusts);
    for (uint8 i = 0; i < m_numJusts; i++)
    {
        ::new(m_justs + i) Justinfo(p[0], p[1], p[2], p[3]);
        p += 8;
    }
//    p += uint8(*p) * 8 + 1;     // ignore justification for now
    if (p + 9 >= eSilf) { releaseBuffers(); return false; }
    m_aLig = be::read<uint16>(p);
#ifndef DISABLE_TRACING
    XmlTraceLog::get().addAttribute(AttrLigComp, *p);
#endif
    if (m_aLig > 127) {
        releaseBuffers();
        return false;
    }
    m_aUser = uint8(*p++);
#ifndef DISABLE_TRACING
    XmlTraceLog::get().addAttribute(AttrUserDefn, m_aUser);
#endif
    m_iMaxComp = uint8(*p++);
#ifndef DISABLE_TRACING
    XmlTraceLog::get().addAttribute(AttrNumLigComp, m_iMaxComp);
#endif
    p += 5;     // skip direction and reserved
#ifndef DISABLE_TRACING
    XmlTraceLog::get().addAttribute(AttrNumCritFeatures, *p);
#endif
    p += uint8(*p) * 2 + 1;        // don't need critical features yet
    p++;        // reserved
    if (p >= eSilf) 
    {
        releaseBuffers();
        return false;
    }
#ifndef DISABLE_TRACING
    XmlTraceLog::get().addAttribute(AttrNumScripts, *p);
#endif
    p += uint8(*p) * 4 + 1;        // skip scripts
    p += 2;     // skip lbGID
    
    if (p + 4 * (m_numPasses + 1) + 6 >= eSilf) 
    {
        releaseBuffers(); 
        return false;
    }
    pPasses = (uint32 *)p;
    p += 4 * (m_numPasses + 1);
    m_numPseudo = be::read<uint16>(p);
#ifndef DISABLE_TRACING
    XmlTraceLog::get().addAttribute(AttrNumPseudo, m_numPseudo);
#endif
    p += 6;
    if (p + m_numPseudo * 6 >= eSilf) 
    {
        releaseBuffers();
        return false;
    }
    m_pseudos = new Pseudo[m_numPseudo];
    for (int i = 0; i < m_numPseudo; i++)
    {
        m_pseudos[i].uid = be::read<uint32>(p);
        m_pseudos[i].gid = be::read<uint16>(p);
#ifndef DISABLE_TRACING
        XmlTraceLog::get().openElement(ElementPseudo);
        XmlTraceLog::get().addAttribute(AttrIndex, i);
        XmlTraceLog::get().addAttribute(AttrGlyphId, m_pseudos[i].uid);
        XmlTraceLog::get().writeUnicode(m_pseudos[i].uid);
        XmlTraceLog::get().closeElement(ElementPseudo);
#endif
    }
    if (p >= eSilf) 
    {
        releaseBuffers();
        return false;
    }

    int clen = readClassMap(p, be::swap<uint32>(*pPasses) - (p - (byte *)pSilf), version);
    if (clen < 0) {
        releaseBuffers();
        return false;
    }
    p += clen;

    for (size_t i = 0; i < m_numPasses; ++i)
    {
        uint32 pOffset = be::swap<uint32>(pPasses[i]);
        uint32 pEnd = be::swap<uint32>(pPasses[i + 1]);
        if ((uint8 *)pSilf + pEnd > eSilf || pOffset > pEnd)
        {
            releaseBuffers();
            return false;
        }
        m_passes[i].init(this);
#ifndef DISABLE_TRACING
        if (XmlTraceLog::get().active())
        {
            XmlTraceLog::get().openElement(ElementPass);
            XmlTraceLog::get().addAttribute(AttrPassId, i);
        }
#endif
        if (!m_passes[i].readPass((char *)pSilf + pOffset, pEnd - pOffset, pOffset, face))
        {
#ifndef DISABLE_TRACING
            XmlTraceLog::get().closeElement(ElementPass);
#endif
            {
        releaseBuffers();
        return false;
            }
        }
#ifndef DISABLE_TRACING
        XmlTraceLog::get().closeElement(ElementPass);
#endif
    }
#ifndef DISABLE_TRACING
    XmlTraceLog::get().closeElement(ElementSilfSub);
#endif
    return true;
}

template<typename T> inline uint32 Silf::readClassOffsets(const byte *&p, size_t data_len)
{
	const T cls_off = 2*sizeof(uint16) + sizeof(T)*(m_nClass+1);
	const uint32 max_off = (be::peek<T>(p + sizeof(T)*m_nClass) - cls_off)/sizeof(uint16);
	// Check that the last+1 offset is less than or equal to the class map length.
	if (be::peek<T>(p) != cls_off || max_off > (data_len - cls_off)/sizeof(uint16))
		return -1;

	// Read in all the offsets.
	m_classOffsets = gralloc<uint32>(m_nClass+1);
	for (uint32 * o = m_classOffsets, * const o_end = o + m_nClass + 1; o != o_end; ++o)
	{
		*o = (be::read<T>(p) - cls_off)/sizeof(uint16);
		if (*o > max_off)
			return 0;
	}
    return max_off;
}

size_t Silf::readClassMap(const byte *p, size_t data_len, uint32 version)
{
	if (data_len < sizeof(uint16)*2)	return -1;

	m_nClass  = be::read<uint16>(p);
	m_nLinear = be::read<uint16>(p);

	// Check that numLinear < numClass,
	// that there is at least enough data for numClasses offsets.
	if (m_nLinear > m_nClass
	 || (m_nClass + 1) * (version >= 0x00040000 ? sizeof(uint32) : sizeof(uint16))> (data_len - 4))
		return -1;

    
    uint32 max_off;
    if (version >= 0x00040000)
        max_off = readClassOffsets<uint32>(p, data_len);
    else
        max_off = readClassOffsets<uint16>(p, data_len);

    if (max_off == 0) return -1;

	// Check the linear offsets are sane, these must be monotonically increasing.
	for (const uint32 *o = m_classOffsets, * const o_end = o + m_nLinear; o != o_end; ++o)
		if (o[0] > o[1])
			return -1;

	// Fortunately the class data is all uint16s so we can decode these now
    m_classData = gralloc<uint16>(max_off);
    for (uint16 *d = m_classData, * const d_end = d + max_off; d != d_end; ++d)
        *d = be::read<uint16>(p);

	// Check the lookup class invariants for each non-linear class
	for (const uint32 *o = m_classOffsets + m_nLinear, * const o_end = m_classOffsets + m_nClass; o != o_end; ++o)
	{
		const uint16 * lookup = m_classData + *o;
		if (lookup[0] == 0							// A LookupClass with no looks is a suspicious thing ...
		 || lookup[0] > (max_off - *o - 4)/2  	    // numIDs lookup pairs fits within (start of LookupClass' lookups array, max_off]
		 || lookup[3] != lookup[0] - lookup[1])		// rangeShift:	 numIDs  - searchRange
			return -1;
	}

	return max_off;
}

uint16 Silf::findPseudo(uint32 uid) const
{
    for (int i = 0; i < m_numPseudo; i++)
        if (m_pseudos[i].uid == uid) return m_pseudos[i].gid;
    return 0;
}

uint16 Silf::findClassIndex(uint16 cid, uint16 gid) const
{
    if (cid > m_nClass) return -1;

    const uint16 * cls = m_classData + m_classOffsets[cid];
    if (cid < m_nLinear)        // output class being used for input, shouldn't happen
    {
        for (unsigned int i = 0, n = m_classOffsets[cid + 1]; i < n; ++i, ++cls)
            if (*cls == gid) return i;
        return -1;
    }
    else
    {
    	const uint16 *	min = cls + 4,		// lookups array
    				 * 	max = min + cls[0]*2; // lookups aray is numIDs (cls[0]) uint16 pairs long
    	do
        {
        	const uint16 * p = min + (-2U & ((max-min)/2));
        	if 	(p[0] > gid)	max = p;
        	else 				min = p;
        }
        while (max - min > 2);
        return min[0] == gid ? min[1] : -1;
    }
}

uint16 Silf::getClassGlyph(uint16 cid, unsigned int index) const
{
    if (cid > m_nClass) return 0;

    uint32 loc = m_classOffsets[cid];
    if (cid < m_nLinear)
    {
        if (index < m_classOffsets[cid + 1] - loc)
            return m_classData[index + loc];
    }
    else        // input class being used for output. Shouldn't happen
    {
        for (unsigned int i = loc + 4; i < m_classOffsets[cid + 1]; i += 2)
            if (m_classData[i + 1] == index) return m_classData[i];
    }
    return 0;
}

bool Silf::runGraphite(Segment *seg, uint8 firstPass, uint8 lastPass) const
{
    assert(seg != 0);
    SlotMap            map(*seg);
    FiniteStateMachine fsm(map);
    vm::Machine        m(map);
    unsigned int       initSize = seg->slotCount();

    if (lastPass == 0)
    {
        if (firstPass == lastPass)
            return true;
        lastPass = m_numPasses;
    }

    for (size_t i = firstPass; i < lastPass; ++i)
    {
#ifndef DISABLE_TRACING
        if (XmlTraceLog::get().active())
        {
	        XmlTraceLog::get().openElement(ElementRunPass);
	        XmlTraceLog::get().addAttribute(AttrNum, i);
        }
#endif

        // bidi and mirroring
        if (i == m_bPass && !(seg->dir() & 2))
            seg->bidiPass(m_aBidi, seg->dir() & 1, m_aMirror);
        else if (i == m_bPass && m_aMirror)
        {
            Slot * s;
            for (s = seg->first(); s; s = s->next())
            {
                unsigned short g = seg->glyphAttr(s->gid(), m_aMirror);
                if (g && (!(seg->dir() & 4) || !seg->glyphAttr(s->gid(), m_aMirror + 1)))
                    s->setGlyph(seg, g);
            }
        }

        // test whether to reorder, prepare for positioning
        m_passes[i].runGraphite(m, fsm);
#ifndef DISABLE_TRACING
            seg->logSegment();
        if (XmlTraceLog::get().active())
        {
            XmlTraceLog::get().closeElement(ElementRunPass);
        }
#endif
        // only subsitution passes can change segment length, cached subsegments are short for their text
        if (m.status() != vm::Machine::finished
        	|| (i < m_pPass && (seg->slotCount() > initSize * MAX_SEG_GROWTH_FACTOR
                               || (seg->slotCount() && seg->slotCount() * MAX_SEG_GROWTH_FACTOR < initSize))))
            return false;
    }
    return true;
}
