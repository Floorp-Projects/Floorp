/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*
   wallet.cpp
*/

#define AutoCapture
#include "wallet.h"
#include "nsNetUtil.h"

#include "nsIServiceManager.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMNSHTMLOptionCollection.h"
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

#include "nsIWalletService.h"
#include "nsIPasswordSink.h"

#ifdef DEBUG_morse
#define morseAssert NS_ASSERTION
#else
#define morseAssert(x,y) 0
#endif 

typedef PRInt32 nsKeyType;

static NS_DEFINE_IID(kIDOMHTMLDocumentIID, NS_IDOMHTMLDOCUMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLFormElementIID, NS_IDOMHTMLFORMELEMENT_IID);
static NS_DEFINE_IID(kIDOMElementIID, NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLInputElementIID, NS_IDOMHTMLINPUTELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLSelectElementIID, NS_IDOMHTMLSELECTELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLOptionElementIID, NS_IDOMHTMLOPTIONELEMENT_IID);

static NS_DEFINE_IID(kIIOServiceIID, NS_IIOSERVICE_IID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

static NS_DEFINE_CID(kNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);
static NS_DEFINE_CID(kProfileCID, NS_PROFILE_CID);

static NS_DEFINE_IID(kIStringBundleServiceIID, NS_ISTRINGBUNDLESERVICE_IID);
static NS_DEFINE_IID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

static NS_DEFINE_IID(kIFileLocatorIID, NS_IFILELOCATOR_IID);
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);

#include "prlong.h"
#include "prinrval.h"

/********************************************************/
/* The following data and procedures are for preference */
/********************************************************/

typedef int (*PrefChangedFunc) (const char *, void *);

extern void
SI_RegisterCallback(const char* domain, PrefChangedFunc callback, void* instance_data);

extern PRBool
SI_GetBoolPref(const char * prefname, PRBool defaultvalue);

extern void
SI_SetBoolPref(const char * prefname, PRBool prefvalue);

extern void
SI_SetCharPref(const char * prefname, const char * prefvalue);

extern void
SI_GetCharPref(const char * prefname, char** aPrefvalue);

#ifdef AutoCapture
static const char *pref_captureForms = "wallet.captureForms";
static const char *pref_enabled = "wallet.enabled";
#else
static const char *pref_WalletNotified = "wallet.Notified";
#endif /* AutoCapture */
static const char *pref_WalletKeyFileName = "wallet.KeyFileName";
static const char *pref_WalletSchemaValueFileName = "wallet.SchemaValueFileName";
static const char *pref_WalletServer = "wallet.Server";
static const char *pref_WalletFetchPatches = "wallet.fetchPatches";
static const char *pref_WalletVersion = "wallet.version";
static const char *pref_WalletLastModified = "wallet.lastModified";

#ifdef AutoCapture
PRIVATE PRBool wallet_captureForms = PR_FALSE;
#else
PRIVATE PRBool wallet_Notified = PR_FALSE;
#endif
PRIVATE char * wallet_Server = nsnull;

#ifdef AutoCapture
PRIVATE void
wallet_SetFormsCapturingPref(PRBool x)
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
    static PRBool first_time = PR_TRUE;

    if(first_time)
    {
        first_time = PR_FALSE;
        x = SI_GetBoolPref(pref_captureForms, PR_TRUE);
        wallet_SetFormsCapturingPref(x);
        SI_RegisterCallback(pref_captureForms, wallet_FormsCapturingPrefChanged, NULL);
    }
}

PRIVATE PRBool
wallet_GetFormsCapturingPref(void)
{
    wallet_RegisterCapturePrefCallbacks();
    return wallet_captureForms;
}

PRIVATE PRBool
wallet_GetEnabledPref(void)
{
  /* This pref is not in the prefs panel.  It's purpose is to remove wallet from all UI */
  static PRBool first_time = PR_TRUE;
  static PRBool enabled = PR_TRUE;
  if (first_time) {
    first_time = PR_FALSE;
    PRBool x = SI_GetBoolPref(pref_enabled, PR_TRUE);
    enabled = x;
  }
  return enabled;
}

#else

PRIVATE void
wallet_SetWalletNotificationPref(PRBool x) {
  SI_SetBoolPref(pref_WalletNotified, x);
  wallet_Notified = x;
}

PRIVATE PRBool
wallet_GetWalletNotificationPref(void) {
  static PRBool first_time = PR_TRUE;
  if (first_time) {
    PRBool x = SI_GetBoolPref(pref_WalletNotified, PR_FALSE);
    wallet_Notified = x;
  }
  return wallet_Notified;
}
#endif /* ! AutoCapture */


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
#include "nsIHTTPChannel.h"
#include "nsIHTTPEventSink.h" 
#include "nsIHTTPHeader.h"
#include "nsISimpleEnumerator.h"
#include "nsXPIDLString.h"
#include "nsIInterfaceRequestor.h" 

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
  nsFileSpec           mDirSpec;
  char                 *mFileName;
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
  CRTFREEIF(mFileName);
}


NS_IMPL_ISUPPORTS1(InputConsumer, nsIStreamListener);


NS_IMETHODIMP
InputConsumer::OnStartRequest(nsIChannel* channel, nsISupports* context)
{
    PRUint32 httpStatus;
    nsXPIDLCString lastmodified;
    nsCOMPtr<nsIHTTPChannel> pHTTPCon(do_QueryInterface(channel));
    if (pHTTPCon) {
        pHTTPCon->GetResponseStatus(&httpStatus);
        if (httpStatus != 304) 
        {
            mOutFile = new nsOutputFileStream(mDirSpec+mFileName);
            if (!mOutFile->is_open())
                return NS_ERROR_FAILURE;
            nsCOMPtr<nsIAtom> lastmodifiedheader;
            lastmodifiedheader = NS_NewAtom("last-modified");
            pHTTPCon->GetResponseHeader(lastmodifiedheader, 
                                        getter_Copies(lastmodified));
            SI_SetCharPref(pref_WalletLastModified, lastmodified);
        }
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
    if (amt == 0) break;
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
  PRUint32 httpStatus;
  nsCOMPtr<nsIHTTPChannel> pHTTPCon(do_QueryInterface(channel));
  if (pHTTPCon) {
    pHTTPCon->GetResponseStatus(&httpStatus);
  }
  if (mOutFile && httpStatus != 304 ) {
      mOutFile->flush();
      mOutFile->close();
  }
  gKeepRunning = 0;
  return NS_OK;
}

NS_IMETHODIMP
InputConsumer::Init(nsFileSpec dirSpec, const char *out)

{
  mDirSpec = dirSpec;
  mFileName = nsCRT::strdup(out);
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

    rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &gEventQ);
    if (NS_FAILED(rv)) return rv;

    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIURI> pURL;
    rv = serv->NewURI(in, nsnull, getter_AddRefs(pURL));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIChannel> pChannel;

    // Async reading thru the calls of the event sink interface
    rv = NS_OpenURI(getter_AddRefs(pChannel), pURL, serv);
    if (NS_FAILED(rv)) {
        printf("ERROR: NewChannelFromURI failed for %s\n", in);
        return rv;
    }

    // Set the If-Modified-Since header providing pref exists and file exists

    char * lastmodified = nsnull;
    SI_GetCharPref(pref_WalletLastModified, &lastmodified);
    if (lastmodified) {
        /* the pref exists */
        nsInputFileStream strm(dirSpec + out);
        if (strm.is_open()) {
            /* the file exists */
            nsCOMPtr<nsIHTTPChannel> pHTTPCon(do_QueryInterface(pChannel));
            if (pHTTPCon) {
                nsCOMPtr<nsIAtom> ifmodifiedsinceHeader;
                ifmodifiedsinceHeader = NS_NewAtom("If-Modified-Since");
                rv = pHTTPCon->SetRequestHeader(ifmodifiedsinceHeader, lastmodified);
                if (NS_FAILED(rv)) return rv;
            }
            Recycle(lastmodified);
        }
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

    rv = pChannel->AsyncRead(listener,  // IStreamListener consumer
                             nsnull);   // ISupports context

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

class wallet_MapElement {
public:
  wallet_MapElement() : itemList(nsnull) {}
  nsAutoString  item1;
  nsAutoString  item2;
  nsVoidArray * itemList;
};

class wallet_Sublist {
public:
  wallet_Sublist() {}
  nsAutoString item;
};

PRIVATE nsVoidArray * wallet_URLFieldToSchema_list=0;
PRIVATE nsVoidArray * wallet_specificURLFieldToSchema_list=0;
PRIVATE nsVoidArray * wallet_FieldToSchema_list=0;
PRIVATE nsVoidArray * wallet_SchemaToValue_list=0;
PRIVATE nsVoidArray * wallet_SchemaConcat_list=0;
PRIVATE nsVoidArray * wallet_URL_list = 0;
#ifdef AutoCapture
PRIVATE nsVoidArray * wallet_DistinguishedSchema_list = 0;
#endif

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

nsIURI * wallet_lastUrl = NULL;

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
  as.ToCString(s, sizeof(s));
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
    ptr->item1.ToCString(item1, sizeof(item1));
    ptr->item2.ToCString(item2, sizeof(item2));
    fprintf(stdout, "%s %s \n", item1, item2);
    wallet_Sublist * ptr1;
    PRInt32 count2 = LIST_COUNT(ptr->itemList);
    for (PRInt32 i2=0; i2<count2; i2++) {
      ptr1 = NS_STATIC_CAST(wallet_Sublist*, ptr->itemList->ElementAt(i2));
      ptr1->item.ToCString(item, sizeof(item));
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


/*************************************************************************/
/* The following routines are used for accessing strings to be localized */
/*************************************************************************/

#define PROPERTIES_URL "chrome://communicator/locale/wallet/wallet.properties"

PUBLIC PRUnichar *
Wallet_Localize(char* genericString) {
  nsresult ret;
  nsAutoString v;

  /* create a URL for the string resource file */
  nsIIOService* pNetService = nsnull;
  ret = nsServiceManager::GetService(kIOServiceCID, kIIOServiceIID,
    (nsISupports**) &pNetService);

  if (NS_FAILED(ret)) {
    printf("cannot get net service\n");
    return v.ToNewUnicode();
  }
  nsIURI *url = nsnull;

  nsIURI *uri = nsnull;
  ret = pNetService->NewURI(PROPERTIES_URL, nsnull, &uri);
  if (NS_FAILED(ret)) {
    printf("cannot create URI\n");
    nsServiceManager::ReleaseService(kIOServiceCID, pNetService);
    return v.ToNewUnicode();
  }

  ret = uri->QueryInterface(NS_GET_IID(nsIURI), (void**)&url);
  NS_RELEASE(uri);
  nsServiceManager::ReleaseService(kIOServiceCID, pNetService);

  if (NS_FAILED(ret)) {
    printf("cannot create URL\n");
    return v.ToNewUnicode();
  }

  /* create a bundle for the localization */
  nsIStringBundleService* pStringService = nsnull;
  ret = nsServiceManager::GetService(kStringBundleServiceCID,
    kIStringBundleServiceIID, (nsISupports**) &pStringService);
  if (NS_FAILED(ret)) {
    printf("cannot get string service\n");
    NS_RELEASE(url);
    return v.ToNewUnicode();
  }
  nsILocale* locale = nsnull;
  nsIStringBundle* bundle = nsnull;
  char* spec = nsnull;
  ret = url->GetSpec(&spec);
  NS_RELEASE(url);
  if (NS_FAILED(ret)) {
    printf("cannot get url spec\n");
    nsServiceManager::ReleaseService(kStringBundleServiceCID, pStringService);
    nsCRT::free(spec);
    return v.ToNewUnicode();
  }
  ret = pStringService->CreateBundle(spec, locale, &bundle);
  nsCRT::free(spec);
  if (NS_FAILED(ret)) {
    printf("cannot create instance\n");
    nsServiceManager::ReleaseService(kStringBundleServiceCID, pStringService);
    return v.ToNewUnicode();
  }
  nsServiceManager::ReleaseService(kStringBundleServiceCID, pStringService);

  /* localize the given string */
  nsAutoString   strtmp; strtmp.AssignWithConversion(genericString);
  const PRUnichar *ptrtmp = strtmp.GetUnicode();
  PRUnichar *ptrv = nsnull;
  ret = bundle->GetStringFromName(ptrtmp, &ptrv);
  NS_RELEASE(bundle);
  if (NS_FAILED(ret)) {
    printf("cannot get string from name\n");
    return v.ToNewUnicode();
  }
  v = ptrv;
  nsCRT::free(ptrv);
  return v.ToNewUnicode();
}

/**********************/
/* Modal dialog boxes */
/**********************/

PUBLIC PRBool
Wallet_Confirm(PRUnichar * szMessage)
{
  PRBool retval = PR_TRUE; /* default value */

  nsresult res;  
  NS_WITH_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, &res);
  if (NS_FAILED(res)) {
    return retval;
  }

  const nsAutoString message = szMessage;
  retval = PR_FALSE; /* in case user exits dialog by clicking X */
  res = dialog->Confirm(message.GetUnicode(), &retval);
  return retval;
}

PUBLIC PRBool
Wallet_ConfirmYN(PRUnichar * szMessage) {
  nsresult res;  
  NS_WITH_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, &res);
  if (NS_FAILED(res)) {
    return PR_FALSE;
  }

  PRInt32 buttonPressed = 1; /* in case user exits dialog by clickin X */
  PRUnichar * yes_string = Wallet_Localize("Yes");
  PRUnichar * no_string = Wallet_Localize("No");
  PRUnichar * confirm_string = Wallet_Localize("Confirm");

  res = dialog->UniversalDialog(
    NULL, /* title message */
    confirm_string, /* title text in top line of window */
    szMessage, /* this is the main message */
    NULL, /* This is the checkbox message */
    yes_string, /* first button text */
    no_string, /* second button text */
    NULL, /* third button text */
    NULL, /* fourth button text */
    NULL, /* first edit field label */
    NULL, /* second edit field label */
    NULL, /* first edit field initial and final value */
    NULL, /* second edit field initial and final value */
    NULL, /* icon: question mark by default */
    NULL, /* initial and final value of checkbox */
    2, /* number of buttons */
    0, /* number of edit fields */
    0, /* is first edit field a password field */
    &buttonPressed);

  Recycle(yes_string);
  Recycle(no_string);
  Recycle(confirm_string);
  return (buttonPressed == 0);
}


PUBLIC PRInt32
Wallet_3ButtonConfirm(PRUnichar * szMessage)
{
  nsresult res;  
  NS_WITH_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, &res);
  if (NS_FAILED(res)) {
    return 0; /* default value is NO */
  }

  PRInt32 buttonPressed = 1; /* default of NO if user exits dialog by clickin X */
  PRUnichar * yes_string = Wallet_Localize("Yes");
  PRUnichar * no_string = Wallet_Localize("No");
  PRUnichar * never_string = Wallet_Localize("Never");
  PRUnichar * confirm_string = Wallet_Localize("Confirm");

  res = dialog->UniversalDialog(
    NULL, /* title message */
    confirm_string, /* title text in top line of window */
    szMessage, /* this is the main message */
    NULL, /* This is the checkbox message */
    yes_string, /* first button text */
    never_string, /* second button text */
    no_string, /* third button text */
    NULL, /* fourth button text */
    /* note: buttons are laid out as FIRST, THIRD, FOURTH, SECOND */
    NULL, /* first edit field label */
    NULL, /* second edit field label */
    NULL, /* first edit field initial and final value */
    NULL, /* second edit field initial and final value */
    NULL,  /* icon: question mark by default */
    NULL, /* initial and final value of checkbox */
    3, /* number of buttons */
    0, /* number of edit fields */
    0, /* is first edit field a password field */
    &buttonPressed);

  Recycle(yes_string);
  Recycle(no_string);
  Recycle(never_string);
  Recycle(confirm_string);

  if (buttonPressed == 0) {
    return 1; /* YES button pressed */
  } else if (buttonPressed == 2) {
    return 0; /* NO button pressed */
  } else if (buttonPressed == 1) {
    return -1; /* NEVER button pressed */
  } else {
    return 0; /* should never happen */
  }
}

PUBLIC void
Wallet_Alert(PRUnichar * szMessage)
{
  nsresult res;  
  NS_WITH_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, &res);
  if (NS_FAILED(res)) {
    return;     // XXX should return the error
  }

  const nsAutoString message = szMessage;
  res = dialog->Alert(message.GetUnicode());
  return;     // XXX should return the error
}

PUBLIC PRBool
Wallet_CheckConfirmYN(PRUnichar * szMessage, PRUnichar * szCheckMessage, PRBool* checkValue) {
  nsresult res;  
  NS_WITH_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, &res);
  if (NS_FAILED(res)) {
    *checkValue = 0;
    return PR_FALSE;
  }

  PRInt32 buttonPressed = 1; /* in case user exits dialog by clickin X */
  PRUnichar * yes_string = Wallet_Localize("Yes");
  PRUnichar * no_string = Wallet_Localize("No");
  PRUnichar * confirm_string = Wallet_Localize("Confirm");

  res = dialog->UniversalDialog(
    NULL, /* title message */
    confirm_string, /* title text in top line of window */
    szMessage, /* this is the main message */
    szCheckMessage, /* This is the checkbox message */
    yes_string, /* first button text */
    no_string, /* second button text */
    NULL, /* third button text */
    NULL, /* fourth button text */
    NULL, /* first edit field label */
    NULL, /* second edit field label */
    NULL, /* first edit field initial and final value */
    NULL, /* second edit field initial and final value */
    NULL,  /* icon: question mark by default */
    checkValue, /* initial and final value of checkbox */
    2, /* number of buttons */
    0, /* number of edit fields */
    0, /* is first edit field a password field */
    &buttonPressed);

  if (NS_FAILED(res)) {
    *checkValue = 0;
  }
  if (*checkValue!=0 && *checkValue!=1) {
    *checkValue = 0; /* this should never happen but it is happening!!! */
  }
  Recycle(yes_string);
  Recycle(no_string);
  Recycle(confirm_string);
  return (buttonPressed == 0);
}

nsresult
wallet_GetString(nsAutoString& result, PRUnichar * szMessage, PRUnichar * szMessage1)
{
  /* doing wrap manually because of bug 27732 */
  PRInt32 i=0;
  while (szMessage[i] != '\0') {
    if (szMessage[i] == '#') {
      szMessage[i] = '\n';
    }
  i++;
  }

  nsAutoString password;
  nsresult res;  
  NS_WITH_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, &res);
  if (NS_FAILED(res)) {
    return res;
  }

  PRUnichar* pwd = NULL;
  PRInt32 buttonPressed = 1; /* in case user exits dialog by clickin X */
  PRUnichar * prompt_string = Wallet_Localize("PromptForPassword");

  res = dialog->UniversalDialog(
    NULL, /* title message */
    prompt_string, /* title text in top line of window */
    szMessage, /* this is the main message */
    NULL, /* This is the checkbox message */
    NULL, /* first button text, becomes OK by default */
    NULL, /* second button text, becomes CANCEL by default */
    NULL, /* third button text */
    NULL, /* fourth button text */
    szMessage1, /* first edit field label */
    NULL, /* second edit field label */
    &pwd, /* first edit field initial and final value */
    NULL, /* second edit field initial and final value */
    NULL,  /* icon: question mark by default */
    NULL, /* initial and final value of checkbox */
    2, /* number of buttons */
    1, /* number of edit fields */
    1, /* is first edit field a password field */
    &buttonPressed);

  Recycle(prompt_string);

  if (NS_FAILED(res)) {
    return res;
  }
  password = pwd;
  delete[] pwd;

  if (buttonPressed == 0) {
    result = password;
    return NS_OK;
  } else {
    return NS_ERROR_FAILURE; /* user pressed cancel */
  }
}

nsresult
wallet_GetDoubleString(nsAutoString& result, PRUnichar * szMessage, PRUnichar * szMessage1, PRUnichar * szMessage2, PRBool& matched)
{
  /* doing wrap manually because of bug 27732 */
  PRInt32 i=0;
  while (szMessage[i] != '\0') {
    if (szMessage[i] == '#') {
      szMessage[i] = '\n';
    }
  i++;
  }

  nsAutoString password, password2;
  nsresult res;  
  NS_WITH_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, &res);
  if (NS_FAILED(res)) {
    return res;
  }

  PRUnichar* pwd = NULL;
  PRUnichar* pwd2 = NULL;
  PRInt32 buttonPressed = 1; /* in case user exits dialog by clickin X */
  PRUnichar * prompt_string = Wallet_Localize("PromptForPassword");

  res = dialog->UniversalDialog(
    NULL, /* title message */
    prompt_string, /* title text in top line of window */
    szMessage, /* this is the main message */
    NULL, /* This is the checkbox message */
    NULL, /* first button text, becomes OK by default */
    NULL, /* second button text, becomes CANCEL by default */
    NULL, /* third button text */
    NULL, /* fourth button text */
    szMessage1, /* first edit field label */
    szMessage2, /* second edit field label */
    &pwd, /* first edit field initial and final value */
    &pwd2, /* second edit field initial and final value */
    NULL,  /* icon: question mark by default */
    NULL, /* initial and final value of checkbox */
    2, /* number of buttons */
    2, /* number of edit fields */
    1, /* is first edit field a password field */
    &buttonPressed);

  Recycle(prompt_string);

  if (NS_FAILED(res)) {
    return res;
  }
  password = pwd;
  password2 = pwd2;
  delete[] pwd;
  delete[] pwd2;
  matched = (password == password2);

  if (buttonPressed == 0) {
    result = password;
    return NS_OK;
  } else {
    return NS_ERROR_FAILURE; /* user pressed cancel */
  }
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
  wallet_Sublist * ptr1;
  PRInt32 count = LIST_COUNT((*list));
  for (PRInt32 i=count-1; i>=0; i--) {
    if (*list == wallet_DistinguishedSchema_list) {
      ptr1 = NS_STATIC_CAST(wallet_Sublist*, (*list)->ElementAt(i));
      (*list)->RemoveElement(ptr1);
      delete ptr1;
    } else {
      ptr = NS_STATIC_CAST(wallet_MapElement*, (*list)->ElementAt(i));
      PRInt32 count2 = LIST_COUNT(ptr->itemList);
      for (PRInt32 i2=0; i2<count2; i2++) {
        ptr1 = NS_STATIC_CAST(wallet_Sublist*, ptr->itemList->ElementAt(i2));
        delete ptr1;
      }
      delete ptr->itemList;
      (*list)->RemoveElement(ptr);
      delete ptr;
    }
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

  wallet_MapElement * mapElement = new wallet_MapElement;
  if (!mapElement) {
    return;
  }

  item1.ToLowerCase();
  mapElement->item1 = item1;
  mapElement->item2 = item2;
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
  if (AT_END==placement) {
    list->AppendElement(mapElement);
    return;
  }
  PRInt32 count = LIST_COUNT(list);
  for (PRInt32 i=0; i<count; i++) {
    ptr = NS_STATIC_CAST(wallet_MapElement*, list->ElementAt(i));
    if((ptr->item1.Compare(item1))==0) {
      if (DUP_OVERWRITE==placement) {
        delete mapElement;
        ptr->item1 = item1;
        ptr->item2 = item2;
      } else if (DUP_BEFORE==placement) {
        list->InsertElementAt(mapElement, i);
      }
      if (DUP_AFTER!=placement) {
        added_to_list = PR_TRUE;
        break;
      }
    } else if((ptr->item1.Compare(item1))>=0) {
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
    if((ptr->item1.Compare(item1))==0) {
      item2 = nsAutoString(ptr->item2);
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


/*************************************************************/
/* The following routines are for reading/writing utf8 files */
/*************************************************************/

/*
 * For purposed of internationalization, characters are represented in memory as 16-bit
 * values (unicode, aka UCS-2) rather than 7 bit values (ascii).  For simplicity, the
 * unicode representation of an ascii character has the upper 9 bits of zero and the
 * lower 7 bits equal to the 7-bit ascii value.
 *
 * These 16-bit unicode values could be stored directly in files.  However such files would
 * not be readable by ascii editors even if they contained all ascii values.  To solve
 * this problem, the 16-bit unicode values are first encoded into a sequence of 8-bit
 * characters before being written to the file -- the encoding is such that unicode
 * characters which have the upper 9 bits of zero are encoded into a single 8-bit character
 * of the same value whereas the remaining unicode characters are encoded into a sequence of
 * more than one 8-bit character.
 *
 * There is a standard 8-bit-encoding of bit strings and it is called UTF-8.  The format of
 * UTF-8 is as follows:
 *
 * Up to  7 bits: 0xxxxxxx
 * Up to 11 bits: 110xxxxx 10xxxxxx
 * Up to 16 bits: 1110xxxx 10xxxxxx 10xxxxxx
 * Up to 21 bits: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
 * Up to 26 bits: 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
 * Up to 31 bits: 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
 *
 * Since we are converting unicode (16-bit) values, we need only be concerned with the
 * first three lines above.
 *
 * There are conversion routines provided in intl/uconv which convert between unicode and
 * UTF-8.  However these routines are extremely cumbersome to use.  So I have a very simple
 * pair of encoding/decoding routines for converting between unicode characters and UTF-8
 * sequences.  Here are the encoding/decoding algorithms that I use:
 *
 *   encoding 16-bit unicode to 8-bit utf8 stream:
 *      if (unicodeChar <= 0x7F) { // up to 7 bits
 *        utf8Char1 = 0xxxxxxx + lower 7 bits of unicodeChar
 *      } else if (unicodeChar <= 0x7FF) { // up to 11 bits
 *        utf8Char1 = 110xxxxx + upper 5 bits of unicodeChar
 *        utf8Char2 = 10xxxxxx + lower 6 bits of unicodeChar
 *      } else { // up to 16 bits
 *        utf8Char1 = 1110xxxx + upper 4 bits of unicodeChar
 *        utf8Char2 = 10xxxxxx + next 6 bits of unicodeChar
 *        utf8Char3 = 10xxxxxx + lower 6 bits of unicodeChar
 *      }
 *
 *   decoding 8-bit utf8 stream to 16-bit unicode:
 *      if (utf8Char1 starts with 0) {
 *        unicodeChar = utf8Char1;
 *      } else if (utf8Char1 starts with 110) {
 *        unicodeChar = (lower 5 bits of utf8Char1)<<6 
 *                    + (lower 6 bits of utf8Char2);
 *      } else if (utf8Char1 starts with 1110) {
 *        unicodeChar = (lower 4 bits of utf8Char1)<<12
 *                    + (lower 6 bits of utf8Char2)<<6
 *                    + (lower 6 bits of utf8Char3);
 *      } else {
 *        error;
 *      }
 *
 */

PUBLIC void
Wallet_UTF8Put(nsOutputFileStream strm, PRUnichar c) {
  if (c <= 0x7F) {
    strm.put((char)c);
  } else if (c <= 0x7FF) {
    strm.put(((PRUnichar)0xC0) | ((c>>6) & 0x1F));
    strm.put(((PRUnichar)0x80) | (c & 0x3F));
  } else {
    strm.put(((PRUnichar)0xE0) | ((c>>12) & 0xF));
    strm.put(((PRUnichar)0x80) | ((c>>6) & 0x3F));
    strm.put(((PRUnichar)0x80) | (c & 0x3F));
  }
}

PUBLIC PRUnichar
Wallet_UTF8Get(nsInputFileStream strm) {
  PRUnichar c = (strm.get() & 0xFF);
  if ((c & 0x80) == 0x00) {
    return c;
  } else if ((c & 0xE0) == 0xC0) {
    return (((c & 0x1F)<<6) + (strm.get() & 0x3F));
  } else if ((c & 0xF0) == 0xE0) {
    return (((c & 0x0F)<<12) + ((strm.get() & 0x3F)<<6) + (strm.get() & 0x3F));
  } else {
    return 0; /* this is an error, input was not utf8 */
  }
}

/*
 * I have an even a simpler set of routines if you are not concerned about UTF-8.  The
 * algorithms for those routines are as follows:
 *
 *   encoding 16-bit unicode to 8-bit simple stream:
 *      if (unicodeChar < 0xFF) {
 *        simpleChar1 = unicodeChar
 *      } else {
 *        simpleChar1 = 0xFF
 *        simpleChar2 = upper 8 bits of unicodeChar
 *        simpleChar3 = lower 8 bits of unicodeChar
 *      }
 *
 *   decoding 8-bit simple stream to 16-bit unicode:
 *      if (simpleChar1 < 0xFF) {
 *        unicodeChar = simpleChar1;
 *      } else {
 *        unicodeChar = 256*simpleChar2 + simpleChar3;
 *      }
 *
 */

PUBLIC void
Wallet_SimplePut(nsOutputFileStream strm, PRUnichar c) {
  if (c < 0xFF) {
    strm.put((char)c);
  } else {
    strm.put((PRUnichar)0xFF);
    strm.put((c>>8) & 0xFF);
    strm.put(c & 0xFF);
  }
}

PUBLIC PRUnichar
Wallet_SimpleGet(nsInputFileStream strm) {
  PRUnichar c = (strm.get() & 0xFF);
  if (c != 0xFF) {
    return c;
  } else {
    return ((strm.get() & 0xFF)<<8) + (strm.get() & 0xFF);
  }
}


/************************************************************/
/* The following routines are for unlocking the stored data */
/************************************************************/

// Dont use a COMPtr here. If you do, then if there is a leak of this object
// there will be crash due to static destructors being called.
nsIKeyedStreamGenerator* gKeyedStreamGenerator;
PRBool gNeedsSetup = PR_TRUE;
nsAutoString key;
PRBool keyCancel = PR_FALSE;
PRBool gIsKeySet = PR_FALSE;
time_t keyExpiresTime;

// 30 minute duration (60*30=1800 seconds)
#define keyDuration 1800
char* keyFileName = nsnull;
char* schemaValueFileName = nsnull;

const char URLFileName[] = "URL.tbl";
const char allFileName[] = "All.tbl";
const char fieldSchemaFileName[] = "FieldSchema.tbl";
const char URLFieldSchemaFileName[] = "URLFieldSchema.tbl";
const char schemaConcatFileName[] = "SchemaConcat.tbl";
#ifdef AutoCapture
const char distinguishedSchemaFileName[] = "DistinguishedSchema.tbl";
#endif

PUBLIC PRBool
Wallet_IsKeySet() {
  return gIsKeySet;
}

NS_METHOD
Wallet_GetMasterPassword(PRUnichar ** password)
{
  NS_ENSURE_ARG_POINTER(password);
  // We just return the key if we have one.
  // If we dont have a key, we do not attempt to popping up a dialog and
  // getting the key from the user. That was the need at the time we
  // are writing this.
  if (!Wallet_IsKeySet()) return NS_ERROR_FAILURE;
  *password = nsCRT::strdup(key.GetUnicode());
  if (!password) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

PUBLIC PRUnichar
Wallet_GetKey(nsKeyType saveCount, nsKeyType writeCount) {
  nsresult rv = NS_OK;
  PRUint8 keyByte1 = 0, keyByte2 = 0;
  if (!gKeyedStreamGenerator)
  {
    /* Get a keyed stream generator */
    // XXX how do we get to the NS_BASIC_STREAM_GENERATOR progid/CID here
    nsCOMPtr<nsIKeyedStreamGenerator> keyGenerator = do_CreateInstance("component://netscape/keyed-stream-generator/basic", &rv);
    if (NS_FAILED(rv)) {
      goto backup;
    }
    gKeyedStreamGenerator = keyGenerator.get();
    NS_ADDREF(gKeyedStreamGenerator);
    // XXX need to checkup signature
  }
  if (gNeedsSetup)
  {
    /* Call setup on the keyed stream generator */
    nsCOMPtr<nsIWalletService> walletService = do_GetService(NS_WALLETSERVICE_PROGID, &rv);
    if (NS_FAILED(rv)) {
      goto backup;
    }
    nsCOMPtr<nsIPasswordSink> passwordSink = do_QueryInterface(walletService, &rv);
    if (NS_FAILED(rv)) {
      goto backup;
    }
    rv = gKeyedStreamGenerator->Setup(saveCount, passwordSink);
    gNeedsSetup = PR_FALSE;
  }

  /* Get two bytes using the keyed stream generator */
  rv = gKeyedStreamGenerator->GetByte(writeCount, &keyByte1);
  if (NS_FAILED(rv)) {
    goto backup;
  }
  rv = gKeyedStreamGenerator->GetByte(writeCount, &keyByte2);
  if (NS_FAILED(rv)) {
    goto backup;
  }
  return (((PRUnichar)keyByte1)<<8) + (PRUnichar)keyByte2;

backup:
  /* Fallback to doing old access. This should never happen. */
  NS_ASSERTION(0, "Bad! Using backup stream generator. Email dp@netscape.com");
  NS_ASSERTION(key.Length()>0, "Master Password was never established");
  if (key.Length() > 0 ) {
    return key.CharAt((PRInt32)(writeCount % key.Length()));
  } else {
    return '~'; /* What else can we do?  We can't recover from this. */
  }
}

PUBLIC void
Wallet_StreamGeneratorReset() {
  if (gKeyedStreamGenerator)
  {
    (void) gKeyedStreamGenerator->Setup(0, NULL);
    gNeedsSetup = PR_TRUE;
  }
}

PUBLIC PRBool
Wallet_InitKeySet(PRBool b) {
  PRBool oldIsKeySet = gIsKeySet;
  gIsKeySet = b;
  // When transitioning from key set to not set, we need to make sure
  // the keyedStreamGenerator also forgets all state. The reverse,
  // setting it up properly after we have a key, happens lazily in GetKey.
  if (oldIsKeySet == PR_TRUE && gIsKeySet == PR_FALSE)
  {
    Wallet_StreamGeneratorReset();
  }
  return oldIsKeySet;
}

extern void SI_RemoveAllSignonData();

PUBLIC PRBool
Wallet_KeyTimedOut() {
  time_t curTime = time(NULL);
  if (Wallet_IsKeySet() && (curTime >= keyExpiresTime)) {
    Wallet_InitKeySet(PR_FALSE);
    SI_RemoveAllSignonData();
    return PR_TRUE;
  }
  return PR_FALSE;
}

PUBLIC void
Wallet_KeyResetTime() {
  if (Wallet_IsKeySet()) {
    keyExpiresTime = time(NULL) + keyDuration;
  }
}

PUBLIC void
WLLT_ExpirePassword() {
  if (Wallet_IsKeySet()) {
    keyExpiresTime = time(NULL);
  }
  SI_RemoveAllSignonData();
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
  nsresult res = spec->GetFileSpec(&dirSpec);
  NS_RELEASE(spec);
  return res;
}

PUBLIC nsresult Wallet_ResourceDirectory(nsFileSpec& dirSpec) {
  nsIFileSpec* spec =
    NS_LocateFileOrDirectory(nsSpecialFileSpec::App_ResDirectory);
  if (!spec) {
    return NS_ERROR_FAILURE;
  }
  nsresult res = spec->GetFileSpec(&dirSpec);
  NS_RELEASE(spec);
  return res;
}

extern void SI_InitSignonFileName();

PUBLIC char *
Wallet_RandomName(char* suffix)
{
  /* pick the current time as the random number */
  time_t curTime = time(NULL);

  /* take 8 least-significant digits + three-digit suffix as the file name */
  char name[13];
  PR_snprintf(name, 13, "%lu.%s", (curTime%100000000), suffix);
  return PL_strdup(name);
}

PRIVATE PRBool
wallet_IsOldKeyFormat() {
  /* old format: key filename = xxxxxxxx.key (12 characters)
   * new format: key filename = xxxxxxxx.k (10 characters)
   */
  return (PL_strlen(keyFileName) == 12);
}

PRIVATE void
wallet_InitKeyFileName() {
  static PRBool namesInitialized = PR_FALSE;
  if (!namesInitialized) {
    SI_GetCharPref(pref_WalletKeyFileName, &keyFileName);
    if (!keyFileName) {
      keyFileName = Wallet_RandomName("k");
      SI_SetCharPref(pref_WalletKeyFileName, keyFileName);
    }
    SI_GetCharPref(pref_WalletSchemaValueFileName, &schemaValueFileName);
    if (!schemaValueFileName) {
      schemaValueFileName = Wallet_RandomName("w");
      SI_SetCharPref(pref_WalletSchemaValueFileName, schemaValueFileName);
    }
    SI_InitSignonFileName();
    namesInitialized = PR_TRUE;
  }
}

/* returns -1 if key does not exist, 0 if key is of length 0, 1 otherwise */
PUBLIC PRInt32
Wallet_KeySize() {
  wallet_InitKeyFileName();
  nsFileSpec dirSpec;
  nsresult rv = Wallet_ProfileDirectory(dirSpec);
  if (NS_FAILED(rv)) {
    return -1;
  }
  nsInputFileStream strm(dirSpec + keyFileName);
  if (!strm.is_open()) {
    return -1;
  } else {
#define BUFSIZE 128
    char buffer[BUFSIZE];
    PRInt32 count;
    count = strm.read(buffer, BUFSIZE);
    strm.close();
    if (wallet_IsOldKeyFormat()) {
      return ((count == 0) ? 0 : 1);
    }
    nsAutoString temp; temp.AssignWithConversion(buffer);
    PRInt32 start = 0;
    for (PRInt32 i=0; i<5; i++) { /* skip over the five lines of the header */
      start = temp.FindChar('\n', PR_FALSE, start);
      if (start == -1) {
        return -1; /* this should never happen, but just in case */
      }
      start++;
    }
    return ((start < count) ? 1 : 0);
  }
}

void
wallet_GetHeader(nsInputFileStream strm, nsKeyType& saveCount, nsKeyType& readCount);

void
wallet_PutHeader(nsOutputFileStream strm, nsKeyType saveCount, nsKeyType writeCount);

/*
 * Note: low-order four bits of saveCount will designate the type of file being accessed.
 * Whenever saveCount is incremented, it will be by 16.  That guarentees that the initial
 * value given to the low-order four bits will never change.
 *
 * The various file types in use by wallet and single signon are:
 *    Password files (.p and .u)
 *    Wallet file (.w)
 *    Key file (.k)
 */
#define INITIAL_SAVECOUNTW 2
#define INITIAL_SAVECOUNTK 3

static nsKeyType saveCountW = INITIAL_SAVECOUNTW;
static nsKeyType saveCountK = INITIAL_SAVECOUNTK;


PRBool
wallet_ReadKeyFile(PRBool useDefaultKey) {
  nsKeyType writeCount = 0;

  if (useDefaultKey && (Wallet_KeySize() == 0) ) {
    Wallet_InitKeySet(PR_TRUE);
    return PR_TRUE;
  }

  nsFileSpec dirSpec;
  nsresult rval = Wallet_ProfileDirectory(dirSpec);
  if (NS_FAILED(rval)) {
    keyCancel = PR_TRUE;
    return PR_FALSE;
  }
  nsInputFileStream strm(dirSpec + keyFileName);
  writeCount = 0;
  if (!wallet_IsOldKeyFormat()) {
    wallet_GetHeader(strm, saveCountK, writeCount);
  }

  /*
   * Note that eof() is not set until after we read past the end of the file.  That
   * is why the following code reads a character and immediately after the read
   * checks for eof()
   */

  for (PRUint32 j = 1; j < key.Length(); j++) {
    if (Wallet_UTF8Get(strm) != ((key.CharAt(j))^Wallet_GetKey(saveCountK, writeCount++))
        || strm.eof()) {
      strm.close();
      Wallet_InitKeySet(PR_FALSE);
      key.SetLength(0);
      keyCancel = PR_FALSE;
      return PR_FALSE;
    }
  }
  if (key.Length() != 0) {
    if (Wallet_UTF8Get(strm) != ((key.CharAt(0))^Wallet_GetKey(saveCountK, writeCount++))
        || strm.eof()) {
      strm.close();
      Wallet_InitKeySet(PR_FALSE);
      key.SetLength(0);
      keyCancel = PR_FALSE;
      return PR_FALSE;
    }
  }

  Wallet_UTF8Get(strm); /* to get past the end of the file so eof() will get set */
  PRBool rv = strm.eof();
  strm.close();
  if (rv) {
    Wallet_InitKeySet(PR_TRUE);
    keyExpiresTime = time(NULL) + keyDuration;
    return PR_TRUE;
  } else {
    Wallet_InitKeySet(PR_FALSE);
    key.SetLength(0);
    keyCancel = PR_FALSE;
    return PR_FALSE;
  }
}

PRBool
wallet_WriteKeyFile(PRBool useDefaultKey) {
  nsKeyType writeCount = 0;

  nsFileSpec dirSpec;
  nsresult rval = Wallet_ProfileDirectory(dirSpec);
  if (NS_FAILED(rval)) {
    keyCancel = PR_TRUE;
    return PR_FALSE;
  }

  if (wallet_IsOldKeyFormat()) {
    /* change name of key file from "xxxxxxxx.key" to "xxxxxxxx.k" */    
    keyFileName[10] = '\0';
    SI_SetCharPref(pref_WalletKeyFileName, keyFileName);
  }

  nsOutputFileStream strm2(dirSpec + keyFileName);
  if (!strm2.is_open()) {
    key.SetLength(0);
    keyCancel = PR_TRUE;
    return PR_FALSE;
  }

  /* write out the header information */
  saveCountK += 16; /* preserve low order four bits which designate the file type */
  wallet_PutHeader(strm2, saveCountK, writeCount);

  /* If we store the key obscured by the key itself, then the result will be zero
   * for all keys (since we are using XOR to obscure).  So instead we store
   * key[1..n],key[0] obscured by the actual key.
   */

  if (!useDefaultKey && (key.Length() != 0)) {
    for (PRUint32 i = 1; i < key.Length(); i++) {
      Wallet_UTF8Put(strm2, (key.CharAt(i))^Wallet_GetKey(saveCountK, writeCount++));
    }
    Wallet_UTF8Put(strm2, (key.CharAt(0))^Wallet_GetKey(saveCountK, writeCount++));
  }
  strm2.flush();
  strm2.close();
  Wallet_InitKeySet(PR_TRUE);
  return PR_TRUE;
}

PUBLIC PRBool
Wallet_SetKey(PRBool isNewkey) {
  nsresult res;
  if (Wallet_IsKeySet() && !isNewkey) {
    return PR_TRUE;
  }
  nsAutoString newkey;
  PRBool useDefaultKey = PR_FALSE;

  if (Wallet_KeySize() < 0) { /* no key has yet been established */

    PRUnichar * message = Wallet_Localize("firstPassword");
    PRUnichar * message1 = Wallet_Localize("enterPassword");
    PRUnichar * message2 = Wallet_Localize("confirmPassword");
    PRUnichar * mismatch = Wallet_Localize("confirmFailed_TryAgain?");
    PRBool matched;
    for (;;) {
      res = wallet_GetDoubleString(newkey, message, message1, message2, matched);
      if (NS_SUCCEEDED(res) && matched) {
        break; /* break out of loop if both passwords matched */
      }
      /* password confirmation failed, ask user if he wants to try again */
      if (NS_FAILED(res) || (!Wallet_Confirm(mismatch))) {
        Recycle(mismatch);
        Recycle(message);
        Recycle(message1);
        Recycle(message2);
        keyCancel = PR_TRUE;
        return FALSE; /* user does not want to try again */
      }    
    }
    PR_FREEIF(mismatch);
    PR_FREEIF(message);
    Recycle(message1);
    PR_FREEIF(message2);

  } else { /* key has previously been established */
    PRUnichar * message;
    PRUnichar * message1 = Wallet_Localize("enterPassword");
    PRUnichar * message2 = Wallet_Localize("confirmPassword");
    PRUnichar * mismatch = Wallet_Localize("confirmFailed_TryAgain?");
    PRBool matched;
    if (isNewkey) { /* user is changing his key */
      message = Wallet_Localize("newPassword");
    } else {
      message = Wallet_Localize("password");
    }
    if ((Wallet_KeySize() == 0) && !isNewkey) { /* prev-established key is default key */
      useDefaultKey = PR_TRUE;
      newkey.AssignWithConversion("~");
    } else { /* ask the user for his key */
      if (isNewkey) { /* user is changing his password */
        for (;;) {
          res = wallet_GetDoubleString(newkey, message, message1, message2, matched);
          if (!NS_FAILED(res) && matched) {
            break; /* break out of loop if both passwords matched */
          }
          /* password confirmation failed, ask user if he wants to try again */
          if (NS_FAILED(res) || (!Wallet_Confirm(mismatch))) {
            Recycle(mismatch);
            Recycle(message);
            Recycle(message1);
            Recycle(message2);
            keyCancel = PR_TRUE;
            return FALSE; /* user does not want to try again */
          }    
        }
      } else {
        res = wallet_GetString(newkey, message, message1);
      }
      if (NS_FAILED(res)) {
        Recycle(message);
        Recycle(message1);
        Recycle(message2);
        Recycle(mismatch);
        keyCancel = PR_TRUE;
        return PR_FALSE; /* user pressed cancel -- does not want to enter a new key */
      }
    }
    Recycle(message);
    Recycle(message1);
    Recycle(message2);
    Recycle(mismatch);
  }

  if (newkey.Length() == 0) { /* user entered a zero-length key */
    if ((Wallet_KeySize() < 0) || isNewkey ){
      /* no key file existed before or using is changing the key */
      useDefaultKey = PR_TRUE;
      newkey.AssignWithConversion("~"); /* use zero-length key */
    }
  }
  Wallet_InitKeySet(PR_TRUE);
  key = newkey;

  if (isNewkey || (Wallet_KeySize() < 0)) {
    /* Either key is to be changed or the file containing the saved key doesn't exist */
    /* In either case we need to (re)create and re(write) the file */
    return wallet_WriteKeyFile(useDefaultKey);
  } else {
    /* file of saved key existed so see if it matches the key the user typed in */

    if (!wallet_ReadKeyFile(useDefaultKey)) {
      return PR_FALSE;
    }

    /* it matched so migrate old keyfile if necessary */
    if (wallet_IsOldKeyFormat()) {
      return wallet_WriteKeyFile(useDefaultKey);
    }
    return PR_TRUE;
  }
}

/******************************************************/
/* The following routines are for accessing the files */
/******************************************************/

#define HEADER_VERSION_1 "#1"

/*
 * get a line from a file
 * return -1 if end of file reached
 * strip carriage returns and line feeds from end of line
 */
PRInt32
wallet_GetLine(nsInputFileStream strm, nsAutoString& line, PRBool obscure,
    nsKeyType saveCount = 0, nsKeyType *readCount = 0, PRBool inHeader = PR_FALSE) {

  /* read the line */
  line.SetLength(0);
  PRUnichar c;
  for (;;) {
    if (inHeader) {
      c = Wallet_UTF8Get(strm);
    } else {
      c = Wallet_UTF8Get(strm)^
        (obscure ? Wallet_GetKey(saveCount, (*readCount)++) : (PRUnichar)0);
    }
    if (c == '\n') {
      break;
    }

    /* note that eof is not set until we read past the end of the file */
    if (strm.eof()) {
      return -1;
    }

    if (c != '\r') {
      line += c;
    }
  }

  return NS_OK;
}

void
wallet_GetHeader(nsInputFileStream strm, nsKeyType& saveCount, nsKeyType& readCount){
  nsAutoString format;
  nsAutoString buffer;
  nsKeyType temp;
  PRInt32 error;

  /* format revision number */
  if (NS_FAILED(wallet_GetLine(strm, format, PR_FALSE, 0, 0, PR_TRUE))) {
    return;
  }
  if (!format.EqualsWithConversion(HEADER_VERSION_1)) {
    /* something's wrong */
    return;
  }

  /* saveCount */
  if (NS_FAILED(wallet_GetLine(strm, buffer, PR_FALSE, 0, 0, PR_TRUE))) {
    return;
  }
  if (NS_FAILED(wallet_GetLine(strm, buffer, PR_FALSE, 0, 0, PR_TRUE))) {
    return;
  }
  temp = (nsKeyType)(buffer.ToInteger(&error));
  if (error) {
    return;
  }
  saveCount = (nsKeyType)(buffer.ToInteger(&error));

  /* readCount */
  if (NS_FAILED(wallet_GetLine(strm, buffer, PR_FALSE, 0, 0, PR_TRUE))) {
    return;
  }
  if (NS_FAILED(wallet_GetLine(strm, buffer, PR_FALSE, 0, 0, PR_TRUE))) {
    return;
  }
  temp = (nsKeyType)(buffer.ToInteger(&error));
  if (error) {
    return;
  }
  readCount = (nsKeyType)(buffer.ToInteger(&error));

  Wallet_StreamGeneratorReset();
}

/*
 * Write a line to a file
 */
void
wallet_PutLine(nsOutputFileStream strm, const nsAutoString& line, PRBool obscure,
   nsKeyType saveCount = 0, nsKeyType *writeCount = 0, PRBool inHeader = PR_FALSE)
{
  for (PRUint32 i=0; i<line.Length(); i++) {
    if (inHeader) {
      Wallet_UTF8Put(strm, line.CharAt(i));
    } else {
    Wallet_UTF8Put(strm, line.CharAt(i)^(obscure ? Wallet_GetKey(saveCount, (*writeCount)++) : (PRUnichar)0));
    }
  }
  Wallet_UTF8Put(strm, '\n'^(obscure ? Wallet_GetKey(saveCount, (*writeCount)++) : (PRUnichar)0));
}

void
wallet_PutHeader(nsOutputFileStream strm, nsKeyType saveCount, nsKeyType writeCount){

  /* format revision number */
  {
    nsAutoString temp1;
    temp1.AssignWithConversion(HEADER_VERSION_1);
    wallet_PutLine(strm, temp1, PR_FALSE, 0, 0, PR_TRUE);
  }

  /* saveCount */
  nsAutoString buffer;
  buffer.AppendInt(PRInt32(saveCount),10);
  wallet_PutLine(strm, buffer, PR_FALSE, 0, 0, PR_TRUE);
  wallet_PutLine(strm, buffer, PR_FALSE, 0, 0, PR_TRUE);

  /* writeCount */
  buffer.SetLength(0);
  buffer.AppendInt(PRInt32(writeCount),10);
  wallet_PutLine(strm, buffer, PR_FALSE, 0, 0, PR_TRUE);
  wallet_PutLine(strm, buffer, PR_FALSE, 0, 0, PR_TRUE);

  Wallet_StreamGeneratorReset();
}

/*
 * write contents of designated list into designated file
 */
void
wallet_WriteToFile(const char * filename, nsVoidArray* list, PRBool obscure) {
  wallet_MapElement * ptr;

  if (obscure && !Wallet_IsKeySet()) {
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
  nsKeyType writeCount = 0;

  /* put out the header */
  if (filename == schemaValueFileName) {
    saveCountW += 16; /* preserve low order four bits which designate the file type */
    wallet_PutHeader(strm, saveCountW, writeCount);
  }

  /* traverse the list */
  PRInt32 count = LIST_COUNT(list);
  for (PRInt32 i=0; i<count; i++) {
    ptr = NS_STATIC_CAST(wallet_MapElement*, list->ElementAt(i));
    wallet_PutLine(strm, (*ptr).item1, obscure, saveCountW, &writeCount);
    if (!(*ptr).item2.IsEmpty()) {
      wallet_PutLine(strm, (*ptr).item2, obscure, saveCountW, &writeCount);
    } else {
      wallet_Sublist * ptr1;
      PRInt32 count2 = LIST_COUNT(ptr->itemList);
      for (PRInt32 j=0; j<count2; j++) {
        ptr1 = NS_STATIC_CAST(wallet_Sublist*, ptr->itemList->ElementAt(j));
        wallet_PutLine(strm, (*ptr1).item, obscure, saveCountW, &writeCount);
      }
    }
    wallet_PutLine(strm, nsAutoString(), obscure, saveCountW, &writeCount);
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
    (const char * filename, nsVoidArray*& list, PRBool obscure, PRBool localFile, PlacementType placement = DUP_AFTER) {

  /* open input stream */
  nsFileSpec dirSpec;
  nsresult rv;
  rv = Wallet_ProfileDirectory(dirSpec);
  if (NS_FAILED(rv) && localFile) {
    /* if we failed to download the file, see if an initial version of it exists */
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
  nsKeyType readCount = 0;

  /* read in the header */
  if (filename == schemaValueFileName) {
    wallet_GetHeader(strm, saveCountW, readCount);
  }

  for (;;) {
    nsAutoString item1;
    if (NS_FAILED(wallet_GetLine(strm, item1, obscure, saveCountW, &readCount))) {
      /* end of file reached */
      break;
    }

#ifdef AutoCapture
    /* Distinguished schema list is a list of single entries, not name/value pairs */
    if (PL_strcmp(filename, distinguishedSchemaFileName) == 0) {
      nsVoidArray* dummy = NULL;
      wallet_WriteToList(item1, item1, dummy, list, placement);
      continue;
    }
#endif

    nsAutoString item2;
    if (NS_FAILED(wallet_GetLine(strm, item2, obscure, saveCountW, &readCount))) {
      /* unexpected end of file reached */
      break;
    }

    nsAutoString item3;
    if (NS_FAILED(wallet_GetLine(strm, item3, obscure, saveCountW, &readCount))) {
      /* end of file reached */
      nsVoidArray* dummy = NULL;
      wallet_WriteToList(item1, item2, dummy, list, placement);
      strm.close();
      return;
    }

    if (item3.Length()==0) {
      /* just a pair of values, no need for a sublist */
      nsVoidArray* dummy = NULL;
      wallet_WriteToList(item1, item2, dummy, list, placement);
    } else {
      /* need to create a sublist and put item2 and item3 onto it */
      nsVoidArray * itemList = new nsVoidArray();
      if (!itemList) {
        break;
      }
      wallet_Sublist * sublist = new wallet_Sublist;
      if (!sublist) {
        break;
      }
      sublist->item = item2;
      itemList->AppendElement(sublist);
      sublist = new wallet_Sublist;
      if (!sublist) {
        break;
      }
      sublist->item = item3;
      itemList->AppendElement(sublist);
      /* add any following items to sublist up to next blank line */
      nsAutoString dummy2;
      for (;;) {
        /* get next item for sublist */
        item3.SetLength(0);
        if (NS_FAILED(wallet_GetLine(strm, item3, obscure, saveCountW, &readCount))) {
          /* end of file reached */
          wallet_WriteToList(item1, dummy2, itemList, list, placement);
          strm.close();
          return;
        }
        if (item3.Length()==0) {
          /* blank line reached indicating end of sublist */
          wallet_WriteToList(item1, dummy2, itemList, list, placement);
          break;
        }
        /* add item to sublist */
        sublist = new wallet_Sublist;
        if (!sublist) {
          break;
        }
        sublist->item = nsAutoString (item3);
        itemList->AppendElement(sublist);
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
    (const char * filename, nsVoidArray*& list, PlacementType placement = DUP_AFTER) {

  /* open input stream */
  nsFileSpec dirSpec;
  nsresult rv;
  rv = Wallet_ProfileDirectory(dirSpec);
  if (NS_FAILED(rv)) {
    /* if we failed to download the file, see if an initial version of it exists */
    rv = Wallet_ResourceDirectory(dirSpec);
    if (NS_FAILED(rv)) {
      return;
    }
  }
  nsInputFileStream strm(dirSpec + filename);
  if (!strm.is_open()) {
    /* file doesn't exist -- that's not an error */
    return;
  }

  /* make sure the list exists */
  if(!list) {
    list = new nsVoidArray();
    if(!list) {
      strm.close();
      return;
    }
  }

  for (;;) {

    nsAutoString item;
    if (NS_FAILED(wallet_GetLine(strm, item, PR_FALSE))) {
      /* end of file reached */
      break;
    }

    nsVoidArray * itemList = new nsVoidArray();
    if (!itemList) {
      break;
    }
    nsAutoString dummyString;
    wallet_WriteToList(item, dummyString, itemList, list, placement);

    for (;;) {
      nsAutoString item1;
      if (NS_FAILED(wallet_GetLine(strm, item1, PR_FALSE))) {
        /* end of file reached */
        break;
      }

      if (item1.Length()==0) {
        /* end of url reached */
        break;
      }

      nsAutoString item2;
      if (NS_FAILED(wallet_GetLine(strm, item2, PR_FALSE))) {
        /* unexpected end of file reached */
        break;
      }

      nsVoidArray* dummyList = NULL;
      wallet_WriteToList(item1, item2, dummyList, itemList, placement);

      nsAutoString item3;
      if (NS_FAILED(wallet_GetLine(strm, item3, PR_FALSE))) {
        /* end of file reached */
        strm.close();
        return;
      }

      if (item3.Length()!=0) {
        /* invalid file format */
        break;
      }
    }
  }
  strm.close();
}

/*********************************************************************/
/* The following are utility routines for the main wallet processing */
/*********************************************************************/
 
nsAutoString
wallet_GetHostFile(nsIURI * url) {
  nsAutoString urlName;
  char* host;
  nsresult rv = url->GetHost(&host);
  if (NS_FAILED(rv)) {
    return nsAutoString();
  }
  urlName.AppendWithConversion(host);
  nsCRT::free(host);
  char* file;
  rv = url->GetPath(&file);
  if (NS_FAILED(rv)) {
    return nsAutoString();
  }
  urlName.AppendWithConversion(file);
  nsCRT::free(file);
  return urlName;
}

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
        value.SetLength(0);
        nsAutoString value2;
        PRInt32 count = LIST_COUNT(itemList2);
        for (PRInt32 i=0; i<count; i++) {
          ptr1 = NS_STATIC_CAST(wallet_Sublist*, itemList2->ElementAt(i));
          if (wallet_ReadFromList(ptr1->item, value2, dummy, wallet_SchemaToValue_list)) {
            if (value.Length()>0) {
              value.AppendWithConversion(" ");
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

    nsAutoString temp = wallet_GetHostFile(wallet_lastUrl);
    temp.AppendWithConversion(":");
    temp.Append(field);

    if (wallet_ReadFromList(temp, value, itemList, wallet_SchemaToValue_list, index2)) {
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
  nsIDOMNSHTMLOptionCollection * options;
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
    if ((NS_SUCCEEDED(result)) && ((type.IsEmpty()) || (type.CompareWithConversion("text", PR_TRUE) == 0))) {
      nsAutoString field;
      result = inputElement->GetName(field);
      if (NS_SUCCEEDED(result)) {
        nsAutoString schema;
        nsAutoString value;
        nsVoidArray* itemList;

        /* get schema name from vcard attribute if it exists */
        nsIDOMElement * element;
        result = elementNode->QueryInterface(kIDOMElementIID, (void**)&element);
        if ((NS_SUCCEEDED(result)) && (nsnull != element)) {
          nsAutoString vcard; vcard.AssignWithConversion("VCARD_NAME");
          result = element->GetAttribute(vcard, schema);
          NS_RELEASE(element);
        }

        /*
         * if schema name was specified in vcard attribute then get value from schema name,
         * otherwise get value from field name by using mapping tables to get schema name
         */
        if (FieldToValue(field, schema, value, itemList, index) == 0) {
          if (value.IsEmpty() && nsnull != itemList) {
            /* pick first of a set of synonymous values */
            value = ((wallet_Sublist *)itemList->ElementAt(0))->item;
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
      nsAutoString schema;
      nsAutoString value;
      nsVoidArray* itemList;
      if (FieldToValue(field, schema, value, itemList, index) == 0) {
        if (!value.IsEmpty()) {
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
            value = ((wallet_Sublist *)itemList->ElementAt(i))->item;
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

PRBool
IsDigit (PRUnichar c) {
  return (c >= '0' && c <= '9');
}

nsresult
wallet_Patch(nsFileSpec dirSpec, nsAutoString& patch, nsInputFileStream patchFile) {
  nsAutoString buffer;
  nsAutoString filename;
  patch.Right(filename, patch.Length()-1);
  PRInt32 line;
  nsresult rv;
  nsInputFileStream oldFile(dirSpec + filename);
  nsOutputFileStream newFile(dirSpec + "tempfile");
  line = 0;

  /* get first patch */
  if (NS_FAILED(wallet_GetLine(patchFile, patch, PR_FALSE)) || patch.CharAt(0) == '@') {
    /* end of patch file */
    return NS_OK;
  }

  /* apply all patches */
  PRBool endOfPatchFile = PR_FALSE;
  for (;;) {

    /* parse the patch command */
    PRInt32 commandPosition = patch.FindCharInSet("acd");
    PRUnichar command = patch.CharAt(commandPosition);
    if (commandPosition == -1) {
      return NS_ERROR_FAILURE; /* invalid line in patch file */
    }

    /* parse the startline and endline from the patch */
    nsAutoString startString, endString;
    PRInt32 startLine, endLine, error;
    patch.Truncate(commandPosition);
    PRInt32 commaPosition = patch.FindChar(',');
    if (commaPosition == -1) {
      startLine = patch.ToInteger(&error);
      if (error != 0) {
        return NS_ERROR_FAILURE;
      }
      endLine = startLine;        
    } else {
      patch.Left(startString, commaPosition);
      startLine = startString.ToInteger(&error);
      if (error != 0) {
        return NS_ERROR_FAILURE;
      }
      patch.Right(endString, patch.Length()-commaPosition-1);
      endLine = endString.ToInteger(&error);        
      if (error != 0) {
        return NS_ERROR_FAILURE;
      }
    }

    /* copy all lines preceding the patch directly to the output file */
    for (; line < startLine; line++) {
      if (NS_FAILED(wallet_GetLine(oldFile, buffer, PR_FALSE))) {
        return NS_ERROR_FAILURE;
      }
      wallet_PutLine(newFile, buffer, PR_FALSE);
    }

    /* skip over changed or deleted lines in the old file */
    if (('c' == command) || ('d' == command)) { /* change or delete lines */
      for (; line <= endLine; line++) {
        rv = wallet_GetLine(oldFile, buffer, PR_FALSE);
        if (NS_FAILED(rv)) {
          return NS_ERROR_FAILURE;
        }
      }
    }

    /*
     * advance patch file to next patch command,
     * inserting new lines from patch file to new file as we go
     */
    for (;;) {
      rv = wallet_GetLine(patchFile, patch, PR_FALSE);
      if (NS_FAILED(rv) || patch.CharAt(0) == '@') {
        endOfPatchFile = PR_TRUE;
        break;
      }
      if (IsDigit(patch.CharAt(0))) {
        break; /* current patch command is finished */
      }
      if (('c' == command) || ('a' == command)) { /* change or add lines */
        /* insert new lines from the patch file into the new file */
        if (patch.CharAt(0) == '>') {
          nsAutoString newLine;
          patch.Right(newLine, patch.Length()-2);
          wallet_PutLine(newFile, newLine, PR_FALSE);
        }
      }
    }

    /* check for end of patch file */
    if (endOfPatchFile) {
      break;
    }
  }

  /* end of patch file reached */
  for (;;) {
    if (NS_FAILED(wallet_GetLine(oldFile, buffer, PR_FALSE))) {
      break;
    }
    wallet_PutLine(newFile, buffer, PR_FALSE);
  }
  oldFile.close();
  newFile.flush();
  newFile.close();
  nsFileSpec x(dirSpec + filename);
  x.Delete(PR_FALSE);
  nsFileSpec y(dirSpec + "tempfile");
  y.Rename(filename);
  return NS_OK;
}

void
wallet_FetchFromNetCenter() {
  nsresult rv;
  nsCAutoString url;

  /* obtain the server from which to fetch the patch files */
  SI_GetCharPref(pref_WalletServer, &wallet_Server);
  if (!wallet_Server || (*wallet_Server == '\0')) {
    /* user does not want to download mapping tables */
    return;
  }
  nsFileSpec dirSpec;
  rv = Wallet_ProfileDirectory(dirSpec);
  if (NS_FAILED(rv)) {
    return;
  }

  /* obtain version number */
  char * version = nsnull;
  SI_GetCharPref(pref_WalletVersion, &version);

  /* obtain fetch method */
  PRBool fetchPatches = PR_FALSE;
  SI_GetBoolPref(pref_WalletFetchPatches, fetchPatches);

  if (fetchPatches) {

    /* obtain composite patch file */
    url = wallet_Server;
    url.Append("patchfile.");
    url.Append(version);

    Recycle(version);
    rv = NS_NewURItoFile(url, dirSpec, "patchfile");
    if (NS_FAILED(rv)) {
      return;
    }

    /* update the version pref */

    nsInputFileStream patchFile(dirSpec + "patchfile");
    nsAutoString patch;
    if (NS_FAILED(wallet_GetLine(patchFile, patch, PR_FALSE))) {
      return;
    }
    patch.StripWhitespace();
    version = patch.ToNewCString();
    SI_SetCharPref(pref_WalletVersion, version);
    Recycle(version);

    /* process the patch file */

    if (NS_FAILED(wallet_GetLine(patchFile, patch, PR_FALSE))) {
      /* normal case -- there is nothing to update */
      return;
    }
    PRBool error = PR_FALSE;
    while (patch.Length() > 0) {
      rv = wallet_Patch(dirSpec, patch, patchFile);
      if (NS_FAILED(rv)) {
        error = PR_TRUE;
        break;
      }
    }

    /* make sure all tables exist and no error occured */
    if (!error) {
#ifdef AutoCapture
      nsInputFileStream table0(dirSpec + distinguishedSchemaFileName);
#endif
      nsInputFileStream table1(dirSpec + schemaConcatFileName);
      nsInputFileStream table2(dirSpec + fieldSchemaFileName);
      nsInputFileStream table3(dirSpec + URLFieldSchemaFileName);
#ifdef AutoCapture
      if (table0.is_open() && table1.is_open() && table2.is_open() && table3.is_open()) {
#else
      if (table1.is_open() && table2.is_open() && table3.is_open()) {
#endif
        return;
      }
    }
  }

  /* failed to do the patching, get files manually */

#ifdef fetchingIndividualFiles
  url = nsCAutoString(wallet_Server) + URLFieldSchemaFileName;
  rv = NS_NewURItoFile(url, dirSpec, URLFieldSchemaFileName);
  if (NS_FAILED(rv)) {
    return;
  }
  url = nsCAutoString(wallet_Server) + schemaConcatFileName;
  rv = NS_NewURItoFile(url, dirSpec, schemaConcatFileName);
  if (NS_FAILED(rv)) {
    return;
  }
  url = nsCAutoString(wallet_Server) + fieldSchemaFileName;
  rv = NS_NewURItoFile(url, dirSpec, fieldSchemaFileName);
  if (NS_FAILED(rv)) {
    return;
  }
  url = nsCAutoString(wallet_Server) + distinguishedSchemaFileName;
  rv = NS_NewURItoFile(url, dirSpec, distinguishedSchemaFileName);
  if (NS_FAILED(rv)) {
    return;
  }
#else
  /* first fetch the composite of all files and put it into a local composite file */
  url = nsCAutoString(wallet_Server); url.Append(allFileName);
  rv = NS_NewURItoFile(url, dirSpec, allFileName);

  /* fetch version number from first line of local composite file */
  nsInputFileStream allFile(dirSpec + allFileName);
  nsAutoString buffer;
  if (NS_FAILED(wallet_GetLine(allFile, buffer, PR_FALSE))) {
    return;
  }
  buffer.StripWhitespace();
  if (buffer.EqualsWithConversion(version)) {
    /* This is an optimization but we are skipping it for now.  If the user's tables
     * become corrupt but his version number indicates that he is up to date, there
     * would be no obvious way for him to restore the tables.  If we did the optimization
     * we would save only about 150 milliseconds at wallet startup.
     */
//  return; /* version hasn't changed so stop now */
  }
  version = buffer.ToNewCString();
  SI_SetCharPref(pref_WalletVersion, version);
  Recycle(version);

  /* get next line of local composite file.
   *  This is name of first subfile in the composite file
   */
  if (NS_FAILED(wallet_GetLine(allFile, buffer, PR_FALSE))) {
    return;
  }

  /* process each subfile in the composite file */
  for (;;) {

    /* obtain subfile name and open it as an output stream */
    if (buffer.CharAt(0) != '@') {
      return; /* error */
    }
    buffer.StripWhitespace();
    nsAutoString filename;
    buffer.Right(filename, buffer.Length()-1);
    nsOutputFileStream thisFile(dirSpec + filename);

    /* copy each line in composite file to the subfile */
    for (;;) {
      if (NS_FAILED(wallet_GetLine(allFile, buffer, PR_FALSE))) {
        /* end of composite file reached */
        return;
      }
      if (buffer.CharAt(0) == '@') {
        /* start of next subfile reached */
        break;
      }
      wallet_PutLine(thisFile, buffer, PR_FALSE);
    }
  }
#endif
}

/*
 * initialization for wallet session (done only once)
 */
void
wallet_Initialize(PRBool fetchTables, PRBool unlockDatabase=PR_TRUE) {
  static PRBool wallet_tablesInitialized = PR_FALSE;
  static PRBool wallet_tablesFetched = PR_FALSE;
  static PRBool wallet_keyInitialized = PR_FALSE;

  /* initialize tables 
   * Note that we don't initialize the tables if this call was made from the wallet
   * editor.  The tables are certainly not needed since all we are doing is displaying
   * the contents of the user wallet.  Furthermore, there is a problem which causes the
   * window to come up blank in that case.  Has something to do with the fact that we
   * were being called from javascript in this case.  So to avoid the problem, the 
   * fetchTables parameter was added and it is set to PR_FALSE in the case of the
   * wallet editor and PR_TRUE in all other cases
   *
   * Similar problem applies to changing password.  Don't need tables in that case and
   * fetching them was causing a hang -- see bug 28148 and bug 28145.
   */
#ifdef DEBUG
//wallet_ClearStopwatch();
//wallet_ResumeStopwatch();
#endif
  if (fetchTables && !wallet_tablesFetched) {
    wallet_FetchFromNetCenter();
    wallet_tablesFetched = PR_TRUE;
  }
  if (!wallet_tablesInitialized) {
#ifdef DEBUG
//wallet_PauseStopwatch();
//wallet_DumpStopwatch();
#endif
    wallet_Clear(&wallet_FieldToSchema_list); /* otherwise we will duplicate the list */
    wallet_Clear(&wallet_URLFieldToSchema_list); /* otherwise we will duplicate the list */
    wallet_Clear(&wallet_SchemaConcat_list); /* otherwise we will duplicate the list */
#ifdef AutoCapture
    wallet_Clear(&wallet_DistinguishedSchema_list); /* otherwise we will duplicate the list */
    wallet_ReadFromFile(distinguishedSchemaFileName, wallet_DistinguishedSchema_list, PR_FALSE, PR_FALSE);
#endif
    wallet_ReadFromFile(fieldSchemaFileName, wallet_FieldToSchema_list, PR_FALSE, PR_FALSE);
    wallet_ReadFromURLFieldToSchemaFile(URLFieldSchemaFileName, wallet_URLFieldToSchema_list);
    wallet_ReadFromFile(schemaConcatFileName, wallet_SchemaConcat_list, PR_FALSE, PR_FALSE);
    wallet_tablesInitialized = PR_TRUE;
  }

  if (!unlockDatabase) {
    return;
  }

  /* see if key has timed out */
  if (Wallet_KeyTimedOut()) {
    wallet_keyInitialized = PR_FALSE;
  }

  if (!wallet_keyInitialized) {
    PRUnichar * message = Wallet_Localize("IncorrectKey_TryAgain?");
    while (!Wallet_SetKey(PR_FALSE)) {
      if (Wallet_CancelKey() || (Wallet_KeySize() < 0) || !Wallet_Confirm(message)) {
        Recycle(message);
        return;
      }
    }
    Recycle(message);
    wallet_Clear(&wallet_SchemaToValue_list); /* otherwise we will duplicate the list */
    wallet_ReadFromFile(schemaValueFileName, wallet_SchemaToValue_list, PR_TRUE, PR_TRUE);
    wallet_keyInitialized = PR_TRUE;
  }

  /* restart key timeout period */
  Wallet_KeyResetTime();

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

  if (Wallet_KeySize() < 0) {

    /* have user create database key if one was never created */
    PRUnichar * message = Wallet_Localize("IncorrectKey_TryAgain?");
    while (!Wallet_SetKey(PR_FALSE)) {
      if (Wallet_CancelKey() || (Wallet_KeySize() < 0) || !Wallet_Confirm(message)) {
        Recycle(message);
        return;
      }
    }
    Recycle(message);
    return;
  }

  /* force the user to supply old database key, for security */
  WLLT_ExpirePassword();

  /* read in user data using old key */
  wallet_Initialize(PR_FALSE);
  if (!Wallet_IsKeySet()) {
    return;
  }
#ifdef SingleSignon
  SI_LoadSignonData(PR_TRUE);
#endif

  /* establish new key */
  Wallet_SetKey(PR_TRUE);

  /* write out user data using new key */
  wallet_WriteToFile(schemaValueFileName, wallet_SchemaToValue_list, PR_TRUE);
#ifdef SingleSignon
  SI_SaveSignonData();
#endif
}

void
wallet_InitializeURLList() {
  static PRBool wallet_URLListInitialized = PR_FALSE;
  if (!wallet_URLListInitialized) {
    wallet_ReadFromFile(URLFileName, wallet_URL_list, PR_FALSE, PR_TRUE);
    wallet_URLListInitialized = PR_TRUE;
  }
}

/*
 * initialization for current URL
 */
void
wallet_InitializeCurrentURL(nsIDocument * doc) {

  /* get url */
  nsIURI* url;
  url = doc->GetDocumentURL();
  if (wallet_lastUrl == url) {
    NS_RELEASE(url);
    return;
  } else {
    if (wallet_lastUrl) {
//??      NS_RELEASE(lastUrl);
    }
    wallet_lastUrl = url;
  }

  /* get host+file */
  nsAutoString urlName = wallet_GetHostFile(url);
  NS_RELEASE(url);
  if (urlName.Length() == 0) {
    return;
  }

  /* get field/schema mapping specific to current url */
  wallet_MapElement * ptr;
  PRInt32 count = LIST_COUNT(wallet_URLFieldToSchema_list);
  for (PRInt32 i=0; i<count; i++) {
    ptr = NS_STATIC_CAST(wallet_MapElement*, wallet_URLFieldToSchema_list->ElementAt(i));
    if (ptr->item1 == urlName) {
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

PRInt32
wallet_GetNextInString(nsAutoString str, nsAutoString& head, nsAutoString& tail) {
  PRInt32 separator = str.Find(SEPARATOR);
  if (separator == -1) {
    return -1;
  }
  str.Left(head, separator);
  str.Mid(tail, separator+PL_strlen(SEPARATOR), str.Length() - (separator+PL_strlen(SEPARATOR)));
  return 0;
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
nsAutoString wallet_url;

PUBLIC void
WLLT_GetPrefillListForViewer(nsAutoString& aPrefillList)
{
  wallet_PrefillElement * ptr;
  nsAutoString buffer;
  PRUnichar * schema;
  PRUnichar * value;
  PRInt32 count = LIST_COUNT(wallet_list);
  for (PRInt32 i=0; i<count; i++) {
    ptr = NS_STATIC_CAST(wallet_PrefillElement*, wallet_list->ElementAt(i));
    schema = ptr->schema->ToNewUnicode();
    value = ptr->value->ToNewUnicode();
    buffer.AppendWithConversion(BREAK);
    buffer.AppendInt(ptr->count,10);
    buffer.AppendWithConversion(BREAK);
    buffer += schema;
    buffer.AppendWithConversion(BREAK);
    buffer += value;
    Recycle(schema);
    Recycle(value);
  }

  PRUnichar * urlUnichar = wallet_url.ToNewUnicode();
  buffer.AppendWithConversion(BREAK);
  buffer.AppendInt(PRInt32(wallet_list));
  buffer.AppendWithConversion(BREAK);
  buffer += urlUnichar;
  Recycle(urlUnichar);

  aPrefillList = buffer;
}

extern PRBool
SI_InSequence(nsAutoString sequence, int number);

extern PRUnichar*
SI_FindValueInArgs(nsAutoString results, nsAutoString name);

PRIVATE void
wallet_FreeURL(wallet_MapElement *url) {

    if(!url) {
        return;
    }
    wallet_URL_list->RemoveElement(url);
    PR_Free(url);
}

PUBLIC void
Wallet_SignonViewerReturn (nsAutoString results) {
    wallet_MapElement *url;
    nsAutoString gone;

    /* step through all nopreviews and delete those that are in the sequence */
    {
      nsAutoString temp1; temp1.AssignWithConversion("|goneP|");
      gone = SI_FindValueInArgs(results, temp1);
    }
    PRInt32 count = LIST_COUNT(wallet_URL_list);
    while (count>0) {
      count--;
      url = NS_STATIC_CAST(wallet_MapElement*, wallet_URL_list->ElementAt(count));
      if (url && SI_InSequence(gone, count)) {
        url->item2.SetCharAt('n', NO_PREVIEW);
        if (url->item2.CharAt(NO_CAPTURE) == 'n') {
          wallet_FreeURL(url);
          wallet_WriteToFile(URLFileName, wallet_URL_list, PR_FALSE);
        }
      }
    }

    /* step through all nocaptures and delete those that are in the sequence */
    {
      nsAutoString temp2; temp2.AssignWithConversion("|goneC|");
      gone = SI_FindValueInArgs(results, temp2);
    }
    PRInt32 count2 = LIST_COUNT(wallet_URL_list);
    while (count2>0) {
      count2--;
      url = NS_STATIC_CAST(wallet_MapElement*, wallet_URL_list->ElementAt(count2));
      if (url && SI_InSequence(gone, count)) {
        url->item2.SetCharAt('n', NO_CAPTURE);
        if (url->item2.CharAt(NO_PREVIEW) == 'n') {
          wallet_FreeURL(url);
          wallet_WriteToFile(URLFileName, wallet_URL_list, PR_FALSE);
        }
      }
    }
}

#ifdef AutoCapture
/*
 * see if user wants to capture data on current page
 */
PRIVATE PRBool
wallet_OKToCapture(char* urlName) {
  nsAutoString url; url.AssignWithConversion(urlName);

  /* exit if pref is not set */
  if (!wallet_GetFormsCapturingPref() || !wallet_GetEnabledPref()) {
    return PR_FALSE;
  }

  /* see if this url is already on list of url's for which we don't want to capture */
  wallet_InitializeURLList();
  nsVoidArray* dummy;
  nsAutoString value; value.AssignWithConversion("nn");
  if (wallet_ReadFromList(url, value, dummy, wallet_URL_list)) {
    if (value.CharAt(NO_CAPTURE) == 'y') {
      return PR_FALSE;
    }
  }

  /* ask user if we should capture the values on this form */
  PRUnichar * message = Wallet_Localize("WantToCaptureForm?");

  PRInt32 button = Wallet_3ButtonConfirm(message);
  /* button 1 = YES, 0 = NO, -1 = NEVER */
  if (button == -1) { /* NEVER button was pressed */
    /* add URL to list with NO_CAPTURE indicator set */
    value.SetCharAt('y', NO_CAPTURE);
    wallet_WriteToList(url, value, dummy, wallet_URL_list, DUP_OVERWRITE);
    wallet_WriteToFile(URLFileName, wallet_URL_list, PR_FALSE);
  }
  Recycle(message);
  return (button == 1);
}
#endif

/*
 * capture the value of a form element
 */
PRIVATE void
wallet_Capture(nsIDocument* doc, nsAutoString field, nsAutoString value, nsAutoString vcard) {

  /* do nothing if there is no value */
  if (!value.Length()) {
    return;
  }

  /* read in the mappings if they are not already present */
  if (!vcard.Length()) {
    wallet_Initialize(PR_TRUE);
    wallet_InitializeCurrentURL(doc);
    if (!Wallet_IsKeySet()) {
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
          mapElement->item1,
          mapElement->item2,
          mapElement->itemList, 
          wallet_SchemaToValue_list);
        delete mapElement;
        return;
      }
      lastIndex = index;
    }

    /* this is a new value so store it */
    dummy = 0;
    wallet_WriteToList(schema, value, dummy, wallet_SchemaToValue_list);
    wallet_WriteToFile(schemaValueFileName, wallet_SchemaToValue_list, PR_TRUE);

  } else {

    /* no field to schema mapping so assume schema name is same as field name */

    /* is this a new value for the schema */
    PRInt32 index = 0;
    PRInt32 lastIndex = index;

    nsAutoString concat_param = wallet_GetHostFile(wallet_lastUrl);
    concat_param.AppendWithConversion(":");
    concat_param.Append(field);

    while(wallet_ReadFromList(concat_param, oldValue, dummy, wallet_SchemaToValue_list, index)) {
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
          mapElement->item1,
          mapElement->item2,
          mapElement->itemList, 
          wallet_SchemaToValue_list);
        delete mapElement;
        return;
      }
      lastIndex = index;

      concat_param = wallet_GetHostFile(wallet_lastUrl);
      concat_param.AppendWithConversion(":");
      concat_param.Append(field);
    }

    /* this is a new value so store it */
    dummy = 0;
    nsAutoString hostFileField = wallet_GetHostFile(wallet_lastUrl);
    hostFileField.AppendWithConversion(":");
    hostFileField.Append(field);

    wallet_WriteToList(hostFileField, value, dummy, wallet_SchemaToValue_list);
    wallet_WriteToFile(schemaValueFileName, wallet_SchemaToValue_list, PR_TRUE);
  }
}

/***************************************************************/
/* The following are the interface routines seen by other dlls */
/***************************************************************/

#define BUFLEN2 5000
#define BREAK '\001'

PUBLIC void
WLLT_GetNopreviewListForViewer(nsAutoString& aNopreviewList)
{
  nsAutoString buffer;
  int nopreviewNum = 0;
  wallet_MapElement *url;

  wallet_InitializeURLList();
  PRInt32 count = LIST_COUNT(wallet_URL_list);
  for (PRInt32 i=0; i<count; i++) {
    url = NS_STATIC_CAST(wallet_MapElement*, wallet_URL_list->ElementAt(i));
    if (url->item2.CharAt(NO_PREVIEW) == 'y') {
      buffer.AppendWithConversion(BREAK);
      buffer.AppendWithConversion("<OPTION value=");
      buffer.AppendInt(nopreviewNum, 10);
      buffer.AppendWithConversion(">");
      buffer += url->item1;
      buffer.AppendWithConversion("</OPTION>\n");
      nopreviewNum++;
    }
  }
  aNopreviewList = buffer;
}

PUBLIC void
WLLT_GetNocaptureListForViewer(nsAutoString& aNocaptureList)
{
  nsAutoString buffer;
  int nocaptureNum = 0;
  wallet_MapElement *url;

  wallet_InitializeURLList();
  PRInt32 count = LIST_COUNT(wallet_URL_list);
  for (PRInt32 i=0; i<count; i++) {
    url = NS_STATIC_CAST(wallet_MapElement*, wallet_URL_list->ElementAt(i));
    if (url->item2.CharAt(NO_CAPTURE) == 'y') {
      buffer.AppendWithConversion(BREAK);
      buffer.AppendWithConversion("<OPTION value=");
      buffer.AppendInt(nocaptureNum, 10);
      buffer.AppendWithConversion(">");
      buffer += url->item1;
      buffer.AppendWithConversion("</OPTION>\n");
      nocaptureNum++;
    }
  }
  aNocaptureList = buffer;
}

PUBLIC void
WLLT_PostEdit(nsAutoString walletList) {
  if (!Wallet_IsKeySet()) {
    return;
  }

  nsFileSpec dirSpec;
  nsresult rv = Wallet_ProfileDirectory(dirSpec);
  if (NS_FAILED(rv)) {
    return;
  }

  nsAutoString tail = walletList;
  nsAutoString head, temp;
  PRInt32 separator;

  /* get first item in list */
  separator = tail.FindChar(BREAK);
  if (-1 == separator) {
    return;
  }
  tail.Left(head, separator);
  tail.Mid(temp, separator+1, tail.Length() - (separator+1));
  tail = temp;

  /* return if OK button was not pressed */
  if (!head.EqualsWithConversion("OK")) {
    return;
  }

  /* open SchemaValue file */
  nsOutputFileStream strm(dirSpec + schemaValueFileName);
  if (!strm.is_open()) {
    NS_ERROR("unable to open file");
    return;
  }
  saveCountW += 16; /* preserve low order four bits which designate the file type */
  nsKeyType writeCount = 0;

  /* write the values in the walletList to the file */
  wallet_PutHeader(strm, saveCountW, writeCount);
  for (;;) {
    separator = tail.FindChar(BREAK);
    if (-1 == separator) {
      break;
    }
    tail.Left(head, separator);
    tail.Mid(temp, separator+1, tail.Length() - (separator+1));
    tail = temp;

    wallet_PutLine(strm, head, PR_TRUE, saveCountW, &writeCount);
  }

  /* close the file and read it back into the SchemaToValue list */
  strm.close();
  wallet_Clear(&wallet_SchemaToValue_list);
  wallet_ReadFromFile(schemaValueFileName, wallet_SchemaToValue_list, PR_TRUE, PR_TRUE);
}

PUBLIC void
WLLT_PreEdit(nsAutoString& walletList) {
  wallet_Initialize(PR_FALSE);
  if (!Wallet_IsKeySet()) {
    return;
  }
  walletList = BREAK;
  wallet_MapElement * ptr;
  PRInt32 count = LIST_COUNT(wallet_SchemaToValue_list);
  for (PRInt32 i=0; i<count; i++) {
    ptr = NS_STATIC_CAST(wallet_MapElement*, wallet_SchemaToValue_list->ElementAt(i));

    walletList += ptr->item1; walletList.AppendWithConversion(BREAK);
    if (!ptr->item2.IsEmpty()) {
      walletList += ptr->item2; walletList.AppendWithConversion(BREAK);
    } else {
      wallet_Sublist * ptr1;
      PRInt32 count2 = LIST_COUNT(ptr->itemList);
      for (PRInt32 i2=0; i2<count2; i2++) {
        ptr1 = NS_STATIC_CAST(wallet_Sublist*, ptr->itemList->ElementAt(i2));
        walletList += ptr1->item; walletList.AppendWithConversion(BREAK);

      }
    }
    walletList.AppendWithConversion(BREAK);
  }
}

/*
 * return after previewing a set of prefills
 */
PUBLIC void
WLLT_PrefillReturn(nsAutoString results) {
  PRUnichar* listAsAscii;
  PRUnichar* fillins;
  PRUnichar* urlName;
  PRUnichar* skip;
  nsAutoString next;

  /* get values that are in environment variables */
  fillins = SI_FindValueInArgs(results, NS_ConvertToString("|fillins|"));
  listAsAscii = SI_FindValueInArgs(results, NS_ConvertToString("|list|"));
  skip = SI_FindValueInArgs(results, NS_ConvertToString("|skip|"));
  urlName = SI_FindValueInArgs(results, NS_ConvertToString("|url|"));

  /* add url to url list if user doesn't want to preview this page in the future */
  if (nsAutoString(skip).EqualsWithConversion("true")) {
    nsAutoString url = nsAutoString(urlName);
    nsVoidArray* dummy;
    nsAutoString value; value.AssignWithConversion("nn");
    wallet_ReadFromList(url, value, dummy, wallet_URL_list);
    value.SetCharAt('y', NO_PREVIEW);
    wallet_WriteToList(url, value, dummy, wallet_URL_list, DUP_OVERWRITE);
    wallet_WriteToFile(URLFileName, wallet_URL_list, PR_FALSE);
  }

  /* process the list, doing the fillins */
  nsVoidArray * list;
  PRInt32 error;
  list = (nsVoidArray *)nsAutoString(listAsAscii).ToInteger(&error);
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
  nsAutoString ptr2 = nsAutoString(fillins);
  /* step through pre-fill list */
  PRInt32 count = LIST_COUNT(list);
  for (PRInt32 i=0; i<count; i++) {
    ptr = NS_STATIC_CAST(wallet_PrefillElement*, list->ElementAt(i));

    /* advance in fillins list each time a new schema name in pre-fill list is encountered */
    if (ptr->count != 0) {
      /* count != 0 indicates a new schema name */
      nsAutoString tail;
      if (wallet_GetNextInString(ptr2, next, tail) == -1) {
        break;
      }
      ptr2 = tail;
      if (next != *ptr->schema) {
        break; /* something's wrong so stop prefilling */
      }
      wallet_GetNextInString(ptr2, next, tail);
      ptr2 = tail;
    }
    if (next == *ptr->value) {
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
              mapElement->item1,
              mapElement->item2,
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
     if ((next == *ptr->value) || ((ptr->count>0) && next.IsEmpty())) {
       if (((next == *ptr->value) || next.IsEmpty()) && ptr->inputElement) {
         ptr->inputElement->SetValue(next);
       } else {
         nsresult result;
         result = wallet_GetSelectIndex(ptr->selectElement, next, ptr->selectIndex);
         if (NS_SUCCEEDED(result)) {
           ptr->selectElement->SetSelectedIndex(ptr->selectIndex);
         } else {
           ptr->selectElement->SetSelectedIndex(0);
        }
      }
    }
  }

  /* Release the prefill list that was generated when we walked thru the html content */
  wallet_ReleasePrefillElementList(list);
}

/*
 * get the form elements on the current page and prefill them if possible
 */
PUBLIC nsresult
WLLT_Prefill(nsIPresShell* shell, PRBool quick) {
  nsAutoString urlName;

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
      nsIURI* url;
      url = doc->GetDocumentURL();
      if (url) {
        urlName = wallet_GetHostFile(url);
        NS_RELEASE(url);
      }
      wallet_Initialize(PR_TRUE);
      if (!Wallet_IsKeySet()) {
        NS_RELEASE(doc);
        return NS_ERROR_FAILURE;
      }
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
  }

  /* return if no elements were put into the list */
  if (LIST_COUNT(wallet_PrefillElement_list) == 0) {
    if (Wallet_IsKeySet()) {
      PRUnichar * message = Wallet_Localize("noPrefills");
      Wallet_Alert(message);
      Recycle(message);
    }
    return NS_ERROR_FAILURE; // indicates to caller not to display preview screen
  }

  /* prefill each element using the list */

  /* determine if url is on list of urls that should not be previewed */
  PRBool noPreview = PR_FALSE;
  if (!quick) {
    wallet_InitializeURLList();
    nsVoidArray* dummy;
    nsAutoString value; value.AssignWithConversion("nn");
    if (urlName.Length() != 0) {
      wallet_ReadFromList(urlName, value, dummy, wallet_URL_list);
      noPreview = (value.CharAt(NO_PREVIEW) == 'y');
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
    wallet_url = urlName;
#ifdef DEBUG
wallet_DumpStopwatch();
wallet_ClearStopwatch();
//wallet_DumpTiming();
//wallet_ClearTiming();
#endif
    return NS_OK; // indicates that caller is to display preview screen
  }
}

extern void
SINGSIGN_RememberSignonData (char* URLName, nsVoidArray * signonData);

PUBLIC void
WLLT_RequestToCapture(nsIPresShell* shell) {
  /* starting with the present shell, get each form element and put them on a list */
  nsresult result;
  if (nsnull != shell) {
    nsIDocument* doc = nsnull;
    result = shell->GetDocument(&doc);
    if (NS_SUCCEEDED(result)) {
      wallet_Initialize(PR_TRUE);
      if (!Wallet_IsKeySet()) {
        NS_RELEASE(doc);
        return;
      }
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
                  /* now find out how many text fields are on the form */
                  PRUint32 numElements;
                  elements->GetLength(&numElements);
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
                            (type.IsEmpty() || (type.CompareWithConversion("text", PR_TRUE) == 0))) {
                          nsAutoString field;
                          result = inputElement->GetName(field);
                          if (NS_SUCCEEDED(result)) {
                            nsAutoString value;
                            result = inputElement->GetValue(value);
                            if (NS_SUCCEEDED(result)) {

                              /* get schema name from vcard attribute if it exists */
                              nsAutoString vcardValue;
                              nsIDOMElement * element;
                              result = elementNode->QueryInterface(kIDOMElementIID, (void**)&element);
                              if ((NS_SUCCEEDED(result)) && (nsnull != element)) {
                                nsAutoString vcardName; vcardName.AssignWithConversion("VCARD_NAME");
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
  }
}

/* should move this to an include file */
class si_SignonDataStruct {
public:
  si_SignonDataStruct() : isPassword(PR_FALSE) {}
  nsAutoString name;
  nsAutoString value;
  PRBool isPassword;
};

PUBLIC void
WLLT_OnSubmit(nsIContent* currentForm) {

  nsCOMPtr<nsIDOMHTMLFormElement> currentFormNode(do_QueryInterface(currentForm));

  /* get url name as ascii string */
  char *URLName = nsnull;
  nsCOMPtr<nsIURI> docURL;
  nsCOMPtr<nsIDocument> doc;
  currentForm->GetDocument(*getter_AddRefs(doc));
  if (!doc) {
    return;
  }
  docURL = dont_AddRef(doc->GetDocumentURL());
  if (!docURL) {
    return;
  }
  (void)docURL->GetSpec(&URLName);

  /* get to the form elements */
  nsCOMPtr<nsIDOMHTMLDocument> htmldoc(do_QueryInterface(doc));
  if (htmldoc == nsnull) {
    nsCRT::free(URLName);
    return;
  }

  nsCOMPtr<nsIDOMHTMLCollection> forms;
  nsresult rv = htmldoc->GetForms(getter_AddRefs(forms));
  if (NS_FAILED(rv) || (forms == nsnull)) {
    nsCRT::free(URLName);
    return;
  }

  PRUint32 numForms;
  forms->GetLength(&numForms);
  for (PRUint32 formX = 0; formX < numForms; formX++) {
    nsCOMPtr<nsIDOMNode> formNode;
    forms->Item(formX, getter_AddRefs(formNode));
    if (nsnull != formNode) {
#ifndef AutoCapture
      PRInt32 fieldcount = 0;
#endif
      nsCOMPtr<nsIDOMHTMLFormElement> formElement(do_QueryInterface(formNode));
      if ((nsnull != formElement)) {
        nsCOMPtr<nsIDOMHTMLCollection> elements;
        rv = formElement->GetElements(getter_AddRefs(elements));
        if ((NS_SUCCEEDED(rv)) && (nsnull != elements)) {
          /* got to the form elements at long last */ 
          nsVoidArray * signonData = new nsVoidArray();
          si_SignonDataStruct * data;
          PRUint32 numElements;
          elements->GetLength(&numElements);
#ifdef AutoCapture
          PRBool OKToPrompt = PR_FALSE;
#endif
          for (PRUint32 elementX = 0; elementX < numElements; elementX++) {
            nsCOMPtr<nsIDOMNode> elementNode;
            elements->Item(elementX, getter_AddRefs(elementNode));
            if (nsnull != elementNode) {
              nsCOMPtr<nsIDOMHTMLInputElement> inputElement(do_QueryInterface(elementNode));
              if ((NS_SUCCEEDED(rv)) && (nsnull != inputElement)) {
                nsAutoString type;
                rv = inputElement->GetType(type);
                if (NS_SUCCEEDED(rv)) {

                  PRBool isText = (type.IsEmpty() || (type.CompareWithConversion("text", PR_TRUE)==0));
                  PRBool isPassword = (type.CompareWithConversion("password", PR_TRUE)==0);
#ifndef AutoCapture
                  if (isText) {
                    fieldcount++;
                  }
#endif
                  if (isText || isPassword) {
                    nsAutoString value;
                    rv = inputElement->GetValue(value);
                    if (NS_SUCCEEDED(rv)) {
                      nsAutoString field;
                      rv = inputElement->GetName(field);
                      if (NS_SUCCEEDED(rv)) {
                        data = new si_SignonDataStruct;
                        data->value = value;
                        if (field.CharAt(0) == '\\') {
                          /*
                           * Note that data saved for browser-generated logins (e.g. http
                           * authentication) use artificial field names starting with
                           * \= (see USERNAMEFIELD and PASSWORDFIELD in singsign.cpp).  To
                           * avoid mistakes whereby saved logins for http authentication is
                           * then prefilled into a field on the html form at the same URL,
                           * we will prevent html field names from starting with \=.  We
                           * do that by doubling up a backslash if it appears in the first
                           * character position
                           */
                          data->name = nsAutoString('\\');
                          data->name.Append(field);
                          
                        } else {
                          data->name = field;
                        }
                        data->isPassword = isPassword;
                        signonData->AppendElement(data);
#ifdef AutoCapture
                        /* get schema from field */
                        wallet_Initialize(PR_FALSE, PR_FALSE);
                        wallet_InitializeCurrentURL(doc);
                        nsAutoString schema;
                        nsVoidArray* dummy;
                        if (schema.Length() ||
                            (wallet_ReadFromList(field, schema, dummy, wallet_specificURLFieldToSchema_list)) ||
                            (wallet_ReadFromList(field, schema, dummy, wallet_FieldToSchema_list))) {
                        }
                        /* see if schema is in distinguished list */
                        schema.ToLowerCase();
                        wallet_MapElement * ptr;
                        PRInt32 count = LIST_COUNT(wallet_DistinguishedSchema_list);
                        for (PRInt32 i=0; i<count; i++) {
                          ptr = NS_STATIC_CAST
                            (wallet_MapElement*, wallet_DistinguishedSchema_list->ElementAt(i));
                          if (ptr->item1 == schema && value.Length() > 0) {
                            OKToPrompt = PR_TRUE;
                            break;
                          }
                        }
#endif
                      }
                    }
                  }
                }
              }
            }
          }

          /* save login if appropriate */
          if (currentFormNode == formNode) {
            SINGSIGN_RememberSignonData (URLName, signonData);
          }
          PRInt32 count2 = signonData->Count();
          for (PRInt32 i=count2-1; i>=0; i--) {
            data = NS_STATIC_CAST(si_SignonDataStruct*, signonData->ElementAt(i));
            delete data;
          }
          delete signonData;

#ifndef AutoCapture
          /* give notification if this is first significant form submitted */
          if ((fieldcount>=3) && !wallet_GetWalletNotificationPref()) {

            /* conditions all met, now give notification */
            PRUnichar * notification = Wallet_Localize("WalletNotification");
            wallet_SetWalletNotificationPref(PR_TRUE);
            Wallet_Alert(notification);
            Recycle(notification);
          }
#else
          /* save form if it meets all necessary conditions */
          if (wallet_GetFormsCapturingPref() && (OKToPrompt) && wallet_OKToCapture(URLName)) {

            /* conditions all met, now save it */
            for (PRUint32 elementY = 0; elementY < numElements; elementY++) {
              nsIDOMNode* elementNode = nsnull;
              elements->Item(elementY, &elementNode);
              if (nsnull != elementNode) {
                nsIDOMHTMLInputElement* inputElement;  
                rv =
                  elementNode->QueryInterface(kIDOMHTMLInputElementIID, (void**)&inputElement);
                if ((NS_SUCCEEDED(rv)) && (nsnull != inputElement)) {

                  /* it's an input element */
                  nsAutoString type;
                  rv = inputElement->GetType(type);
                  if ((NS_SUCCEEDED(rv)) &&
                      (type.IsEmpty() || (type.CompareWithConversion("text", PR_TRUE) == 0))) {
                    nsAutoString field;
                    rv = inputElement->GetName(field);
                    if (NS_SUCCEEDED(rv)) {
                      nsAutoString value;
                      rv = inputElement->GetValue(value);
                      if (NS_SUCCEEDED(rv)) {

                        /* get schema name from vcard attribute if it exists */
                        nsAutoString vcardValue;
                        nsIDOMElement * element;
                        rv = elementNode->QueryInterface(kIDOMElementIID, (void**)&element);
                        if ((NS_SUCCEEDED(rv)) && (nsnull != element)) {
                          nsAutoString vcardName; vcardName.AssignWithConversion("VCARD_NAME");
                          rv = element->GetAttribute(vcardName, vcardValue);
                          NS_RELEASE(element);
                        }
                        wallet_Capture(doc, field, value, vcardValue);
                      }
                    }
                  }
                }
              }
            }
          }
#endif
        }
      }
    }
  }
  nsCRT::free(URLName);
}

PUBLIC void
WLLT_FetchFromNetCenter() {
//  wallet_FetchFromNetCenter();
}

