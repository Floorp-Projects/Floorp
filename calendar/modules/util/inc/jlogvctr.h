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
 * jlogvctr.h
 * John Sun
 * 8/17/98 4:43:51 PM
 */

#ifndef __JLOGVECTOR_H_
#define __JLOGVECTOR_H_

#include "jlogerr.h"
#include "nscalutilexp.h"

class NS_CAL_UTIL nsCalLogErrorVector
{
public:

    enum ECompType
    {
        // these better match the ICalComponent::ICAL_COMPONENT enum
        ECompType_VEVENT = 0,
        ECompType_VTODO = 1,
        ECompType_VJOURNAL = 2,
        ECompType_VFREEBUSY = 3,
        ECompType_VTIMEZONE = 4,
        
        ECompType_NSCALENDAR = 5,
        ECompType_XCOMPONENT = 6
    };

private:
    /*-----------------------------
    ** MEMBERS
    **---------------------------*/
    
    ECompType m_ICalComponentType;
    JulianPtrArray * m_ErrorVctr;
    t_bool m_bValidEvent;
    UnicodeString m_UID;
#if 0
    UnicodeString m_RID;
#endif
public:

    /*-----------------------------
    ** CONSTRUCTORS and DESTRUCTORS
    **---------------------------*/

    nsCalLogErrorVector();
    nsCalLogErrorVector(ECompType iICalComponentType);
    ~nsCalLogErrorVector();
    
    /*----------------------------- 
    ** ACCESSORS (GET AND SET) 
    **---------------------------*/
    
    void SetValid(t_bool b) { m_bValidEvent = b; }
    t_bool IsValid() const { return m_bValidEvent; }
    
    void SetComponentType(ECompType iComponentType) { m_ICalComponentType = iComponentType; }
    ECompType GetComponentType() const { return m_ICalComponentType; }

    JulianPtrArray * GetErrors() const { return m_ErrorVctr; }

    const UnicodeString & GetUID() const { return m_UID; }
    void SetUID(UnicodeString & uid) { m_UID = uid; }

#if 0
    void SetRecurrenceID(UnicodeString & rid) { m_RID = rid; }
    const UnicodeString & GetRecurrenceID() const { return m_RID; }
#endif
    /*----------------------------- 
    ** UTILITIES 
    **---------------------------*/ 
    
    void AddError(nsCalLogError * error);
};


#endif /* __JLOGVECTOR_H_ */

