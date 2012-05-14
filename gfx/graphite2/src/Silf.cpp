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
#include "inc/debug.h"
#include "inc/Endian.h"
#include "inc/Silf.h"
#include "inc/Segment.h"
#include "inc/Rule.h"


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


bool Silf::readGraphite(const byte * const silf_start, size_t lSilf, const Face& face, uint32 version)
{
    const byte * p = silf_start,
    		   * const silf_end = p + lSilf;

    if (version >= 0x00030000)
    {
        if (lSilf < 28)		{ releaseBuffers(); return false; }
        be::skip<int32>(p);	   // ruleVersion
        be::skip<uint16>(p,2); // passOffset & pseudosOffset
    }
    else if (lSilf < 20) 	{ releaseBuffers(); return false; }
    be::skip<uint16>(p);  // maxGlyphID
    be::skip<int16>(p,2); // extra ascent & descent
    m_numPasses = be::read<uint8>(p);
    m_sPass     = be::read<uint8>(p);
    m_pPass     = be::read<uint8>(p);
    m_jPass     = be::read<uint8>(p);
    m_bPass     = be::read<uint8>(p);
    m_flags     = be::read<uint8>(p);
    be::skip<uint8>(p,2); //  max{Pre,Post}Context.
    m_aPseudo   = be::read<uint8>(p);
    m_aBreak    = be::read<uint8>(p);
    m_aBidi     = be::read<uint8>(p);
    m_aMirror   = be::read<uint8>(p);
    be::skip<byte>(p);     // skip reserved stuff

    // Read Justification levels.
    m_numJusts  = be::read<uint8>(p);
    if (p + m_numJusts * 8 >= silf_end)  { releaseBuffers(); return false; }
    m_justs = gralloc<Justinfo>(m_numJusts);
    for (uint8 i = 0; i < m_numJusts; i++)
    {
        ::new(m_justs + i) Justinfo(p[0], p[1], p[2], p[3]);
        be::skip<byte>(p,8);
    }

    if (p + sizeof(uint16) + sizeof(uint8)*8 >= silf_end) { releaseBuffers(); return false; }
    m_aLig      = be::read<uint16>(p);
    m_aUser     = be::read<uint8>(p);
    m_iMaxComp  = be::read<uint8>(p);
    be::skip<byte>(p,5); 						// direction and 4 reserved bytes
    be::skip<uint16>(p, be::read<uint8>(p)); 	// don't need critical features yet
    be::skip<byte>(p);							// reserved
    if (p >= silf_end)   { releaseBuffers(); return false; }
    be::skip<uint32>(p, be::read<uint8>(p));	// don't use scriptTag array.
    be::skip<uint16>(p); // lbGID
    if (p >= silf_end)   { releaseBuffers(); return false; }
    const byte * o_passes = p,
               * const passes_start = silf_start + be::read<uint32>(p);

    if (m_numPasses > 128 || passes_start >= silf_end
    	|| m_pPass < m_sPass
    	|| m_jPass < m_pPass
    	|| (m_bPass != 0xFF && (m_bPass < m_jPass || m_bPass > m_numPasses))
    	|| m_aLig > 127) {
        releaseBuffers(); return false;
    }
    be::skip<uint32>(p, m_numPasses);
    if (p + sizeof(uint16) >= passes_start)  { releaseBuffers(); return false; }
    m_numPseudo = be::read<uint16>(p);
    be::skip<uint16>(p, 3);	// searchPseudo, pseudoSelector, pseudoShift
    if (p + m_numPseudo*(sizeof(uint32) + sizeof(uint16)) >= passes_start) {
        releaseBuffers(); return false;
    }
    m_pseudos = new Pseudo[m_numPseudo];
    for (int i = 0; i < m_numPseudo; i++)
    {
        m_pseudos[i].uid = be::read<uint32>(p);
        m_pseudos[i].gid = be::read<uint16>(p);
    }

    const size_t clen = readClassMap(p, passes_start - p, version);
    if (clen == 0 || p + clen > passes_start)  { releaseBuffers(); return false; }

    m_passes = new Pass[m_numPasses];
    for (size_t i = 0; i < m_numPasses; ++i)
    {
        const byte * const pass_start = silf_start + be::read<uint32>(o_passes),
        		   * const pass_end = silf_start + be::peek<uint32>(o_passes);
        if (pass_start > pass_end || pass_end > silf_end) {
        	releaseBuffers(); return false;
        }

        m_passes[i].init(this);
        if (!m_passes[i].readPass(pass_start, pass_end - pass_start, pass_start - silf_start, face))
        {
        	releaseBuffers();
        	return false;
        }
    }
    return true;
}

template<typename T> inline uint32 Silf::readClassOffsets(const byte *&p, size_t data_len)
{
	const T cls_off = 2*sizeof(uint16) + sizeof(T)*(m_nClass+1);
	const uint32 max_off = (be::peek<T>(p + sizeof(T)*m_nClass) - cls_off)/sizeof(uint16);
	// Check that the last+1 offset is less than or equal to the class map length.
	if (be::peek<T>(p) != cls_off || max_off > (data_len - cls_off)/sizeof(uint16))
		return 0;

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
	if (data_len < sizeof(uint16)*2)	return 0;

	m_nClass  = be::read<uint16>(p);
	m_nLinear = be::read<uint16>(p);

	// Check that numLinear < numClass,
	// that there is at least enough data for numClasses offsets.
	if (m_nLinear > m_nClass
	 || (m_nClass + 1) * (version >= 0x00040000 ? sizeof(uint32) : sizeof(uint16))> (data_len - 4))
		return 0;

    
    uint32 max_off;
    if (version >= 0x00040000)
        max_off = readClassOffsets<uint32>(p, data_len);
    else
        max_off = readClassOffsets<uint16>(p, data_len);

    if (max_off == 0) return 0;

	// Check the linear offsets are sane, these must be monotonically increasing.
	for (const uint32 *o = m_classOffsets, * const o_end = o + m_nLinear; o != o_end; ++o)
		if (o[0] > o[1])
			return 0;

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
			return 0;
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
        	const uint16 * p = min + (-2 & ((max-min)/2));
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

#if !defined GRAPHITE2_NTRACING
    if (dbgout)
    {
    	char version[64];
    	sprintf(version, "%d.%d.%d",
    			GR2_VERSION_MAJOR, GR2_VERSION_MINOR, GR2_VERSION_BUGFIX);
    	*dbgout << json::object
    				<< "version"	<< version
    				<< "passes"		<< json::array;
    }
#endif

    for (size_t i = firstPass; i < lastPass; ++i)
    {
    	// bidi and mirroring
        if (i == m_bPass)
        {
#if !defined GRAPHITE2_NTRACING
        	if (dbgout)
        	{
        		*dbgout << json::item << json::object
        					<< "id"		<< -1
        					<< "slots"	<< json::array;
        		seg->positionSlots(0);
        		for(Slot * s = seg->first(); s; s = s->next())
        			*dbgout		<< dslot(seg, s);
        		*dbgout			<< json::close
        					<< "rules"	<< json::array << json::close
        					<< json::close;
        	}
#endif

        	if (!(seg->dir() & 2))
            	seg->bidiPass(m_aBidi, seg->dir() & 1, m_aMirror);
        	else if (m_aMirror)
            {
                Slot * s;
                for (s = seg->first(); s; s = s->next())
                {
                    unsigned short g = seg->glyphAttr(s->gid(), m_aMirror);
                    if (g && (!(seg->dir() & 4) || !seg->glyphAttr(s->gid(), m_aMirror + 1)))
                        s->setGlyph(seg, g);
                }
            }
        }

#if !defined GRAPHITE2_NTRACING
    	if (dbgout)
    	{
    		*dbgout << json::item << json::object
    					<< "id"		<< i+1
    					<< "slots"	<< json::array;
    		seg->positionSlots(0);
    		for(Slot * s = seg->first(); s; s = s->next())
    			*dbgout		<< dslot(seg, s);
    		*dbgout			<< json::close;
    	}
#endif

        // test whether to reorder, prepare for positioning
        m_passes[i].runGraphite(m, fsm);
        // only subsitution passes can change segment length, cached subsegments are short for their text
        if (m.status() != vm::Machine::finished
        	|| (i < m_pPass && (seg->slotCount() > initSize * MAX_SEG_GROWTH_FACTOR
            || (seg->slotCount() && seg->slotCount() * MAX_SEG_GROWTH_FACTOR < initSize))))
            return false;
    }
#if !defined GRAPHITE2_NTRACING
	if (dbgout)
	{
		*dbgout 			<< json::item
							<< json::close // Close up the passes array
				<< "output" << json::array;
		for(Slot * s = seg->first(); s; s = s->next())
			*dbgout		<< dslot(seg, s);
		seg->finalise(0);					// Call this here to fix up charinfo back indexes.
		*dbgout			<< json::close
				<< "advance" << seg->advance()
				<< "chars"	 << json::array;
		for(size_t i = 0, n = seg->charInfoCount(); i != n; ++i)
			*dbgout 	<< json::flat << *seg->charinfo(i);
		*dbgout			<< json::close	// Close up the chars array
					<< json::close;		// Clsoe up the segment object
	}
#endif
    return true;
}
