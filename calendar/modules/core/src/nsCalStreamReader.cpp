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


#include "jdefines.h"

#include <unistring.h>
#include "nsCalStreamReader.h"
#include "prprty.h"
#include "tmbevent.h"
#include "nscal.h"
#include "jlog.h"
#include "keyword.h"
#include "icalsrdr.h"

#include "nspr.h"

//---------------------------------------------------------------------

nsCalStreamReader::nsCalStreamReader()
{
    m_Reader = 0;
    m_OutCalendars = 0;
    m_Thread = 0;
    m_CallerData = 0;
    m_bParseStarted = FALSE;
    m_bParseFinished = FALSE;
}

//---------------------------------------------------------------------

nsCalStreamReader::~nsCalStreamReader()
{
}

//---------------------------------------------------------------------

nsCalStreamReader::nsCalStreamReader(nsCapiCallbackReader * reader, 
                           JulianPtrArray * outCalendars,
                           PRThread * thread, void* condVar)
{
    m_Reader = reader;
    m_OutCalendars = outCalendars;
    m_Thread = thread;
    m_CallerData = condVar;
    m_bParseStarted = FALSE;
    m_bParseFinished = FALSE;
}

//---------------------------------------------------------------------

JulianUtility::MimeEncoding
nsCalStreamReader::stringToEncodingType(UnicodeString & propVal)
{
    if (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_s7bit) == 0)
    {
        return JulianUtility::MimeEncoding_7bit;
    }
    else if (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sQUOTED_PRINTABLE) == 0)
    {
        return JulianUtility::MimeEncoding_QuotedPrintable;
    }
    else if (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sBase64) == 0)
    {
        return JulianUtility::MimeEncoding_Base64;
    }
    else
        return JulianUtility::MimeEncoding_7bit;
}

//---------------------------------------------------------------------

void nsCalStreamReader::ParseCalendars()
{
    nsCalStreamReader::ParseCalendars((ICalReader *) m_Reader, m_OutCalendars);
}

//---------------------------------------------------------------------

void nsCalStreamReader::ParseCalendars(ICalReader * reader, 
                                  JulianPtrArray * outCalendars)
{
    if (outCalendars == 0)
        return;

    JulianPtrArray * parameters = new JulianPtrArray();
   
    // TODO: this needs to be changed to capireader later
    PR_ASSERT(parameters != 0);
    if (parameters != 0)
    {
        JLog * log = 0;
        ErrorCode status = ZERO_ERROR;
        UnicodeString strLine, propName, propVal;
        JulianUtility::MimeEncoding encoding = JulianUtility::MimeEncoding_7bit;

        nsCapiCallbackReader * cr = (nsCapiCallbackReader *) reader;

        while(TRUE)
        {
            cr->readFullLine(strLine, status);
            ICalProperty::Trim(strLine);

#if TESTING_ITIPRIG
            if (FALSE) TRACE("\t--Parser: line (size = %d) = ---%s---\r\n", 
                strLine.size(), strLine.toCString(""));       
#endif
            if (FAILURE(status) && strLine.size() == 0)
            {
                //PR_Notify((PRMonitor *) cr->getMonitor());
                if (cr->isFinished())
                {
                    break;
                }
#if TESTING_ITIPRIG
                if (FALSE) TRACE("\t--jParser: yielding\r\n");
#endif
                PR_Sleep(PR_INTERVAL_NO_WAIT);
               //break;
            }

            ICalProperty::parsePropertyLine(strLine, propName,
                propVal, parameters);
                                
#if TESTING_ITIPRIG
            if (TRUE) TRACE("\t--Parser: propName = --%s--, propVal = --%s--,paramSize = %d\r\n",
                propName.toCString(""), propVal.toCString(""), parameters->GetSize());
#endif
            if ((propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sBEGIN) == 0) &&
                (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVCALENDAR) == 0))
            {
                // parse an NSCalendar, add it to outCalendars                    
                NSCalendar * cal = new NSCalendar(log);
                UnicodeString fileName;
                cal->parse(reader, fileName, encoding);
                outCalendars->Add(cal);
            }
            else if (propName.compareIgnoreCase(
                JulianKeyword::Instance()->ms_sCONTENT_TRANSFER_ENCODING) == 0)
            {
                ICalProperty::Trim(propVal);
                encoding = stringToEncodingType(propVal);
                cr->setEncoding(encoding);
            }
            ICalProperty::deleteICalParameterVector(parameters);
            parameters->RemoveAll();
    
            //PR_ExitMonitor((PRMonitor *)cr->getMonitor());
        }

        ICalProperty::deleteICalParameterVector(parameters);
        parameters->RemoveAll();
        delete parameters; parameters = 0;
        setParseFinished();
    }    
}
//---------------------------------------------------------------------

void main_CalStreamReader(void* p)
{
    /*
     * We've been started. We must wait until our parent thread
     * signals to begin working...
     */
    nsCalStreamReader * pStreamReader = (nsCalStreamReader *) p;
    PRMonitor * pMon = (PRMonitor*)pStreamReader->getCallerData();
    PR_EnterMonitor(pMon);  /* wait until the other thread releases this */
    PR_ExitMonitor(pMon);   /* now that we've got it, let both threads run */

    /*
     * We only enter the Monitor when the parent has exited it. So,
     * our parent thread has signaled us to begin the work at hand.
     */
    pStreamReader->ParseCalendars();

    /*
     * The parent will try to re-enter the monitor when it is waiting for
     * this thread to complete parsing the calendars. 
     */
    PR_EnterMonitor(pMon);
    pStreamReader->setParseFinished();
    PR_Notify(pMon);
    PR_ExitMonitor(pMon);

    /*
     * We're done.
     */
#if 0
    PR_ProcessExit(0);
#endif
}

//---------------------------------------------------------------------



