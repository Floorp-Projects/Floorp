/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
#include "nsIRobotSink.h"
#include "nsIRobotSinkObserver.h"
#include "nsIParser.h"
#include "nsIWebWidget.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "nsIURL.h"
#include "nsIStreamListener.h"

static NS_DEFINE_IID(kIRobotSinkObserverIID, NS_IROBOTSINKOBSERVER_IID);

class RobotSinkObserver : public nsIRobotSinkObserver {
public:
  RobotSinkObserver() {
    NS_INIT_REFCNT();
  }

  ~RobotSinkObserver() {
  }

  NS_DECL_ISUPPORTS

  NS_IMETHOD ProcessLink(const nsString& aURLSpec);
  NS_IMETHOD VerifyDirectory (const char * verify_dir);
    
};

static nsVoidArray * g_workList;
static nsVoidArray * g_duplicateList;
static int g_iProcessed;
static int g_iMaxProcess = 5000;
static PRBool g_bHitTop;
static PRBool g_bReadyForNextUrl;

NS_IMPL_ISUPPORTS(RobotSinkObserver, kIRobotSinkObserverIID);

NS_IMETHODIMP RobotSinkObserver::VerifyDirectory(const char * verify_dir)
{
   return NS_OK;
}

NS_IMETHODIMP RobotSinkObserver::ProcessLink(const nsString& aURLSpec)
{
  if (!g_bHitTop) {
     
     nsAutoString str;
     // Geez this is ugly. temporary hack to not process html files
     str.Truncate();
     nsString(aURLSpec).Right(str,1);
     if (!str.Equals("/"))
     {
        str.Truncate();
        nsString(aURLSpec).Right(str,4);
        if (!str.Equals("html"))
        {
           str.Truncate();
           nsString(aURLSpec).Right(str,3);
           if (!str.Equals("htm"))
              return NS_OK;
        }
     }
     PRInt32 nCount = g_duplicateList->Count();
     if (nCount > 0)
     {
        for (PRInt32 n = 0; n < nCount; n++)
        {
           nsString * pstr = (nsString *)g_duplicateList->ElementAt(n);
           if (pstr->Equals(aURLSpec)) {
              fputs ("DR: (duplicate found '",stdout);
              fputs (aURLSpec,stdout);
              fputs ("')\n",stdout);
              return NS_OK;
           }
        }
     }
     g_duplicateList->AppendElement(new nsString(aURLSpec));
     str.Truncate();
     nsString(aURLSpec).Left(str,5);
     if (str.Equals("http:")) {
        char str_num[25];
        g_iProcessed++;
        if (g_iProcessed == g_iMaxProcess)
           g_bHitTop = PR_TRUE;
        sprintf(str_num, "%d", g_iProcessed);
        g_workList->AppendElement(new nsString(aURLSpec));
        fputs("DebugRobot ",stdout);
        fputs(str_num, stdout);
        fputs(": ",stdout);
        fputs(aURLSpec,stdout);
        fputs("\n", stdout);
     }
     else {
        fputs ("DR: (cannot process URL types '",stdout);
        fputs (aURLSpec,stdout);
        fputs ("')\n",stdout);
     }
  }
  return NS_OK;
}

extern "C" NS_EXPORT void SetVerificationDirectory(char * verify_dir);

class CStreamListener:  public nsIStreamListener 
{
public:
  CStreamListener() {
    NS_INIT_REFCNT();
  }

  ~CStreamListener() {
  }

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetBindInfo(void) { return NS_OK; }
  NS_IMETHOD OnProgress(PRInt32 Progress, PRInt32 ProgressMax, const nsString& aMsg) { return NS_OK; }
  NS_IMETHOD OnStartBinding(const char *aContentType) { return NS_OK; }
  NS_IMETHOD OnDataAvailable(nsIInputStream *pIStream, PRInt32 length)   { return NS_OK; }
  NS_IMETHOD OnStopBinding(PRInt32 status, const nsString& aMsg);
};

NS_IMETHODIMP CStreamListener::OnStopBinding(PRInt32 status, const nsString& aMsg)
{
   fputs("CStreamListener: stream complete: ", stdout);
   fputs(aMsg, stdout);
   fputs("\n", stdout);
   g_bReadyForNextUrl = PR_TRUE;
   return NS_OK;
}

nsresult CStreamListener::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                        
  return NS_OK;                                                        
}

NS_IMPL_ADDREF(CStreamListener)
NS_IMPL_RELEASE(CStreamListener)

//----------------------------------------------------------------------
extern "C" NS_EXPORT int DebugRobot(
   nsVoidArray * workList, 
   nsIWebWidget * ww, 
   int iMaxLoads, 
   char * verify_dir,
   void (*yieldProc )(const char *)
   )
{
  CStreamListener * pl = new CStreamListener; 
  NS_ADDREF(pl);
  if (nsnull==workList)
     return -1;
  g_iMaxProcess = iMaxLoads;
  g_iProcessed = 0;
  g_bHitTop = PR_FALSE;
  g_duplicateList = new nsVoidArray();
  RobotSinkObserver* myObserver = new RobotSinkObserver();
  //SetVerificationDirectory(verify_dir);
  NS_ADDREF(myObserver);
  g_workList = workList;

  for (;;) {
    PRInt32 n = g_workList->Count();
    if (0 == n) {
      break;
    }
    nsString* urlName = (nsString*) g_workList->ElementAt(n - 1);
    g_workList->RemoveElementAt(n - 1);

    // Create url
    nsIURL* url;
    nsresult rv = NS_NewURL(&url, *urlName);
    if (NS_OK != rv) {
      printf("invalid URL: '");
      fputs(*urlName, stdout);
      printf("'\n");
      return -1;
    }
    delete urlName;

    nsIParser* parser;
    rv = NS_NewHTMLParser(&parser);
    if (NS_OK != rv) {
      printf("can't make parser\n");
      return -1;
    }

    nsIRobotSink* sink;
    rv = NS_NewRobotSink(&sink);
    if (NS_OK != rv) {
      printf("can't make parser\n");
      return -1;
    }
    sink->Init(url);
    sink->AddObserver(myObserver);

    parser->SetContentSink(sink);
    g_bReadyForNextUrl = PR_FALSE;
    parser->Parse(url, pl);/* XXX hook up stream listener here! */
    while (!g_bReadyForNextUrl) {
       if (yieldProc != NULL)
          (*yieldProc)(url->GetSpec());
    }
    g_bReadyForNextUrl = PR_FALSE;
    if (ww) {
      ww->LoadURL(url->GetSpec(), pl);/* XXX hook up stream listener here! */
      while (!g_bReadyForNextUrl) {
         if (yieldProc != NULL)
            (*yieldProc)(url->GetSpec());
      }
    }  

    NS_RELEASE(sink);
    NS_RELEASE(parser);
    NS_RELEASE(url);
  }

  NS_RELEASE(pl);
  NS_RELEASE(myObserver);

  return 0;
}
