/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2
-*- 
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

/****************************************************************
***  Local CAPI
***  Steve Mansour
*****************************************************************/
#include "jdefines.h"
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#include "capi.h"               /* public capi file */
#include "privcapi.h"           /* private capi implementation */


/* julian includes */
#include "nscal.h"
#include "icalcomp.h"
#include "icalredr.h"
#include "icalfrdr.h"
#include "ptrarray.h"
#include "jlog.h"

#ifdef XP_PC
#include "io.h"
#endif

/****************************************************************
***    UTILITY ROUTINES
***    In case we want to actually put this code into a dll,
***    this section should contain all the external calls.
*****************************************************************/
size_t CAPI_strlen(const char * s)
{
    return strlen(s);
}

char *CAPI_mktemp( char *psTemplate )
{
// XXX: Port this
#ifdef XP_PC
  return _mktemp(psTemplate);
#else
  return (nsnull);
#endif
}

int CAPI_access( const char *path, int mode )
{
#ifdef XP_PC
    return _access(path,mode);
#else
  return (0);
#endif
}

void *CAPI_malloc(size_t i)
{
    return malloc( i );
}

void CAPI_free(void *p)
{
    if (0 != p)
        free(p);
}

int CAPI_unlink( const char *filename )
{
// XXX: Port this
#ifdef XP_PC
    int iStatus = _unlink(filename);
    if (iStatus < 0)
    {
        int iError = errno;
        //TRACE( _sys_errlist[errno] );
        return iError;
    }
#endif
    return 0;
}

char *CAPI_strdup(const char* s)
{
    if (0 != s)
    {
        char* p = (char *) malloc( 1 + strlen(s) );
        if (0 == p)
            return 0;
        return strcpy(p,s);
    }
    return 0;
}

FILE *CAPI_fopen(const char *psFile, const char *psFlags)
{
    return fopen(psFile,psFlags);
}

size_t CAPI_fread( void *buffer, size_t size, size_t count, FILE *stream )
{
    return fread( buffer, size, count, stream );
}

size_t CAPI_fwrite( const void *buffer, size_t size, size_t count, FILE *stream )
{
    return fwrite( buffer, size, count, stream );
}

int CAPI_fclose(FILE *pFile )
{
    return fclose(pFile);
}

void *CAPI_memcpy( void *dest, const void *src, size_t count )
{
    return memcpy(dest,src,count);
}

void *CAPI_memset( void *dest, int c, size_t count )
{
    return memset(dest,c,count);
}

/************************************************************************/

/**
 * This is a useful piece of code but it's in the wrong
 * place. Given a path, it ensures that the path exists, creating
 * whatever needs to be created.
 * @return 0 on success
 *         file creation errors otherwise.
 */
PRInt32 CAPI_EnsureUserPath( JulianString& sPath )
{
  JulianString sTmp;
  PRInt32 i;
  nsCurlParser::ConvertToFileChars(sPath);
  for (i = 0; -1 != (i = sPath.Strpbrk(i,"/\\")); i++ )
  {
    sTmp = sPath.Left(i);
    if (PR_SUCCESS != PR_Access(sTmp.GetBuffer(), PR_ACCESS_EXISTS))
    {
      /*
       * Try to create it...
       */
      if (PR_SUCCESS != PR_MkDir(sTmp.GetBuffer(),PR_RDWR))
      {
        PRInt32 iError = PR_GetError();
        return iError;
      }
    }
  }

  /*
   * OK, the path was there or it has been created. Now make
   * sure we can write to it.
   */
  if (PR_SUCCESS != PR_Access(sPath.GetBuffer(), PR_ACCESS_WRITE_OK))
  {
      PRInt32 iError = PR_GetError();
      return iError;
  }

  return 0;
}



/****************************************************************
***    MIME / ICAL ASSEMBLY FUNCTIONS
*****************************************************************/
UnicodeString & createMimeStartHeader(UnicodeString & u)
{
    u = "MIME-Version: 1.0\r\n";
    u += "Content-Type: multipart/text\r\n";
    u += " boundary=\"5ZY4HSUIYKHTPFPN7Q30ROE94YXWQNBI\"\r\n";
    u += "Content-Transfer-Encoding: 7bit\r\n";
    u += "\r\n";
    u += "This is a multipart message in MIME format containing iCalendar data.\r\n";
    return u;
}

UnicodeString & appendSeperator(UnicodeString & u)
{
    u += "--5ZY4HSUIYKHTPFPN7Q30ROE94YXWQNBI\r\n";
    return u;
}

UnicodeString & appendMultipartMessageHeader(UnicodeString & u, UnicodeString & filename)
{
    appendSeperator(u);
    u += "Content-Type: text/calendar\r\n";
    u += "Content-Disposition: attachment; filename=\"";
    u += filename;
    u += "\"\r\n";
    u += "Content-Transfer-Encoding: 7-bit\r\n\r\n";
    return u;
}

UnicodeString & appendBeginVCalendar(UnicodeString & u)
{
    u += "BEGIN:VCALENDAR\r\n";
    return u;
}

UnicodeString & appendEndVCalendar(UnicodeString & u)
{
    u += "END:VCALENDAR\r\n\r\n";
    return u;
}


/****************************************************************
***    CAPI FUNCTIONS
*****************************************************************/

/**
 *  return the implementation specific information
 */
CAPIStatus CAPI_Capabilities( 
    const char** ppsVal,        /* o: a string describing the capabilities  */
    const char* psHost,         /* i: server host  */
    long lFlags )               /* i: bit flags (none at this time; set to 0)  */
{
    static char* tpsCap = "Vendor: Netscape\n\
Version: 1.0\n\
ServerVersion: 1.0 (localhost)\n\
CAPIVersion: 1.0\n\
fanout: no\n\
atomicTransactions: no\n";

    *ppsVal = tpsCap;

    return CAPI_ERR_OK;
}

/**
 *  Delete an event from a list of calendar handles
 */
#define DEL_EXIT(x) {iRetStatus = x; goto DELEVENT_EXIT;}

CAPIStatus CAPI_DeleteEvent( 
    CAPISession s,              /* i: login session handle  */
    CAPIHandle* pHList,         /* i: list of CAPIHandles for delete  */
    int iHandleCount,           /* i: number of valid handles in ppH  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    char* psUID,                /* i: UID of the event to delete  */
    char* dtRecurrenceID,       /* i: recurrence-id, NULL means ignore  */
    int iModifier)              /* i: one of CAPI_THISINSTANCE, CAPI_THISANDPRIOR, 
                                      CAPI_THISANDFUTURE only valid if recurrence-id 
                                      is non-NULL  */
{

    /*
     * Read the entire calendar into an NSCalendar : pCal
     * Read the supplied event to delete into another ns calendar : pCalDel
     * Search for the event in pCalDel in pCal.  
     * If found delete it from pCal then save pCal
     */
    PCAPISESSION *pSession = (PCAPISESSION*)s;
    PCAPIHANDLE* pHandle;
    NSCalendar *pCal = 0;
    CAPIStatus iRetStatus = CAPI_ERR_OK;
    ICalReader * pRedr = 0;
    JLog* pLog = 0;
    UnicodeString uFilename;
    t_bool bWriteStatus;
    size_t iHits;
    ErrorCode status = ZERO_ERROR;
    VEvent *pEvent;
    JulianPtrArray* pEventList = 0;

    if (0 == pSession)
        return CAPI_ERR2_SESSION_BAD;
    if (0 == pHList)
        return CAPI_ERR2_HANDLE_BAD;

    for (int i = 0; i < iHandleCount; i++)
    {
        pHandle = (PCAPIHANDLE*) pHList[i];
        if (0 == pHandle)
            DEL_EXIT(CAPI_ERR2_HANDLE_BAD)

        /*
         *  create an NSCalendar, pCal
         */
        pCal = new NSCalendar(pLog);
        if (pCal == 0)
            DEL_EXIT(CAPI_ERR1_INTERNAL)

        /*
         *  Try to load pHandle->psFile in pCal.
         *  If the file doesn't exist, we don't have to create it and
         *  load it, because we're deleting events anyway. If there's nothing
         *  there to start with, there's nothing to delete.
         */
        if (CAPI_access(pHandle->psFile, 00 /*F_OK*/))
            continue;
        pRedr = (ICalReader *) new ICalFileReader(pHandle->psFile, status);
        if (pRedr == 0)
            DEL_EXIT(CAPI_ERR1_INTERNAL)
        if (FAILURE(status))
            DEL_EXIT(CAPI_ERR1_INTERNAL)

        /*
         *  Load original datastore...
         */
        uFilename = pHandle->psFile;
        pCal->parse(pRedr, uFilename);
        delete pRedr; pRedr = 0;

        /*
         *  Try to find the event
         */
        pEventList = pCal->getEvents();
        if (0 != pEventList)
        {
            pEvent = 0;
            iHits = 0;
            for (int j = 0; j < pEventList->GetSize(); j++)
            {
                pEvent = (VEvent *) pEventList->GetAt(j);
                if (0 == pEvent)
                    continue;
                if (0 == (pEvent->getUID().compare(psUID)))
                {
                    /*
                     * need to check recurrence id here...
                     */
                    iHits++;
                    pEventList->RemoveAt(j);
                    delete pEvent;
                    pEvent = 0;
                    --j;
                }
            }
        }


        /*
         *  Write the updated calendar store out to disk...
         */
        if (iHits > 0)
        {
            pCal->export(pHandle->psFile, bWriteStatus);
            if (!bWriteStatus)
                DEL_EXIT(CAPI_ERR1_INTERNAL);
        }

        delete pCal;
        pCal = 0;

    }


DELEVENT_EXIT:
    if (0 != pCal)
        delete pCal;
    if (0 != pRedr)
        delete pRedr;

    return iRetStatus;
}

/**
 *  Destroy a list of handles.
 */
CAPIStatus CAPI_DestroyHandles( 
    CAPISession s,              /* i: login session handle  */
    CAPIHandle* pHList,         /* i: array of handles to destroy  */
    int iHandleCount,           /* i: number of valid handles in ppH  */
    long lFlags)                /* i: bit flags (none at this time; set to 0)  */
{
    PCAPIHANDLE* pHandle;
    int i;
    for (i = 0; i < iHandleCount; i++ )
    {
        pHandle = (PCAPIHANDLE *)pHList[i];
        if (pHandle != 0)
        {
            if (pHandle->pCurl)
              delete pHandle->pCurl;
            CAPI_free(pHandle->psFile);
            CAPI_free(pHandle);
        }
    }
    return CAPI_ERR_OK;
}

/**
 *  Destroy a list of streams
 */
CAPIStatus CAPI_DestroyStreams( 
    CAPISession s,              /* i: login session handle  */
    CAPIStream* pS,             /* i: array of streams to destroy  */
    int iCount,                 /* i: number of valid handles in ppH  */
    long lFlags)                /* i: bit flags (none at this time; set to 0)  */
{
    PCAPIStream** paS = (PCAPIStream **) pS;
    for (int i = 0; i < iCount; i++)
      CAPI_free(paS[i]);
    return CAPI_ERR_OK;
}

/**
 * return events that have alarms set to go off in the supplied time range.
 */
CAPIStatus CAPI_FetchEventsByAlarmRange( 
    CAPISession s,              /* i: login session handle  */
    CAPIHandle* pH,             /* i: list of CAPIHandles for Fetch  */
    int iHandleCount,           /* i: number of valid handles in ppH  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    char* dStart,               /* i: range start time, ex: "19980704T080000Z"  */
    char* dEnd,                 /* i: range end time, ex: "19980704T180000Z"  */
    char** ppsPropList,         /* i: list of properties to return in events  */
    int iPropCount,             /* i: number of properties in *ppsPropList  */
    CAPIStream stream)          /* i: stream to which solution set will be written  */
{
    return CAPI_ERR1_IMPLENTATION;
}

#define FBID_EXIT(x) {iRetStatus = x; goto FBID_EXIT;}
#define FBRANGE_EXIT(x) {iRetStatus = x; goto FBRANGE_EXIT;}
/**
 *  Fetch an event from the local data store that matches the supplied UID, 
 *  recurrence id, and modifier.
 */
CAPIStatus CAPI_FetchEventsByID( 
    CAPISession s,              /* i: login session handle  */
    CAPIHandle h,               /* i: calendar from which to fetch events  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    char* psUID,                /* i: UID of the event to fetch  */
    char* dtRecurrenceID,       /* i: recurrence-id, NULL means ignore */
    int iModifier,              /* i: one of CAPI_THISINSTANCE,  */
                                /*    CAPI_THISANDPRIOR, CAPI_THISANDFUTURE  */
                                /*    only valid if recurrence-id is non-NULL  */
    char** ppsPropList,         /* i: list of properties returned in events   */
    int iPropCount,             /* i: number of properties in the list  */
    CAPIStream str)             /* i: stream to which solution set will be written  */
{
    PCAPISESSION *pSession = (PCAPISESSION*)s;
    PCAPIHANDLE* pHandle = (PCAPIHANDLE*)h;
    PCAPIStream* pStream = (PCAPIStream*)str;

    size_t iLen;
    size_t iAmountHandled;
    char *p;
    CAPIStatus iStatus;
    CAPIStatus iRetStatus = CAPI_ERR_OK;
    char *pBuf = 0;
    size_t iBufSize = BUFSIZ;
    NSCalendar * pCal = 0;
    ICalReader * pRedr = 0;
    JulianPtrArray * evtVctr = 0;
    ICalComponent * ic = 0;
    JLog * pLog = 0;
    UnicodeString usEvt;
    UnicodeString u;
    UnicodeString uRid;
    UnicodeString uModifier;
    UnicodeString strFmt;
    ErrorCode status = ZERO_ERROR;
    int32 i = 0;
    UnicodeString uFilename = "calendar.ics";
    char * pCopy = 0;

    if (0 == pSession)
        return CAPI_ERR2_SESSION_BAD;
    if (0 == pHandle)
        return CAPI_ERR2_HANDLE_BAD;
    if (0 == pStream)
        return CAPI_ERR2_STREAM_BAD;
    
    /*
     *  For this trivial implementation, we're going to read
     *  everything in the file and send it to the handler...
     */
    if (0 != pStream->pfnRcvCallback)
    {
        /*
         *  We need an NSCalendar to do in-memory manipulations.
         */
        pCal = new NSCalendar(pLog);
        if (pCal == 0)
            return CAPI_ERR1_INTERNAL;
        evtVctr = new JulianPtrArray();
        if (evtVctr == 0) 
            FBID_EXIT(CAPI_ERR1_INTERNAL);

        /*
         *  Try to load pHandle->psFile in pCal
         *  if file error, return CAPI_ERR1_INTERNAL
         */

        if (CAPI_access(pHandle->psFile, 00 /*F_OK*/))
        {
            FILE *pFile = 0;
            if ( 0 == (pFile =CAPI_fopen(pHandle->psFile,"w")))
                FBID_EXIT(CAPI_ERR1_INTERNAL);

            if (0 != CAPI_fclose(pFile))
                FBID_EXIT(CAPI_ERR1_INTERNAL);
        }
        pRedr = (ICalReader *) new ICalFileReader(pHandle->psFile, status);
        if (pRedr == 0)
                FBID_EXIT(CAPI_ERR1_INTERNAL);
        if (FAILURE(status))
                FBID_EXIT(CAPI_ERR1_INTERNAL);

        /* parse reader */
        uFilename = pHandle->psFile;
        pCal->parse(pRedr, uFilename);

        /*
         * Apply the filter:
         *  JulianPtrArray EventVector
         *  pCal->GetEventsByID(&EventVector, psUID,dtRecurrenceID,iModifier );
         */
        /* TODO: use getEvents(uid, rid, range) */
        
        u = psUID;
        uRid = dtRecurrenceID;
        uModifier = "";
        if (iModifier == CAPI_THISANDPRIOR)
            uModifier = "THISANDPRIOR";
        else if (iModifier == CAPI_THISANDFUTURE)
            uModifier = "THISANDFUTURE";

        //pCal->getEvents(evtVctr, u);
        pCal->getEventsByComponentID(evtVctr, u, uRid, uModifier);

        /*
         *  Stream out initialization ICAL: 
         *     MIME header:
         *     Multipart-mime header:
         *     BEGIN:VCALENDAR
         *     separator
         *     END:VCALENDAR
         */
        u = createMimeStartHeader(u);
        appendMultipartMessageHeader(u, uFilename);
        appendBeginVCalendar(u);

        if (0 == evtVctr->GetSize())
        {
            appendEndVCalendar(u);
            appendSeperator(u);
            pCopy = u.toCString("");
            
            if (pCopy != 0)
            {
                for(p = pCopy, iLen = CAPI_strlen(pCopy);iLen > 0; p += iAmountHandled)
                {
                    iStatus = (*pStream->pfnRcvCallback)(pStream->userDataRcv,p,iLen,&iAmountHandled);
                    if (CAPI_CALLBACK_CONTINUE != iStatus)
                        FBID_EXIT(CAPI_ERR1_CALLBACK);
                    iLen -= iAmountHandled;     
                }
                delete [] pCopy; pCopy = 0;
            }
            else
            {
                 FBID_EXIT(CAPI_ERR1_CALLBACK);
            }
        }

        /*
         * For each event in  EventVector... 
         *     0.  TODO LATER:  filter the event string by the supplied property list
         *     1.  Put an ICAL version of the event into sEvent
         *     
         */
        ICalComponent::makeFormatString(ppsPropList, iPropCount, strFmt);        
        for (i = 0; i < evtVctr->GetSize(); i++)
        {
            ic = (ICalComponent *) evtVctr->GetAt(i);

            usEvt = ic->format(ICalComponent::componentToString(ic->GetType()),
                strFmt, "", FALSE);

            // prepend MIME header to first event
            // append END to last event
            if (i == 0)
            {
                usEvt.insert(0, u);
            }
            if (i == evtVctr->GetSize() - 1)
            {
                appendEndVCalendar(usEvt);
                appendSeperator(usEvt);
            }
            pCopy = usEvt.toCString("");
            if (pCopy == 0)
                FBID_EXIT(CAPI_ERR1_MEMORY);
    
            for(p = pCopy, iLen = CAPI_strlen(pCopy);iLen > 0; p += iAmountHandled)
            {
                iStatus = (*pStream->pfnRcvCallback)(pStream->userDataRcv,p,iLen,&iAmountHandled);
                if (CAPI_CALLBACK_CONTINUE != iStatus)
                    FBID_EXIT(CAPI_ERR1_CALLBACK);
                iLen -= iAmountHandled;     
            }
            delete [] pCopy; pCopy = 0;
        }

        /*
         * Signal the end of transmission...
         */
        (*pStream->pfnRcvCallback)(pStream->userDataRcv,p,0,&iAmountHandled);
       

FBID_EXIT:
        if (0 != pCal)
            delete pCal;
        if (0 != evtVctr)
            delete evtVctr;
        if (0 != pRedr)
            delete pRedr;
        if (0 != pCopy)
            delete [] pCopy;

    }
    return iRetStatus;
}

/**
 * Get a list of all the events that overlap a time range.
 */
CAPIStatus CAPI_FetchEventsByRange( 
    CAPISession s,              /* i: login session handle  */
    CAPIHandle* pH,             /* i: list of CAPIHandles for fetch  */
    int iHandleCount,           /* i: number of valid handles in ppH  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    char* dStart,               /* i: range start time  */
    char* dEnd,                 /* i: range end time  */
    char** ppsPropList,         /* i: list of properties returned in events  */
    int iPropCount,             /* i: number of properties in the list  */
    CAPIStream str)             /* i: stream to which solution set will be written  */
{
    PCAPISESSION *pSession = (PCAPISESSION*)s;
    PCAPIHANDLE* pHandle = (PCAPIHANDLE*)pH;
    PCAPIStream* pStream = (PCAPIStream*)str;

    size_t iLen;
    size_t iAmountHandled;
    char *p;
    int i;
    CAPIStatus iStatus;
    CAPIStatus iRetStatus = CAPI_ERR_OK;
    char *pBuf = 0;
    size_t iBufSize = BUFSIZ;

    /* julian variables */
    NSCalendar * pCal = 0;
    ICalReader * pRedr = 0;
    JulianPtrArray * evtVctr = 0;
    ICalComponent * ic = 0;
    JLog * pLog = 0;
    UnicodeString usEvt;
    UnicodeString usHeader;
    UnicodeString strFmt;
    ErrorCode status = ZERO_ERROR;
    char * pCopy = 0;
    int32 j = 0;
    DateTime start;
    DateTime end;
    UnicodeString uFilename;
    UnicodeString uTemp;

    if (0 == pSession)
        return CAPI_ERR2_SESSION_BAD;
    if (0 == pH)
        return CAPI_ERR2_HANDLE_BAD;
    if (0 == pStream)
        return CAPI_ERR2_STREAM_BAD;
        
    /*
     *  For this trivial implementation, we're going to read
     *  everything in the file and send it to the handler...
     */
    if (0 != pStream->pfnRcvCallback)
    {
        /* 
         * For each handle, fetch events by range, print out event
         * print mime-header if first handle.
         * print mime-multipart stuff if first event.
         */
        for (i = 0; i < iHandleCount; i++)
        {
            /*pCal = 0;
            evtVctr = 0;
            pRedr = 0;
            pCopy = 0;*/
            pHandle = (PCAPIHANDLE *) pH[i];
            if (pHandle == 0)
            {
                return CAPI_ERR2_HANDLE_BAD;
            }

            pCal = new NSCalendar(pLog);
            if (pCal == 0)
                FBRANGE_EXIT(CAPI_ERR1_INTERNAL);

            evtVctr = new JulianPtrArray();
            if (evtVctr == 0)
                FBRANGE_EXIT(CAPI_ERR1_INTERNAL);

            /*
             *  Try to load pHandle->psFile in pCal
             *  if file error, return CAPI_ERR1_INTERNAL
             */

            if (CAPI_access(pHandle->psFile, 00 /*F_OK*/))
            {
                FILE * pFile = 0;
                if ( 0 == (pFile =CAPI_fopen(pHandle->psFile,"w")))
                    FBRANGE_EXIT(CAPI_ERR1_INTERNAL);
                if (0 != CAPI_fclose(pFile))
                    FBRANGE_EXIT(CAPI_ERR1_INTERNAL);
            }
            
            pRedr = (ICalReader *) new ICalFileReader(pHandle->psFile, status);
            if (pRedr == 0)
                FBRANGE_EXIT(CAPI_ERR1_INTERNAL);
            if (FAILURE(status))
                FBRANGE_EXIT(CAPI_ERR1_INTERNAL);

            /* parse reader */
            uFilename = pHandle->psFile;
            pCal->parse(pRedr, uFilename);

            /* apply filter */
            uTemp = dStart;
            start.setTimeString(uTemp);
            uTemp = dEnd;
            end.setTimeString(uTemp);
            pCal->getEventsByRange(evtVctr, start, end);
            
            /*
             *  Stream out initialization ICAL: 
             *  Mime headers, Multipart headers, seperator, BEGIN:VCALENDAR, END:VCALENDAR
             */

            /** 
             * create mime start header if this is the first handle of the list
             */
            if (i == 0)
            {
                usHeader = createMimeStartHeader(usHeader);
            }
            else 
            {
                usHeader = "";
            }
            /**
             * append mime-multipart header and BEGIN:VCALENDAR
             */ 
            appendMultipartMessageHeader(usHeader, uFilename);
            appendBeginVCalendar(usHeader);

            if (0 == evtVctr->GetSize())
            {
                appendEndVCalendar(usHeader);
                if (i == iHandleCount - 1)
                {
                    appendSeperator(usHeader);
                }
                pCopy = usHeader.toCString("");
            
                if (pCopy != 0)
                {
                    for(p = pCopy, iLen = CAPI_strlen(pCopy);iLen > 0; p += iAmountHandled)
                    {
                        iStatus = (*pStream->pfnRcvCallback)(pStream->userDataRcv,p,iLen,&iAmountHandled);                            
                        if (CAPI_CALLBACK_CONTINUE != iStatus)
                            FBRANGE_EXIT(CAPI_ERR1_CALLBACK);
                        iLen -= iAmountHandled;     
                    }
                    delete [] pCopy; pCopy = 0;
                }
                else
                {
                    FBRANGE_EXIT(CAPI_ERR1_CALLBACK);
                }
            }
            else
            {
                ICalComponent::makeFormatString(ppsPropList, iPropCount, strFmt);
                for (j = 0; j < evtVctr->GetSize(); j++)
                {
                    ic = (ICalComponent *) evtVctr->GetAt(j);

                    usEvt = ic->format(ICalComponent::componentToString(ic->GetType()),
                       strFmt, "", FALSE);
           
                    // prepend mime-header to first handle, first event
                    // prepend multipart-header to first event, non-first handle
                    if (j == 0)
                    {
                        if (i == 0)
                        {
                            usEvt.insert(0, usHeader);
                        }
                        else
                        {
                            uTemp = "";
                            appendMultipartMessageHeader(uTemp, uFilename);
                            usEvt.insert(0, uTemp);
                        }
                    }
                    // append END:VCALENDAR to last event.
                    if (j == evtVctr->GetSize() - 1)
                    {
                        appendEndVCalendar(usEvt);
                        if (i == iHandleCount - 1)
                        {
                            appendSeperator(usEvt);
                        }
                    }

                    pCopy = usEvt.toCString("");
                    if (pCopy == 0)
                        FBRANGE_EXIT(CAPI_ERR1_MEMORY);
    
                    for(p = pCopy, iLen = CAPI_strlen(pCopy);iLen > 0; p += iAmountHandled)
                    {
                        iStatus = (*pStream->pfnRcvCallback)(pStream->userDataRcv,p,iLen,&iAmountHandled);
                        if (CAPI_CALLBACK_CONTINUE != iStatus)
                            FBRANGE_EXIT(CAPI_ERR1_CALLBACK);
                        iLen -= iAmountHandled;     
                    }
                    delete [] pCopy; pCopy = 0;
                }
            }

            /*
             *  we're paranoid about a path that doesn't delete something
             */
            if (0 != pCal)
            {
                delete pCal;
                pCal = 0;
            }
            if (0 != evtVctr)
            {
                delete evtVctr;
                evtVctr = 0;
            }
            if (0 != pRedr)
            {
                delete pRedr;
                pRedr = 0;
            }
            if (0 != pCopy)
            {
                delete pCopy;
                pCopy = 0;
            }
        }
    
        /*
         * Signal the end of transmission...
         */
        (*pStream->pfnRcvCallback)(pStream->userDataRcv,p,0,&iAmountHandled);

    }

FBRANGE_EXIT:
    if (0 != pCal)
        delete pCal;
    if (0 != evtVctr)
        delete evtVctr;
    if (0 != pRedr)
        delete pRedr;
    if (0 != pCopy)
        delete pCopy;

    return iRetStatus;
}

/**
 *  Return a handle to a specific calendar. In this implementation,
 *  the "user" should be the name of the file you want to open.
 */
CAPIStatus CAPI_GetHandle( 
    CAPISession s,              /* i: login session handle  */
    char* u,                    /* i: user as defined in Login  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    CAPIHandle* pH)             /* o: handle  */
{
    PCAPISESSION* pSession = (PCAPISESSION *) s;
    PCAPIHANDLE* pHandle = (PCAPIHANDLE *) CAPI_malloc(sizeof(PCAPIHANDLE));
    if (0 == pSession)
        return CAPI_ERR2_SESSION_NULL;
    *pH = 0;
    if (0 == pHandle)
        return CAPI_ERR1_MEMORY;
    
    pHandle->pSession = pSession;
    pHandle->pCurl = new nsCurlParser(u);

    /*
     *  For local capi, the user name is a file name. If it is not fully
     *  qualified, use the Curl in the session to fill in the missing parts.
     */
    *pHandle->pCurl |= *pSession->pCurl;

    char* psLocalFile = pHandle->pCurl->ToLocalFile();
    pHandle->psFile = CAPI_strdup(psLocalFile);
    PR_Free(psLocalFile);
    
    JulianString sPath = pHandle->pCurl->CSIDPath();
    if (NS_OK != CAPI_EnsureUserPath( sPath ))
    {
      // XXX  do some error thing here...
      // we should probably pop up a file dialog and let
      // the user point us to a path to use.
      return 1;
    }

    *pH = pHandle;
    return CAPI_ERR_OK;
}

/**
 *  Close a session, release any associated memory.
 */
CAPIStatus CAPI_Logoff( 
    CAPISession* s,             /* io: session from login  */
    long lFlags)                /* i: bit flags (none at this time; set to 0)  */
{
    if (0 != s)
    {
        PCAPISESSION* pSession = (PCAPISESSION *) *s;
        CAPI_free(pSession->psUser);
        CAPI_free(pSession->psPassword);
        CAPI_free(pSession->psHost);
        delete pSession->pCurl;
        CAPI_free(pSession);
        *s = 0;
        return CAPI_ERR_OK;
    }
    return CAPI_ERR2_SESSION_NULL;
}
    
/**
 *  Open a session.
 */
CAPIStatus CAPI_Logon( 
    const char* psUser,         /* i: Calendar store (and ":extra" information )  */
    const char* psPassword,     /* i: password for sUser  */
    const char* psHost,         /* i: calendar server host (and :port)  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    CAPISession* ps)            /* io: the session  */
{
    PCAPISESSION* pSession = (PCAPISESSION *) CAPI_malloc(sizeof(PCAPISESSION));
    *ps = 0;
    if (0 == pSession)
        return CAPI_ERR1_MEMORY;
    pSession->psUser = CAPI_strdup(psUser);
    pSession->psPassword = CAPI_strdup(psPassword);
    pSession->psHost = CAPI_strdup(psHost);

    pSession->pCurl = new nsCurlParser();
    pSession->pCurl->SetHost(psHost);
    pSession->pCurl->SetCSID(psUser);

    /*
     *  TODO:
     *  find / create the file associated with this user
     */

    *ps = (CAPISession) pSession;
    return CAPI_ERR_OK;
}

/**
 *  Open a session.
 */
CAPIStatus CAPI_LogonCurl( 
    const char* psCurl,         /* i: the calendar url  */
    const char* psPassword,     /* i: password for sUser  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    CAPISession* ps)            /* io: the session  */
{
    PCAPISESSION* pSession = (PCAPISESSION *) CAPI_malloc(sizeof(PCAPISESSION));
    *ps = 0;
    if (0 == pSession)
        return CAPI_ERR1_MEMORY;
    pSession->psPassword = CAPI_strdup(psPassword);

    pSession->pCurl = new nsCurlParser(psCurl);
    pSession->psUser = CAPI_strdup(pSession->pCurl->GetCSID().GetBuffer());
    pSession->psHost = CAPI_strdup(pSession->pCurl->GetHost().GetBuffer());

    /*
     *  TODO:
     *  find / create the file associated with this user
     */

    *ps = (CAPISession) pSession;
    return CAPI_ERR_OK;
}

/**
 *  Set CAPI_Stream values
 *  pOpaqueStream is allocated if it is null.
 */
CAPIStatus CAPI_SetStreamCallbacks ( 
    CAPISession s,                      /* i: login session handle  */
    CAPIStream* pOpaqueStream,          /* io: The stream to modify  */
    CAPICallback pfnSndCallback,        /* i: Snd iCalendar data    */
    void* userDataSnd,                  /* i: a user supplied value */
    CAPICallback pfnRcvCallback,        /* i: Rcv iCalendar data  */
    void* userDataRcv,                  /* i: a user supplied value */
    long lFlags )                       /* i: bit flags (none at this time; set to 0)  */
{
    PCAPISESSION *pSession = (PCAPISESSION*)s;
    PCAPIStream* pRet = (PCAPIStream *)pOpaqueStream;

    if (0 == *pOpaqueStream)
    {
      pRet = (PCAPIStream *) CAPI_malloc(sizeof(PCAPIStream));
      if (0 == pRet)
          return CAPI_ERR1_MEMORY;
      CAPI_memset(pRet,0,sizeof(PCAPIStream));
    }

    pRet->pfnSndCallback    = pfnSndCallback;
    pRet->userDataSnd       = userDataSnd;
    pRet->pfnRcvCallback    = pfnRcvCallback;
    pRet->userDataRcv       = userDataRcv;

    *pOpaqueStream = pRet;

    return CAPI_ERR_OK;
}


#define STOR_EXIT(x) {iRetStatus = x; goto STOREVENT_EXIT;}
/**
 *  Store the supplied event stream into the list of calendar stores pointed to
 *  by the supplied list of handles.
 */
CAPIStatus CAPI_StoreEvent( 
    CAPISession s,                      /* i: login session handle  */
    CAPIHandle* pHList,                 /* i: list of CAPIHandles for store  */
    int iHandleCount,                   /* i: number of valid handles in pH  */
    long lFlags,                        /* i: bit flags (none at this time; set to 0)  */
    CAPIStream str )                    /* i: stream for reading data to store    */
{
    FILE *pFile;
    PCAPISESSION *pSession = (PCAPISESSION*)s;
    PCAPIHANDLE* pHandle;
    PCAPIStream* pStream = (PCAPIStream*)(*(void **)str);
    NSCalendar *pCal = 0;
    char* pBuf = 0;
    size_t iBufSize = BUFSIZ;
    CAPIStatus iRetStatus = CAPI_ERR_OK;
    CAPIStatus iStatus;
    ICalReader * pRedr = 0;
    JLog* pLog = 0;
    UnicodeString uFilename;
    int iTempFileCreated = 0;
    int iWorking;
    t_bool bWriteStatus;
    size_t iLen;
    ErrorCode status = ZERO_ERROR;

    if (0 == pSession)
        return CAPI_ERR2_SESSION_BAD;
    if (0 == pHList)
        return CAPI_ERR2_HANDLE_BAD;
    if (0 == pStream)
        return CAPI_ERR2_STREAM_BAD;
    
    /*
     *   import the user supplied stream into pCal.
     *   In the interest of minimizing development time (ie, we're lazy)
     *   we're just going to save the user supplied data into a temporary file,
     *   import it into pCal, and write the result.
     */

    /*
     * get a temporary file name...
     */

    char sTemplate[20];
    char* psTempFile;
    sprintf(sTemplate,"CAPIXXXXXX");
    psTempFile = CAPI_mktemp( sTemplate );
    if( 0 == psTempFile)
        return CAPI_ERR1_INTERNAL;
    if( 0 == (pFile = CAPI_fopen(psTempFile,"w")))
        return CAPI_ERR1_INTERNAL;

    iTempFileCreated = 1;
    pBuf = (char *) CAPI_malloc(iBufSize);
    if (0 == pBuf)
        STOR_EXIT(CAPI_ERR1_MEMORY);

    for( iWorking = 1; iWorking != 0; )
    {
        iStatus = (*pStream->pfnSndCallback)(
            pStream->userDataSnd, /* user data */
            pBuf,                 /* place to put the supplied data */
            iBufSize,             /* the size of the buffer */
            &iLen);               /* how much data was put in the buffer */
        if (iStatus > 0)
        {
            STOR_EXIT(CAPI_ERR1_CALLBACK);
        }
        else
        {
            if (iStatus == -1)
                iWorking = 0;
            if (iLen > 0)
            {
                if (iLen != CAPI_fwrite(pBuf,1,iLen,pFile))
                    STOR_EXIT(CAPI_ERR1_CALLBACK);
            }
        }
    }

    /*
     *  Temporary file has been written. Close pFile and import its contents into each
     *  of the caller supplied CAPI_handles
     */
    if (0 != CAPI_fclose(pFile))
        STOR_EXIT(CAPI_ERR1_INTERNAL);
    pFile = 0;
    CAPI_free(pBuf);
    pBuf = 0;

    if (0 != pStream->pfnSndCallback)
    {

        for (int i = 0; i < iHandleCount; i++)
        {
            pHandle = (PCAPIHANDLE*) pHList[i];
            if (0 == pHandle)
                STOR_EXIT(CAPI_ERR2_HANDLE_BAD);

            /*
             *  create an NSCalendar, pCal
             */
            pCal = new NSCalendar(pLog);
            if (pCal == 0)
                STOR_EXIT(CAPI_ERR1_INTERNAL);

            /*
             *  Try to load pHandle->psFile in pCal
             *  if file error, return CAPI_ERR1_INTERNAL
             */

            if (CAPI_access(pHandle->psFile, 00 /*F_OK*/))
            {
                pFile = 0;
                if ( 0 == (pFile =CAPI_fopen(pHandle->psFile,"w")))
                    STOR_EXIT(CAPI_ERR1_INTERNAL);

                if (0 != CAPI_fclose(pFile))
                    STOR_EXIT(CAPI_ERR1_INTERNAL);
                pFile = 0;
            }
            pRedr = (ICalReader *) new ICalFileReader(pHandle->psFile, status);
            if (pRedr == 0)
                STOR_EXIT(CAPI_ERR1_INTERNAL);
            if (FAILURE(status))
                STOR_EXIT(CAPI_ERR1_INTERNAL);

            /*
             *  Load original datastore...
             */
            uFilename = pHandle->psFile;
            pCal->parse(pRedr, uFilename);
            delete pRedr; pRedr = 0;


            /*
             *  Import new event to store
             */
            pRedr = (ICalReader *) new ICalFileReader(psTempFile, status);
            if (pRedr == 0)
                STOR_EXIT(CAPI_ERR1_INTERNAL);
            if (FAILURE(status))
                STOR_EXIT(CAPI_ERR1_INTERNAL);
            uFilename = pHandle->psFile;
            pCal->parse(pRedr, uFilename);
            delete pRedr; pRedr = 0;

            /*
             *  Write the updated calendar store out to disk...
             */
            pCal->export(pHandle->psFile, bWriteStatus);
            if (!bWriteStatus)
                STOR_EXIT(CAPI_ERR1_INTERNAL);

            delete pCal;
            pCal = 0;

        }
    }

    if (0 != CAPI_unlink(psTempFile))
        STOR_EXIT(CAPI_ERR1_INTERNAL);
    iTempFileCreated = 0;

STOREVENT_EXIT:
    if (0 != pFile)
        CAPI_fclose(pFile);
    if (0 != pCal)
        delete pCal;
    if (0 != pRedr)
        delete pRedr;
    if (iTempFileCreated)
        CAPI_unlink(psTempFile);
    if (0 != pBuf)
        CAPI_free(pBuf);

    return iRetStatus;
}

/***************************************************************************************
***   Simple test program
****************************************************************************************/
#if 0       /* define test program */
typedef struct
{
    char *p;
    size_t iSize;
} CAPICTX;

/**
 *  Send data to CAPI. This is invoked on calls such as CAPI_StoreEvent.
 */
int SndData(void* pData, char* pBuf, size_t iSize, size_t *piTransferred)
{
    CAPICTX* pCtx = (CAPICTX*)pData;
    *piTransferred = (pCtx->iSize > iSize) ? iSize : pCtx->iSize;
    CAPI_memcpy(pBuf, pCtx->p, *piTransferred );
    pCtx->iSize -= *piTransferred;
    pCtx->p += *piTransferred;
    return pCtx->iSize > 0 ? CAPI_CALLBACK_CONTINUE : CAPI_CALLBACK_DONE;
}

/**
 *  Receive data from CAPI. This is invoked on calls such as
 *  CAPI_FetchEventsByID, CAPI_FetchEventsByRange.
 */
int RcvData(void* pData, char* pBuf, size_t iSize, size_t *piTransferred)
{
    CAPICTX* pCtx = (CAPICTX*)pData;
    *piTransferred = (pCtx->iSize > iSize) ? iSize : pCtx->iSize;
    CAPI_memcpy(pCtx->p, pBuf, *piTransferred );

    /*
     * for now, we're just going to print out whatever we get...
     */
    {
        char sBuf[2000];
        CAPI_memcpy(sBuf,pCtx->p,*piTransferred);
        sBuf[*piTransferred]=0;
        printf( "%s", sBuf );
    }
    return CAPI_CALLBACK_CONTINUE;    /* return values > 0 are error numbers. */
}

void main( int argc, char *argv[], char *envp[] )
{
    char* psCap;
    CAPIStatus s;
    CAPISession Session;
    CAPIHandle Handle;
    char sBuf[BUFSIZ];
    CAPIStream SndStream;
    CAPIStream RcvStream;

    CAPICTX MyCtx;

    if (CAPI_ERR_OK != (s = CAPI_Capabilities( &psCap, "localhost", 0 )))
    {
        fprintf(stderr,"CAPI_Capabilities(): %ld\n", s);
        exit(1);
    }

    /*
     *  CAPABILITIES
     */
    printf( "CAPI_Capabilities:\n%s\n", psCap );

    /*
     *  LOGON
     */
    if (CAPI_ERR_OK != (s = CAPI_Logon( "sman", "bla", "localhost", 0L, &Session )))
    {
        fprintf(stderr,"CAPI_Logon(): %ld\n", s );
        exit(1);
    }
    printf("CAPI_Logon(): success\n" );

    /*
     *  GET HANDLE
     */
    if (CAPI_ERR_OK != (s = CAPI_GetHandle( Session, "c:/temp/junk.txt", 0, &Handle )))
    {
        fprintf(stderr,"CAPI_Logon(): %ld\n", s );
        exit(1);
    }
    printf("CAPI_GetHandle(): success\n" );

    /*
     *   STORE EVENT
     */
    strcpy(sBuf,"Content-type: text/calendar\n\
Content-encoding: 7bit\n\
\n\
BEGIN:VCALENDAR\n\
METHOD:PUBLISH\n\
PRODID:-//ACME/DesktopCalendar//EN\n\
VERSION:2.0\n\
BEGIN:VEVENT\n\
ORGANIZER:mailto:a@example.com\n\
DTSTART:19970701T200000Z\n\
DTEND:19970701T220000Z\n\
DTSTAMP:19970701T180000Z\n\
SUMMARY:BIG TIME local capi store event\n\
UID:QQIIDDLL098503945-34059873405-340598340@example.com\n\
END:VEVENT\n\
END:VCALENDAR\n" );
    MyCtx.p = sBuf;
    MyCtx.iSize = strlen(sBuf);
    if ( CAPI_ERR_OK != (s = CAPI_SetStreamCallbacks(Session, &SndStream, SndData, &MyCtx, 0, &MyCtx,0)))
    {
        fprintf(stderr,"CAPI_SetStreamCallbacks(): %ld\n", s );
        exit(1);
    }
    if ( CAPI_ERR_OK != (s = CAPI_StoreEvent( Session,&Handle,1,0,&SndStream)))
    {
        fprintf(stderr,"CAPI_StoreEvent(): %ld\n", s );
        exit(1);
    }

    /*
     *   FETCH EVENT
     */
    MyCtx.p = sBuf;
    MyCtx.iSize = sizeof(sBuf);
    if ( CAPI_ERR_OK != (s = CAPI_SetStreamCallbacks(Session, &RcvStream, 0,0,RcvData, &MyCtx,0)))
    {
        fprintf(stderr,"CAPI_SetStreamCallbacks(): %ld\n", s );
        exit(1);
    }
    if ( CAPI_ERR_OK != (s = CAPI_FetchEventsByID( Session,Handle,0,"098503945-34059873405-340598340@example.com",0,0,0,0,&RcvStream)))
    {
        fprintf(stderr,"CAPI_FetchEventsByID(): %ld\n", s );
        exit(1);
    }
     
    /*
     *  DESTROY HANDLES
     */
    if (CAPI_ERR_OK != (s = CAPI_DestroyHandles(Session, &Handle, 1, 0L)))
    {
        fprintf(stderr,"CAPI_DestroyHandles(): %ld\n", s );
        exit(1);
    }
    printf("CAPI_DestroyHandles(): success\n" );

    /*
     *  LOGOFF
     */
    if (CAPI_ERR_OK != (s = CAPI_Logoff( &Session, 0L)))
    {
        fprintf(stderr,"CAPI_Logoff(): %ld\n", s );
        exit(1);
    }
    printf("CAPI_Logoff(): success\n" );

    if (0 != Session)
    {
        fprintf(stderr,"Session was not NULL after CAPI_Logoff\n");
        exit(1);
    }
    printf("CAPI Test Program:  normal exit\n" );
}
#endif      /* define test program */
