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
#include "nsX400Parser.h"

#include "capi.h"
#include "nsICapi.h"

#include "nsCoreCIID.h"
#include "nsLayer.h"
#include "nsCalUser.h"

/* for CAPI to work in general form */
#include "nsCapiCallbackReader.h"
#include "nsCalStreamReader.h"

#ifdef NS_WIN32
#include "windows.h"
#endif

#ifdef NS_UNIX
#include "Xm/Xm.h"
#include "Xm/MainW.h"
#include "Xm/Frame.h"
#include "Xm/XmStrDefs.h"
#include "Xm/DrawingA.h"

extern XtAppContext app_context;
extern Widget topLevel;

#endif

// All Applications must specify this *special* application CID
// to their own unique IID.
nsIID kIXPCOMApplicationShellCID = NS_CAL_SHELL_CID ; 

static NS_DEFINE_IID(kIXPFCObserverManagerIID, NS_IXPFC_OBSERVERMANAGER_IID);
static NS_DEFINE_IID(kCXPFCObserverManagerCID, NS_XPFC_OBSERVERMANAGER_CID);

// hardcode names of dll's
#ifdef NS_WIN32
  #define CAPI_DLL    "calcapi10.dll"
  #define CORE_DLL    "calcore10.dll"
#else
  #define CAPI_DLL   "libcalcapi10.so"
  #define CORE_DLL   "libcalcore10.so"
#endif

static NS_DEFINE_IID(kCCapiLocalCID,            NS_CAPI_LOCAL_CID);
static NS_DEFINE_IID(kCCapiCSTCID,              NS_CAPI_CST_CID);
static NS_DEFINE_IID(kCLayerCID,                NS_LAYER_CID);
static NS_DEFINE_IID(kCLayerCollectionCID,      NS_LAYER_COLLECTION_CID);

// All Application Must implement this function
nsresult NS_RegisterApplicationShellFactory()
{

  nsresult res = nsRepository::RegisterFactory(kIXPCOMApplicationShellCID,
                                               new nsCalShellFactory(kIXPCOMApplicationShellCID),
                                               PR_FALSE) ;

  return res;
}

/*
 *  CAPI Callback used to fetch data from a general CAPI location
 *  Gets nsCalStreamReader object.  
 *  If parse not started yet.
 *     Start the parser - the parser will block automatically when
 *     no more data to parse.
 *  When parse is blocked, set size handled
 */
int RcvData(void * pData, 
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
 * nsCalendarShell Definition
 */
nsCalendarShell::nsCalendarShell()
{
  NS_INIT_REFCNT();

  mShellInstance  = nsnull ;
  mDocumentContainer = nsnull ;
  mObserverManager = nsnull;
  mCAPIPassword = nsnull;
  m_pCalendar = nsnull;
  mpLoggedInUser = nsnull;
  mCommandServer = nsnull;
  mCAPISession = nsnull;
  mCAPIHandle = nsnull;
}

nsCalendarShell::~nsCalendarShell()
{
  m_SessionMgr.GetAt(0L)->mCapi->CAPI_DestroyHandles(mCAPISession, &mCAPIHandle, 1, 0L);
  Logoff();

  NS_IF_RELEASE(mObserverManager);

  if (mCAPIPassword)
    PR_Free(mCAPIPassword);
  if (m_pCalendar)
    delete m_pCalendar;
  if (mpLoggedInUser)
    delete mpLoggedInUser;

  NS_IF_RELEASE(mDocumentContainer);
  NS_IF_RELEASE(mShellInstance);
  NS_IF_RELEASE(mCommandServer);
}

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIAppShellIID, NS_IAPPSHELL_IID);


nsresult nsCalendarShell::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      

  if(aIID.Equals(kISupportsIID))    {  //do IUnknown...
    *aInstancePtr = (nsISupports*)(nsIApplicationShell *)(this);                                        
  }
  else if(aIID.Equals(kIXPCOMApplicationShellCID)) {  //do nsIContentSink base class...
    *aInstancePtr = (nsICalendarShell*)(this);                                        
  }
  else if(aIID.Equals(kIAppShellIID)) {  //do nsIContentSink base class...
    *aInstancePtr = (nsIAppShell*)(this);                                        
  }
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  ((nsISupports*) *aInstancePtr)->AddRef();
  return NS_OK;                                                        
}

NS_IMPL_ADDREF(nsCalendarShell)
NS_IMPL_RELEASE(nsCalendarShell)

nsresult nsCalendarShell::Init()
{

  /*
   * Register class factrories needed for application
   */

  RegisterFactories() ;

  /*
   * Load Application Prefs
   */

  LoadPreferences();

  /*
   * Logon to the system
   */

  Logon();

  /*
   * Create the UI
   */

  LoadUI();

  return NS_OK;
}

/**
 * This is a useful piece of code but it's in the wrong
 * place. Given a path, it ensures that the path exists, creating
 * whatever needs to be created.
 * @return NS_OK on success
 *         file creation errors otherwise.
 */
nsresult nsCalendarShell::EnsureUserPath( JulianString& sPath )
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
        return (nsresult) iError;
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
      return (nsresult) iError;
  }

  return NS_OK;
}

/* 
 * required to set gasViewPropList and giViewPropListCount for now
 * will need to change local CAPI so null gasViewPropList will return 
 * all properties.
 */
char * gasViewPropList[10] = {
  "ATTENDEE", "DTSTART", "DTEND", "UID", "RECURRENCE-ID",
    "DTSTAMP", "SUMMARY", "DESCRIPTION", "ORGANIZER", "TRANSP"
};
int giViewPropListCount = 10;

/**
 * Given an nsICapi interface, log in and get some initial data.
 * @return NS_OK on success.
 */
nsresult nsCalendarShell::InitialLoadData()
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

  /*
   * Select the capi interface to use for this operation...
   */
  nsICapi* pCapi = m_SessionMgr.GetAt(0L)->mCapi;

  /*
   * Begin a calendar for the logged in user...
   */
  m_pCalendar = new NSCalendar(0);
  SetNSCalendar(m_pCalendar);

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
    mCAPISession, &RcvStream, 0,0,RcvData, pCalStreamReader,0);

  if (CAPI_ERR_OK != capiStatus)
    return 1;   /* XXX: really need to fix this up */

  {
    /* XXX: Get rid of the local variables  as soon as 
     *      local capi can take a null list or as soon as
     *      cs&t capi can take a list.
     */
  nsCurlParser sessionURL(msCalURL);
  char** asList = gasViewPropList;
  int iListSize = giViewPropListCount;

  if (nsCurlParser::eCAPI == sessionURL.GetProtocol())
  {
    asList = 0;
    iListSize = 0;
  }

  capiStatus = pCapi->CAPI_FetchEventsByRange( 
      mCAPISession, &mCAPIHandle, 1, 0,
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
          m_pCalendar->addEvent(pEvent);
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
  /* todo: need to delete calendars in pParsedCalList without deleting events in it */
  capiStatus = pCapi->CAPI_DestroyStreams(mCAPISession, &RcvStream, 1, 0);
  if (CAPI_ERR_OK != capiStatus)
    return 1;   /* XXX: really need to fix this up */

  /*
   * register the calendar...
   */
  if (NS_OK != (res = m_CalList.Add(m_pCalendar)))
    return res;

  return NS_OK;
}

/**
 * This method establishes a logged in user and opens a connection to 
 * the calendar server (or local database file if they're working offline).
 *
 * @return NS_OK on success
 *         1 = could not create or write to a local capi directory
 */
nsresult nsCalendarShell::Logon()
{
  CAPIStatus s;
  nsresult res;

  /*
   *  Getting the first calendar by user name should be reviewed.
   */
  nsCurlParser theURL(mpLoggedInUser->GetUserName());
  nsCurlParser sessionURL(msCalURL);
  theURL |= sessionURL;

  /*
   *  Ask the session manager for a session...
   */
  res = m_SessionMgr.GetSession(
        msCalURL.GetBuffer(),  // may contain a password, if so it will be used
        0L, 
        GetCAPIPassword(), 
        mCAPISession);
  
  JulianString sHandle = mpLoggedInUser->GetUserName();
  
  if (nsCurlParser::eCAPI == theURL.GetProtocol())
  {
    nsX400Parser x(theURL.GetExtra());
    x.Delete("ND");
    x.GetValue(sHandle);
    sHandle.Prepend(":");   // this is disgusting. we must get cst to fix this
  }

  s = m_SessionMgr.GetAt(0L)->mCapi->CAPI_GetHandle(mCAPISession,sHandle.GetBuffer(),0,&mCAPIHandle);

  if (CAPI_ERR_OK != s)
    return NS_OK;

  switch(theURL.GetProtocol())
  {
    case nsCurlParser::eFILE:
    case nsCurlParser::eCAPI:
    {
      InitialLoadData();
    }
    break;

    default:
    {
      /* need to report that this is an unhandled curl type */
    }
    break;
  }

  return NS_OK;
}

nsresult nsCalendarShell::Logoff()
{
  /*
   * Shut down any open CAPI sessions.
   */
  m_SessionMgr.Shutdown();

  return NS_OK;
}


/*
 *  DEFAULT PREFERENCES
 *  If you have a new preference for calendar, here's what to do:
 *  1. Add the defines for the key name and a default value in nscalstrings.h
 *  2. Add the key name and default value defines in the table below.
 *  3. you're all set
 */
typedef struct
{
  char* psKey;
  char* psVal;
} JULIAN_KEY_VAL;

static JULIAN_KEY_VAL KVPairs[] =
{
  CAL_STRING_PREF_JULIAN_UI_XML_MENUBAR,  CAL_STRING_RESOURCE_UI_MENUBAR,
  CAL_STRING_PREF_JULIAN_UI_XML_TOOLBAR,  CAL_STRING_RESOURCE_UI_TOOLBAR,
  CAL_STRING_PREF_JULIAN_UI_XML_CALENDAR, CAL_STRING_RESOURCE_UI_CALENDAR,
  CAL_STRING_PREF_LOCAL_ADDRESS,          CAL_STRING_PREF_LOCAL_ADDRESS_DEFAULT,
  CAL_STRING_PREF_PREFERRED_ADDR,         CAL_STRING_PREF_PREFERRED_ADDR_DEFAULT,
  CAL_STRING_PREF_USERNAME,               CAL_STRING_PREF_USERNAME_DEFAULT,
  CAL_STRING_PREF_USER_DISPLAY_NAME,      CAL_STRING_PREF_USER_DISPLAY_NAME_DEFAULT,
  
  /* Enter new key, default-value pairs above this line */
  0, 0
};

/**
 * Load all the preferences listed in the table above. Set any
 * preferences that do not exist to their default value. Save the
 * preferences upon completion.
 * @return NS_OK
 */
nsresult nsCalendarShell::SetDefaultPreferences()
{
  char pref[1024];
  int i = 1024;

  for (int j = 0; 0 != KVPairs[j].psKey ; j++)
  {
    if (0 != mShellInstance->GetPreferences()->GetCharPref(KVPairs[j].psKey, pref, &i))
      mShellInstance->GetPreferences()->SetCharPref(KVPairs[j].psKey, KVPairs[j].psVal);
  }

  return mShellInstance->GetPreferences()->SavePrefFile();
}

/*
 * the list of environment variables we're interested in...
 */
static char *gtVars[] = 
{
  "$USERNAME",
  "$USERPROFILE",
  0
};

/**
 *  Preferences may contain references to variables that are filled
 *  in at run time.  The current list is:
 *
 *  <PRE>
 *  variable         meaning
 *  ---------------  ----------------------------------------------
 *  $USERNAME        the OS username or other default user name
 *  $USERPROFILE     the file path to where the user's local data
 *                   should be stored.
 *  </PRE>
 *
 *  This routine replaces the variables with their current values.
 *  The results are returned in the supplied buffer.
 *
 *  @param s    the calendar url preference
 *  @param iBufSize  the amount of space available in psCurl
 *  @return 0 on success
 */
nsresult nsCalendarShell::EnvVarsToValues(JulianString& s)
{
  PRInt32 iIndex;
  PRInt32 i;
  char* p;
  char* psEnv;

  /*
   * Replace all callouts of the env vars with their values...
   */
  for (i = 0; 0 != gtVars[i]; i++)
  {
    p = gtVars[i];
    if (-1 != s.Find(p))
    {
      psEnv = PR_GetEnv(&p[1]);
      if (0 != psEnv)
      {
        while (-1 != (iIndex = s.Find(p)))
        {
            s.Replace(iIndex, iIndex + PL_strlen(p) - 1, psEnv);
        }
      }
    }
  }
  return 0;
}


/**
 * Load the preferences that help us get bootstrapped. Specifically,
 * determine the cal url for the user.
 * @return NS_OK on success. Otherwise errors from retrieving preferences.
 */
nsresult nsCalendarShell::LoadPreferences()
{
  nsresult res = nsApplicationManager::GetShellInstance(this, &mShellInstance);
  nsCurlParser curl;

  if (NS_OK != res)
    return res ;

  /*
   * Load the overriding user preferences
   */
  mShellInstance->GetPreferences()->Startup("prefs.js");

  /*
   * Set the default preferences
   */
  SetDefaultPreferences();
  char sBuf[1024];
  int iBufSize = 1024;
  int iSavePrefs = 0;
  JulianString s;

  sBuf[0]=0;

  mpLoggedInUser = new nsCalLoggedInUser();
  if (nsnull == mpLoggedInUser)
  {
    // XXX
    // return NOT OK
  }

  /*
   * fill in info for the user...
   */
  mShellInstance->GetPreferences()->GetCharPref(CAL_STRING_PREF_USERNAME,sBuf, &iBufSize );
  s = sBuf;
  EnvVarsToValues(s);
  mpLoggedInUser->SetUserName(s.GetBuffer());

  /*
   * Add the logged in user to the user list...
   */
  m_UserList.Add( mpLoggedInUser );

  /*
   *  Get the local cal address.
   */
  mShellInstance->GetPreferences()->GetCharPref(CAL_STRING_PREF_LOCAL_ADDRESS,sBuf, &iBufSize );
  s = sBuf;
  EnvVarsToValues(s);
  curl = s;   
  curl.ResolveFileURL();
  mpLoggedInUser->SetLocalCapiUrl(curl.GetCSID().GetBuffer());


  /*
   *  Load the preferred cal address. For now, the local cal address is the default too...
   */
  mShellInstance->GetPreferences()->GetCharPref(CAL_STRING_PREF_PREFERRED_ADDR,sBuf, &iBufSize );
  s = sBuf;
  EnvVarsToValues(s);
  curl = s;   
  curl.ResolveFileURL();
  mpLoggedInUser->AddCalAddr(new JulianString(curl.GetCurl().GetBuffer()));

  /*
   *  Get the user's display name
   */
  mShellInstance->GetPreferences()->GetCharPref(CAL_STRING_PREF_USER_DISPLAY_NAME,sBuf, &iBufSize );
  mpLoggedInUser->SetDisplayName(sBuf);

  /*
   *  Set the curl to use for the logged in user's calendar...
   */
  msCalURL = * mpLoggedInUser->GetPreferredCalAddr();

  /*
   *  XXX: We gotta ask for this or save a secure password pref or something...
   */
  SetCAPIPassword("HeyBaby");

  return NS_OK;
}

/*
 * Load the UI for this instance
 */

nsresult nsCalendarShell::LoadUI()
{

  /*
   * First, create the ObserverManager
   */

  nsresult res = nsRepository::CreateInstance(kCXPFCObserverManagerCID, 
                               nsnull, 
                               kIXPFCObserverManagerIID, 
                               (void **)&mObserverManager);

  if (NS_OK != res)
    return res ;

  mObserverManager->Init();
  
  /*
   * Now create an actual window into the world
   */

  nsRect aRect(20,20,800,600) ;

  /*
   * Get our nsIAppShell interface and pass it down
   */

  nsIAppShell * appshell = nsnull;

  res = QueryInterface(kIAppShellIID,(void**)&appshell);

  if (NS_OK != res)
    return res ;

  mShellInstance->CreateApplicationWindow(appshell, aRect);

  NS_RELEASE(appshell);

  /* 
   * Load a Container
   */

  static NS_DEFINE_IID(kCCalContainerCID, NS_CALENDAR_CONTAINER_CID);

  res = NS_NewCalendarContainer((nsICalendarContainer **)&(mDocumentContainer)) ;

  if (NS_OK != res)
    return res ;

  mDocumentContainer->SetApplicationShell((nsIApplicationShell*)this);
  //((nsCalendarContainer *)mDocumentContainer)->m_pCalendarShell = this;

  mDocumentContainer->SetToolbarManager(mShellInstance->GetToolbarManager());

  // Start the widget from upper left corner
  aRect.x = aRect.y = 0;

  mDocumentContainer->Init(mShellInstance->GetApplicationWidget(),
                           aRect, 
                           this) ;

  char pUI[1024];
  int i = 1024;

  /*
   * Now Load the Canvas UI
   */
#ifdef XP_UNIX
  mShellInstance->GetPreferences()->GetCharPref(CAL_STRING_PREF_JULIAN_UI_XML_CALENDAR,pUI,&i);
  res = mDocumentContainer->LoadURL(pUI,nsnull);
#else
  mShellInstance->GetPreferences()->GetCharPref(CAL_STRING_PREF_JULIAN_UI_XML_MENUBAR,pUI,&i);
  res = mDocumentContainer->LoadURL(pUI,nsnull);
  mShellInstance->GetPreferences()->GetCharPref(CAL_STRING_PREF_JULIAN_UI_XML_CALENDAR,pUI,&i);
  res = mDocumentContainer->LoadURL(pUI,nsnull);
  mShellInstance->GetPreferences()->GetCharPref(CAL_STRING_PREF_JULIAN_UI_XML_TOOLBAR,pUI,&i);
  res = mDocumentContainer->LoadURL(pUI,nsnull);
#endif

  mShellInstance->ShowApplicationWindow(PR_TRUE) ;

  return res ;
}

nsresult nsCalendarShell::SetObserverManager(nsIXPFCObserverManager * aObserverManager)
{
  mObserverManager = aObserverManager;
  return NS_OK;
}

nsIXPFCObserverManager * nsCalendarShell::GetObserverManager()
{
  return (mObserverManager);
}


nsresult nsCalendarShell::SetCAPIPassword(char * aCAPIPassword)
{
  if (mCAPIPassword)
    PR_Free(mCAPIPassword);

  mCAPIPassword = (char *) PR_Malloc(nsCRT::strlen(aCAPIPassword)+1);

  nsCRT::memcpy(mCAPIPassword, aCAPIPassword, nsCRT::strlen(aCAPIPassword)+1);

  return NS_OK;
}

char * nsCalendarShell::GetCAPIPassword()
{
  return (mCAPIPassword);
}


nsresult nsCalendarShell::SetCAPIHandle(CAPIHandle aCAPIHandle)
{
  mCAPIHandle = aCAPIHandle;
  return NS_OK;
}

CAPIHandle nsCalendarShell::GetCAPIHandle()
{
  return (mCAPIHandle);
}

nsresult nsCalendarShell::SetCAPISession(CAPISession aCAPISession)
{
  mCAPISession = aCAPISession;
  return NS_OK;
}

CAPISession nsCalendarShell::GetCAPISession()
{
  return (mCAPISession);
}

nsresult nsCalendarShell::SetNSCalendar(NSCalendar * aNSCalendar)
{
  m_pCalendar = aNSCalendar;
  return NS_OK;
}

NSCalendar * nsCalendarShell::GetNSCalendar()
{
  return (m_pCalendar);
}


void nsCalendarShell::Create(int* argc, char ** argv)
{
  return;
}
void nsCalendarShell::Exit()
{
  NS_IF_RELEASE(mDocumentContainer);

  NLS_Terminate();

  return;
}

nsresult nsCalendarShell::Run()
{
  mShellInstance->Run();
  return NS_OK;
}

void nsCalendarShell::SetDispatchListener(nsDispatchListener* aDispatchListener)
{
  return ;
}
void* nsCalendarShell::GetNativeData(PRUint32 aDataType)
{
#ifdef XP_UNIX
  if (aDataType == NS_NATIVE_SHELL)
    return topLevel;

  return nsnull;
#else
  return (mShellInstance->GetApplicationWidget());
#endif

}

nsresult nsCalendarShell::RegisterFactories()
{
  /*
    Until an installer does this for us, we do it here
  */

  // hardcode names of dll's
#ifdef NS_WIN32
  #define CALUI_DLL "calui10.dll"
  #define XPFC_DLL  "xpfc10.dll"
#else
  #define CALUI_DLL "libcalui10.so"
  #define XPFC_DLL "libxpfc10.so"
#endif

  // register graphics classes
  static NS_DEFINE_IID(kCXPFCCanvasCID, NS_XPFC_CANVAS_CID);
  static NS_DEFINE_IID(kCCalTimeContextCID, NS_CAL_TIME_CONTEXT_CID);
  static NS_DEFINE_IID(kCCalContextControllerCID, NS_CAL_CONTEXT_CONTROLLER_CID);

  nsRepository::RegisterFactory(kCXPFCCanvasCID, XPFC_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCCalTimeContextCID, CALUI_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCCalContextControllerCID, CALUI_DLL, PR_FALSE, PR_FALSE);

  static NS_DEFINE_IID(kCVectorCID, NS_VECTOR_CID);
  static NS_DEFINE_IID(kCVectorIteratorCID, NS_VECTOR_ITERATOR_CID);

  nsRepository::RegisterFactory(kCVectorCID, XPFC_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCVectorIteratorCID, XPFC_DLL, PR_FALSE, PR_FALSE);

  static NS_DEFINE_IID(kCXPFCObserverManagerCID,   NS_XPFC_OBSERVERMANAGER_CID);
  nsRepository::RegisterFactory(kCXPFCObserverManagerCID, XPFC_DLL, PR_FALSE, PR_FALSE);

  // Register the CAPI implementations
  nsRepository::RegisterFactory(kCCapiLocalCID, CAPI_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCCapiCSTCID,   CAPI_DLL, PR_FALSE, PR_FALSE);

  // Register the Core Implementations
  nsRepository::RegisterFactory(kCLayerCID, CORE_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCLayerCollectionCID, CORE_DLL, PR_FALSE, PR_FALSE);

  return NS_OK;
}

nsresult nsCalendarShell::GetWebViewerContainer(nsIWebViewerContainer ** aWebViewerContainer)
{
  *aWebViewerContainer = (nsIWebViewerContainer*) mDocumentContainer;
  NS_ADDREF(*aWebViewerContainer);
  return NS_OK;
}

nsEventStatus nsCalendarShell::HandleEvent(nsGUIEvent *aEvent)
{
  nsEventStatus result = nsEventStatus_eConsumeNoDefault;

  switch(aEvent->message) {

      case NS_CREATE:
      {
        return nsEventStatus_eConsumeNoDefault;
      }
      break ;

      case NS_DESTROY:
      {
        if (mShellInstance->GetApplicationWidget() == aEvent->widget)
        {
          mShellInstance->ExitApplication() ;
          result = mDocumentContainer->HandleEvent(aEvent);
          Exit();
          return result;
        }
        if (mDocumentContainer != nsnull)
          return (mDocumentContainer->HandleEvent(aEvent));
      }
      break ;

      case NS_PAINT:
      {
        ((nsPaintEvent*)aEvent)->rect->x = 0 ;
        ((nsPaintEvent*)aEvent)->rect->y = 0 ;
        if (mDocumentContainer != nsnull)
          return (mDocumentContainer->HandleEvent(aEvent));
      }
      break;

      case NS_SIZE:
      {
        ((nsSizeEvent*)aEvent)->windowSize->x = 0 ;
        ((nsSizeEvent*)aEvent)->windowSize->y = 0 ;
      }
      default:
      {
        if (mDocumentContainer != nsnull)
          return (mDocumentContainer->HandleEvent(aEvent));
      }
      break;

  }

  return nsEventStatus_eIgnore; 

}

nsresult nsCalendarShell::ReceiveCommand(nsString& aCommand, nsString& aReply)
{
  /*
   * Call SendCommand on the CommandCanvas!
   */
  nsIXPFCCanvas * root = nsnull ;
  nsIXPFCCanvas * canvas = nsnull;

  nsString name("CommandCanvas");

  mDocumentContainer->GetDocumentWidget()->GetRootCanvas(&root);

  canvas = root->CanvasFromName(name);

  if (canvas != nsnull)
  {
    nsCalCommandCanvas * cc ;

    static NS_DEFINE_IID(kCalCommandCanvasCID, NS_CAL_COMMANDCANVAS_CID);
    nsresult res = canvas->QueryInterface(kCalCommandCanvasCID,(void**)&cc);

    if (res == NS_OK)
    {
      cc->SendCommand(aCommand, aReply);
      NS_RELEASE(cc);
    }
  }

  NS_RELEASE(root);
  return NS_OK;
}

nsresult nsCalendarShell::StartCommandServer()
{

  if (mCommandServer)
    return NS_OK;

  static NS_DEFINE_IID(kICommandServerIID, NS_IXPFC_COMMAND_SERVER_IID);
  static NS_DEFINE_IID(kCCommandServerCID, NS_XPFC_COMMAND_SERVER_CID);
  static NS_DEFINE_IID(kIApplicationShellIID, NS_IAPPLICATIONSHELL_IID);

  nsresult res = nsRepository::CreateInstance(kCCommandServerCID, 
                                              nsnull, 
                                              kICommandServerIID, 
                                              (void **)&mCommandServer);

  if (NS_OK != res)
    return res ;

  mCommandServer->Init((nsIApplicationShell *)this);
 
  return NS_OK;
}
