/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/*
   wallet.cpp
*/

#include "wallet.h"
#ifndef NECKO
#include "nsINetService.h"
#else
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#endif // NECKO

#include "nsIServiceManager.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIURL.h"

#include "nsFileStream.h"
#include "nsSpecialSystemDirectory.h"

#include "nsINetSupportDialogService.h"
#include "nsIStringBundle.h"
#include "nsILocale.h"
#include "nsIFileLocator.h"
#include "nsIFileSpec.h"
#include "nsFileLocations.h"
#include "prmem.h"
#include "prprf.h"  
#include "nsIProfile.h"
#include "nsIContent.h"
#include "nsVoidArray.h"

static NS_DEFINE_IID(kIDOMHTMLDocumentIID, NS_IDOMHTMLDOCUMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLFormElementIID, NS_IDOMHTMLFORMELEMENT_IID);
static NS_DEFINE_IID(kIDOMElementIID, NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLInputElementIID, NS_IDOMHTMLINPUTELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLSelectElementIID, NS_IDOMHTMLSELECTELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLOptionElementIID, NS_IDOMHTMLOPTIONELEMENT_IID);

#ifndef NECKO
static NS_DEFINE_IID(kINetServiceIID, NS_INETSERVICE_IID);
static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);
#else
static NS_DEFINE_IID(kIIOServiceIID, NS_IIOSERVICE_IID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
#endif // NECKO

static NS_DEFINE_CID(kNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);
static NS_DEFINE_CID(kProfileCID, NS_PROFILE_CID);

static NS_DEFINE_IID(kIStringBundleServiceIID, NS_ISTRINGBUNDLESERVICE_IID);
static NS_DEFINE_IID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

static NS_DEFINE_IID(kIFileLocatorIID, NS_IFILELOCATOR_IID);
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);

#include "prlong.h"
#include "prinrval.h"

/*************************************
 * Code that really belongs in necko *
 *************************************/

#ifdef WIN32 
#include <windows.h>
#endif
#include "nsFileStream.h"
#include "nsIFileSpec.h"
#include "nsIEventQueueService.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsIStreamObserver.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIChannel.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"

static NS_DEFINE_CID(kEventQueueServiceCID,      NS_EVENTQUEUESERVICE_CID);
#ifdef ReallyInNecko
static NS_DEFINE_CID(kIOServiceCID,              NS_IOSERVICE_CID);
#endif

static int gKeepRunning = 0;
static nsIEventQueue* gEventQ = nsnull;

class InputConsumer : public nsIStreamListener
{
public:

  InputConsumer();
  virtual ~InputConsumer();

  // ISupports interface...
  NS_DECL_ISUPPORTS

  // IStreamListener interface...
  NS_IMETHOD OnStartRequest(nsIChannel* channel, nsISupports* context);

  NS_IMETHOD OnDataAvailable(nsIChannel* channel, nsISupports* context,
                             nsIInputStream *aIStream, 
                             PRUint32 aSourceOffset,
                             PRUint32 aLength);

  NS_IMETHOD OnStopRequest(nsIChannel* channel, nsISupports* context,
                           nsresult aStatus,
                           const PRUnichar* aMsg);

  NS_IMETHOD Init(nsFileSpec dirSpec, const char *out);

  nsOutputFileStream   *mOutFile;

};


InputConsumer::InputConsumer():
mOutFile(nsnull)
{
  NS_INIT_REFCNT();
}


InputConsumer::~InputConsumer()
{
  if (mOutFile) {
      delete mOutFile;
  }
}


NS_IMPL_ISUPPORTS(InputConsumer,nsCOMTypeInfo<nsIStreamListener>::GetIID());


NS_IMETHODIMP
InputConsumer::OnStartRequest(nsIChannel* channel, nsISupports* context)
{
    if (! mOutFile->is_open()) 
    {
       return NS_ERROR_FAILURE;
    }
    return NS_OK;
}


NS_IMETHODIMP
InputConsumer::OnDataAvailable(nsIChannel* channel, 
                               nsISupports* context,
                               nsIInputStream *aIStream, 
                               PRUint32 aSourceOffset,
                               PRUint32 aLength)
{
  char buf[1001];
  PRUint32 amt;
  nsresult rv;
  do {
    rv = aIStream->Read(buf, 1000, &amt);
    if (rv == NS_BASE_STREAM_EOF) break;
    if (NS_FAILED(rv)) return rv;
    buf[amt] = '\0';
    mOutFile->write(buf,amt);
  } while (amt);

  return NS_OK;
}

NS_IMETHODIMP
InputConsumer::OnStopRequest(nsIChannel* channel, 
                             nsISupports* context,
                             nsresult aStatus,
                             const PRUnichar* aMsg)
{
  if (mOutFile) {
      mOutFile->flush();
      mOutFile->close();
  }
  gKeepRunning = 0;
  return NS_OK;
}

NS_IMETHODIMP
InputConsumer::Init(nsFileSpec dirSpec, const char *out)

{
  mOutFile = new nsOutputFileStream(dirSpec+out);
  return NS_OK;
}

#ifdef ReallyInNecko
NECKO_EXPORT(nsresult)
#else
nsresult
#endif
NS_NewURItoFile(const char *in, nsFileSpec dirSpec, const char *out)
{
    nsresult rv;
    gKeepRunning = 0;

    // Create the Event Queue for this thread...
    NS_WITH_SERVICE(nsIEventQueueService, eventQService, 
                    kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = eventQService->CreateThreadEventQueue();
    if (NS_FAILED(rv)) return rv;

    rv = eventQService->GetThreadEventQueue(PR_CurrentThread(), &gEventQ);
    if (NS_FAILED(rv)) return rv;

    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIURI> pURL;
    rv = serv->NewURI(in, nsnull, getter_AddRefs(pURL));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIChannel> pChannel;

    // Async reading thru the calls of the event sink interface
    rv = serv->NewChannelFromURI("load", pURL, nsnull, 
                                 getter_AddRefs(pChannel));
    if (NS_FAILED(rv)) {
        printf("ERROR: NewChannelFromURI failed for %s\n", in);
        return rv;
    }
            
    InputConsumer* listener;
    listener = new InputConsumer;
    NS_IF_ADDREF(listener);
    if (!listener) {
        NS_ERROR("Failed to create a new stream listener!");
        return NS_ERROR_OUT_OF_MEMORY;;
    }
    rv = listener->Init(dirSpec, out);

    if (NS_FAILED(rv)) {
        NS_RELEASE(listener);
        return rv;
    }

    rv = pChannel->AsyncRead(0,         // starting position
                             -1,        // number of bytes to read
                             nsnull,    // ISupports context
                             listener); // IStreamListener consumer

    if (NS_SUCCEEDED(rv)) {
         gKeepRunning = 1;
    }

    NS_RELEASE(listener);
    if (NS_FAILED(rv)) return rv;

    // Enter the message pump to allow the URL load to proceed.
    while ( gKeepRunning ) {
#ifdef WIN32
        MSG msg;

        if (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            gKeepRunning = 0;
        }
#else
        PLEvent *gEvent;
        rv = gEventQ->GetEvent(&gEvent);
        if (NS_SUCCEEDED(rv)) {
            rv  = gEventQ->HandleEvent(gEvent);
        }
#endif /* !WIN32 */
    }
    return rv;
}

/***************************************************/
/* The following declarations define the data base */
/***************************************************/

enum PlacementType {DUP_IGNORE, DUP_OVERWRITE, DUP_BEFORE, DUP_AFTER, AT_END};

typedef struct _wallet_MapElement {
  nsAutoString * item1;
  nsAutoString * item2;
  nsVoidArray * itemList;
} wallet_MapElement;

typedef struct _wallet_Sublist {
  nsAutoString * item;
} wallet_Sublist;

PRIVATE nsVoidArray * wallet_URLFieldToSchema_list=0;
PRIVATE nsVoidArray * wallet_specificURLFieldToSchema_list=0;
PRIVATE nsVoidArray * wallet_FieldToSchema_list=0;
PRIVATE nsVoidArray * wallet_SchemaToValue_list=0;
PRIVATE nsVoidArray * wallet_SchemaConcat_list=0;
PRIVATE nsVoidArray * wallet_URL_list = 0;

#define LIST_COUNT(list) (list ? list->Count() : 0)

#define NO_CAPTURE 0
#define NO_PREVIEW 1

typedef struct _wallet_PrefillElement {
  nsIDOMHTMLInputElement* inputElement;
  nsIDOMHTMLSelectElement* selectElement;
  nsAutoString * schema;
  nsAutoString * value;
  PRInt32 selectIndex;
  PRUint32 count;
} wallet_PrefillElement;

/***********************************************************/
/* The following routines are for diagnostic purposes only */
/***********************************************************/

#ifdef DEBUG

void
wallet_Pause(){
  fprintf(stdout,"%cpress y to continue\n", '\007');
  char c;
  for (;;) {
    c = getchar();
    if (tolower(c) == 'y') {
      fprintf(stdout,"OK\n");
      break;
    }
  }
  while (c != '\n') {
    c = getchar();
  }
}

void
wallet_DumpAutoString(nsAutoString as){
  char s[100];
  as.ToCString(s, 100);
  fprintf(stdout, "%s\n", s);
}

void
wallet_Dump(nsVoidArray * list) {
  wallet_MapElement * ptr;
  char item1[100];
  char item2[100];
  char item[100];
  PRInt32 count = LIST_COUNT(list);
  for (PRInt32 i=0; i<count; i++) {
    ptr = NS_STATIC_CAST(wallet_MapElement*, list->ElementAt(i));
    ptr->item1->ToCString(item1, 100);
    ptr->item2->ToCString(item2, 100);
    fprintf(stdout, "%s %s \n", item1, item2);
    wallet_Sublist * ptr1;
    PRInt32 count2 = LIST_COUNT(ptr->itemList);
    for (PRInt32 i2=0; i2<count2; i2++) {
      ptr1 = NS_STATIC_CAST(wallet_Sublist*, ptr->itemList->ElementAt(i2));
      ptr1->item->ToCString(item, 100);
      fprintf(stdout, "     %s \n", item);
    }
  }
  wallet_Pause();
}

/******************************************************************/
/* The following diagnostic routines are for timing purposes only */
/******************************************************************/

const PRInt32 timing_max = 1000;
PRInt64 timings [timing_max];
char timingID [timing_max];
PRInt32 timing_index = 0;

PRInt64 stopwatch = LL_Zero();
PRInt64 stopwatchBase;
PRBool stopwatchRunning = PR_FALSE;

void
wallet_ClearTiming() {
  timing_index  = 0;
}

void
wallet_DumpTiming() {
  PRInt32 i, r4;
  PRInt64 r1, r2, r3;
  for (i=1; i<timing_index; i++) {
#ifndef	XP_MAC
    LL_SUB(r1, timings[i], timings[i-1]);
    LL_I2L(r2, 100);
    LL_DIV(r3, r1, r2);
    LL_L2I(r4, r3);
    fprintf(stdout, "time %c = %ld\n", timingID[i], (long)r4);
#endif
    if (i%20 == 0) {
      wallet_Pause();
    }
  }
  wallet_Pause();
}

void
wallet_AddTiming(char c) {
  if (timing_index<timing_max) {
    timingID[timing_index] = c;
#ifndef	XP_MAC
    // note: PR_IntervalNow returns a 32 bit value!
    LL_I2L(timings[timing_index++], PR_IntervalNow());
#endif
  }
}

void
wallet_ClearStopwatch() {
  stopwatch = LL_Zero();
  stopwatchRunning = PR_FALSE;
}

void
wallet_ResumeStopwatch() {
  if (!stopwatchRunning) {
#ifndef	XP_MAC
    // note: PR_IntervalNow returns a 32 bit value!
    LL_I2L(stopwatchBase, PR_IntervalNow());
#endif
    stopwatchRunning = PR_TRUE;
  }
}

void
wallet_PauseStopwatch() {
  PRInt64 r1, r2;
  if (stopwatchRunning) {
#ifndef	XP_MAC
    // note: PR_IntervalNow returns a 32 bit value!
    LL_I2L(r1, PR_IntervalNow());
    LL_SUB(r2, r1, stopwatchBase);
    LL_ADD(stopwatch, stopwatch, r2);
#endif
    stopwatchRunning = PR_FALSE;
  }
}

void
wallet_DumpStopwatch() {
  PRInt64 r1, r2;
  PRInt32 r3;
  if (stopwatchRunning) {
#ifndef	XP_MAC
    // note: PR_IntervalNow returns a 32 bit value!
    LL_I2L(r1, PR_IntervalNow());
    LL_SUB(r2, r1, stopwatchBase);
    LL_ADD(stopwatch, stopwatch, r2);
    LL_I2L(stopwatchBase, PR_IntervalNow());
#endif
  }
#ifndef	XP_MAC
  LL_I2L(r1, 100);
  LL_DIV(r2, stopwatch, r1);
  LL_L2I(r3, r2);
  fprintf(stdout, "stopwatch = %ld\n", (long)r3);  
#endif
}
#endif /* DEBUG */

/********************************************************/
/* The following data and procedures are for preference */
/********************************************************/

typedef int (*PrefChangedFunc) (const char *, void *);

extern void
SI_RegisterCallback(const char* domain, PrefChangedFunc callback, void* instance_data);

extern PRBool
SI_GetBoolPref(const char * prefname, PRBool defaultvalue);

static const char *pref_captureForms =
    "wallet.captureForms";
PRIVATE Bool wallet_captureForms = PR_FALSE;

PRIVATE void
wallet_SetFormsCapturingPref(Bool x)
{
    /* do nothing if new value of pref is same as current value */
    if (x == wallet_captureForms) {
        return;
    }

    /* change the pref */
    wallet_captureForms = x;
}

MODULE_PRIVATE int PR_CALLBACK
wallet_FormsCapturingPrefChanged(const char * newpref, void * data)
{
    PRBool x;
    x = SI_GetBoolPref(pref_captureForms, PR_TRUE);
    wallet_SetFormsCapturingPref(x);
    return 0; /* this is PREF_NOERROR but we no longer include prefapi.h */
}

void
wallet_RegisterCapturePrefCallbacks(void)
{
    PRBool x;
    static Bool first_time = PR_TRUE;

    if(first_time)
    {
        first_time = PR_FALSE;
        x = SI_GetBoolPref(pref_captureForms, PR_TRUE);
        wallet_SetFormsCapturingPref(x);
        SI_RegisterCallback(pref_captureForms, wallet_FormsCapturingPrefChanged, NULL);
    }
}

PRIVATE Bool
wallet_GetFormsCapturingPref(void)
{
    wallet_RegisterCapturePrefCallbacks();
    return wallet_captureForms;
}

/*************************************************************************/
/* The following routines are used for accessing strings to be localized */
/*************************************************************************/

#define TEST_URL "resource:/res/wallet.properties"

PUBLIC char*
Wallet_Localize(char* genericString) {
  nsresult ret;
  nsAutoString v("");

  /* create a URL for the string resource file */
#ifndef NECKO
  nsINetService* pNetService = nsnull;
  ret = nsServiceManager::GetService(kNetServiceCID, kINetServiceIID,
    (nsISupports**) &pNetService);
#else
  nsIIOService* pNetService = nsnull;
  ret = nsServiceManager::GetService(kIOServiceCID, kIIOServiceIID,
    (nsISupports**) &pNetService);
#endif // NECKO

  if (NS_FAILED(ret)) {
    printf("cannot get net service\n");
    return v.ToNewCString();
  }
  nsIURI *url = nsnull;

#ifndef NECKO
  ret = pNetService->CreateURL(&url, nsString(TEST_URL), nsnull, nsnull,
    nsnull);
  nsServiceManager::ReleaseService(kNetServiceCID, pNetService);

#else
  nsIURI *uri = nsnull;
  ret = pNetService->NewURI(TEST_URL, nsnull, &uri);
  if (NS_FAILED(ret)) {
    printf("cannot create URI\n");
    nsServiceManager::ReleaseService(kIOServiceCID, pNetService);
    return v.ToNewCString();
  }

  ret = uri->QueryInterface(nsIURI::GetIID(), (void**)&url);
  nsServiceManager::ReleaseService(kIOServiceCID, pNetService);

#endif // NECKO

  if (NS_FAILED(ret)) {
    printf("cannot create URL\n");
    return v.ToNewCString();
  }

  /* create a bundle for the localization */
  nsIStringBundleService* pStringService = nsnull;
  ret = nsServiceManager::GetService(kStringBundleServiceCID,
    kIStringBundleServiceIID, (nsISupports**) &pStringService);
  if (NS_FAILED(ret)) {
    printf("cannot get string service\n");
    return v.ToNewCString();
  }
  nsILocale* locale = nsnull;
  nsIStringBundle* bundle = nsnull;
#if 1
#ifndef NECKO
  const char* spec = nsnull;
#else
  char* spec = nsnull;
#endif /* NECKO */
  ret = url->GetSpec(&spec);
  if (NS_FAILED(ret)) {
    printf("cannot get url spec\n");
    nsServiceManager::ReleaseService(kStringBundleServiceCID, pStringService);
#ifdef NECKO
    nsCRT::free(spec);
#endif /* NECKO */
    return v.ToNewCString();
  }
  ret = pStringService->CreateBundle(spec, locale, &bundle);
#ifdef NECKO
  nsCRT::free(spec);
#endif /* NECKO */
#else
  ret = pStringService->CreateBundle(url, locale, &bundle);
#endif
  if (NS_FAILED(ret)) {
    printf("cannot create instance\n");
    nsServiceManager::ReleaseService(kStringBundleServiceCID, pStringService);
    return v.ToNewCString();
  }
  nsServiceManager::ReleaseService(kStringBundleServiceCID, pStringService);

  /* localize the given string */
#if 1
  nsString   strtmp(genericString);
  const PRUnichar *ptrtmp = strtmp.GetUnicode();
  PRUnichar *ptrv = nsnull;
  ret = bundle->GetStringFromName(ptrtmp, &ptrv);
  v = ptrv;
#else
  ret = bundle->GetStringFromName(nsString(genericString), v);
#endif
  NS_RELEASE(bundle);
  if (NS_FAILED(ret)) {
    printf("cannot get string from name\n");
    return v.ToNewCString();
  }
  return v.ToNewCString();
}

/**********************/
/* Modal dialog boxes */
/**********************/

PUBLIC PRBool
Wallet_Confirm(char * szMessage)
{
#ifdef NECKO
  PRBool retval = PR_TRUE; /* default value */

  nsresult res;  
  NS_WITH_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, &res);
  if (NS_FAILED(res)) {
    return retval;
  }

  const nsString message = szMessage;
  retval = PR_FALSE; /* in case user exits dialog by clicking X */
  res = dialog->Confirm(message.GetUnicode(), &retval);
  if (NS_FAILED(res)) {
    return retval;
  }

  return retval;
#else
  PRBool retval = PR_TRUE; /* default value */
  nsINetSupportDialogService* dialog = NULL;
  nsresult res = nsServiceManager::GetService(kNetSupportDialogCID,
  nsINetSupportDialogService::GetIID(), (nsISupports**)&dialog);
  if (NS_FAILED(res)) {
    return retval;
  }
  if (dialog) {
    const nsString message = szMessage;
    retval = PR_FALSE; /* in case user exits dialog by clicking X */
    dialog->Confirm(message, &retval);
  }
  nsServiceManager::ReleaseService(kNetSupportDialogCID, dialog);
  return retval;
#endif
}

PUBLIC PRBool
Wallet_ConfirmYN(char * szMessage)
{
#ifdef NECKO
  PRBool retval = PR_TRUE; /* default value */

  nsresult res;  
  NS_WITH_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, &res);
  if (NS_FAILED(res)) {
    return retval;
  }

  const nsString message = szMessage;
  retval = PR_FALSE; /* in case user exits dialog by clicking X */
  res = dialog->ConfirmYN(message.GetUnicode(), &retval);
  if (NS_FAILED(res)) {
    return retval;
  }

  return retval;
#else
  PRBool retval = PR_TRUE; /* default value */
  nsINetSupportDialogService* dialog = NULL;
  nsresult res = nsServiceManager::GetService(kNetSupportDialogCID,
  nsINetSupportDialogService::GetIID(), (nsISupports**)&dialog);
  if (NS_FAILED(res)) {
    return retval;
  }
  if (dialog) {
    const nsString message = szMessage;
    retval = PR_FALSE; /* in case user exits dialog by clicking X */
    dialog->ConfirmYN(message, &retval);
  }
  nsServiceManager::ReleaseService(kNetSupportDialogCID, dialog);
  return retval;
#endif
}

PUBLIC void
Wallet_Alert(char * szMessage)
{
#ifdef NECKO
  nsresult res;  
  NS_WITH_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, &res);
  if (NS_FAILED(res)) {
    return;     // XXX should return the error
  }

  const nsString message = szMessage;
  res = dialog->Alert(message.GetUnicode());
  return;     // XXX should return the error
#else
  nsINetSupportDialogService* dialog = NULL;
  nsresult res = nsServiceManager::GetService(kNetSupportDialogCID,
  nsINetSupportDialogService::GetIID(), (nsISupports**)&dialog);
  if (NS_FAILED(res)) {
    return;
  }
  if (dialog) {
    const nsString message = szMessage;
    dialog->Alert(message);
  }
  nsServiceManager::ReleaseService(kNetSupportDialogCID, dialog);
  return;
#endif
}

PUBLIC PRBool
Wallet_CheckConfirmYN(char * szMessage, char * szCheckMessage, PRBool* checkValue)
{
#ifdef NECKO
  PRBool retval = PR_TRUE; /* default value */

  nsresult res;  
  NS_WITH_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, &res);
  if (NS_FAILED(res)) {
    *checkValue = 0;
    return retval;
  }

  const nsString message = szMessage;
  const nsString checkMessage = szCheckMessage;
  retval = PR_FALSE; /* in case user exits dialog by clicking X */
  res = dialog->ConfirmCheckYN(message.GetUnicode(), checkMessage.GetUnicode(), checkValue, &retval);
  if (NS_FAILED(res)) {
    *checkValue = 0;
  }
  if (*checkValue!=0 && *checkValue!=1) {
    *checkValue = 0; /* this should never happen but it is happening!!! */
  }
  return retval;
#else
  PRBool retval = PR_TRUE; /* default value */
  nsINetSupportDialogService* dialog = NULL;
  nsresult res = nsServiceManager::GetService(kNetSupportDialogCID,
  nsINetSupportDialogService::GetIID(), (nsISupports**)&dialog);
  if (NS_FAILED(res)) {
    return retval;
  }
  if (dialog) {
    const nsString message = szMessage;
    const nsString checkMessage = szCheckMessage;
    retval = PR_FALSE; /* in case user exits dialog by clicking X */
    dialog->ConfirmCheckYN(message, checkMessage, &retval, checkValue);
    if (*checkValue!=0 && *checkValue!=1) {
      *checkValue = 0; /* this should never happen but it is happening!!! */
    }
  }
  nsServiceManager::ReleaseService(kNetSupportDialogCID, dialog);
  return retval;
#endif
}

char * wallet_GetString(char * szMessage)
{
#ifdef NECKO
  nsString password;
  PRBool retval;

  nsresult res;  
  NS_WITH_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, &res);
  if (NS_FAILED(res)) {
    return NULL;     // XXX should return the error
  }

  const nsString message = szMessage;
  PRUnichar* pwd;
  retval = PR_FALSE; /* in case user exits dialog by clicking X */
  res = dialog->PromptPassword(message.GetUnicode(), &pwd, &retval);
  if (NS_FAILED(res)) {
    return NULL;
  }
  password = pwd;
  delete[] pwd;

  if (retval) {
    return password.ToNewCString();
  } else {
    return NULL; /* user pressed cancel */
  }
#else
  nsString password;
  PRBool retval;
  nsINetSupportDialogService* dialog = NULL;
  nsresult res = nsServiceManager::GetService(kNetSupportDialogCID,
  nsINetSupportDialogService::GetIID(), (nsISupports**)&dialog);
  if (NS_FAILED(res)) {
    return NULL; /* failure value */
  }
  if (dialog) {
    const nsString message = szMessage;
    retval = PR_FALSE; /* in case user exits dialog by clicking X */
    dialog->PromptPassword(message, password, &retval);
  }
  nsServiceManager::ReleaseService(kNetSupportDialogCID, dialog);
  if (retval) {
    return password.ToNewCString();
  } else {
    return NULL; /* user pressed cancel */
  }
#endif
}

/**********************************************************************************/
/* The following routines are for locking the data base.  They are not being used */
/**********************************************************************************/

#ifdef junk
//#include "prpriv.h" /* for NewNamedMonitor */

static PRMonitor * wallet_lock_monitor = NULL;
static PRThread  * wallet_lock_owner = NULL;
static int wallet_lock_count = 0;

PRIVATE void
wallet_lock(void) {
  if(!wallet_lock_monitor) {
//        wallet_lock_monitor =
//            PR_NewNamedMonitor("wallet-lock");
  }

  PR_EnterMonitor(wallet_lock_monitor);

  while(PR_TRUE) {

    /* no current owner or owned by this thread */
    PRThread * t = PR_CurrentThread();
    if(wallet_lock_owner == NULL || wallet_lock_owner == t) {
      wallet_lock_owner = t;
      wallet_lock_count++;

      PR_ExitMonitor(wallet_lock_monitor);
      return;
    }

    /* owned by someone else -- wait till we can get it */
    PR_Wait(wallet_lock_monitor, PR_INTERVAL_NO_TIMEOUT);
  }
}

PRIVATE void
wallet_unlock(void) {
  PR_EnterMonitor(wallet_lock_monitor);

#ifdef DEBUG
  /* make sure someone doesn't try to free a lock they don't own */
  PR_ASSERT(wallet_lock_owner == PR_CurrentThread());
#endif

  wallet_lock_count--;

  if(wallet_lock_count == 0) {
    wallet_lock_owner = NULL;
    PR_Notify(wallet_lock_monitor);
  }
  PR_ExitMonitor(wallet_lock_monitor);
}

#endif

/**********************************************************/
/* The following routines are for accessing the data base */
/**********************************************************/

/*
 * clear out the designated list
 */
void
wallet_Clear(nsVoidArray ** list) {
  wallet_MapElement * ptr;
  PRInt32 count = LIST_COUNT((*list));
  for (PRInt32 i=count-1; i>=0; i--) {
    ptr = NS_STATIC_CAST(wallet_MapElement*, (*list)->ElementAt(i));
    delete ptr->item1;
    delete ptr->item2;
    wallet_Sublist * ptr1;
    PRInt32 count2 = LIST_COUNT(ptr->itemList);
    for (PRInt32 i2=0; i2<count2; i2++) {
      ptr1 = NS_STATIC_CAST(wallet_Sublist*, ptr->itemList->ElementAt(i2));
      delete ptr1->item;
    }
    delete ptr->itemList;
    (*list)->RemoveElement(ptr);
    delete ptr;
  }
  *list = 0;
}

/*
 * add an entry to the designated list
 */
void
wallet_WriteToList(
    nsAutoString& item1,
    nsAutoString& item2,
    nsVoidArray* itemList,
    nsVoidArray*& list,
    PlacementType placement = DUP_BEFORE) {

  wallet_MapElement * ptr;
  PRBool added_to_list = PR_FALSE;

  wallet_MapElement * mapElement;
  mapElement = PR_NEW(wallet_MapElement);

  mapElement->item1 = &item1;
  mapElement->item2 = &item2;
  mapElement->itemList = itemList;

  /* make sure the list exists */
  if(!list) {
      list = new nsVoidArray();
      if(!list) {
          return;
      }
  }

  /*
   * Add new entry to the list in alphabetical order by item1.
   * If identical value of item1 exists, use "placement" parameter to 
   * determine what to do
   */
  item1.ToLowerCase();
  if (AT_END==placement) {
    list->AppendElement(mapElement);
    return;
  }
  PRInt32 count = LIST_COUNT(list);
  for (PRInt32 i=0; i<count; i++) {
    ptr = NS_STATIC_CAST(wallet_MapElement*, list->ElementAt(i));
    if((ptr->item1->Compare(item1))==0) {
      if (DUP_OVERWRITE==placement) {
        delete ptr->item1;
        delete ptr->item2;
        delete mapElement;
        ptr->item1 = &item1;
        ptr->item2 = &item2;
      } else if (DUP_BEFORE==placement) {
        list->InsertElementAt(mapElement, i);
      }
      if (DUP_AFTER!=placement) {
        added_to_list = PR_TRUE;
        break;
      }
    } else if((ptr->item1->Compare(item1))>=0) {
      list->InsertElementAt(mapElement, i);
      added_to_list = PR_TRUE;
      break;
    }
  }
  if (!added_to_list) {
    list->AppendElement(mapElement);
  }
}

/*
 * fetch an entry from the designated list
 */
PRBool
wallet_ReadFromList(
  nsAutoString item1,
  nsAutoString& item2,
  nsVoidArray*& itemList,
  nsVoidArray*& list,
  PRInt32& index)
{
  if (!list || (index == -1)) {
    return PR_FALSE;
  }

  /* find item1 in the list */
  wallet_MapElement * ptr;
  item1.ToLowerCase();
  PRInt32 count = LIST_COUNT(list);
  for (PRInt32 i=index; i<count; i++) {
    ptr = NS_STATIC_CAST(wallet_MapElement*, list->ElementAt(i));
    if((ptr->item1->Compare(item1))==0) {
      item2 = nsAutoString(*ptr->item2);
      itemList = ptr->itemList;
      index = i+1;
      if (index == count) {
        index = -1;
      }
      return PR_TRUE;
    }
  }
  index = 0;
  return PR_FALSE;
}

PRBool
wallet_ReadFromList(
  nsAutoString item1,
  nsAutoString& item2,
  nsVoidArray*& itemList,
  nsVoidArray*& list)
{
  PRInt32 index = 0;
  return wallet_ReadFromList(item1, item2, itemList, list, index);
}

/************************************************************/
/* The following routines are for unlocking the stored data */
/************************************************************/

#define maxKeySize 100
char key[maxKeySize+1];
PRUint32 keyPosition = 0;
PRBool keyCancel = PR_FALSE;
PRBool keySet = PR_FALSE;

PUBLIC void
Wallet_RestartKey() {
  keyPosition = 0;
}

PUBLIC char
Wallet_GetKey() {
  if (keyPosition >= PL_strlen(key)) {
    keyPosition = 0;
  }
  return key[keyPosition++];
}

PUBLIC PRBool
Wallet_KeySet() {
  return keySet;
}

PUBLIC PRBool
Wallet_CancelKey() {
  return keyCancel;
}

PUBLIC nsresult Wallet_ProfileDirectory(nsFileSpec& dirSpec) {
  /* return the profile */
  nsIFileSpec* spec =
    NS_LocateFileOrDirectory(nsSpecialFileSpec::App_UserProfileDirectory50);
  if (!spec) {
    return NS_ERROR_FAILURE;
  }
  return spec->GetFileSpec(&dirSpec);
}

PUBLIC nsresult Wallet_ResourceDirectory(nsFileSpec& dirSpec) {
  nsIFileSpec* spec =
    NS_LocateFileOrDirectory(nsSpecialFileSpec::App_ResDirectory);
  if (!spec) {
    return NS_ERROR_FAILURE;
  }
  return spec->GetFileSpec(&dirSpec);
}

/* returns -1 if key does not exist, 0 if key is of length 0, 1 otherwise */
PRIVATE PRInt32
wallet_KeySize() {
  nsFileSpec dirSpec;
  nsresult rv = Wallet_ProfileDirectory(dirSpec);
  if (NS_FAILED(rv)) {
    return -1;
  }
  nsInputFileStream strm(dirSpec + "key");
  if (!strm.is_open()) {
    return -1;
  } else {
    strm.get();
    PRInt32 ret = (strm.eof() ? 0 : 1);
    strm.close();
    return ret;
  }
}

PUBLIC PRBool
Wallet_SetKey(PRBool isNewkey) {
  if (Wallet_KeySet() && !isNewkey) {
    return PR_TRUE;
  }

  Wallet_RestartKey();

  /* ask the user for his key */
  char * password;
  if (wallet_KeySize() < 0) { /* no password has yet been established */
    password = Wallet_Localize("firstPassword");
  } else if (isNewkey) {
    password = Wallet_Localize("newPassword");
  } else {
    password = Wallet_Localize("password");
  }

  char * newkey;
  if ((wallet_KeySize() == 0) && !isNewkey) {
    newkey = PL_strdup("");
  } else {
    newkey = wallet_GetString(password);
  }
  keyCancel = PR_FALSE;
  if (newkey == NULL) { /* user hit cancel button */
    if (wallet_KeySize() < 0) { /* no password file existed before */
      newkey  = PL_strdup(""); /* use zero-length password */
    } else if (isNewkey) { /* user is changing the password */
      newkey  = PL_strdup(""); /* use zero-length password */
    } else {
      keyCancel = PR_TRUE;
      return PR_FALSE; /* user could not supply the correct password */
    }
  }
  PR_FREEIF(password);
  for (; (keyPosition < PL_strlen(newkey) && keyPosition < maxKeySize); keyPosition++) {
    key[keyPosition] = newkey[keyPosition];
  }
  key[keyPosition] = '\0';
  PR_FREEIF(newkey);
  Wallet_RestartKey();

  /* verify this with the saved key */
  if (isNewkey || (wallet_KeySize() < 0)) {

    /*
     * Either key is to be changed or the file containing the saved key doesn' exist.
     * In either case we need to (re)create and re(write) the file.
     */

    nsFileSpec dirSpec;
    nsresult rval = Wallet_ProfileDirectory(dirSpec);
    if (NS_FAILED(rval)) {
      return PR_FALSE;
    }
    nsOutputFileStream strm2(dirSpec + "key");
    if (!strm2.is_open()) {
      *key = '\0';
      return PR_FALSE;
    }

    /* If we store the key obscured by the key itself, then the result will be zero
     * for all keys (since we are using XOR to obscure).  So instead we store
     * key[1..n],key[0] obscured by the actual key.
     */

    if (PL_strlen(key) != 0) {
      char* p = key+1;
      while (*p) {
        strm2.put(*(p++)^Wallet_GetKey());
      }
      strm2.put((*key)^Wallet_GetKey());
    }
    strm2.flush();
    strm2.close();
    Wallet_RestartKey();
    keySet = PR_TRUE;
    return PR_TRUE;

  } else {

    /* file of saved key existed so see if it matches the key the user typed in */

    /*
     * Note that eof() is not set until after we read past the end of the file.  That
     * is why the following code reads a character and immediately after the read
     * checks for eof()
     */

    /* test for a null key */
    if ((PL_strlen(key) == 0) && (wallet_KeySize() == 0) ) {
      Wallet_RestartKey();
      keySet = PR_TRUE;
      return PR_TRUE;
    }

    nsFileSpec dirSpec;
    nsresult rval = Wallet_ProfileDirectory(dirSpec);
    if (NS_FAILED(rval)) {
      return PR_FALSE;
    }
    nsInputFileStream strm(dirSpec + "key");
    Wallet_RestartKey();
    char* p = key+1;
    while (*p) {
      if (strm.get() != (*(p++)^Wallet_GetKey()) || strm.eof()) {
        strm.close();
        *key = '\0';
        return PR_FALSE;
      }
    }
    if (strm.get() != ((*key)^Wallet_GetKey()) || strm.eof()) {
      strm.close();
      *key = '\0';
      return PR_FALSE;
    }
    strm.get(); /* to get past the end of the file so eof() will get set */
    PRBool rv = strm.eof();
    strm.close();
    if (rv) {
      Wallet_RestartKey();
      keySet = PR_TRUE;
      return PR_TRUE;
    } else {
      *key = '\0';
      return PR_FALSE;
    }
  }
}

/******************************************************/
/* The following routines are for accessing the files */
/******************************************************/

/*
 * get a line from a file
 * return -1 if end of file reached
 * strip carriage returns and line feeds from end of line
 */
PRInt32
wallet_GetLine(nsInputFileStream strm, nsAutoString*& aLine, PRBool obscure) {

  /* read the line */
  aLine = new nsAutoString("");   
  if (!aLine) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  char c;
  for (;;) {
    c = strm.get()^(obscure ? Wallet_GetKey() : (char)0);
    if (c == '\n') {
      break;
    }

    /* note that eof is not set until we read past the end of the file */
    if (strm.eof()) {
      return -1;
    }

    if (c != '\r') {
      *aLine += c;
    }
  }

  return NS_OK;
}

/*
 * Write a line to a file
 * return -1 if an error occurs
 */
PRInt32
wallet_PutLine(nsOutputFileStream strm, const nsString& aLine, PRBool obscure)
{
  /* allocate a buffer from the heap */
  char * cp = new char[aLine.Length() + 1];
  if (! cp) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  aLine.ToCString(cp, aLine.Length() + 1);

  /* output each character */
  char* p = cp;
  while (*p) {
    strm.put(*(p++)^(obscure ? Wallet_GetKey() : (char)0));
  }
  strm.put('\n'^(obscure ? Wallet_GetKey() : (char)0));

  delete[] cp;
  return 0;
}

/*
 * write contents of designated list into designated file
 */
void
wallet_WriteToFile(char* filename, nsVoidArray* list, PRBool obscure) {
  wallet_MapElement * ptr;

  if (obscure && !Wallet_KeySet()) {
    return;
  }

  /* open output stream */
  nsFileSpec dirSpec;
  nsresult rv = Wallet_ProfileDirectory(dirSpec);
  if (NS_FAILED(rv)) {
    return;
  }
  nsOutputFileStream strm(dirSpec + filename);
  if (!strm.is_open()) {
    NS_ERROR("unable to open file");
    return;
  }

  /* make sure the list exists */
  if(!list) {
    return;
  }
  Wallet_RestartKey();

  /* traverse the list */
  PRInt32 count = LIST_COUNT(list);
  for (PRInt32 i=0; i<count; i++) {
    ptr = NS_STATIC_CAST(wallet_MapElement*, list->ElementAt(i));
    if (NS_FAILED(wallet_PutLine(strm, *ptr->item1, obscure))) {
      break;
    }
    if (*ptr->item2 != "") {
      if (NS_FAILED(wallet_PutLine(strm, *ptr->item2, obscure))) {
        break;
      }
    } else {
      wallet_Sublist * ptr1;
      PRInt32 count2 = LIST_COUNT(ptr->itemList);
      for (PRInt32 j=0; j<count2; j++) {
        ptr1 = NS_STATIC_CAST(wallet_Sublist*, ptr->itemList->ElementAt(j));
        if (NS_FAILED(wallet_PutLine(strm, *ptr->item1, obscure))) {
          break;
        }
      }
    }
    if (NS_FAILED(wallet_PutLine(strm, "", obscure))) {
      break;
    }
  }

  /* close the stream */
  strm.flush();
  strm.close();
}

/*
 * Read contents of designated file into designated list
 */
void
wallet_ReadFromFile
    (char* filename, nsVoidArray*& list, PRBool obscure, PRBool localFile, PlacementType placement = DUP_AFTER) {

  /* open input stream */
  nsFileSpec dirSpec;
  nsresult rv;
  if (localFile) {
    rv = Wallet_ProfileDirectory(dirSpec);
  } else {
    rv = Wallet_ResourceDirectory(dirSpec);
  }
  if (NS_FAILED(rv)) {
    return;
  }
  nsInputFileStream strm(dirSpec + filename);
  if (!strm.is_open()) {
    /* file doesn't exist -- that's not an error */
    return;
  }
  Wallet_RestartKey();

  for (;;) {
    nsAutoString * aItem1;
    if (NS_FAILED(wallet_GetLine(strm, aItem1, obscure))) {
      /* end of file reached */
      break;
    }

    nsAutoString * aItem2;
    if (NS_FAILED(wallet_GetLine(strm, aItem2, obscure))) {
      /* unexpected end of file reached */
      delete aItem1;
      break;
    }

    nsAutoString * aItem3;
    if (NS_FAILED(wallet_GetLine(strm, aItem3, obscure))) {
      /* end of file reached */
      nsVoidArray* dummy = NULL;
      wallet_WriteToList(*aItem1, *aItem2, dummy, list, placement);
      strm.close();
      return;
    }

    if (aItem3->Length()==0) {
      /* just a pair of values, no need for a sublist */
      nsVoidArray* dummy = NULL;
      wallet_WriteToList(*aItem1, *aItem2, dummy, list, placement);
    } else {
      /* need to create a sublist and put item2 and item3 onto it */
      nsVoidArray * itemList = new nsVoidArray();
      if (!itemList) {
        delete aItem1;
        delete aItem2;
        delete aItem3;
        break;
      }
      wallet_Sublist * sublist;
      sublist = PR_NEW(wallet_Sublist);
      sublist->item = new nsAutoString (*aItem2);
      if (!(sublist->item)) {
        delete itemList;
        delete aItem1;
        delete aItem2;
        delete aItem3;
        break;
      }
      itemList->AppendElement(sublist);
      delete aItem2;
      sublist = PR_NEW(wallet_Sublist);
      sublist->item = new nsAutoString (*aItem3);
      if (!(sublist->item)) {
        delete itemList;
        delete aItem1;
        delete aItem3;
        break;
      }
      itemList->AppendElement(sublist);
      delete aItem3;
      /* add any following items to sublist up to next blank line */
      nsAutoString * dummy2 = new nsAutoString("");
      if (!dummy2) {
        delete itemList;
        delete aItem1;
        break;
      }
      for (;;) {
        /* get next item for sublist */
        if (NS_FAILED(wallet_GetLine(strm, aItem3, obscure))) {
          /* end of file reached */
          wallet_WriteToList(*aItem1, *dummy2, itemList, list, placement);
          strm.close();
          return;
        }
        if (aItem3->Length()==0) {
          /* blank line reached indicating end of sublist */
          wallet_WriteToList(*aItem1, *dummy2, itemList, list, placement);
          break;
        }
        /* add item to sublist */
        sublist = PR_NEW(wallet_Sublist);
        sublist->item = new nsAutoString (*aItem3);
        if (!sublist->item) {
          delete aItem3;
          break;
        }
        itemList->AppendElement(sublist);
        delete aItem3;
      }
    }
  }
  strm.close();
}

/*
 * Read contents of designated URLFieldToSchema file into designated list
 */
void
wallet_ReadFromURLFieldToSchemaFile
    (char* filename, nsVoidArray*& list, PlacementType placement = DUP_AFTER) {

  /* open input stream */
  nsFileSpec dirSpec;
  nsresult rv = Wallet_ResourceDirectory(dirSpec);
  if (NS_FAILED(rv)) {
    return;
  }
  nsInputFileStream strm(dirSpec + filename);
  if (!strm.is_open()) {
    /* file doesn't exist -- that's not an error */
    return;
  }
  /* Wallet_RestartKey();  not needed since file is not encoded */

  /* make sure the list exists */
  if(!list) {
    list = new nsVoidArray();
    if(!list) {
      strm.close();
      return;
    }
  }

  for (;;) {

    nsAutoString * aItem;
    if (NS_FAILED(wallet_GetLine(strm, aItem, PR_FALSE))) {
      /* end of file reached */
      break;
    }

    nsVoidArray * itemList = new nsVoidArray();
    if (!itemList) {
      delete aItem;
      break;
    }
    nsAutoString * dummyString = new nsAutoString("");
    if (!dummyString) {
      delete aItem;
      delete itemList;
      break;
    }
    wallet_WriteToList(*aItem, *dummyString, itemList, list, placement);

    for (;;) {
      nsAutoString * aItem1;
      if (NS_FAILED(wallet_GetLine(strm, aItem1, PR_FALSE))) {
        /* end of file reached */
        break;
        strm.close();
        return;
      }

      if (aItem1->Length()==0) {
        /* end of url reached */
        break;
      }

      nsAutoString * aItem2;
      if (NS_FAILED(wallet_GetLine(strm, aItem2, PR_FALSE))) {
        /* unexpected end of file reached */
        delete aItem1;
        break;
      }

      nsVoidArray* dummyList = NULL;
      wallet_WriteToList(*aItem1, *aItem2, dummyList, itemList, placement);

      nsAutoString * aItem3;
      if (NS_FAILED(wallet_GetLine(strm, aItem3, PR_FALSE))) {
        /* end of file reached */
        strm.close();
        return;
      }

      if (aItem3->Length()!=0) {
        /* invalid file format */
        delete aItem3;
        break;
      }
      delete aItem3;
    }
  }
  strm.close();
}

/*********************************************************************/
/* The following are utility routines for the main wallet processing */
/*********************************************************************/
 
/*
 * given a field name, get the value
 */
PRInt32 FieldToValue(
    nsAutoString field,
    nsAutoString& schema,
    nsAutoString& value,
    nsVoidArray*& itemList,
    PRInt32& index)
{
  /* return if no SchemaToValue list exists */
  if (!wallet_SchemaToValue_list) {
    return -1;
  }

  /* if no schema name is given, fetch schema name from field/schema tables */
  nsVoidArray* dummy;
  if (index == -1) {
    index = 0;
  }
  if ((schema.Length() > 0) ||
      wallet_ReadFromList(field, schema, dummy, wallet_specificURLFieldToSchema_list) ||
      wallet_ReadFromList(field, schema, dummy, wallet_FieldToSchema_list)) {
    /* schema name found, now fetch value from schema/value table */ 
    PRInt32 index2 = index;
    if (wallet_ReadFromList(schema, value, itemList, wallet_SchemaToValue_list, index2)) {
      /* value found, prefill it into form */
      index = index2;
      return 0;
    } else {
      /* value not found, see if concatenation rule exists */
      nsVoidArray * itemList2;

      nsAutoString dummy2;
      if (wallet_ReadFromList(schema, dummy2, itemList2, wallet_SchemaConcat_list)) {
        /* concatenation rules exist, generate value as a concatenation */
        wallet_Sublist * ptr1;
        value = nsAutoString("");
        nsAutoString value2;
        PRInt32 count = LIST_COUNT(itemList2);
        for (PRInt32 i=0; i<count; i++) {
          ptr1 = NS_STATIC_CAST(wallet_Sublist*, itemList2->ElementAt(i));
          if (wallet_ReadFromList(*(ptr1->item), value2, dummy, wallet_SchemaToValue_list)) {
            if (value.Length()>0) {
              value += " ";
            }
            value += value2;
          }
        }
        index = -1;
        itemList = nsnull;
        if (value.Length()>0) {
          return 0;
        }
      }
    }
  } else {
    /* schema name not found, use field name as schema name and fetch value */
    PRInt32 index2 = index;
    if (wallet_ReadFromList(field, value, itemList, wallet_SchemaToValue_list, index2)) {
      /* value found, prefill it into form */
      schema = nsAutoString(field);
      index = index2;
      return 0;
    }
  }
  index = -1;
  return -1;
}

PRInt32
wallet_GetSelectIndex(
  nsIDOMHTMLSelectElement* selectElement,
  nsAutoString value,
  PRInt32& index)
{
  nsresult result;
  PRUint32 length;
  selectElement->GetLength(&length);
  nsIDOMHTMLCollection * options;
  result = selectElement->GetOptions(&options);
  if ((NS_SUCCEEDED(result)) && (nsnull != options)) {
    PRUint32 numOptions;
    options->GetLength(&numOptions);
    for (PRUint32 optionX = 0; optionX < numOptions; optionX++) {
      nsIDOMNode* optionNode = nsnull;
      options->Item(optionX, &optionNode);
      if (nsnull != optionNode) {
        nsIDOMHTMLOptionElement* optionElement = nsnull;
        result = optionNode->QueryInterface(kIDOMHTMLOptionElementIID, (void**)&optionElement);
        if ((NS_SUCCEEDED(result)) && (nsnull != optionElement)) {
          nsAutoString optionValue;
          nsAutoString optionText;
          optionElement->GetValue(optionValue);
          optionElement->GetText(optionText);
          if (value==optionValue || value==optionText) {
            index = optionX;
            return 0;
          }
          NS_RELEASE(optionElement);
        }
        NS_RELEASE(optionNode);
      }
    }
    NS_RELEASE(options);
  }
  return -1;
}

PRInt32
wallet_GetPrefills(
  nsIDOMNode* elementNode,
  nsIDOMHTMLInputElement*& inputElement,  
  nsIDOMHTMLSelectElement*& selectElement,
  nsAutoString*& schemaPtr,
  nsAutoString*& valuePtr,
  PRInt32& selectIndex,
  PRInt32& index)
{
  nsresult result;

  /* get prefills for input element */
  result = elementNode->QueryInterface(kIDOMHTMLInputElementIID, (void**)&inputElement);
  if ((NS_SUCCEEDED(result)) && (nsnull != inputElement)) {
    nsAutoString type;
    result = inputElement->GetType(type);
    if ((NS_SUCCEEDED(result)) && ((type =="") || (type.Compare("text", PR_TRUE) == 0))) {
      nsAutoString field;
      result = inputElement->GetName(field);
      if (NS_SUCCEEDED(result)) {
        nsAutoString schema("");
        nsAutoString value;
        nsVoidArray* itemList;

        /* get schema name from vcard attribute if it exists */
        nsIDOMElement * element;
        result = elementNode->QueryInterface(kIDOMElementIID, (void**)&element);
        if ((NS_SUCCEEDED(result)) && (nsnull != element)) {
          nsAutoString vcard("VCARD_NAME");
          result = element->GetAttribute(vcard, schema);
          NS_RELEASE(element);
        }

        /*
         * if schema name was specified in vcard attribute then get value from schema name,
         * otherwise get value from field name by using mapping tables to get schema name
         */
        if (FieldToValue(field, schema, value, itemList, index) == 0) {
          if (value == "" && nsnull != itemList) {
            /* pick first of a set of synonymous values */
            value = *(((wallet_Sublist *)itemList->ElementAt(0))->item);
          }
          valuePtr = new nsAutoString(value);
          if (!valuePtr) {
            NS_RELEASE(inputElement);
            return -1;
          }
          schemaPtr = new nsAutoString(schema);
          if (!schemaPtr) {
            delete valuePtr;
            NS_RELEASE(inputElement);
            return -1;
          }
          selectElement = nsnull;
          selectIndex = -1;
          return NS_OK;
        }
      }
    }
    NS_RELEASE(inputElement);
    return -1;
  }

  /* get prefills for dropdown list */
  result = elementNode->QueryInterface(kIDOMHTMLSelectElementIID, (void**)&selectElement);
  if ((NS_SUCCEEDED(result)) && (nsnull != selectElement)) {
    nsAutoString field;
    result = selectElement->GetName(field);
    if (NS_SUCCEEDED(result)) {
      nsAutoString schema("");
      nsAutoString value;
      nsVoidArray* itemList;
      if (FieldToValue(field, schema, value, itemList, index) == 0) {
        if (value != "") {
          /* no synonym list, just one value to try */
          result = wallet_GetSelectIndex(selectElement, value, selectIndex);
          if (NS_SUCCEEDED(result)) {
            /* value matched one of the values in the drop-down list */
            valuePtr = new nsAutoString(value);
            if (!valuePtr) {
              NS_RELEASE(selectElement);
              return -1;
            }
            schemaPtr = new nsAutoString(schema);
            if (!schemaPtr) {
              delete valuePtr;
              NS_RELEASE(selectElement);
              return -1;
            }
            inputElement = nsnull;
            return NS_OK;
          }
        } else {
          /* synonym list exists, try each value */
          for (PRInt32 i=0; i<LIST_COUNT(itemList); i++) {
            value = *(((wallet_Sublist *)itemList->ElementAt(i))->item);
            result = wallet_GetSelectIndex(selectElement, value, selectIndex);
            if (NS_SUCCEEDED(result)) {
              /* value matched one of the values in the drop-down list */
              valuePtr = new nsAutoString(value);
              if (!valuePtr) {
                NS_RELEASE(selectElement);
                return -1;
              }
              schemaPtr = new nsAutoString(schema);
              if (!schemaPtr) {
                delete valuePtr;
                NS_RELEASE(selectElement);
                return -1;
              }
              inputElement = nsnull;
              return NS_OK;
            }
          }
        }
      }
    }
    NS_RELEASE(selectElement);
  }
  return -1;
}

/*
 * initialization for wallet session (done only once)
 */
void
wallet_Initialize() {
  static PRBool wallet_Initialized = PR_FALSE;
  static PRBool wallet_keyInitialized = PR_FALSE;
  if (!wallet_Initialized) {
    wallet_ReadFromFile("FieldSchema.tbl", wallet_FieldToSchema_list, PR_FALSE, PR_FALSE);
    wallet_ReadFromURLFieldToSchemaFile("URLFieldSchema.tbl", wallet_URLFieldToSchema_list);
    wallet_ReadFromFile("SchemaConcat.tbl", wallet_SchemaConcat_list, PR_FALSE, PR_FALSE);
    wallet_Initialized = PR_TRUE;
  }

  if (!wallet_keyInitialized) {
    Wallet_RestartKey();
    char * message = Wallet_Localize("IncorrectKey_TryAgain?");
    while (!Wallet_SetKey(PR_FALSE)) {
      if (Wallet_CancelKey() || !Wallet_Confirm(message)) {
        PR_FREEIF(message);
        return;
      }
    }
    PR_FREEIF(message);
    wallet_ReadFromFile("SchemaValue.tbl", wallet_SchemaToValue_list, PR_TRUE, PR_TRUE);
    wallet_keyInitialized = PR_TRUE;
  }

#if DEBUG
//    fprintf(stdout,"Field to Schema table \n");
//    wallet_Dump(wallet_FieldToSchema_list);

//    fprintf(stdout,"SchemaConcat table \n");
//    wallet_Dump(wallet_SchemaConcat_list);

//    fprintf(stdout,"URL Field to Schema table \n");
//    char item1[100];
//    wallet_MapElement * ptr;
//    PRInt32 count = LIST_COUNT(wallet_URLFieldToSchema_list);
//    for (PRInt32 i=0; i<count; i++) {
//      ptr = NS_STATIC_CAST(wallet_MapElement*, wallet_URLFieldToSchema_list->ElementAt(i));
//      ptr->item1->ToCString(item1, 100);
//      fprintf(stdout, item1);
//      fprintf(stdout,"\n");
//      wallet_Dump(ptr->itemList);
//    }
//    fprintf(stdout,"Schema to Value table \n");
//    wallet_Dump(wallet_SchemaToValue_list);
#endif

}

#ifdef SingleSignon
extern int SI_SaveSignonData();
extern int SI_LoadSignonData(PRBool fullLoad);
#endif

PUBLIC
void WLLT_ChangePassword() {

  /* do nothing if password was never set */
  if (wallet_KeySize() < 0) {
    return;
  }

  /* read in user data using old key */

  wallet_Initialize();
#ifdef SingleSignon
  SI_LoadSignonData(PR_TRUE);
#endif
  if (!Wallet_KeySet()) {
    return;
  }

  /* establish new key */
  Wallet_SetKey(PR_TRUE);

  /* write out user data using new key */
  wallet_WriteToFile("SchemaValue.tbl", wallet_SchemaToValue_list, PR_TRUE);
#ifdef SingleSignon
  SI_SaveSignonData();
#endif
}

void
wallet_InitializeURLList() {
  static PRBool wallet_URLListInitialized = PR_FALSE;
  if (!wallet_URLListInitialized) {
    wallet_ReadFromFile("URL.tbl", wallet_URL_list, PR_FALSE, PR_TRUE);
    wallet_URLListInitialized = PR_TRUE;
  }
}
/*
 * initialization for current URL
 */
void
wallet_InitializeCurrentURL(nsIDocument * doc) {
  static nsIURI * lastUrl = NULL;

  /* get url */
  nsIURI* url;
  url = doc->GetDocumentURL();
  if (lastUrl == url) {
    NS_RELEASE(url);
    return;
  } else {
    if (lastUrl) {
//??      NS_RELEASE(lastUrl);
    }
    lastUrl = url;
  }

  /* get host+file */
#ifdef NECKO
  char* host;
#else
  const char* host;
#endif
  nsresult rv = url->GetHost(&host);
  if (NS_FAILED(rv)) {
    return;
  }
  nsAutoString urlName = nsAutoString(host);
#ifdef NECKO
  nsCRT::free(host);
#endif
#ifdef NECKO
  char* file;
  url->GetPath(&file);
#else
  const char* file;
  url->GetFile(&file);
#endif
  urlName = urlName + file;
#ifdef NECKO
  nsCRT::free(file);
#endif
  NS_RELEASE(url);

  /* get field/schema mapping specific to current url */
  wallet_MapElement * ptr;
  PRInt32 count = LIST_COUNT(wallet_URLFieldToSchema_list);
  for (PRInt32 i=0; i<count; i++) {
    ptr = NS_STATIC_CAST(wallet_MapElement*, wallet_URLFieldToSchema_list->ElementAt(i));
    if (*(ptr->item1) == urlName) {
      wallet_specificURLFieldToSchema_list = ptr->itemList;
      break;
    }
  }
#ifdef DEBUG
//  fprintf(stdout,"specific URL Field to Schema table \n");
//  wallet_Dump(wallet_specificURLFieldToSchema_list);
#endif
}

#define SEPARATOR "#*%$"

nsAutoString *
wallet_GetNextInString(char*& ptr) {
  nsAutoString * result;
  char * endptr;
  endptr = PL_strstr(ptr, SEPARATOR);
  if (!endptr) {
    return NULL;
  }
  *endptr = '\0';
  result = new nsAutoString(ptr);
  *endptr = SEPARATOR[0];
  ptr = endptr + PL_strlen(SEPARATOR);
  return result;
}

void
wallet_ReleasePrefillElementList(nsVoidArray * wallet_PrefillElement_list) {
  if (wallet_PrefillElement_list) {
    wallet_PrefillElement * ptr;
    PRInt32 count = LIST_COUNT(wallet_PrefillElement_list);
    for (PRInt32 i=count-1; i>=0; i--) {
      ptr = NS_STATIC_CAST(wallet_PrefillElement*, wallet_PrefillElement_list->ElementAt(i));
      if (ptr->inputElement) {
        NS_RELEASE(ptr->inputElement);
      } else {
        NS_RELEASE(ptr->selectElement);
      }
      delete ptr->schema;
      delete ptr->value;
      wallet_PrefillElement_list->RemoveElement(ptr);
      delete ptr;
    }
  }
}

#define BUFLEN3 5000
#define BREAK '\001'

nsVoidArray * wallet_list;
nsString wallet_url;

PUBLIC void
WLLT_GetPrefillListForViewer(nsString& aPrefillList)
{
  char *buffer = (char*)PR_Malloc(BUFLEN3);
  int g = 0;
  wallet_PrefillElement * ptr;
  buffer[0] = '\0';
  char * schema;
  char * value;
  PRInt32 count = LIST_COUNT(wallet_list);
  for (PRInt32 i=0; i<count; i++) {
    ptr = NS_STATIC_CAST(wallet_PrefillElement*, wallet_list->ElementAt(i));
    schema = ptr->schema->ToNewCString();
    value = ptr->value->ToNewCString();
    g += PR_snprintf(buffer+g, BUFLEN3-g,
      "%c%d%c%s%c%s",
      BREAK, ptr->count,
      BREAK, schema,
      BREAK, value);
    delete []schema;
    delete []value;
  }
  char * urlCString;
  urlCString = wallet_url.ToNewCString();
  g += PR_snprintf(buffer+g, BUFLEN3-g,"%c%ld%c%s", BREAK, wallet_list, BREAK, urlCString);
  delete []urlCString;
  aPrefillList = buffer;
  PR_FREEIF(buffer);
}

extern PRBool
SI_InSequence(char* sequence, int number);

extern char*
SI_FindValueInArgs(nsAutoString results, char* name);

PRIVATE void
wallet_FreeURL(wallet_MapElement *url) {

    if(!url) {
        return;
    }
    wallet_URL_list->RemoveElement(url);
    PR_FREEIF(url->item1);
    PR_FREEIF(url->item2);
    PR_Free(url);
}

PUBLIC void
Wallet_SignonViewerReturn (nsAutoString results) {
    wallet_MapElement *url;
    char * gone;

    /* step through all nopreviews and delete those that are in the sequence */
    gone = SI_FindValueInArgs(results, "|goneP|");
    PRInt32 count = LIST_COUNT(wallet_URL_list);
    while (count>0) {
      count--;
      url = NS_STATIC_CAST(wallet_MapElement*, wallet_URL_list->ElementAt(count));
      if (url && SI_InSequence(gone, count)) {
        url->item2->SetCharAt('n', NO_PREVIEW);
        if (url->item2->CharAt(NO_CAPTURE) == 'n') {
          wallet_FreeURL(url);
          wallet_WriteToFile("URL.tbl", wallet_URL_list, PR_FALSE);
        }
      }
    }
    delete[] gone;

    /* step through all nocaptures and delete those that are in the sequence */
    gone = SI_FindValueInArgs(results, "|goneC|");
    PRInt32 count2 = LIST_COUNT(wallet_URL_list);
    while (count2>0) {
      count2--;
      url = NS_STATIC_CAST(wallet_MapElement*, wallet_URL_list->ElementAt(count2));
      if (url && SI_InSequence(gone, count)) {
        url->item2->SetCharAt('n', NO_CAPTURE);
        if (url->item2->CharAt(NO_PREVIEW) == 'n') {
          wallet_FreeURL(url);
          wallet_WriteToFile("URL.tbl", wallet_URL_list, PR_FALSE);
        }
      }
    }
    delete[] gone;
}

/*
 * see if user wants to capture data on current page
 */

PRIVATE PRBool
wallet_OKToCapture(char* urlName) {
  nsAutoString * url = new nsAutoString(urlName);

  /* see if this url is already on list of url's for which we don't want to capture */
  wallet_InitializeURLList();
  nsVoidArray* dummy;
  nsAutoString * value = new nsAutoString("nn");
  if (!url || !value) {
    return PR_FALSE;
  }
  if (wallet_ReadFromList(*url, *value, dummy, wallet_URL_list)) {
    if (value->CharAt(NO_CAPTURE) == 'y') {
      return PR_FALSE;
    }
  }

  /* ask user if we should capture the values on this form */
  char * message = Wallet_Localize("WantToCaptureForm?");
  char * checkMessage = Wallet_Localize("NeverSave");
  PRBool checkValue;
  PRBool result = Wallet_CheckConfirmYN(message, checkMessage, &checkValue);
  if (!result) {
    if (checkValue) {
      /* add URL to list with NO_CAPTURE indicator set */
      value->SetCharAt('y', NO_CAPTURE);
      wallet_WriteToList(*url, *value, dummy, wallet_URL_list, DUP_OVERWRITE);
      wallet_WriteToFile("URL.tbl", wallet_URL_list, PR_FALSE);
    } else {
      delete url;
    }
  }
  PR_FREEIF(checkMessage);
  PR_FREEIF(message);
  return result;
}

/*
 * capture the value of a form element
 */
PRIVATE void
wallet_Capture(nsIDocument* doc, nsString field, nsString value, nsString vcard) {

  /* do nothing if there is no value */
  if (!value.Length()) {
    return;
  }

  /* read in the mappings if they are not already present */
  if (!vcard.Length()) {
    wallet_Initialize();
    wallet_InitializeCurrentURL(doc);
    if (!Wallet_KeySet()) {
      return;
    }
  }

  nsAutoString oldValue;

  /* is there a mapping from this field name to a schema name */
  nsAutoString schema(vcard);
  nsVoidArray* dummy;
  if (schema.Length() ||
      (wallet_ReadFromList(field, schema, dummy, wallet_specificURLFieldToSchema_list)) ||
      (wallet_ReadFromList(field, schema, dummy, wallet_FieldToSchema_list))) {

    /* field to schema mapping already exists */

    /* is this a new value for the schema */
    PRInt32 index = 0;
    PRInt32 lastIndex = index;
    while(wallet_ReadFromList(schema, oldValue, dummy, wallet_SchemaToValue_list, index)) {
      if (oldValue == value) {
        /*
         * Remove entry from wallet_SchemaToValue_list and then reinsert.  This will
         * keep multiple values in that list for the same field ordered with
         * most-recently-used first.  That's useful since the first such entry
         * is the default value used for pre-filling.
         */
        wallet_MapElement * mapElement =
          (wallet_MapElement *) (wallet_SchemaToValue_list->ElementAt(lastIndex));
        wallet_SchemaToValue_list->RemoveElementAt(lastIndex);
        wallet_WriteToList(
          *(mapElement->item1),
          *(mapElement->item2),
          mapElement->itemList, 
          wallet_SchemaToValue_list);
        delete mapElement;
        return;
      }
      lastIndex = index;
    }

    /* this is a new value so store it */
    nsAutoString * aValue = new nsAutoString(value);
    nsAutoString * aSchema = new nsAutoString(schema);
    dummy = 0;
    wallet_WriteToList(*aSchema, *aValue, dummy, wallet_SchemaToValue_list);
    wallet_WriteToFile("SchemaValue.tbl", wallet_SchemaToValue_list, PR_TRUE);

  } else {

    /* no field to schema mapping so assume schema name is same as field name */

    /* is this a new value for the schema */
    PRInt32 index = 0;
    PRInt32 lastIndex = index;
    while(wallet_ReadFromList(field, oldValue, dummy, wallet_SchemaToValue_list, index)) {
      if (oldValue == value) {
        /*
         * Remove entry from wallet_SchemaToValue_list and then reinsert.  This will
         * keep multiple values in that list for the same field ordered with
         * most-recently-used first.  That's useful since the first such entry
         * is the default value used for pre-filling.
         */
        wallet_MapElement * mapElement =
          (wallet_MapElement *) (wallet_SchemaToValue_list->ElementAt(lastIndex));
        wallet_SchemaToValue_list->RemoveElementAt(lastIndex);
        wallet_WriteToList(
          *(mapElement->item1),
          *(mapElement->item2),
          mapElement->itemList, 
          wallet_SchemaToValue_list);
        delete mapElement;
        return;
      }
      lastIndex = index;
    }

    /* this is a new value so store it */
    nsAutoString * aField = new nsAutoString(field);
    nsAutoString * aValue = new nsAutoString(value);
    dummy = 0;
    wallet_WriteToList(*aField, *aValue, dummy, wallet_SchemaToValue_list);
    wallet_WriteToFile("SchemaValue.tbl", wallet_SchemaToValue_list, PR_TRUE);
  }
}

/***************************************************************/
/* The following are the interface routines seen by other dlls */
/***************************************************************/

#define BUFLEN2 5000
#define BREAK '\001'

PUBLIC void
WLLT_GetNopreviewListForViewer(nsString& aNopreviewList)
{
  char *buffer = (char*)PR_Malloc(BUFLEN2);
  int g = 0, nopreviewNum;
  wallet_MapElement *url;
  char* urlCString;

  wallet_InitializeURLList();
  buffer[0] = '\0';
  nopreviewNum = 0;
  PRInt32 count = LIST_COUNT(wallet_URL_list);
  for (PRInt32 i=0; i<count; i++) {
    url = NS_STATIC_CAST(wallet_MapElement*, wallet_URL_list->ElementAt(i));
    if (url->item2->CharAt(NO_PREVIEW) == 'y') {
      urlCString = url->item1->ToNewCString();
      g += PR_snprintf(buffer+g, BUFLEN2-g,
"%c        <OPTION value=%d>%s</OPTION>\n",
      BREAK,
      nopreviewNum,
      urlCString
      );
      delete[] urlCString;
      nopreviewNum++;
    }
  }
  aNopreviewList = buffer;
  PR_FREEIF(buffer);
}

PUBLIC void
WLLT_GetNocaptureListForViewer(nsString& aNocaptureList)
{
  char *buffer = (char*)PR_Malloc(BUFLEN2);
  int g = 0, nocaptureNum;
  wallet_MapElement *url;
  char* urlCString;

  wallet_InitializeURLList();
  buffer[0] = '\0';
  nocaptureNum = 0;
  PRInt32 count = LIST_COUNT(wallet_URL_list);
  for (PRInt32 i=0; i<count; i++) {
    url = NS_STATIC_CAST(wallet_MapElement*, wallet_URL_list->ElementAt(i));
    if (url->item2->CharAt(NO_CAPTURE) == 'y') {
      urlCString = url->item1->ToNewCString();
      g += PR_snprintf(buffer+g, BUFLEN2-g,
"%c        <OPTION value=%d>%s</OPTION>\n",
      BREAK,
      nocaptureNum,
      urlCString
      );
      delete[] urlCString;
      nocaptureNum++;
    }
  }
  aNocaptureList = buffer;
  PR_FREEIF(buffer);
}

PUBLIC void
WLLT_PostEdit(nsAutoString walletList) {
  if (!Wallet_KeySet()) {
    return;
  }

  char* separator;

  nsFileSpec dirSpec;
  nsresult rv = Wallet_ProfileDirectory(dirSpec);
  if (NS_FAILED(rv)) {
    return;
  }

  /* convert walletList to a C string */
  char *walletListAsCString = walletList.ToNewCString();
  char *nextItem = walletListAsCString;

  /* return if OK button was not pressed */
  separator = strchr(nextItem, BREAK);
  if (!separator) {
    delete[] walletListAsCString; 
    return;
  }
  *separator = '\0';
  if (PL_strcmp(nextItem, "OK")) {
    *separator = BREAK;
    delete []walletListAsCString;
    return;
  }
  nextItem = separator+1;
  *separator = BREAK;

  /* open SchemaValue file */
  nsOutputFileStream strm(dirSpec + "SchemaValue.tbl");
  if (!strm.is_open()) {
    NS_ERROR("unable to open file");
    delete []walletListAsCString;
    return;
  }
  Wallet_RestartKey();

  /* write the values in the walletList to the file */
  for (int i=0; (*nextItem != '\0'); i++) {
    separator = strchr(nextItem, BREAK);
    if (!separator) {
      strm.close();
      delete[] walletListAsCString; 
      return;
    }
    *separator = '\0';
    if (NS_FAILED(wallet_PutLine(strm, nextItem, PR_TRUE))) {
      break;
    }
    nextItem = separator+1;
    *separator = BREAK;
  }

  /* close the file and read it back into the SchemaToValue list */
  strm.close();
  wallet_Clear(&wallet_SchemaToValue_list);
  wallet_ReadFromFile("SchemaValue.tbl", wallet_SchemaToValue_list, PR_TRUE, PR_TRUE);
  delete []walletListAsCString;
}

PUBLIC void
WLLT_PreEdit(nsAutoString& walletList) {
  wallet_Initialize();
  if (!Wallet_KeySet()) {
    return;
  }
  walletList = BREAK;
  wallet_MapElement * ptr;
  PRInt32 count = LIST_COUNT(wallet_SchemaToValue_list);
  for (PRInt32 i=0; i<count; i++) {
    ptr = NS_STATIC_CAST(wallet_MapElement*, wallet_SchemaToValue_list->ElementAt(i));

    walletList += *(ptr->item1) + BREAK;
    if (*ptr->item2 != "") {
      walletList += *(ptr->item2) + BREAK;
    } else {
      wallet_Sublist * ptr1;
      PRInt32 count2 = LIST_COUNT(ptr->itemList);
      for (PRInt32 i2=0; i2<count2; i2++) {
        ptr1 = NS_STATIC_CAST(wallet_Sublist*, ptr->itemList->ElementAt(i2));
        walletList += *(ptr1->item) + BREAK;

      }
    }
    walletList += BREAK;
  }
}

/*
 * return after previewing a set of prefills
 */
PUBLIC void
WLLT_PrefillReturn(nsAutoString results) {
  char* listAsAscii;
  char* fillins;
  char* urlName;
  char* skip;
  nsAutoString * next;

  /* get values that are in environment variables */
  fillins = SI_FindValueInArgs(results, "|fillins|");
  listAsAscii = SI_FindValueInArgs(results, "|list|");
  skip = SI_FindValueInArgs(results, "|skip|");
  urlName = SI_FindValueInArgs(results, "|url|");

  /* add url to url list if user doesn't want to preview this page in the future */
  if (!PL_strcmp(skip, "true")) {
    nsAutoString * url = new nsAutoString(urlName);
    nsVoidArray* dummy;
    nsAutoString * value = new nsAutoString("nn");
    if (!url || !value) {
      wallet_ReadFromList(*url, *value, dummy, wallet_URL_list);
      value->SetCharAt('y', NO_PREVIEW);
      wallet_WriteToList(*url, *value, dummy, wallet_URL_list, DUP_OVERWRITE);
      wallet_WriteToFile("URL.tbl", wallet_URL_list, PR_FALSE);
    }
  }

  /* process the list, doing the fillins */
  nsVoidArray * list;
  sscanf(listAsAscii, "%ld", &list);
  if (fillins[0] == '\0') { /* user pressed CANCEL */
    wallet_ReleasePrefillElementList(list);
    return;
  }

  /*
   * note: there are two lists involved here and we are stepping through both of them.
   * One is the pre-fill list that was generated when we walked through the html content.
   * For each pre-fillable item, it contains n entries, one for each possible value that
   * can be prefilled for that field.  The first entry for each field can be identified
   * because it has a non-zero count field (in fact, the count is the number of entries
   * for that field), all subsequent entries for the same field have a zero count field.
   * The other is the fillin list which was generated by the html dialog that just
   * finished.  It contains one entry for each pre-fillable item specificying that
   * particular value that should be prefilled for that item.
   */

  wallet_PrefillElement * ptr;
  char * ptr2;
  ptr2 = fillins;
  /* step through pre-fill list */
  PRBool first = PR_TRUE;
  PRInt32 count = LIST_COUNT(list);
  for (PRInt32 i=0; i<count; i++) {
    ptr = NS_STATIC_CAST(wallet_PrefillElement*, list->ElementAt(i));

    /* advance in fillins list each time a new schema name in pre-fill list is encountered */
    if (ptr->count != 0) {
      /* count != 0 indicates a new schema name */
      if (!first) {
        delete next;
      } else {
        first = PR_FALSE;
      }
      next = wallet_GetNextInString(ptr2);
      if (nsnull == next) {
        break;
      }
      if (*next != *ptr->schema) {
        break; /* something's wrong so stop prefilling */
      }
      delete next;
      next = wallet_GetNextInString(ptr2);
    }
    if (*next == *ptr->value) {
      /*
       * Remove entry from wallet_SchemaToValue_list and then reinsert.  This will
       * keep multiple values in that list for the same field ordered with
       * most-recently-used first.  That's useful since the first such entry
       * is the default value used for pre-filling.
       */
      /*
       * Test for ptr->count being zero is an optimization that avoids us from doing a
       * reordering if the current entry already was first
       */
      if (ptr->count == 0) {
        nsAutoString oldValue;
        PRInt32 index = 0;
        PRInt32 lastIndex = index;
        nsVoidArray* dummy;
        while(wallet_ReadFromList(*ptr->schema, oldValue, dummy, wallet_SchemaToValue_list, index)) {
          if (oldValue == *ptr->value) {
            wallet_MapElement * mapElement =
              (wallet_MapElement *) (wallet_SchemaToValue_list->ElementAt(lastIndex));
            wallet_SchemaToValue_list->RemoveElementAt(lastIndex);
            wallet_WriteToList(
              *(mapElement->item1),
              *(mapElement->item2),
              mapElement->itemList,
              wallet_SchemaToValue_list);
            delete mapElement;
            break;
          }
          lastIndex = index;
        }
      }
    }

    /* Change the value */
     if ((*next == *ptr->value) || ((ptr->count>0) && (*next == ""))) {
       if (((*next == *ptr->value) || (*next == "")) && ptr->inputElement) {
         ptr->inputElement->SetValue(*next);
       } else {
         nsresult result;
         result = wallet_GetSelectIndex(ptr->selectElement, *next, ptr->selectIndex);
         if (NS_SUCCEEDED(result)) {
           ptr->selectElement->SetSelectedIndex(ptr->selectIndex);
         } else {
           ptr->selectElement->SetSelectedIndex(0);
        }
      }
    }
  }
  if (next != nsnull ) {
    delete next;
  }

  /* Release the prefill list that was generated when we walked thru the html content */
  wallet_ReleasePrefillElementList(list);
}

/*
 * get the form elements on the current page and prefill them if possible
 */
PUBLIC nsresult
WLLT_Prefill(nsIPresShell* shell, nsString url, PRBool quick) {

  /* create list of elements that can be prefilled */
  nsVoidArray *wallet_PrefillElement_list=new nsVoidArray();
  if (!wallet_PrefillElement_list) {
    return NS_ERROR_FAILURE;
  }

  /* starting with the present shell, get each form element and put them on a list */
  nsresult result;
  if (nsnull != shell) {
    nsIDocument* doc = nsnull;
    result = shell->GetDocument(&doc);
    if (NS_SUCCEEDED(result)) {
      wallet_Initialize();
      wallet_InitializeCurrentURL(doc);
      nsIDOMHTMLDocument* htmldoc = nsnull;
      result = doc->QueryInterface(kIDOMHTMLDocumentIID, (void**)&htmldoc);
      if ((NS_SUCCEEDED(result)) && (nsnull != htmldoc)) {
        nsIDOMHTMLCollection* forms = nsnull;
        htmldoc->GetForms(&forms);
        if (nsnull != forms) {
          PRUint32 numForms;
          forms->GetLength(&numForms);
          for (PRUint32 formX = 0; formX < numForms; formX++) {
            nsIDOMNode* formNode = nsnull;
            forms->Item(formX, &formNode);
            if (nsnull != formNode) {
              nsIDOMHTMLFormElement* formElement = nsnull;
              result = formNode->QueryInterface(kIDOMHTMLFormElementIID, (void**)&formElement);
              if ((NS_SUCCEEDED(result)) && (nsnull != formElement)) {
                nsIDOMHTMLCollection* elements = nsnull;
                result = formElement->GetElements(&elements);
                if ((NS_SUCCEEDED(result)) && (nsnull != elements)) {
                  /* got to the form elements at long last */
                  PRUint32 numElements;
                  elements->GetLength(&numElements);
                  for (PRUint32 elementX = 0; elementX < numElements; elementX++) {
                    nsIDOMNode* elementNode = nsnull;
                    elements->Item(elementX, &elementNode);
                    if (nsnull != elementNode) {
                      wallet_PrefillElement * prefillElement;
                      PRInt32 index = 0;
                      wallet_PrefillElement * firstElement = nsnull;
                      PRUint32 numberOfElements = 0;
                      for (;;) {
                        /* loop to allow for multiple values */
                        /* first element in multiple-value group will have its count
                         * field set to the number of elements in group.  All other
                         * elements in group will have count field set to 0
                         */
                        prefillElement = PR_NEW(wallet_PrefillElement);
                        if (wallet_GetPrefills
                            (elementNode,
                            prefillElement->inputElement,
                            prefillElement->selectElement,
                            prefillElement->schema,
                            prefillElement->value,
                            prefillElement->selectIndex,
                            index) != -1) {
                          /* another value found */
                          if (nsnull == firstElement) {
                            firstElement = prefillElement;
                          }
                          numberOfElements++;
                          prefillElement->count = 0;
                          wallet_PrefillElement_list->AppendElement(prefillElement);
                          if (index == -1) {
                            /* value was found from concat rules, can't resume from here */
                            break;
                          }
                        } else {
                          /* value not found, stop looking for more values */
                          delete prefillElement;
                          break;
                        }
                      }
                      if (numberOfElements>0) {
                        firstElement->count = numberOfElements;
                      }
                      NS_RELEASE(elementNode);
                    }
                  }
                  NS_RELEASE(elements);
                }
                NS_RELEASE(formElement);
              }
              NS_RELEASE(formNode);
            }
          }
          NS_RELEASE(forms);
        }
        NS_RELEASE(htmldoc);
      }
      NS_RELEASE(doc);
    }
    NS_RELEASE(shell);
  }

  /* return if no elements were put into the list */
  if (LIST_COUNT(wallet_PrefillElement_list) == 0) {
    if (Wallet_KeySet()) {
      char * message = Wallet_Localize("noPrefills");
      Wallet_Alert(message);
      PR_FREEIF(message);
    }
    return NS_ERROR_FAILURE; // indicates to caller not to display preview screen
  }

  /* prefill each element using the list */

  /* determine if url is on list of urls that should not be previewed */
  PRBool noPreview = PR_FALSE;
  if (!quick) {
    wallet_InitializeURLList();
    nsVoidArray* dummy;
    nsAutoString * value = new nsAutoString("nn");
    nsAutoString * urlPtr = new nsAutoString(url);
    if (!value || !urlPtr) {
      wallet_ReadFromList(*urlPtr, *value, dummy, wallet_URL_list);
      noPreview = (value->CharAt(NO_PREVIEW) == 'y');
      delete value;
      delete urlPtr;
    }
  }

  /* determine if preview is necessary */
  if (noPreview || quick) {
    /* prefill each element without any preview for user verification */
    wallet_PrefillElement * ptr;
    PRInt32 count = LIST_COUNT(wallet_PrefillElement_list);
    for (PRInt32 i=0; i<count; i++) {
      ptr = NS_STATIC_CAST(wallet_PrefillElement*, wallet_PrefillElement_list->ElementAt(i));
      if (ptr->count) {
        if (ptr->inputElement) {
          ptr->inputElement->SetValue(*(ptr->value));
        } else {
          ptr->selectElement->SetSelectedIndex(ptr->selectIndex);
        }
      }
    }
    /* go thru list just generated and release everything */
    wallet_ReleasePrefillElementList(wallet_PrefillElement_list);
    return NS_ERROR_FAILURE; // indicates to caller not to display preview screen
  } else {
    /* let user preview and verify the prefills first */
    wallet_list = wallet_PrefillElement_list;
    wallet_url = url;
#ifdef DEBUG
wallet_DumpStopwatch();
wallet_ClearStopwatch();
//wallet_DumpTiming();
//wallet_ClearTiming();
#endif
    return NS_OK; // indicates that caller is to display preview screen
  }
}

#define FORM_TYPE_TEXT          1
#define FORM_TYPE_PASSWORD      7
#define MAX_ARRAY_SIZE 500

extern void
SINGSIGN_RememberSignonData
  (char* URLName, char** name_array, char** value_array, char** type_array, PRInt32 value_cnt);

PUBLIC void
WLLT_OnSubmit(nsIContent* formNode) {

  /* get url name as ascii string */
  char *URLName = nsnull;
  nsIURI* docURL = nsnull;
  nsIDocument* doc = nsnull;
  formNode->GetDocument(doc);
#ifdef NECKO
  char* spec;
#else
  const char* spec;
#endif
  if (!doc) {
    return;
  }
  docURL = doc->GetDocumentURL();
  if (!docURL) {
    return;
  }
  (void)docURL->GetSpec(&spec);
  URLName = (char*)PR_Malloc(PL_strlen(spec)+1);
  PL_strcpy(URLName, spec);
  NS_IF_RELEASE(docURL);
#ifdef NECKO
  nsCRT::free(spec);
#endif

  /* get to the form elements */
  PRInt32 count = 0;
  nsIDOMHTMLFormElement* formElement = nsnull;
  nsresult result = formNode->QueryInterface(kIDOMHTMLFormElementIID, (void**)&formElement);
  if ((NS_SUCCEEDED(result)) && (nsnull != formElement)) {
    nsIDOMHTMLCollection* elements = nsnull;
    result = formElement->GetElements(&elements);
    if ((NS_SUCCEEDED(result)) && (nsnull != elements)) {

      char* name_array[MAX_ARRAY_SIZE];
      char* value_array[MAX_ARRAY_SIZE];
      uint8 type_array[MAX_ARRAY_SIZE];
      PRInt32 value_cnt = 0;

      /* got to the form elements at long last */
      /* now find out how many text fields are on the form */
      /* also build arrays for single signon */
      PRUint32 numElements;
      elements->GetLength(&numElements);
      for (PRUint32 elementX = 0; elementX < numElements; elementX++) {
        nsIDOMNode* elementNode = nsnull;
        elements->Item(elementX, &elementNode);
        if (nsnull != elementNode) {
          nsIDOMHTMLInputElement* inputElement;  
          result =
            elementNode->QueryInterface(kIDOMHTMLInputElementIID, (void**)&inputElement);
          if ((NS_SUCCEEDED(result)) && (nsnull != inputElement)) {
            nsAutoString type;
            result = inputElement->GetType(type);
            if (NS_SUCCEEDED(result)) {
              PRBool isText = ((type == "") || (type.Compare("text", PR_TRUE)==0));
              PRBool isPassword = (type.Compare("password", PR_TRUE)==0);
              if (isText) {
                count++;
              }
              if (value_cnt < MAX_ARRAY_SIZE) {
                if (isText) {
                  type_array[value_cnt] = FORM_TYPE_TEXT;
                } else if (isPassword) {
                  type_array[value_cnt] = FORM_TYPE_PASSWORD;
                }
                nsAutoString value;
                result = inputElement->GetValue(value);
                if (NS_SUCCEEDED(result)) {
                  nsAutoString field;
                  result = inputElement->GetName(field);
                  if (NS_SUCCEEDED(result)) {
                    value_array[value_cnt] = value.ToNewCString();
                    name_array[value_cnt] = field.ToNewCString();
                    value_cnt++;
                  }
                }
              }
            }
            NS_RELEASE(inputElement);
          }
          NS_RELEASE(elementNode);
        }
      }

      /* save login if appropriate */
      SINGSIGN_RememberSignonData
        (URLName, (char**)name_array, (char**)value_array, (char**)type_array, value_cnt);

      /* save form if it meets all necessary conditions */
      if (wallet_GetFormsCapturingPref() && (count>=3) && wallet_OKToCapture(URLName)) {

        /* conditions all met, now save it */
        for (PRUint32 elementY = 0; elementY < numElements; elementY++) {
          nsIDOMNode* elementNode = nsnull;
          elements->Item(elementY, &elementNode);
          if (nsnull != elementNode) {
            nsIDOMHTMLInputElement* inputElement;  
            result =
              elementNode->QueryInterface(kIDOMHTMLInputElementIID, (void**)&inputElement);
            if ((NS_SUCCEEDED(result)) && (nsnull != inputElement)) {

              /* it's an input element */
              nsAutoString type;
              result = inputElement->GetType(type);
              if ((NS_SUCCEEDED(result)) &&
                  ((type =="") || (type.Compare("text", PR_TRUE) == 0))) {
                nsAutoString field;
                result = inputElement->GetName(field);
                if (NS_SUCCEEDED(result)) {
                  nsAutoString value;
                  result = inputElement->GetValue(value);
                  if (NS_SUCCEEDED(result)) {

                    /* get schema name from vcard attribute if it exists */
                    nsAutoString vcardValue("");
                    nsIDOMElement * element;
                    result = elementNode->QueryInterface(kIDOMElementIID, (void**)&element);
                    if ((NS_SUCCEEDED(result)) && (nsnull != element)) {
                      nsAutoString vcardName("VCARD_NAME");
                      result = element->GetAttribute(vcardName, vcardValue);
                      NS_RELEASE(element);
                    }
                    wallet_Capture(doc, field, value, vcardValue);
                  }
                }
              }
              NS_RELEASE(inputElement);
            }
            NS_RELEASE(elementNode);
          }
        }
      }
      NS_RELEASE(elements);
    }
    NS_RELEASE(formElement);
  }
}

PUBLIC void
WLLT_FetchFromNetCenter() {

  /* temporary patch to avoid bug 11766 */
  return;

  nsresult rv;

  nsFileSpec dirSpec;
  rv = Wallet_ResourceDirectory(dirSpec);
  if (NS_FAILED(rv)) {
    return;
  }
  rv = NS_NewURItoFile("http://people.netscape.com/morse/wallet/URLFieldSchema.tbl",
                       dirSpec, "URLFieldSchema.tbl");
  if (NS_FAILED(rv)) {
    return;
  }
  rv = NS_NewURItoFile("http://people.netscape.com/morse/wallet/SchemaConcat.tbl",
                       dirSpec, "SchemaConcat.tbl");
  if (NS_FAILED(rv)) {
    return;
  }
  rv = NS_NewURItoFile("http://people.netscape.com/morse/wallet/FieldSchema.tbl",
                       dirSpec, "FieldSchema.tbl");
  if (NS_FAILED(rv)) {
    return;
  }
}
