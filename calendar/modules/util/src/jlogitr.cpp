/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil c-basic-offset: 2 -*- 
 * 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape 
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
/* 
 * jlogitr.cpp
 * John Sun
 * 8/17/98 6:30:56 PM
 */

#include "stdafx.h"
#include "jdefines.h"
#include <unistring.h>
#include "jlogitr.h"

//---------------------------------------------------------------------
nsCalLogIterator::nsCalLogIterator(JulianPtrArray * toIterate,
                                     nsCalLogErrorVector::ECompType iComponentType,
                                     t_bool bValid):
m_LogToIterateOver(toIterate),
m_iComponentType(iComponentType),
m_bValid(bValid)
{
}
//---------------------------------------------------------------------


nsCalLogIterator::nsCalLogIterator()
{
}

//---------------------------------------------------------------------

nsCalLogIterator * 
nsCalLogIterator::createIterator(JulianPtrArray * toIterate, 
                                  nsCalLogErrorVector::ECompType iComponentType,
                                  t_bool bValid)
{
    if (toIterate == 0)
        return 0;
    else
        return new nsCalLogIterator(toIterate, iComponentType, bValid);
}

//---------------------------------------------------------------------

nsCalLogErrorVector * 
nsCalLogIterator::firstElement()
{
    return findNextElement(0);
}

//---------------------------------------------------------------------

nsCalLogErrorVector * 
nsCalLogIterator::nextElement()
{
    return findNextElement(++m_iIndex);
}

//---------------------------------------------------------------------

nsCalLogErrorVector * 
nsCalLogIterator::findNextElement(t_int32 startIndex)
{
    if (m_LogToIterateOver != 0)
    {
        nsCalLogErrorVector * errVctr = 0;
        t_int32 i;
        for (i = startIndex; i < m_LogToIterateOver->GetSize(); i++)
        {
            errVctr = (nsCalLogErrorVector *) m_LogToIterateOver->GetAt(i);
            if ((errVctr->GetComponentType() == m_iComponentType) &&
                (errVctr->IsValid() == m_bValid))
            {
                m_iIndex = i;
                return errVctr;
            }
        }
    }
    return 0;
}

//---------------------------------------------------------------------




