/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- 
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
 * jparser.h
 * John Sun
 * 4/28/98 10:45:21 AM
 */
#ifndef __JULIANPARSER_H_
#define __JULIANPARSER_H_

#include "ptrarray.h"
#include "icalredr.h"
#include "capiredr.h"
#include "jutility.h"
#include "nspr.h"

/**
 *  This class will be removed later.  It is a hack I created to 
 *  parse the MIME-message from the CS&T server fetch results.
 *  I will try to extract the iCalendar information from it.
 *  I will need to handle multi-threaded parsing somehow as well.
 */
class JulianParser
{
private:
    /*char * m_Buffer;*/

    static nsCalUtility::MimeEncoding stringToEncodingType(UnicodeString & propVal);

    JulianPtrArray * m_OutCalendars;
    ICalCAPIReader * m_Reader;
    t_bool m_bParseStarted;
    t_bool m_bParseFinished;
    PRThread * m_Thread;

public:
    JulianParser();
    JulianParser(ICalCAPIReader * reader, JulianPtrArray * outCalendars,
        PRThread * thread);
    /*
    void setBuffer(char * buf)
    {
        strcat(m_Buffer, buf);
    }
    */

    t_bool isParseStarted() const { return m_bParseStarted; }
    void setParseStarted() { m_bParseStarted = TRUE; }
    
    t_bool isParseFinished() const { return m_bParseFinished; }
    void setParseFinished() { m_bParseFinished = TRUE; }

    ICalCAPIReader * getReader() const { return m_Reader; } 
    PRThread * getThread() const { return m_Thread; }

    void ParseCalendars(ICalReader * reader, 
        JulianPtrArray * outCalendars);

    void ParseCalendars();
    
};

#ifdef XP_CPLUSPLUS
extern "C"{
#endif

    void jparser_ParseCalendarsZero(void * jp, void *);

#ifdef XP_CPLUSPLUS
}
#endif

#endif /* __JULIANPARSER_H_ */

