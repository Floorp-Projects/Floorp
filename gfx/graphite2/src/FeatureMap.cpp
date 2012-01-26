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

#include "inc/Main.h"
#include "inc/Endian.h"
#include "inc/FeatureMap.h"
#include "inc/FeatureVal.h"
#include "graphite2/Font.h"
#include "inc/TtfUtil.h"
#include <cstdlib>
#include "inc/Face.h"


using namespace graphite2;

namespace
{
	static int cmpNameAndFeatures(const void *ap, const void *bp)
	{
		const NameAndFeatureRef & a = *static_cast<const NameAndFeatureRef *>(ap),
								& b = *static_cast<const NameAndFeatureRef *>(bp);
		return (a < b ? -1 : (b < a ? 1 : 0));
	}
}


bool FeatureMap::readFeats(const Face & face)
{
    size_t lFeat;
    const byte *pFeat = face.getTable(TtfUtil::Tag::Feat, &lFeat);
    const byte *pOrig = pFeat;
    uint16 *defVals=0;
    uint32 version;
    if (!pFeat) return true;
    if (lFeat < 12) return false;

    version = be::read<uint32>(pFeat);
    if (version < 0x00010000) return false;
    m_numFeats = be::read<uint16>(pFeat);
    be::skip<uint16>(pFeat);
    be::skip<uint32>(pFeat);
    if (m_numFeats * 16U + 12 > lFeat) { m_numFeats = 0; return false; }		//defensive
    if (!m_numFeats) return true;
    m_feats = new FeatureRef[m_numFeats];
    defVals = gralloc<uint16>(m_numFeats);
    byte currIndex = 0;
    byte currBits = 0;     //to cause overflow on first Feature

    for (int i = 0, ie = m_numFeats; i != ie; i++)
    {
        uint32 name;
        if (version < 0x00020000)
            name = be::read<uint16>(pFeat);
        else
            name = be::read<uint32>(pFeat);
        uint16 numSet = be::read<uint16>(pFeat);

        uint32 offset;
        if (version < 0x00020000)
            offset = be::read<uint32>(pFeat);
        else
        {
            be::skip<uint16>(pFeat);
            offset = be::read<uint32>(pFeat);
        }
        if (offset > lFeat)
        {
            free(defVals);
            return false;
        }
        uint16 flags = be::read<uint16>(pFeat);
        uint16 uiName = be::read<uint16>(pFeat);
        const byte *pSet = pOrig + offset;
        uint16 maxVal = 0;

        if (numSet == 0)
        {
            --m_numFeats;
            continue;
        }

        if (offset + numSet * 4 > lFeat) return false;
        FeatureSetting *uiSet = gralloc<FeatureSetting>(numSet);
        for (int j = 0; j < numSet; j++)
        {
            int16 val = be::read<int16>(pSet);
            if (val > maxVal) maxVal = val;
            if (j == 0) defVals[i] = val;
            uint16 label = be::read<uint16>(pSet);
            new (uiSet + j) FeatureSetting(label, val);
        }
        uint32 mask = 1;
        byte bits = 0;
        for (bits = 0; bits < 32; bits++, mask <<= 1)
        {
            if (mask > maxVal)
            {
                if (bits + currBits > 32)
                {
                    currIndex++;
                    currBits = 0;
                    mask = 2;
                }
                ::new (m_feats + i) FeatureRef (currBits, currIndex,
                                               (mask - 1) << currBits, flags,
                                               name, uiName, numSet, uiSet, &face);
                currBits += bits;
                break;
            }
        }
    }
    m_defaultFeatures = new Features(currIndex + 1, *this);
    m_pNamedFeats = new NameAndFeatureRef[m_numFeats];
    for (int i = 0; i < m_numFeats; i++)
    {
    	m_feats[i].applyValToFeature(defVals[i], *m_defaultFeatures);
        m_pNamedFeats[i] = m_feats+i;
    }
    
    free(defVals);

    qsort(m_pNamedFeats, m_numFeats, sizeof(NameAndFeatureRef), &cmpNameAndFeatures);

    return true;
}

bool SillMap::readFace(const Face & face)
{
    if (!m_FeatureMap.readFeats(face)) return false;
    if (!readSill(face)) return false;
    return true;
}


bool SillMap::readSill(const Face & face)
{
    size_t lSill;
    const byte *pSill = face.getTable(TtfUtil::Tag::Sill, &lSill);
    const byte *pBase = pSill;

    if (!pSill) return true;
    if (lSill < 12) return false;
    if (be::read<uint32>(pSill) != 0x00010000UL) return false;
    m_numLanguages = be::read<uint16>(pSill);
    m_langFeats = new LangFeaturePair[m_numLanguages];
    if (!m_langFeats || !m_FeatureMap.m_numFeats) { m_numLanguages = 0; return true; }        //defensive

    pSill += 6;     // skip the fast search
    if (lSill < m_numLanguages * 8U + 12) return false;

    for (int i = 0; i < m_numLanguages; i++)
    {
        uint32 langid = be::read<uint32>(pSill);
        uint16 numSettings = be::read<uint16>(pSill);
        uint16 offset = be::read<uint16>(pSill);
        if (offset + 8U * numSettings > lSill && numSettings > 0) return false;
        Features* feats = new Features(*m_FeatureMap.m_defaultFeatures);
        const byte *pLSet = pBase + offset;

        for (int j = 0; j < numSettings; j++)
        {
            uint32 name = be::read<uint32>(pLSet);
            uint16 val = be::read<uint16>(pLSet);
            pLSet += 2;
            const FeatureRef* pRef = m_FeatureMap.findFeatureRef(name);
            if (pRef)
                pRef->applyValToFeature(val, *feats);
        }
        //std::pair<uint32, Features *>kvalue = std::pair<uint32, Features *>(langid, feats);
        //m_langMap.insert(kvalue);
        m_langFeats[i].m_lang = langid;
        m_langFeats[i].m_pFeatures = feats;
    }
    return true;
}


Features* SillMap::cloneFeatures(uint32 langname/*0 means default*/) const
{
    if (langname)
    {
        // the number of languages in a font is usually small e.g. 8 in Doulos
        // so this loop is not very expensive
        for (uint16 i = 0; i < m_numLanguages; i++)
        {
            if (m_langFeats[i].m_lang == langname)
                return new Features(*m_langFeats[i].m_pFeatures);
        }
    }
    return new Features (*m_FeatureMap.m_defaultFeatures);
}



const FeatureRef *FeatureMap::findFeatureRef(uint32 name) const
{
    NameAndFeatureRef *it;
    
    for (it = m_pNamedFeats; it < m_pNamedFeats + m_numFeats; ++it)
        if (it->m_name == name)
            return it->m_pFRef;
    return NULL;
}

bool FeatureRef::applyValToFeature(uint16 val, Features & pDest) const 
{ 
    if (val>m_max || !m_pFace)
      return false;
    if (pDest.m_pMap==NULL)
      pDest.m_pMap = &m_pFace->theSill().theFeatureMap();
    else
      if (pDest.m_pMap!=&m_pFace->theSill().theFeatureMap())
        return false;       //incompatible
    pDest.reserve(m_index);
    pDest[m_index] &= ~m_mask;
    pDest[m_index] |= (uint32(val) << m_bits);
    return true;
}

uint16 FeatureRef::getFeatureVal(const Features& feats) const
{ 
  if (m_index < feats.size() && &m_pFace->theSill().theFeatureMap()==feats.m_pMap) 
    return (feats[m_index] & m_mask) >> m_bits; 
  else
    return 0;
}


