/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include <stdio.h>
#include "nscore.h"
#include "nsError.h"
#include "nsCom.h"
#include "capi.h"
#include "julnstr.h"
#include "nsDateTime.h"
#include "nscalexport.h"
#include "nsLayer.h"
#include "nsCurlParser.h"
#include "nsCalendarShell.h"
#include "jdefines.h"
#include "nsCalShellFactory.h"
#include "nsString.h"
#include "nsFont.h"
#include "nsCalShellCIID.h"
#include "nsCRT.h"
#include "plstr.h"
#include "prmem.h"
#include "prenv.h"
#include "icalfrdr.h"
#include "nsIPref.h"
#include "nsCalendarShell.h"
#include "nscalstrings.h"
#include "nsxpfcCIID.h"
#include "nsIAppShell.h"
#include "nsICommandServer.h"
#include "nsCalUICIID.h"
#include "nsCalCommandCanvas.h"
#include "nlsloc.h"
#include "nsCapiCIID.h"
#include "nspr.h"
#include "prcvar.h"
#include "nsCalStreamReader.h"
#include "nsCalSessionMgr.h"

static NS_DEFINE_IID(kILayerIID,       NS_ILAYER_IID);

nsLayer::nsLayer(nsISupports* outer)
{
  NS_INIT_REFCNT();
  mpCal = 0;
  mpShell = 0;
}

NS_IMPL_QUERY_INTERFACE(nsLayer, kILayerIID)
NS_IMPL_ADDREF(nsLayer)
NS_IMPL_RELEASE(nsLayer)

nsLayer::~nsLayer()
{
  if (nsnull != mpCal)
  {
    delete mpCal;
    mpCal = nsnull;
  }
}

nsresult nsLayer::Init()
{
  mpCal = new NSCalendar(0);

  return (NS_OK);
}

nsresult nsLayer::SetCurl(const JulianString& s)
{
  msCurl = s;
  return (NS_OK);
}

nsresult nsLayer::GetCurl(JulianString& s)
{
  s = msCurl;
  return (NS_OK);
}


nsresult nsLayer::SetCal(NSCalendar* aCal)
{
  mpCal = aCal;
  return (NS_OK);
}


nsresult nsLayer::GetCal(NSCalendar*& aCal)
{
  aCal = mpCal;
  return (NS_OK);
}

/**
 * Check to see if the layer matches the supplied curl.
 * In this case, matching means that the host and CSID 
 * values are equal.
 * @param  s  the curl
 * @return NS_OK on success
 */
nsresult nsLayer::URLMatch(const JulianString& aCurl, PRBool& aMatch)
{
  nsCurlParser curlA(msCurl);
  nsCurlParser curlB(aCurl);

  aMatch = 0;

  if ( ( curlA.GetHost() == curlB.GetHost() ) &&
       ( curlA.GetCSID() == curlB.GetCSID() ) )
    aMatch = 1;

  return NS_OK;
}


/*
 *  CAPI Callback used to fetch data from a general CAPI location
 *  Gets nsCalStreamReader object.  
 *  If parse not started yet.
 *     Start the parser - the parser will block automatically when
 *     no more data to parse.
 *  When parse is blocked, set size handled
 */
static int RcvData(void * pData, 
            char * pBuf, 
            size_t iSize, 
            size_t * piTransferred)
{
    nsCalStreamReader * pCalStreamReader = (nsCalStreamReader *) pData;
    nsCapiCallbackReader * pCapiCallbackReader = pCalStreamReader->getReader();
    if (!pCalStreamReader->isParseStarted())
    {
        PRMonitor * pMon = (PRMonitor*) pCalStreamReader->getCallerData();
        pCalStreamReader->setParseStarted();

        /*
         * Start up the thread that will receive the ical data
         */
        PR_ExitMonitor(pMon);
    }

    /*
     * We're going to be adding a new buffer (unicode string) to the 
     * list of data. We don't want the other thread accessing the list
     * while we're doing this. So, we enter the monitor...
     */
    PR_EnterMonitor((PRMonitor *)pCapiCallbackReader->getMonitor());

    /*
     * if we're finished, set the CapiCallbackReader to finished.
     */
    if (iSize == 0)
    {
      pCapiCallbackReader->setFinished();
    }
    else
    {
      /*
       * XXX: may want to ensure that pBuf is 0 terminated.
       */
      char * pBufCopy = new char[strlen(pBuf)];
      strncpy(pBufCopy, pBuf, (size_t) strlen(pBuf));
      nsCapiBufferStruct * capiBuffer = new nsCapiBufferStruct();
      capiBuffer->m_pBuf = pBufCopy;
      capiBuffer->m_pBufSize = iSize;
      pCapiCallbackReader->AddBuffer(capiBuffer);
      *piTransferred = iSize;
    }

    /*
     * The parsing thread may be waiting on more data before it
     * can continue. When this happens, it enters a PR_WAIT for
     * this monitor. We've just finished adding more data, so we
     * want to notify the other thread now if it's waiting.
     */
    PR_Notify((PRMonitor *)pCapiCallbackReader->getMonitor());
    PR_ExitMonitor((PRMonitor *)pCapiCallbackReader->getMonitor());

    /*
     * Now that another buffer is available for parsing, we want 
     * the parsing thread to take over. This will help keep the 
     * list of unparsed buffers to a minimum.
     */
    // PR_Sleep(PR_INTERVAL_NO_WAIT);
    return iSize > 0 ? 0 : -1;
}

/* 
 * required to set gasViewPropList and giViewPropListCount for now
 * will need to change local CAPI so null gasViewPropList will return 
 * all properties.
 */
static char * gasViewPropList[10] = {
  "ATTENDEE", "DTSTART", "DTEND", "UID", "RECURRENCE-ID",
    "DTSTAMP", "SUMMARY", "DESCRIPTION", "ORGANIZER", "TRANSP"
};
static int giViewPropListCount = 10;

/**
 * Given an nsICapi interface, log in and get some initial data.
 * @return NS_OK on success.
 */
nsresult nsLayer::FetchEventsByRange(
                      DateTime* aStart, 
                      DateTime* aStop,
                      JulianPtrArray* anArray
                      )
{
  /*
   * Before we do anything, see if we've cached the info..
   */
  if ( aStart->compareTo(mpCal->getEventsSpanStart()) >= 0 &&
       0 >= aStop->compareTo(mpCal->getEventsSpanEnd()) )
  {
    mpCal->getEventsByRange(anArray, *aStart, *aStop);
    return NS_OK;
  }


  ErrorCode status = ZERO_ERROR;
  DateTime d;
  char * psDTStart = 0;
  char * psDTEnd = 0;
  CAPIStream RcvStream = 0;
  CAPIStatus capiStatus;
  JulianPtrArray * pParsedCalList = new JulianPtrArray(); // destroyed
  nsCalStreamReader * pCalStreamReader = 0;   // destroyed
  PRThread * parseThread = 0;
  PRThread * mainThread = 0;
  PRMonitor * pCBReaderMonitor = 0;           // destroyed
  PRMonitor *pThreadMonitor = 0;              // destroyed

  NS_ASSERTION(0 != mpShell, "null shell in fetchEventsByRange");   
  if (0 == mpShell)
    return 1; // XXX fix this

  /*
   * Select the capi interface to use for this operation...
   * Each layer stores its curl, ask for a session based on
   * the curl...
   */
  nsICapi* pCapi = 0;
  nsCalSession *pSession;
  nsCurlParser curl(msCurl);
  if (NS_OK == (mpShell->mSessionMgr.GetSession(msCurl, 0, curl.GetPassword().GetBuffer(), pSession)))
  {
    if (0 != pSession)
    {
      pCapi = pSession->mCapi;
    }
  }
  else
  {
    NS_ASSERTION(1,"Could not get a session");
    pCapi = mpShell->mSessionMgr.GetAt(0L)->mCapi;  // should comment this
  }

  /*
   * Set up the range of time for which we'll pull events...
   */
  psDTStart = aStart->toISO8601().toCString("");
  psDTEnd = aStop->toISO8601().toCString("");

  /*
   * The data is actually read and parsed in another thread. Set it all
   * up here...
   */
  mainThread = PR_CurrentThread();    
  pCBReaderMonitor = PR_NewMonitor();  // destroyed
  nsCapiCallbackReader * capiReader = new nsCapiCallbackReader(pCBReaderMonitor);
  pThreadMonitor = ::PR_NewMonitor(); // destroyed
  PR_EnterMonitor(pThreadMonitor);
  pCalStreamReader = new nsCalStreamReader(capiReader, pParsedCalList, parseThread, pThreadMonitor);
  parseThread = PR_CreateThread(PR_USER_THREAD,
                 main_CalStreamReader,
                 pCalStreamReader,
                 PR_PRIORITY_NORMAL,
                 PR_LOCAL_THREAD,
                 PR_UNJOINABLE_THREAD,
                 0);

  capiStatus = pCapi->CAPI_SetStreamCallbacks(
    pSession->m_Session, &RcvStream, 0,0 ,RcvData, pCalStreamReader,0);

  if (CAPI_ERR_OK != capiStatus)
    return 1;   /* XXX: really need to fix this up */

  {
    /* XXX: Get rid of the local variables  as soon as 
     *      local capi can take a null list or as soon as
     *      cs&t capi can take a list.
     */
    nsCurlParser sessionURL(msCurl);
    char** asList = gasViewPropList;
    int iListSize = giViewPropListCount;

    if (nsCurlParser::eCAPI == sessionURL.GetProtocol())
    {
      asList = 0;
      iListSize = 0;
    }

    CAPIHandle h = 0;
    JulianString sHandle(sessionURL.GetCSID());

    /*
     * The handle name may be a file name. We need to make sure that
     * the characters are in URL form. That is "C|/bla" instead of
     * "C:/bla"
     */
    nsCurlParser::ConvertToURLFileChars(sHandle);
    capiStatus = pCapi->CAPI_GetHandle(
      pSession->m_Session,
      sHandle.GetBuffer(),0,&h);
    if (0 != capiStatus)
      return 1; /* XXX really need to fix this */
    capiStatus = pCapi->CAPI_FetchEventsByRange( 
      pSession->m_Session, &h, 1, 0,
      psDTStart, psDTEnd, 
      asList, iListSize, RcvStream);
  }  
  
  if (CAPI_ERR_OK != capiStatus)
    return 1;   /* XXX: really need to fix this up */

  /*
   * Wait here until we know the thread completed.
   */
  if (!pCalStreamReader->isParseFinished() )
  {
    PR_EnterMonitor(pThreadMonitor);
    PR_Wait(pThreadMonitor,PR_INTERVAL_NO_TIMEOUT);
    PR_ExitMonitor(pThreadMonitor);
  }

  /*
   * Load the retrieved events ito our calendar...
   */
  int i,j;
  NSCalendar* pCal;
  JulianPtrArray* pEventList;
  ICalComponent* pEvent;
  for ( i = 0; i < pParsedCalList->GetSize(); i++)
  {
    pCal = (NSCalendar*)pParsedCalList->GetAt(i);
    pEventList = pCal->getEvents();
    if (0 != pEventList)
    {
      for (j = 0; j < pEventList->GetSize(); j++)
      {
        pEvent = (ICalComponent*)pEventList->GetAt(j);
        if (0 != pEvent)
        {
#if 0
          UnicodeString usFmt("%(yyyy-MMM-dd hh:mma)B-%(hh:mma)e  %S");         // XXX this needs to be put in a resource, then a variable that can be switched
          char *psBuf = pEvent->toStringFmt(usFmt).toCString("");
          delete psBuf;
#endif
          anArray->Add(pEvent);
          mpCal->addEvent(pEvent);
        }
      }
    }
  }

  /**
   * cleanup allocated memory
   */
  delete [] psDTStart; psDTStart = 0;
  delete [] psDTEnd; psDTEnd = 0;
  delete pCalStreamReader; pCalStreamReader = 0;
  delete capiReader; capiReader = 0;
  PR_DestroyMonitor(pThreadMonitor);
  PR_DestroyMonitor(pCBReaderMonitor);

  /*
   * now that we have successfully retrieved information from aStart
   * to aStop, update the Calendar's known range for events.
   */
  mpCal->updateEventsRange(*aStart, *aStop);

  /*
   * todo: need to delete calendars in pParsedCalList without 
   * deleting events in it 
   */
  capiStatus = pCapi->CAPI_DestroyStreams(pSession->m_Session, &RcvStream, 1, 0);
  if (CAPI_ERR_OK != capiStatus)
    return 1;   /* XXX: really need to fix this up */

  /*
   * register the calendar...
   */
  mpShell->mCalList.Add(mpCal);

  return NS_OK;
}

/**
 *  Send data to CAPI. This is invoked on calls such as CAPI_StoreEvent.
 */
int SndData(void* pData, char* pBuf, size_t iSize, size_t *piTransferred)
{
    nsCapiBufferStruct* pCtx = (nsCapiBufferStruct*)pData;  
    *piTransferred = (pCtx->m_pBufSize > iSize) ? iSize : pCtx->m_pBufSize; 
    memcpy(pBuf,pCtx->m_pBuf,*piTransferred); 
    pCtx->m_pBufSize -= *piTransferred; 
    pCtx->m_pBuf += *piTransferred; 
    return pCtx->m_pBufSize > 0 ? 0 : -1; 
}

/**
 * Save this event
 * @return NS_OK on success.
 */
nsresult
nsLayer::StoreEvent(VEvent& addEvent)
{
  ErrorCode status = ZERO_ERROR;
  PRThread * mainThread = 0;
  CAPIStream SndStream = 0;
  CAPIStatus capiStatus;
  PRMonitor * pCBReaderMonitor = 0;           // destroyed
  PRMonitor *pThreadMonitor = 0;              // destroyed

  /*
   * Select the capi interface to use for this operation...
   * Each layer stores its curl, ask for a session based on
   * the curl...
   */
  nsICapi* pCapi = 0;
  nsCalSession *pSession;
  nsCurlParser curl(msCurl);
  if (NS_OK == (mpShell->mSessionMgr.GetSession(msCurl, 0, curl.GetPassword().GetBuffer(), pSession)))
  {
    if (0 != pSession)
    {
      pCapi = pSession->mCapi;
    }
  }
  else
  {
    NS_ASSERTION(1,"Could not get a session");
    pCapi = mpShell->mSessionMgr.GetAt(0L)->mCapi;  // should comment this
  }

  /*
   * The data is actually read and parsed in another thread. Set it all
   * up here...
   */
#if 0
  mainThread = PR_CurrentThread();    
  pCBReaderMonitor = PR_NewMonitor();  // destroyed
  nsCapiCallbackReader * capiReader = new nsCapiCallbackReader(pCBReaderMonitor);
  pThreadMonitor = ::PR_NewMonitor(); // destroyed
  PR_EnterMonitor(pThreadMonitor);
  parseThread = PR_CreateThread(PR_USER_THREAD,
                 main_CalStreamReader,
                 pCalStreamReader,
                 PR_PRIORITY_NORMAL,
                 PR_LOCAL_THREAD,
                 PR_UNJOINABLE_THREAD,
                 0);

  capiStatus = pCapi->CAPI_SetStreamCallbacks(
    pSession->m_Session, &RcvStream, 0, 0, RcvData, pCalStreamReader,0);
#else
  UnicodeString        thisEvent;
  nsCapiBufferStruct   sBuf;

  thisEvent = "Content-type: text/calendar\nContent-encoding: 7bit\n\nBEGIN:VCALENDAR\n";
  thisEvent += "METHOD: PUBLISH\n";
  thisEvent += addEvent.toICALString();
  thisEvent += "END:VCALENDAR\n";
 
  sBuf.m_pBuf = thisEvent.toCString("");
  sBuf.m_pBufSize = strlen(sBuf.m_pBuf);
  capiStatus = pCapi->CAPI_SetStreamCallbacks(pSession->m_Session, &SndStream, &SndData, (void *)&sBuf, nsnull, nsnull, 0);

  nsCurlParser sessionURL(msCurl);
  char** asList = gasViewPropList;
  int iListSize = giViewPropListCount;

  if (nsCurlParser::eCAPI == sessionURL.GetProtocol())
  {
    asList = 0;
    iListSize = 0;
  }

  CAPIHandle h = 0;
  JulianString sHandle(sessionURL.GetCSID());

  /*
   * The handle name may be a file name. We need to make sure that
   * the characters are in URL form. That is "C|/bla" instead of
   * "C:/bla"
   */
  nsCurlParser::ConvertToURLFileChars(sHandle);
  capiStatus = pCapi->CAPI_GetHandle( pSession->m_Session, sHandle.GetBuffer(), 0, &h);
  if (0 != capiStatus)
    return 1; /* XXX really need to fix this */

  if ( CAPI_ERR_OK != (pCapi->CAPI_StoreEvent( pSession->m_Session, &h, 1, 0, &SndStream)))
  {
  }

#endif
  if (CAPI_ERR_OK != capiStatus)
    return 1;   /* XXX: really need to fix this up */

}


