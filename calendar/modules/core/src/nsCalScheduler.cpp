/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2
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

/**
 *  nsCalScheduler
 *     Wraps the functions that get and put events on a calendar store.
 *
 *  sman
 */

#include "jdefines.h"
#include "julnstr.h"
#include "nsCalShellFactory.h"
#include "nsString.h"
#include "nsFont.h"
#include "nsCalShellCIID.h"
#include "nsXPFCObserverManager.h"
#include "nsCRT.h"
#include "plstr.h"
#include "prmem.h"
#include "prenv.h"
#include "julnstr.h"
#include "icalfrdr.h"
#include "nsIPref.h"
#include "nsCurlParser.h"
#include "nsCalUser.h"
#include "nsCalLoggedInUser.h"
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
#include "nsCalScheduler.h"
#include "nsCalSessionMgr.h"

nsCalScheduler::nsCalScheduler()
{
  mpShell = 0;
}

nsCalScheduler::~nsCalScheduler()
{

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
nsresult nsCalScheduler::InitialLoadData(/*nsIUser* pUser*/)
{
  nsresult res;
  ErrorCode status = ZERO_ERROR;
  DateTime d;
  char * psDTStart = 0;
  char * psDTEnd = 0;
  CAPIStream RcvStream = 0;
  CAPIStatus capiStatus;
  JulianPtrArray * pParsedCalList = new JulianPtrArray(); // destroyed
  nsCalStreamReader * pCalStreamReader = 0;  // destroyed
  PRThread * parseThread = 0;
  PRThread * mainThread = 0;
  PRMonitor * pCBReaderMonitor = 0; /// destroyed
  PRMonitor *pThreadMonitor = 0; /// destroyed

  if (0 == mpShell)
    return 1; // XXX fix this

  /*
   * Select the capi interface to use for this operation...
   */
  nsICapi* pCapi = mpShell->mSessionMgr.GetAt(0L)->mCapi;

  /*
   * Set up the range of time for which we'll pull events...
   */
  int iOffset = 30;
  d.prevDay(iOffset);
  psDTStart = d.toISO8601().toCString("");
  d.nextDay(2 * iOffset);
  psDTEnd = d.toISO8601().toCString("");

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
    mpShell->mCAPISession, &RcvStream, 0,0,RcvData, pCalStreamReader,0);

  if (CAPI_ERR_OK != capiStatus)
    return 1;   /* XXX: really need to fix this up */

  {
    /* XXX: Get rid of the local variables  as soon as 
     *      local capi can take a null list or as soon as
     *      cs&t capi can take a list.
     */
  nsCurlParser sessionURL(mpShell->msCalURL);
  char** asList = gasViewPropList;
  int iListSize = giViewPropListCount;

  if (nsCurlParser::eCAPI == sessionURL.GetProtocol())
  {
    asList = 0;
    iListSize = 0;
  }

  capiStatus = pCapi->CAPI_FetchEventsByRange( 
      mpShell->mCAPISession, &mpShell->mCAPIHandle, 1, 0,
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

    /**
     *  Change the ownership of the pCal's VEvents' vector
     *  from pCal to m_pCalendar
     */
    pEventList = pCal->changeEventsOwnership();
    if (0 != pEventList)
    {
      for (j = 0; j < pEventList->GetSize(); j++)
      {
        pEvent = (ICalComponent*)pEventList->GetAt(j);
        if (0 != pEvent)
          mpShell->mpCalendar->addEvent(pEvent);
      }
    }

    /**
     * delete pEventList since the vector is no longer needed
     * (I only care about the VEvents inside the vector, 
     *  not the vector itself)
     */
    delete pEventList; pEventList = 0;
  }

  /**
   * cleanup allocated memory
   */

  /**
   *  Delete each calendar in pParsedCalList.
   *  I won't delete the events in them because there ownership
   *  has been changed.
   */
  for (i = pParsedCalList->GetSize() - 1; i >= 0; i--)
  {
    pCal = (NSCalendar *)pParsedCalList->GetAt(i);
    delete pCal; pCal = 0;
  }
  
  delete [] psDTStart; psDTStart = 0;
  delete [] psDTEnd; psDTEnd = 0;
  delete pCalStreamReader; pCalStreamReader = 0;
  delete capiReader; capiReader = 0;
  PR_DestroyMonitor(pThreadMonitor);
  PR_DestroyMonitor(pCBReaderMonitor);
  capiStatus = pCapi->CAPI_DestroyStreams(mpShell->mCAPISession, &RcvStream, 1, 0);
  if (CAPI_ERR_OK != capiStatus)
    return 1;   /* XXX: really need to fix this up */

  /*
   * register the calendar...
   */
  if (NS_OK != (res = mpShell->mCalList.Add(mpShell->mpCalendar)))
    return res;

  return NS_OK;
}



