/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- 
 * 
 * The contents of this file are subject to the Netscape Public License 
 * Version 1.0 (the "NPL"); you may not use this file except in 
 * compliance with the NPL.  You may obtain a copy of the NPL at 
 * http://www.mozilla.org/NPL/ 
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis, 
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL 
 * for the specific language governing rights and limitations under the 
 * NPL. 
 * 
 * The Initial Developer of this code under the NPL is Netscape 
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights 
 * Reserved. 
 */

#ifndef __NSQQnsCalStreamReader_H_
#define __NSQQnsCalStreamReader_H_

#include "nscapiexport.h"
#include "ptrarray.h"
#include "icalredr.h"
#include "nsCapiCallbackReader.h"
#include "jutility.h"
#include "nspr.h"

/**
 *  This class will be removed later.  It is a hack I created to 
 *  parse the MIME-message from the CS&T server fetch results.
 *  I will try to extract the iCalendar information from it.
 *  I will need to handle multi-threaded parsing somehow as well.
 */
CLASS_EXPORT_CAPI nsCalStreamReader
{
private:
    static nsCalUtility::MimeEncoding stringToEncodingType(UnicodeString & propVal);


    /* dont deallocate these */
    JulianPtrArray * m_OutCalendars;
    nsCapiCallbackReader * m_Reader;
    t_bool m_bParseStarted;
    t_bool m_bParseFinished;
    PRThread * m_Thread;
    void * m_CallerData;

public:
    nsCalStreamReader();
    ~nsCalStreamReader();
    nsCalStreamReader(
        nsCapiCallbackReader * reader, 
        JulianPtrArray * outCalendars,
        PRThread * thread, 
        void* condvar);
    t_bool isParseStarted() const   { return m_bParseStarted; }
    void setParseStarted()          { m_bParseStarted = TRUE; }
    
    t_bool isParseFinished() const  { return m_bParseFinished; }
    void setParseFinished()         { m_bParseFinished = TRUE; }

    nsCapiCallbackReader * getReader() const { return m_Reader; } 
    PRThread * getThread() const    { return m_Thread; }
    void * getCallerData() const    { return m_CallerData; }

    void ParseCalendars(
        ICalReader * reader, 
        JulianPtrArray * outCalendars);

    void ParseCalendars();
};

#ifdef XP_CPLUSPLUS
extern "C"{
#endif

    void NS_CAPIEXPORT main_CalStreamReader(void * jp);

#ifdef XP_CPLUSPLUS
};
#endif

#endif /* __NSQQnsCalStreamReader_H_ */

