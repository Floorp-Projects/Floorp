/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil c-basic-offset: 2 -*- 
 * 
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the 'NPL'); you may not use this file except in 
 * compliance with the NPL.  You may obtain a copy of the NPL at 
 * http://www.mozilla.org/NPL/ 
 * 
 * Software distributed under the NPL is distributed on an 'AS IS' basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL 
 * for the specific language governing rights and limitations under the 
 * NPL. 
 * 
 * The Initial Developer of this code under the NPL is Netscape 
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights 
 * Reserved. 
 */
/* 
 * jlogitr.h
 * John Sun
 * 8/17/98 6:16:29 PM
 */

#ifndef __JULIANLOGITERATOR_H_
#define __JULIANLOGITERATOR_H_

#include "ptrarray.h"
#include "jlogvctr.h"
#include "nscalutilexp.h"

class NS_CAL_UTIL JulianLogIterator
{
private:
    /*-----------------------------
    ** MEMBERS
    **---------------------------*/
    JulianPtrArray * m_LogToIterateOver;
    JulianLogErrorVector::ECompType m_iComponentType;
    t_bool m_bValid;
    t_int32 m_iIndex;

    /*-----------------------------
    ** PRIVATE METHODS
    **---------------------------*/
    
    JulianLogIterator();
    JulianLogIterator(JulianPtrArray * toIterate, JulianLogErrorVector::ECompType iComponentType,
        t_bool bValid);
    
    JulianLogErrorVector * findNextElement(t_int32 startIndex);
public:

    
    static JulianLogIterator * createIterator(JulianPtrArray * toIterate,
        JulianLogErrorVector::ECompType iComponentType, t_bool bValid);

    /**
     * Do this to create iterator of VEVENT log messages
      t_int32 i;
      JulianLogErrorVector * evtErrVctr = 0;
      JulianLogError * error = 0;
      if (log != 0)
      {
         JulianLogIterator * itr = log->createIterator((t_int32) ICalComponent::ICAL_COMPONENT_VEVENT)
         for (evtErrVctr = itr->firstElement(); evtErrVctr != 0; evtErrVctr = itr->nextElement())
         {
              if (evtErrVctr->GetErrors() != 0)
              {    
                    for (i = 0; i < evtErrVctr->GetErrors()->GetSize(); i++)
                    {
                          error = (JulianLogError *) errVctr->GetErrors()->GetAt(i);
                          // do what you want.    
                    }
              }
         }
     */
    JulianLogErrorVector * firstElement();
    JulianLogErrorVector * nextElement();
};

#endif /* __JULIANLOGITERATOR_H_ */

