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
#include "singsign.h"

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

#include "nsIWalletService.h"

#ifdef DEBUG_morse
#define morseAssert NS_ASSERTION
#else
#define morseAssert(x,y) 0
#endif 

static NS_DEFINE_IID(kIDOMHTMLDocumentIID, NS_IDOMHTMLDOCUMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLFormElementIID, NS_IDOMHTMLFORMELEMENT_IID);
static NS_DEFINE_IID(kIDOMElementIID, NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLInputElementIID, NS_IDOMHTMLINPUTELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLSelectElementIID, NS_IDOMHTMLSELECTELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLOptionElementIID, NS_IDOMHTMLOPTIONELEMENT_IID);

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

static NS_DEFINE_CID(kNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);
static NS_DEFINE_CID(kProfileCID, NS_PROFILE_CID);

static NS_DEFINE_IID(kIStringBundleServiceIID, NS_ISTRINGBUNDLESERVICE_IID);
static NS_DEFINE_IID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

static NS_DEFINE_IID(kIFileLocatorIID, NS_IFILELOCATOR_IID);
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);

#include "prlong.h"
#include "prinrval.h"

#include "prlog.h"
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=nsWallet:5
//    set NSPR_LOG_FILE=nspr.log
//
PRLogModuleInfo* gWalletLog = nsnull;


/********************************************************/
/* The following data and procedures are for preference */
/********************************************************/

static const char *pref_WalletExtractTables = "wallet.extractTables";
static const char *pref_Caveat = "wallet.caveat";
#ifdef AutoCapture
static const char *pref_captureForms = "wallet.captureForms";
static const char *pref_enabled = "wallet.enabled";
#else
static const char *pref_WalletNotified = "wallet.Notified";
#endif /* AutoCapture */
static const char *pref_WalletSchemaValueFileName = "wallet.SchemaValueFileName";
static const char *pref_WalletServer = "wallet.Server";
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

static void
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
  nsFileSpec           mFileSpec;     // request filename
  nsFileSpec           mDownloadFileSpec; // tmp filename for download
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
            mOutFile = new nsOutputFileStream(mDownloadFileSpec);
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
  PR_LOG(gWalletLog, PR_LOG_ALWAYS,
    ("InputConsumer::OnDataAvailable[%x]. aLength%u\n", this, aLength));
  char buf[1001];
  PRUint32 amt;
  nsresult rv = NS_OK;
  do {
    amt = 0;
    rv = aIStream->Read(buf, 1000, &amt);
    if (amt == 0) {
      rv = NS_OK;
      break;
    }
    if (NS_FAILED(rv)) {
      break;
    }
    buf[amt] = '\0';
    mOutFile->write(buf,amt);
  } while (amt);
  PR_LOG(gWalletLog, PR_LOG_ALWAYS, ("InputConsumer::OnDataAvailable[%x] done.\n", this));
  return rv;
}

NS_IMETHODIMP
InputConsumer::OnStopRequest(nsIChannel* channel, 
                             nsISupports* context,
                             nsresult aStatus,
                             const PRUnichar* aMsg)
{
  PR_LOG(gWalletLog, PR_LOG_ALWAYS, ("InputConsumer::OnStopRequest[%x]\n", this));
  PRUint32 httpStatus;
  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIHTTPChannel> pHTTPCon(do_QueryInterface(channel));
  if (pHTTPCon) {
    pHTTPCon->GetResponseStatus(&httpStatus);
  }
  if (mOutFile && httpStatus != 304 ) {
      mOutFile->flush();
      mOutFile->close();
      rv = NS_OK;
  }

  /* rename the downloaded file to the file that was requested */
  if (NS_SUCCEEDED(rv)) {
    mFileSpec.Delete(PR_FALSE);
    mDownloadFileSpec.Rename(mFileName);
    SI_SetBoolPref(pref_WalletExtractTables, PR_TRUE);
  }

  return rv;
}

NS_IMETHODIMP
InputConsumer::Init(nsFileSpec dirSpec, const char *out)

{
  mDirSpec = dirSpec;
  mFileName = nsCRT::strdup(out);
  mFileSpec = dirSpec + mFileName;
  // Create a temp download filename
  nsCAutoString downloadFilename = mFileName;
  downloadFilename.Append(",d");
  mDownloadFileSpec = dirSpec + downloadFilename;
  return NS_OK;
}

nsresult
NS_NewURItoFile(const char *in, nsFileSpec dirSpec, const char *out)
{
    nsresult rv;
    if (gWalletLog == nsnull)
        gWalletLog = PR_NewLogModule ("nsWallet");

    nsCOMPtr<nsIIOService> serv = do_GetService(kIOServiceCID, &rv);
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

    /* Turn off the cache since we are doing our own caching of this file */

    nsLoadFlags loadAttribs = 0;
    pChannel->GetLoadAttributes(&loadAttribs);
    loadAttribs |= nsIChannel::FORCE_RELOAD;
    pChannel->SetLoadAttributes(loadAttribs);

    /* Trigger the async download */

    rv = pChannel->AsyncRead(listener,  // IStreamListener consumer
                             nsnull);   // ISupports context

    /* return NS_OK iff there was an earlier version of the file downloaded */
    if (listener->mFileSpec.Exists()) {
      rv = NS_OK;
    } else {
      rv = NS_ERROR_FAILURE;
    }

    NS_RELEASE(listener);
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

static void
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

static void
wallet_DumpAutoString(const nsString& as){
  char s[100];
  as.ToCString(s, sizeof(s));
  fprintf(stdout, "%s\n", s);
}

static void
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

static void
wallet_ClearTiming() {
  timing_index  = 0;
  LL_I2L(timings[timing_index++], PR_IntervalNow());
}

static void
wallet_DumpTiming() {
  PRInt32 i, r4;
  PRInt64 r1, r2, r3;
  for (i=1; i<timing_index; i++) {
    LL_SUB(r1, timings[i], timings[i-1]);
    LL_I2L(r2, 100);
    LL_DIV(r3, r1, r2);
    LL_L2I(r4, r3);
    fprintf(stdout, "time %c = %ld\n", timingID[i], (long)r4);
    if (i%20 == 0) {
      wallet_Pause();
    }
  }
  wallet_Pause();
}

static void
wallet_AddTiming(char c) {
  if (timing_index<timing_max) {
    timingID[timing_index] = c;
    // note: PR_IntervalNow returns a 32 bit value!
    LL_I2L(timings[timing_index++], PR_IntervalNow());
  }
}

static void
wallet_ClearStopwatch() {
  stopwatch = LL_Zero();
  stopwatchRunning = PR_FALSE;
}

static void
wallet_ResumeStopwatch() {
  if (!stopwatchRunning) {
    // note: PR_IntervalNow returns a 32 bit value!
    LL_I2L(stopwatchBase, PR_IntervalNow());
    stopwatchRunning = PR_TRUE;
  }
}

static void
wallet_PauseStopwatch() {
  PRInt64 r1, r2;
  if (stopwatchRunning) {
    // note: PR_IntervalNow returns a 32 bit value!
    LL_I2L(r1, PR_IntervalNow());
    LL_SUB(r2, r1, stopwatchBase);
    LL_ADD(stopwatch, stopwatch, r2);
    stopwatchRunning = PR_FALSE;
  }
}

static void
wallet_DumpStopwatch() {
  PRInt64 r1, r2;
  PRInt32 r3;
  if (stopwatchRunning) {
    // note: PR_IntervalNow returns a 32 bit value!
    LL_I2L(r1, PR_IntervalNow());
    LL_SUB(r2, r1, stopwatchBase);
    LL_ADD(stopwatch, stopwatch, r2);
    LL_I2L(stopwatchBase, PR_IntervalNow());
  }
  LL_I2L(r1, 100);
  LL_DIV(r2, stopwatch, r1);
  LL_L2I(r3, r2);
  fprintf(stdout, "stopwatch = %ld\n", (long)r3);  
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

  /* create a bundle for the localization */
  nsCOMPtr<nsIStringBundleService> pStringService = do_GetService(kStringBundleServiceCID, &ret);
  if (NS_FAILED(ret)) {
    printf("cannot get string service\n");
    return v.ToNewUnicode();
  }
  nsCOMPtr<nsILocale> locale;
  nsCOMPtr<nsIStringBundle> bundle;
  ret = pStringService->CreateBundle(PROPERTIES_URL, locale, getter_AddRefs(bundle));
  if (NS_FAILED(ret)) {
    printf("cannot create instance\n");
    return v.ToNewUnicode();
  }

  /* localize the given string */
  nsAutoString   strtmp; strtmp.AssignWithConversion(genericString);
  const PRUnichar *ptrtmp = strtmp.GetUnicode();
  PRUnichar *ptrv = nsnull;
  ret = bundle->GetStringFromName(ptrtmp, &ptrv);
  if (NS_FAILED(ret)) {
    printf("cannot get string from name\n");
    return v.ToNewUnicode();
  }
  v = ptrv;
  nsCRT::free(ptrv);

  /* convert # to newlines */
  PRUint32 i;
  for (i=0; i<v.Length(); i++) {
    if (v.CharAt(i) == '#') {
      v.SetCharAt('\n', i);
    }
  }

  return v.ToNewUnicode();
}

/**********************/
/* Modal dialog boxes */
/**********************/

PUBLIC PRBool
Wallet_Confirm(PRUnichar * szMessage, nsIDOMWindow* window)
{
  PRBool retval = PR_TRUE; /* default value */

  nsresult res;  
  nsCOMPtr<nsIPrompt> dialog; 
  window->GetPrompter(getter_AddRefs(dialog)); 
  if (!dialog) {
    return retval;
  } 

  const nsAutoString message = szMessage;
  retval = PR_FALSE; /* in case user exits dialog by clicking X */
  res = dialog->Confirm(nsnull, message.GetUnicode(), &retval);
  return retval;
}

PUBLIC PRBool
Wallet_ConfirmYN(PRUnichar * szMessage, nsIDOMWindow* window) {
  nsresult res;  
  nsCOMPtr<nsIPrompt> dialog; 
  window->GetPrompter(getter_AddRefs(dialog)); 
  if (!dialog) {
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
Wallet_3ButtonConfirm(PRUnichar * szMessage, nsIDOMWindow* window)
{
  nsresult res;  
  nsCOMPtr<nsIPrompt> dialog; 
  window->GetPrompter(getter_AddRefs(dialog)); 
  if (!dialog) {
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
    no_string, /* second button text */
    never_string, /* third button text */
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

  return buttonPressed;
}

PRIVATE void
wallet_Alert(PRUnichar * szMessage, nsIDOMWindow* window)
{
  nsresult res;
  nsCOMPtr<nsIPrompt> dialog; 
  window->GetPrompter(getter_AddRefs(dialog)); 
  if (!dialog) {
    return;     // XXX should return the error
  } 

  const nsAutoString message = szMessage;
  PRUnichar * title = Wallet_Localize("CaveatTitle");
  res = dialog->Alert(title, message.GetUnicode());
  Recycle(title);
  return;     // XXX should return the error
}

PRIVATE void
wallet_Alert(PRUnichar * szMessage, nsIPrompt* dialog)
{
  nsresult res;  
  const nsAutoString message = szMessage;
  PRUnichar * title = Wallet_Localize("CaveatTitle");
  res = dialog->Alert(title, message.GetUnicode());
  Recycle(title);
  return;     // XXX should return the error
}

PUBLIC PRBool
Wallet_CheckConfirmYN
    (PRUnichar * szMessage, PRUnichar * szCheckMessage, PRBool* checkValue,
     nsIDOMWindow* window) {
  nsresult res;
  nsCOMPtr<nsIPrompt> dialog; 
  window->GetPrompter(getter_AddRefs(dialog)); 
  if (!dialog) {
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


/*******************************************************/
/* The following routines are for Encyption/Decryption */
/*******************************************************/

#include "nsISecretDecoderRing.h"
nsISecretDecoderRing* gSecretDecoderRing;
PRBool gEncryptionFailure = PR_FALSE;

PRIVATE nsresult
wallet_CryptSetup() {
  if (!gSecretDecoderRing)
  {
    /* Get a secret decoder ring */
    nsresult rv = NS_OK;
    nsCOMPtr<nsISecretDecoderRing> secretDecoderRing
      = do_CreateInstance("netscape.security.sdr", &rv);
    if (NS_FAILED(rv)) {
      return NS_ERROR_FAILURE;
    }
    gSecretDecoderRing = secretDecoderRing.get();
    NS_ADDREF(gSecretDecoderRing);
  }
  return NS_OK;
}

#define PREFIX "~"
#include "plbase64.h"

PRIVATE nsresult EncryptString (const char * text, char *& crypt) {

  /* use SecretDecoderRing if encryption pref is set */
  nsresult rv;
  if (SI_GetBoolPref(pref_Crypto, PR_FALSE)) {
    rv = wallet_CryptSetup();
    if (NS_SUCCEEDED(rv)) {
      rv = gSecretDecoderRing->EncryptString(text, &crypt);
    }
    if (NS_FAILED(rv)) {
      gEncryptionFailure = PR_TRUE;
    }
    return rv;
  }

  /* otherwise do our own obscuring using Base64 encoding */
  char * crypt0 = PL_Base64Encode((const char *)text, 0, NULL);
  if (!crypt0) {
    return NS_ERROR_FAILURE;
  }
  crypt = (char *)PR_Malloc(PL_strlen(PREFIX) + PL_strlen(crypt0) + 1);
  PRUint32 i;
  for (i=0; i<PL_strlen(PREFIX); i++) {
    crypt[i] = PREFIX[i];
  }
  for (i=0; i<PL_strlen(crypt0); i++) {
    crypt[PL_strlen(PREFIX)+i] = crypt0[i];
  }
  crypt[PL_strlen(PREFIX) + PL_strlen(crypt0)] = '\0';
  Recycle(crypt0);

  return NS_OK;
}

PRIVATE nsresult DecryptString (const char * crypt, char *& text) {

  /* treat zero-length crypt string as a special case */
  if (crypt[0] == '\0') {
    text = (char *)PR_Malloc(1);
    text[0] = '\0';
    return NS_OK;
  }

  /* use SecretDecoderRing if crypt doesn't starts with prefix */
  if (crypt[0] != PREFIX[0]) {
    nsresult rv = wallet_CryptSetup();
    if (NS_SUCCEEDED(rv)) {
      rv = gSecretDecoderRing->DecryptString(crypt, &text);
    }
    if (NS_FAILED(rv)) {
      gEncryptionFailure = PR_TRUE;
    }
    return rv;
  }

  /* otherwise do our own de-obscuring */

  if (PL_strlen(crypt) == PL_strlen(PREFIX)) {
    text = (char *)PR_Malloc(1);
    text[0] = '\0';
    return NS_OK;
  }
  text = PL_Base64Decode(&crypt[PL_strlen(PREFIX)], 0, NULL);
  if (!text) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

PUBLIC void
WLLT_ExpirePassword(PRBool* status) {
  nsresult rv = wallet_CryptSetup();
  if (NS_SUCCEEDED(rv)) {
    rv = gSecretDecoderRing->Logout();
  }
  *status = NS_SUCCEEDED(rv);
}

PRBool changingPassword = PR_FALSE;

PUBLIC void
WLLT_ChangePassword(PRBool* status) {
  nsresult rv = wallet_CryptSetup();
  if (NS_SUCCEEDED(rv)) {
    changingPassword = PR_TRUE;
    rv = gSecretDecoderRing->ChangePassword();
    changingPassword = PR_FALSE;
  }
  *status = NS_SUCCEEDED(rv);
}

PUBLIC nsresult
Wallet_Encrypt (const nsString& text, nsString& crypt) {

  /* convert text from unichar to UTF8 */
  nsAutoString UTF8text;
  PRUnichar c;
  for (PRUint32 i=0; i<text.Length(); i++) {
    c = text.CharAt(i);
    if (c <= 0x7F) {
      UTF8text.Append(c);
    } else if (c <= 0x7FF) {
      UTF8text += PRUnichar((0xC0) | ((c>>6) & 0x1F));
      UTF8text += PRUnichar((0x80) | (c & 0x3F));
    } else {
      UTF8text += PRUnichar((0xE0) | ((c>>12) & 0xF));
      UTF8text += PRUnichar((0x80) | ((c>>6) & 0x3F));
      UTF8text += PRUnichar((0x80) | (c & 0x3F));
    }
  }
  
  /* encrypt text to crypt */
  char * cryptCString = nsnull;
  char * UTF8textCString = UTF8text.ToNewCString();
  nsresult rv = EncryptString(UTF8textCString, cryptCString);
  Recycle (UTF8textCString);
  if NS_FAILED(rv) {
    return rv;
  }
  crypt.AssignWithConversion(cryptCString);
  Recycle (cryptCString);
  return NS_OK;
}

PUBLIC nsresult
Wallet_Decrypt(const nsString& crypt, nsString& text) {

  /* decrypt crypt to text */
  char * cryptCString = crypt.ToNewCString();
  char * UTF8textCString = nsnull;

  nsresult rv = DecryptString(cryptCString, UTF8textCString);
  Recycle(cryptCString);
  if NS_FAILED(rv) {
    return rv;
  }

  /* convert text from UTF8 to unichar */
  PRUnichar c1, c2, c3;
  text.Truncate(0);
  text.SetCapacity(2 * crypt.Length());
  
  for (PRUint32 i=0; i<PL_strlen(UTF8textCString); ) {
    c1 = (PRUnichar)UTF8textCString[i++];    
    if ((c1 & 0x80) == 0x00) {
      text += c1;
    } else if ((c1 & 0xE0) == 0xC0) {
      c2 = (PRUnichar)UTF8textCString[i++];    
      text += (PRUnichar)(((c1 & 0x1F)<<6) + (c2 & 0x3F));
    } else if ((c1 & 0xF0) == 0xE0) {
      c2 = (PRUnichar)UTF8textCString[i++];    
      c3 = (PRUnichar)UTF8textCString[i++];    
      text += (PRUnichar)(((c1 & 0x0F)<<12) + ((c2 & 0x3F)<<6) + (c3 & 0x3F));
    } else {
      Recycle(UTF8textCString);
      return NS_ERROR_FAILURE; /* this is an error, input was not utf8 */
    }
  }
  Recycle(UTF8textCString);
  return NS_OK;
}

PUBLIC nsresult
Wallet_Encrypt2(const nsString& text, nsString& crypt)
{
  return Wallet_Encrypt (text, crypt);
}

PUBLIC nsresult
Wallet_Decrypt2 (const nsString& crypt, nsString& text)
{
  return Wallet_Decrypt (crypt, text);
}


/**********************************************************/
/* The following routines are for accessing the data base */
/**********************************************************/

/*
 * clear out the designated list
 */
static void
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
 * alocate another mapElement
 * We are going to buffer up allocations because it was found that alocating one
 * element at a time was very inefficient on the mac
 */
static wallet_MapElement *
wallet_AlocateMapElement() {
  const PRInt32 alocations = 500;
  static wallet_MapElement* mapElementTable;
  static PRInt32 last = alocations;
  if (last >= alocations) {
    mapElementTable = new wallet_MapElement[alocations];
    if (!mapElementTable) {
      return nsnull;
    }
    last = 0;
  }
  return &mapElementTable[last++];
}

/*
 * add an entry to the designated list
 */
static PRBool
wallet_WriteToList(
    nsString item1,     // not ref. Locally modified
    nsString item2,     // not ref. Locally modified
    nsVoidArray* itemList,
    nsVoidArray*& list,
    PRBool obscure,
    PlacementType placement = DUP_BEFORE) {

  wallet_MapElement * ptr;
  PRBool added_to_list = PR_FALSE;

  wallet_MapElement * mapElement;
  if (list == wallet_URLFieldToSchema_list || list == wallet_FieldToSchema_list ||
      list == wallet_SchemaConcat_list  || list == wallet_DistinguishedSchema_list) {
    mapElement = wallet_AlocateMapElement();
  } else {
    mapElement = new wallet_MapElement;
  }
  if (!mapElement) {
    return PR_FALSE;
  }

  item1.ToLowerCase();
  if (obscure) {
    nsAutoString crypt;
    if (NS_FAILED(Wallet_Encrypt(item2, crypt))) {
      return PR_FALSE;
    }
    item2 = crypt;
  }
  mapElement->item1 = item1;
  mapElement->item2 = item2;
  mapElement->itemList = itemList;

  /* make sure the list exists */
  if(!list) {
      list = new nsVoidArray();
      if(!list) {
          return PR_FALSE;
      }
  }

  /*
   * Add new entry to the list in alphabetical order by item1.
   * If identical value of item1 exists, use "placement" parameter to 
   * determine what to do
   */
  if (AT_END==placement) {
    list->AppendElement(mapElement);
    return PR_TRUE;
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
  return PR_TRUE;
}

/*
 * fetch an entry from the designated list
 */
static PRBool
wallet_ReadFromList(
  nsString item1,
  nsString& item2,
  nsVoidArray*& itemList,
  nsVoidArray*& list,
  PRBool obscure,
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
      if (obscure) {
        if (NS_FAILED(Wallet_Decrypt(ptr->item2, item2))) {
          return PR_FALSE;
        }
      } else {
        item2 = nsAutoString(ptr->item2);
      }
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
  nsString item1,
  nsString& item2,
  nsVoidArray*& itemList,
  nsVoidArray*& list,
  PRBool obscure)
{
  PRInt32 index = 0;
  return wallet_ReadFromList(item1, item2, itemList, list, obscure, index);
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
Wallet_UTF8Put(nsOutputFileStream& strm, PRUnichar c) {
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

static PRUnichar
wallet_Get(nsInputFileStream& strm) {
  const PRUint32 buflen = 1000;
  static char buf[buflen+1];
  static PRUint32 last = 0;
  static PRUint32 next = 0;
  if (next >= last) {
    next = 0;
    last = strm.read(buf, buflen);
    if (last <= 0 || strm.eof()) {
      /* note that eof is not set until we read past the end of the file */
      return 0;
    }
  }
  return (buf[next++] & 0xFF);
}

PUBLIC PRUnichar
Wallet_UTF8Get(nsInputFileStream& strm) {
  PRUnichar c = wallet_Get(strm);
  if ((c & 0x80) == 0x00) {
    return c;
  } else if ((c & 0xE0) == 0xC0) {
    return (((c & 0x1F)<<6) + (wallet_Get(strm) & 0x3F));
  } else if ((c & 0xF0) == 0xE0) {
    return (((c & 0x0F)<<12) + ((wallet_Get(strm) & 0x3F)<<6) +
           (wallet_Get(strm) & 0x3F));
  } else {
    return 0; /* this is an error, input was not utf8 */
  }
}/*

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
Wallet_SimplePut(nsOutputFileStream& strm, PRUnichar c) {
  if (c < 0xFF) {
    strm.put((char)c);
  } else {
    strm.put((PRUnichar)0xFF);
    strm.put((c>>8) & 0xFF);
    strm.put(c & 0xFF);
  }
}

PUBLIC PRUnichar
Wallet_SimpleGet(nsInputFileStream& strm) {
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

char* schemaValueFileName = nsnull;

const char URLFileName[] = "URL.tbl";
const char allFileName[] = "wallet1.html";
const char fieldSchemaFileName[] = "FieldSchema.tbl";
const char URLFieldSchemaFileName[] = "URLFieldSchema.tbl";
const char schemaConcatFileName[] = "SchemaConcat.tbl";
#ifdef AutoCapture
const char distinguishedSchemaFileName[] = "DistinguishedSchema.tbl";
#endif


/******************************************************/
/* The following routines are for accessing the files */
/******************************************************/

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

PUBLIC nsresult Wallet_DefaultsDirectory(nsFileSpec& dirSpec) {
  nsIFileSpec* spec =
    NS_LocateFileOrDirectory(nsSpecialFileSpec::App_DefaultsFolder50);
  if (!spec) {
    return NS_ERROR_FAILURE;
  }
  nsresult res = spec->GetFileSpec(&dirSpec);
  dirSpec += "wallet";
  NS_RELEASE(spec);
  return res;
}

PUBLIC char *
Wallet_RandomName(char* suffix)
{
  /* pick the current time as the random number */
  time_t curTime = time(NULL);

  /* take 8 least-significant digits + three-digit suffix as the file name */
  char name[13];
  PR_snprintf(name, 13, "%lu.%s", ((int)curTime%100000000), suffix);
  return PL_strdup(name);
}

/*
 * get a line from a file
 * return -1 if end of file reached
 * strip carriage returns and line feeds from end of line
 */

static PRInt32
wallet_GetLine(nsInputFileStream& strm, nsString& line)
{
  const PRUint32 kInitialStringCapacity = 64;

  /* read the line */
  line.Truncate(0);
  
  PRInt32 stringLen = 0;
  PRInt32 stringCap = kInitialStringCapacity;
  line.SetCapacity(stringCap);
  
  PRUnichar c;
  static PRUnichar lastC = '\0';
  for (;;) {
    c = Wallet_UTF8Get(strm);

    /* check for eof */
    if (c == 0) {
      return -1;
    }

    /* check for line terminator (mac=CR, unix=LF, win32=CR+LF */
    if (c == '\n' && lastC == '\r') {
        continue; /* ignore LF if preceded by a CR */
    }
    lastC = c;
    if (c == '\n' || c == '\r') {
      break;
    }

    stringLen ++;
    // buffer string grows
    if (stringLen == stringCap)
    {
      stringCap += stringCap;   // double buffer len
      line.SetCapacity(stringCap);
    }
    line += c;
  }

  return NS_OK;
}

static PRBool
wallet_GetHeader(nsInputFileStream& strm)
{
  nsAutoString format;
  nsAutoString buffer;

  /* format revision number */
  if (NS_FAILED(wallet_GetLine(strm, format))) {
    return PR_FALSE;
  }
  if (!format.EqualsWithConversion(HEADER_VERSION)) {
    /* something's wrong */
    return PR_FALSE;
  }
  return PR_TRUE;
}

/*
 * Write a line to a file
 */
static void
wallet_PutLine(nsOutputFileStream& strm, const nsString& line) {
  for (PRUint32 i=0; i<line.Length(); i++) {
    Wallet_UTF8Put(strm, line.CharAt(i));
  }
  Wallet_UTF8Put(strm, '\n');
}

static void
wallet_PutHeader(nsOutputFileStream& strm) {

  /* format revision number */
  {
    nsAutoString temp1;
    temp1.AssignWithConversion(HEADER_VERSION);
    wallet_PutLine(strm, temp1);
  }
}

/*
 * write contents of designated list into designated file
 */
static void
wallet_WriteToFile(const char * filename, nsVoidArray* list) {
  wallet_MapElement * ptr;

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

  /* put out the header */
  if (filename == schemaValueFileName) {
    wallet_PutHeader(strm);
  }

  /* traverse the list */
  PRInt32 count = LIST_COUNT(list);
  for (PRInt32 i=0; i<count; i++) {
    ptr = NS_STATIC_CAST(wallet_MapElement*, list->ElementAt(i));
    wallet_PutLine(strm, (*ptr).item1);
    if (!(*ptr).item2.IsEmpty()) {
      wallet_PutLine(strm, (*ptr).item2);
    } else {
      wallet_Sublist * ptr1;
      PRInt32 count2 = LIST_COUNT(ptr->itemList);
      for (PRInt32 j=0; j<count2; j++) {
        ptr1 = NS_STATIC_CAST(wallet_Sublist*, ptr->itemList->ElementAt(j));
        wallet_PutLine(strm, (*ptr1).item);
      }
    }
    wallet_PutLine(strm, nsAutoString());
  }

  /* close the stream */
  strm.flush();
  strm.close();
}

/*
 * Read contents of designated file into designated list
 */
static void
wallet_ReadFromFile
    (const char * filename, nsVoidArray*& list, PRBool localFile, PlacementType placement = AT_END) {

  /* open input stream */
  nsFileSpec dirSpec;
  nsresult rv;
  rv = Wallet_ProfileDirectory(dirSpec);
  if (NS_FAILED(rv)) {
    return;
  }
  nsInputFileStream strm(dirSpec + filename);
  if (!strm.is_open()) {
    if (!localFile) {
      /* if we failed to download the file, see if an initial version of it exists */
      rv = Wallet_DefaultsDirectory(dirSpec);
      if (NS_FAILED(rv)) {
        return;
      }
      nsInputFileStream strm2(dirSpec + filename);
      strm = strm2;
    }
    if (!strm.is_open()) {
      /* still not open so give up */
      return;
    }
  }

  /* read in the header */
  if (filename == schemaValueFileName) {
    if (!wallet_GetHeader(strm)) {
      /* something's wrong -- ignore the file */
      strm.close();
      return;
    }
  }

  for (;;) {
    static nsAutoString item1;
    item1.Truncate(0);
    if (NS_FAILED(wallet_GetLine(strm, item1))) {
      /* end of file reached */
      break;
    }

#ifdef AutoCapture
    /* Distinguished schema list is a list of single entries, not name/value pairs */
    if (PL_strcmp(filename, distinguishedSchemaFileName) == 0) {
      nsVoidArray* dummy = NULL;
      wallet_WriteToList(item1, item1, dummy, list, PR_FALSE, placement);
      continue;
    }
#endif

    static nsAutoString item2;
    item2.Truncate(0);
    if (NS_FAILED(wallet_GetLine(strm, item2))) {
      /* unexpected end of file reached */
      break;
    }
    if (item2.Length()==0) {
      /* the value must have been deleted */
      nsVoidArray* dummy = NULL;
      wallet_WriteToList(item1, item2, dummy, list, PR_FALSE, placement);
      continue;
    }

    static nsAutoString item3;
    item3.Truncate(0);
    if (NS_FAILED(wallet_GetLine(strm, item3))) {
      /* end of file reached */
      nsVoidArray* dummy = NULL;
      wallet_WriteToList(item1, item2, dummy, list, PR_FALSE, placement);
      strm.close();
      return;
    }

    if (item3.Length()==0) {
      /* just a pair of values, no need for a sublist */
      nsVoidArray* dummy = NULL;
      wallet_WriteToList(item1, item2, dummy, list, PR_FALSE, placement);
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
      static nsAutoString dummy2;
      dummy2.Truncate(0);
      for (;;) {
        /* get next item for sublist */
        item3.SetLength(0);
        if (NS_FAILED(wallet_GetLine(strm, item3))) {
          /* end of file reached */
          wallet_WriteToList(item1, dummy2, itemList, list, PR_FALSE, placement);
          strm.close();
          return;
        }
        if (item3.Length()==0) {
          /* blank line reached indicating end of sublist */
          wallet_WriteToList(item1, dummy2, itemList, list, PR_FALSE, placement);
          break;
        }
        /* add item to sublist */
        sublist = new wallet_Sublist;
        if (!sublist) {
          break;
        }
        sublist->item = item3;
        itemList->AppendElement(sublist);
      }
    }
  }
  strm.close();
}

/*
 * Read contents of designated URLFieldToSchema file into designated list
 */
static void
wallet_ReadFromURLFieldToSchemaFile
    (const char * filename, nsVoidArray*& list, PlacementType placement = AT_END) {

  /* open input stream */
  nsFileSpec dirSpec;
  nsresult rv;
  rv = Wallet_ProfileDirectory(dirSpec);
  if (NS_FAILED(rv)) {
    return;
  }
  nsInputFileStream strm(dirSpec + filename);
  if (!strm.is_open()) {
    /* if we failed to download the file, see if an initial version of it exists */
    rv = Wallet_DefaultsDirectory(dirSpec);
    if (NS_FAILED(rv)) {
      return;
    }
    nsInputFileStream strm2(dirSpec + filename);
    strm = strm2;
    if (!strm.is_open()) {
      /* still not open so give up */
      return;
    }
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

    static nsAutoString item;
    item.Truncate(0);
    if (NS_FAILED(wallet_GetLine(strm, item))) {
      /* end of file reached */
      break;
    }

    nsVoidArray * itemList = new nsVoidArray();
    if (!itemList) {
      break;
    }
    nsAutoString dummyString;
    wallet_WriteToList(item, dummyString, itemList, list, PR_FALSE, placement);

    for (;;) {
      static nsAutoString item1;
      item1.Truncate(0);
      if (NS_FAILED(wallet_GetLine(strm, item1))) {
        /* end of file reached */
        break;
      }

      if (item1.Length()==0) {
        /* end of url reached */
        break;
      }

      static nsAutoString item2;
      item2.Truncate(0);
      if (NS_FAILED(wallet_GetLine(strm, item2))) {
        /* unexpected end of file reached */
        break;
      }

      nsVoidArray* dummyList = NULL;
      wallet_WriteToList(item1, item2, dummyList, itemList, PR_FALSE, placement);

      static nsAutoString item3;
      item3.Truncate(0);
      if (NS_FAILED(wallet_GetLine(strm, item3))) {
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

PUBLIC void
Wallet_GiveCaveat(nsIDOMWindow* window, nsIPrompt* dialog) {
  /* test for first capturing of data ever and give caveat if so */
  if (!SI_GetBoolPref(pref_Caveat, PR_FALSE)) {
    SI_SetBoolPref(pref_Caveat, PR_TRUE);
    PRUnichar * message = Wallet_Localize("Caveat");
    if (window) {
      wallet_Alert(message, window);
    } else {
      wallet_Alert(message, dialog);
    }
    Recycle(message);
  }
}
 
static void
wallet_GetHostFile(nsIURI * url, nsString& outHostFile)
{
  outHostFile.Truncate(0);
  nsAutoString urlName;
  char* host;
  nsresult rv = url->GetHost(&host);
  if (NS_FAILED(rv)) {
    return;
  }
  urlName.AppendWithConversion(host);
  nsCRT::free(host);
  char* file;
  rv = url->GetPath(&file);
  if (NS_FAILED(rv)) {
    return;
  }
  urlName.AppendWithConversion(file);
  nsCRT::free(file);

  PRInt32 queryPos = urlName.FindChar('?');
  PRInt32 stringEnd = (queryPos == kNotFound) ? urlName.Length() : queryPos;
  urlName.Left(outHostFile, stringEnd);
}

static nsString&
Strip(const nsString& text, nsString& stripText) {
  for (PRUint32 i=0; i<text.Length(); i++) {
    PRUnichar c = text.CharAt(i);
    if ((c>='0' && c<='9') || (c>='A' && c<='Z') || (c>='a' && c<='z') || c>'~') {
      stripText += c;
    }
  }
  return stripText;
}

/*
 * given a field name, get the value
 */
static PRInt32 FieldToValue(
    const nsString& field,
    nsString& schema,
    nsString& value,
    nsVoidArray*& itemList,
    PRInt32& index)
{
  /* return if no SchemaToValue list exists */
  if (!wallet_SchemaToValue_list) {
    return -1;
  }

  /* if no schema name is given, fetch schema name from field/schema tables */
  nsVoidArray* dummy;
  nsString stripField;
  if ((schema.Length() > 0) ||
      wallet_ReadFromList(field, schema, dummy, wallet_specificURLFieldToSchema_list, PR_FALSE) ||
      wallet_ReadFromList(Strip(field, stripField), schema, dummy, wallet_FieldToSchema_list, PR_FALSE)) {
    /* schema name found, now fetch value from schema/value table */ 
    PRInt32 index2 = index;
    if ((index >= 0) &&
        wallet_ReadFromList(schema, value, itemList, wallet_SchemaToValue_list, PR_TRUE, index2)) {
      /* value found, prefill it into form */
      index = index2;
      return 0;
    } else {
      /* value not found, see if concatenation rule exists */
      nsVoidArray * itemList2;
      nsAutoString dummy2;
      if (index > 0) {
        index = 0;
      }
      PRInt32 index4 = 0;
      while (wallet_ReadFromList(schema, dummy2, itemList2, wallet_SchemaConcat_list, PR_FALSE, index4)) {
        /* concatenation rules exist, generate value as a concatenation */
        wallet_Sublist * ptr1;
        value.SetLength(0);
        nsAutoString value2;

        if (dummy2.Length() > 0) {
          PRInt32 index5 = 0;
          for (PRInt32 j=0; j>index; j -= 2) {
            if (!wallet_ReadFromList(dummy2, value2, dummy, wallet_SchemaToValue_list, PR_TRUE, index5)) {
              break;
            }
          }
          if (wallet_ReadFromList(dummy2, value2, dummy, wallet_SchemaToValue_list, PR_TRUE, index5)) {
            value += value2;
          }
        }

        PRInt32 count = LIST_COUNT(itemList2);
        for (PRInt32 i=0; i<count; i++) {
          ptr1 = NS_STATIC_CAST(wallet_Sublist*, itemList2->ElementAt(i));

          /* skip over values found previously */
          /*   note: a returned index of -1 means not-found.  So we will use the
           *   negative even numbers (-2, -4, -6) to designate found as a concatenation
           *   where -2 means first value of each concatenation, -4 means second value, etc.
           */
          PRInt32 index3 = 0;
          for (PRInt32 j=0; j>index; j -= 2) {
            if (!wallet_ReadFromList(ptr1->item, value2, dummy, wallet_SchemaToValue_list, PR_TRUE, index3)) {
              break;
            }
          }

          if (wallet_ReadFromList(ptr1->item, value2, dummy, wallet_SchemaToValue_list, PR_TRUE, index3)) {
            if (value.Length()>0) {
              value.AppendWithConversion(" ");
            }
            value += value2;
          }
        }
        itemList = nsnull;
        if (value.Length()>0) {
          index -= 2;
          if (index == -(2*count)) {
            index = -1;
          }
          return 0;
        }
      }
    }
  } else {
    /* schema name not found, use field name as schema name and fetch value */
    PRInt32 index2 = index;

    nsAutoString temp;
    wallet_GetHostFile(wallet_lastUrl, temp);
    temp.AppendWithConversion(":");
    temp.Append(field);

    if (wallet_ReadFromList(temp, value, itemList, wallet_SchemaToValue_list, PR_TRUE, index2)) {
      /* value found, prefill it into form */
      schema = field;
      index = index2;
      return 0;
    }
  }
  index = -1;
  return -1;
}

static PRInt32
wallet_GetSelectIndex(
  nsIDOMHTMLSelectElement* selectElement,
  const nsString& value,
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
          nsAutoString valueLC = value;
          valueLC.ToLowerCase();
          optionValue.ToLowerCase();
          optionText.ToLowerCase();
          if (valueLC==optionValue || valueLC==optionText) {
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

static PRInt32
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
        nsCOMPtr<nsIDOMElement> element = do_QueryInterface(elementNode);
        if (element) {
          nsAutoString vcard; vcard.AssignWithConversion("VCARD_NAME");
          result = element->GetAttribute(vcard, schema);
        }

        /*
         * if schema name was specified in vcard attribute then get value from schema name,
         * otherwise get value from field name by using mapping tables to get schema name
         */
        if (FieldToValue(field, schema, value, itemList, index) == 0) {
          if (value.IsEmpty() && nsnull != itemList) {
            /* pick first of a set of synonymous values */
            nsAutoString encryptedValue = ((wallet_Sublist *)itemList->ElementAt(0))->item;
            if (NS_FAILED(Wallet_Decrypt(encryptedValue, value))) {
              NS_RELEASE(inputElement);
              return -1;
            }
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

static void
wallet_UseFileFetchedDuringPreviousBrowserSession() {

  /* see if we need to extract wallet tables from the composit file */
  if (!SI_GetBoolPref(pref_WalletExtractTables, PR_FALSE)) {
    return;
  }

  nsresult rv;
  nsFileSpec dirSpec;
  rv = Wallet_ProfileDirectory(dirSpec);
  if (NS_FAILED(rv)) {
    return;
  }

  /* obtain version number */
  char * version = nsnull;
  SI_GetCharPref(pref_WalletVersion, &version);

  /* fetch version number from first line of local composite file */
  nsInputFileStream allFile(dirSpec + allFileName);
  nsAutoString buffer;
  if (NS_FAILED(wallet_GetLine(allFile, buffer))) {
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
  if (NS_FAILED(wallet_GetLine(allFile, buffer))) {
    return;
  }

  /* process each subfile in the composite file */
  PRBool atEnd = PR_FALSE;
  while(!atEnd) {
    /* obtain subfile name and open it as an output stream */
    if (buffer.CharAt(0) != '@') {
      break; /* error */
    }
    buffer.StripWhitespace();
    nsAutoString filename;
    buffer.Right(filename, buffer.Length()-1);
    nsOutputFileStream thisFile(dirSpec + filename);

    /* copy each line in composite file to the subfile */
    for (;;) {
      if (NS_FAILED(wallet_GetLine(allFile, buffer))) {
        /* end of composite file reached */
        atEnd = PR_TRUE;
        break;
      }
      if (buffer.Length() > 0 && buffer.CharAt(0) == '@') {
        /* start of next subfile reached */
        break;
      }
      wallet_PutLine(thisFile, buffer);
    }
  }
  SI_SetBoolPref(pref_WalletExtractTables, PR_FALSE);
}

static void
wallet_FetchFileForUseInNextBrowserSession() {

  /* obtain the server from which to fetch the composite file to be used in next session */
#if 1
  nsXPIDLString ustr;
  SI_GetLocalizedUnicharPref(pref_WalletServer, getter_Copies(ustr));
  wallet_Server = NS_ConvertUCS2toUTF8(ustr);
#else
  SI_GetCharPref(pref_WalletServer, &wallet_Server);
#endif
  if (!wallet_Server || (*wallet_Server == '\0')) {
    /* user does not want to download mapping tables */
    return;
  }

  /* Fetch the composite files and put it into a local composite file
   * to be used in the next browser session.
   */
  nsCAutoString url;
  url = nsCAutoString(wallet_Server);
  url.Append(allFileName);
  nsFileSpec dirSpec;
  nsresult rv = Wallet_ProfileDirectory(dirSpec);
  if (NS_FAILED(rv)) {
    return;
  }
  rv = NS_NewURItoFile(url, dirSpec, allFileName);
}

static void
wallet_FetchFromNetCenter() {
  wallet_UseFileFetchedDuringPreviousBrowserSession();
  wallet_FetchFileForUseInNextBrowserSession();
}

/*
 * initialization for wallet session (done only once)
 */

static void
wallet_Initialize(PRBool fetchTables, PRBool unlockDatabase=PR_TRUE) {
  static PRBool wallet_tablesInitialized = PR_FALSE;
  static PRBool wallet_tablesFetched = PR_FALSE;
  static PRBool wallet_ValuesReadIn = PR_FALSE;
  static PRBool namesInitialized = PR_FALSE;

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
//    printf("******** start profile\n");
//             ProfileStart();

    wallet_Clear(&wallet_FieldToSchema_list); /* otherwise we will duplicate the list */
    wallet_Clear(&wallet_URLFieldToSchema_list); /* otherwise we will duplicate the list */
    wallet_Clear(&wallet_SchemaConcat_list); /* otherwise we will duplicate the list */
#ifdef AutoCapture
    wallet_Clear(&wallet_DistinguishedSchema_list); /* otherwise we will duplicate the list */
    wallet_ReadFromFile(distinguishedSchemaFileName, wallet_DistinguishedSchema_list, PR_FALSE);
#endif
    wallet_ReadFromFile(fieldSchemaFileName, wallet_FieldToSchema_list, PR_FALSE);
    wallet_ReadFromURLFieldToSchemaFile(URLFieldSchemaFileName, wallet_URLFieldToSchema_list);
    wallet_ReadFromFile(schemaConcatFileName, wallet_SchemaConcat_list, PR_FALSE);

//    ProfileStop();
//   printf("****** end profile\n");
    wallet_tablesInitialized = PR_TRUE;
  }

  if (!unlockDatabase) {
    return;
  }

  if (!namesInitialized) {
    SI_GetCharPref(pref_WalletSchemaValueFileName, &schemaValueFileName);
    if (!schemaValueFileName) {
      schemaValueFileName = Wallet_RandomName("w");
      SI_SetCharPref(pref_WalletSchemaValueFileName, schemaValueFileName);
    }
    SI_InitSignonFileName();
    namesInitialized = PR_TRUE;
  }

  if (!wallet_ValuesReadIn) {
    wallet_Clear(&wallet_SchemaToValue_list); /* otherwise we will duplicate the list */
    wallet_ReadFromFile(schemaValueFileName, wallet_SchemaToValue_list, PR_TRUE);
    wallet_ValuesReadIn = PR_TRUE;
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
//      ptr->item1.ToCString(item1, 100);
//      fprintf(stdout, item1);
//      fprintf(stdout,"\n");
//      wallet_Dump(ptr->itemList);
//    }
//    fprintf(stdout,"Schema to Value table \n");
//    wallet_Dump(wallet_SchemaToValue_list);
#endif

}

static void
wallet_InitializeURLList() {
  static PRBool wallet_URLListInitialized = PR_FALSE;
  if (!wallet_URLListInitialized) {
    wallet_ReadFromFile(URLFileName, wallet_URL_list, PR_TRUE);
    wallet_URLListInitialized = PR_TRUE;
  }
}

/*
 * initialization for current URL
 */
static void
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
  nsAutoString urlName;
  wallet_GetHostFile(url, urlName);
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

static PRInt32
wallet_GetNextInString(const nsString& str, nsString& head, nsString& tail) {
  PRInt32 separator = str.Find(SEPARATOR);
  if (separator == -1) {
    return -1;
  }
  str.Left(head, separator);
  str.Mid(tail, separator+PL_strlen(SEPARATOR), str.Length() - (separator+PL_strlen(SEPARATOR)));
  return 0;
}

static void
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
WLLT_GetPrefillListForViewer(nsString& aPrefillList)
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

PRIVATE void
wallet_FreeURL(wallet_MapElement *url) {

    if(!url) {
        return;
    }
    wallet_URL_list->RemoveElement(url);
    PR_Free(url);
}

PUBLIC void
Wallet_SignonViewerReturn(const nsString& results)
{
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
          wallet_WriteToFile(URLFileName, wallet_URL_list);
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
          wallet_WriteToFile(URLFileName, wallet_URL_list);
        }
      }
    }
}

#ifdef AutoCapture
/*
 * see if user wants to capture data on current page
 */
PRIVATE PRBool
wallet_OKToCapture(char* urlName, nsIDOMWindow* window) {
  nsAutoString url; url.AssignWithConversion(urlName);

  /* exit if pref is not set */
  if (!wallet_GetFormsCapturingPref() || !wallet_GetEnabledPref()) {
    return PR_FALSE;
  }

  /* see if this url is already on list of url's for which we don't want to capture */
  wallet_InitializeURLList();
  nsVoidArray* dummy;
  nsAutoString value; value.AssignWithConversion("nn");
  if (wallet_ReadFromList(url, value, dummy, wallet_URL_list, PR_FALSE)) {
    if (value.CharAt(NO_CAPTURE) == 'y') {
      return PR_FALSE;
    }
  }

  /* ask user if we should capture the values on this form */
  PRUnichar * message = Wallet_Localize("WantToCaptureForm?");

  PRInt32 button = Wallet_3ButtonConfirm(message, window);
  if (button == NEVER_BUTTON) {
    /* add URL to list with NO_CAPTURE indicator set */
    value.SetCharAt('y', NO_CAPTURE);
    if (wallet_WriteToList(url, value, dummy, wallet_URL_list, PR_FALSE, DUP_OVERWRITE)) {
      wallet_WriteToFile(URLFileName, wallet_URL_list);
    }
  }
  Recycle(message);
  return (button == YES_BUTTON);
}
#endif

/*
 * capture the value of a form element
 */
PRIVATE PRBool
wallet_Capture(nsIDocument* doc, const nsString& field, const nsString& value, const nsString& vcard)
{
  /* do nothing if there is no value */
  if (!value.Length()) {
    return PR_FALSE;
  }

  /* read in the mappings if they are not already present */
  if (!vcard.Length()) {
    wallet_Initialize(PR_TRUE);
    wallet_InitializeCurrentURL(doc);
  }

  nsAutoString oldValue;

  /* is there a mapping from this field name to a schema name */
  nsAutoString schema(vcard);
  nsVoidArray* dummy;
  nsString stripField;
  if (schema.Length() ||
      (wallet_ReadFromList(field, schema, dummy, wallet_specificURLFieldToSchema_list, PR_FALSE)) ||
      (wallet_ReadFromList(Strip(field, stripField), schema, dummy, wallet_FieldToSchema_list, PR_FALSE))) {

    /* field to schema mapping already exists */

    /* is this a new value for the schema */
    PRInt32 index = 0;
    PRInt32 lastIndex = index;
    while(wallet_ReadFromList(schema, oldValue, dummy, wallet_SchemaToValue_list, PR_TRUE, index)) {
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
          wallet_SchemaToValue_list,
          PR_FALSE); /* note: obscure=false, otherwise we will obscure an obscured value */
        delete mapElement;
        return PR_TRUE;
      }
      lastIndex = index;
    }

    /* this is a new value so store it */
    dummy = 0;
    if (wallet_WriteToList(schema, value, dummy, wallet_SchemaToValue_list, PR_TRUE)) {
      wallet_WriteToFile(schemaValueFileName, wallet_SchemaToValue_list);
    }

  } else {

    /* no field to schema mapping so assume schema name is same as field name */

    /* is this a new value for the schema */
    PRInt32 index = 0;
    PRInt32 lastIndex = index;

    nsAutoString concat_param;
    wallet_GetHostFile(wallet_lastUrl, concat_param);
    concat_param.AppendWithConversion(":");
    concat_param.Append(field);

    while(wallet_ReadFromList(concat_param, oldValue, dummy, wallet_SchemaToValue_list, PR_TRUE, index)) {
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
          wallet_SchemaToValue_list,
          PR_FALSE); /* note: obscure=false, otherwise we will obscure an obscured value */
        delete mapElement;
        return PR_TRUE;
      }
      lastIndex = index;

      wallet_GetHostFile(wallet_lastUrl, concat_param);
      concat_param.AppendWithConversion(":");
      concat_param.Append(field);
    }

    /* this is a new value so store it */
    dummy = 0;
    nsAutoString hostFileField;
    wallet_GetHostFile(wallet_lastUrl, hostFileField);
    hostFileField.AppendWithConversion(":");
    hostFileField.Append(field);

    if (wallet_WriteToList(hostFileField, value, dummy, wallet_SchemaToValue_list, PR_TRUE)) {
      wallet_WriteToFile(schemaValueFileName, wallet_SchemaToValue_list);
    }
  }
  return PR_TRUE;
}

/***************************************************************/
/* The following are the interface routines seen by other dlls */
/***************************************************************/

#define BUFLEN2 5000
#define BREAK '\001'

PUBLIC void
WLLT_GetNopreviewListForViewer(nsString& aNopreviewList)
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
WLLT_GetNocaptureListForViewer(nsString& aNocaptureList)
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
WLLT_PostEdit(const nsString& walletList)
{
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

  /* write the values in the walletList to the file */
  wallet_PutHeader(strm);
  for (;;) {
    separator = tail.FindChar(BREAK);
    if (-1 == separator) {
      break;
    }
    tail.Left(head, separator);
    tail.Mid(temp, separator+1, tail.Length() - (separator+1));
    tail = temp;

    wallet_PutLine(strm, head);
  }

  /* close the file and read it back into the SchemaToValue list */
  strm.close();
  wallet_Clear(&wallet_SchemaToValue_list);
  wallet_ReadFromFile(schemaValueFileName, wallet_SchemaToValue_list, PR_TRUE);
}

PUBLIC void
WLLT_PreEdit(nsString& walletList)
{
  wallet_Initialize(PR_FALSE);
  walletList.AssignWithConversion(BREAK);
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

PUBLIC void
WLLT_DeleteAll() {
  wallet_Initialize(PR_FALSE);
  wallet_Clear(&wallet_SchemaToValue_list);
  wallet_WriteToFile(schemaValueFileName, wallet_SchemaToValue_list);
  SI_DeleteAll();
  SI_SetBoolPref(pref_Crypto, PR_FALSE);
}

MODULE_PRIVATE int PR_CALLBACK
wallet_ReencryptAll(const char * newpref, void* window) {
  PRUnichar * message;

  /* prevent reentry for the case that the user doesn't supply correct master password */
  static PRInt32 level = 0;
  if (level != 0) {
    return 0; /* this is PREF_NOERROR but we no longer include prefapi.h */
  }
  level ++;
  PRInt32 count = LIST_COUNT(wallet_SchemaToValue_list);
  PRInt32 i = 0;
  nsAutoString value;

  /* logout first so there is no conversion unless user knows the master password */
if (!changingPassword) {
  nsresult rv = wallet_CryptSetup();
  if (NS_SUCCEEDED(rv)) {
    rv = gSecretDecoderRing->Logout();
  }
  if (NS_FAILED(rv)) {
    goto fail;
  }
  wallet_Initialize(PR_FALSE);
}
  wallet_MapElement * ptr;
  gEncryptionFailure = PR_FALSE;
  for (i=0; i<count && !gEncryptionFailure; i++) {
    ptr = NS_STATIC_CAST(wallet_MapElement*, wallet_SchemaToValue_list->ElementAt(i));
    if (!ptr->item2.IsEmpty()) {
      if (NS_FAILED(Wallet_Decrypt(ptr->item2, value))) {
        goto fail;
      }
      if (NS_FAILED(Wallet_Encrypt(value, ptr->item2))) {
        goto fail;
      }
    } else {
      wallet_Sublist * ptr1;
      PRInt32 count2 = LIST_COUNT(ptr->itemList);
      for (PRInt32 i2=0; i2<count2; i2++) {
        ptr1 = NS_STATIC_CAST(wallet_Sublist*, ptr->itemList->ElementAt(i2));
        if (NS_FAILED(Wallet_Decrypt(ptr1->item, value))) {
          goto fail;
        }
        if (NS_FAILED(Wallet_Encrypt(value, ptr1->item))) {
          goto fail;
        }
      }
    }
  }
  wallet_WriteToFile(schemaValueFileName, wallet_SchemaToValue_list);
  if (!SINGSIGN_ReencryptAll()) {
    goto fail;
  }

  /* force a rewriting of prefs.js to make sure pref_Crypto got updated
   *
   *   Note: In the event of a crash after changing this pref (either way), the user
   *   could get misled as to what state his storage was in.  If the crash occurred 
   *   after changing to encrypted, he could think he was encrypting in the future (because
   *   he remembered changed to encypting at one time) but his new values are only being
   *   obscurred.  If the crash occurred after changing to obscured, later on he might
   *   think his store was encrypted (because he checked the pref panel and that's what
   *   it told him) whereas some of the earlier values are actually obscured and so not
   *   protected.  For both these reasons, we force this rewriting of the prefs file now.
   */
  SI_SetBoolPref(pref_Crypto, SI_GetBoolPref(pref_Crypto, PR_TRUE));

//  message = Wallet_Localize("Converted");
//  wallet_Alert(message, (nsIDOMWindow *)window);
//  Recycle(message);
  level--;
  return 0; /* this is PREF_NOERROR but we no longer include prefapi.h */
fail:
  /* toggle the pref back to its previous value */
  SI_SetBoolPref(pref_Crypto, !SI_GetBoolPref(pref_Crypto, PR_TRUE));

  /* alert the user to the failure */
  message = Wallet_Localize("NotConverted");
  wallet_Alert(message, (nsIDOMWindow *)window);
  Recycle(message);
  level--;
  return 1;
}

PUBLIC void
WLLT_InitReencryptCallback(nsIDOMWindow* window) {
  static PRBool registered = PR_FALSE;
  static nsIDOMWindow* lastWindow;
  if (registered) {
    SI_UnregisterCallback(pref_Crypto, wallet_ReencryptAll, lastWindow);
  }
  SI_RegisterCallback(pref_Crypto, wallet_ReencryptAll, window);
  lastWindow = window;
  registered = PR_TRUE;
}

/*
 * return after previewing a set of prefills
 */
PUBLIC void
WLLT_PrefillReturn(const nsString& results)
{
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
    wallet_ReadFromList(url, value, dummy, wallet_URL_list, PR_FALSE);
    value.SetCharAt('y', NO_PREVIEW);
    if (wallet_WriteToList(url, value, dummy, wallet_URL_list, PR_FALSE, DUP_OVERWRITE)) {
      wallet_WriteToFile(URLFileName, wallet_URL_list);
    }
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
        while(wallet_ReadFromList(*ptr->schema, oldValue, dummy, wallet_SchemaToValue_list, PR_TRUE, index)) {
          if (oldValue == *ptr->value) {
            wallet_MapElement * mapElement =
              (wallet_MapElement *) (wallet_SchemaToValue_list->ElementAt(lastIndex));
            wallet_SchemaToValue_list->RemoveElementAt(lastIndex);
            wallet_WriteToList(
              mapElement->item1,
              mapElement->item2,
              mapElement->itemList,
              wallet_SchemaToValue_list,
              PR_FALSE); /* note: obscure=false, otherwise we will obscure an obscured value */
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
WLLT_Prefill(nsIPresShell* shell, PRBool quick, nsIDOMWindow* win)
{
  nsAutoString urlName;

  /* create list of elements that can be prefilled */
  nsVoidArray *wallet_PrefillElement_list=new nsVoidArray();
  if (!wallet_PrefillElement_list) {
    return NS_ERROR_FAILURE;
  }

  /* starting with the present shell, get each form element and put them on a list */
  nsresult result;
  if (nsnull != shell)
  {
    nsCOMPtr<nsIDocument> doc;
    shell->GetDocument(getter_AddRefs(doc));
    if (doc)
    {
      nsCOMPtr<nsIURI> url = getter_AddRefs(doc->GetDocumentURL());
      if (url) {
        wallet_GetHostFile(url, urlName);
      }
      wallet_Initialize(PR_TRUE);
      gEncryptionFailure = PR_FALSE;
      wallet_InitializeCurrentURL(doc);
      
      nsCOMPtr<nsIDOMHTMLDocument> htmldoc = do_QueryInterface(doc);
      if (htmldoc)
      {
        nsCOMPtr<nsIDOMHTMLCollection> forms;
        htmldoc->GetForms(getter_AddRefs(forms));
        if (forms)
        {
          PRUint32 numForms;
          forms->GetLength(&numForms);
          for (PRUint32 formX = 0; (formX < numForms) && !gEncryptionFailure; formX++)
          {
            nsCOMPtr<nsIDOMNode> formNode;
            forms->Item(formX, getter_AddRefs(formNode));
            if (formNode)
            {
              nsCOMPtr<nsIDOMHTMLFormElement> formElement = do_QueryInterface(formNode);
              if (formElement)
              {
                nsCOMPtr<nsIDOMHTMLCollection> elements;
                result = formElement->GetElements(getter_AddRefs(elements));
                if (elements)
                {
                  /* got to the form elements at long last */
                  PRUint32 numElements;
                  elements->GetLength(&numElements);
                  for (PRUint32 elementX = 0; (elementX < numElements) && !gEncryptionFailure; elementX++)
                  {
                    nsCOMPtr<nsIDOMNode> elementNode;
                    elements->Item(elementX, getter_AddRefs(elementNode));
                    if (elementNode)
                    {
                      wallet_PrefillElement * prefillElement;
                      PRInt32 index = 0;
                      wallet_PrefillElement * firstElement = nsnull;
                      PRUint32 numberOfElements = 0;
                      for (; !gEncryptionFailure;) {
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
                        } else {
                          /* value not found, stop looking for more values */
                          delete prefillElement;
                          break;
                        }
                      } // for
                      
                      if (numberOfElements>0) {
                        firstElement->count = numberOfElements;
                      }
                    }
                    
                  } // for
                  
                }
              }
            }
          }
        }
      }
    }
  }

  /* return if no elements were put into the list */
  if (LIST_COUNT(wallet_PrefillElement_list) == 0) {
    if (!gEncryptionFailure) {
      PRUnichar * message = Wallet_Localize("noPrefills");
      wallet_Alert(message, win);
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
      wallet_ReadFromList(urlName, value, dummy, wallet_URL_list, PR_FALSE);
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
////wallet_DumpStopwatch();
////wallet_ClearStopwatch();
//wallet_DumpTiming();
//wallet_ClearTiming();
#endif
    return NS_OK; // indicates that caller is to display preview screen
  }
}

PUBLIC void
WLLT_RequestToCapture(nsIPresShell* shell, nsIDOMWindow* win, PRUint32* status) {

  /* starting with the present shell, get each form element and put them on a list */
  nsresult result;
  PRInt32 captureCount = 0;
  gEncryptionFailure = PR_FALSE;
  if (nsnull != shell)
  {
    nsCOMPtr<nsIDocument> doc;
    result = shell->GetDocument(getter_AddRefs(doc));
    if (NS_SUCCEEDED(result))
    {
      wallet_Initialize(PR_TRUE);
      wallet_InitializeCurrentURL(doc);
      nsCOMPtr<nsIDOMHTMLDocument> htmldoc = do_QueryInterface(doc);
      if (htmldoc)
      {
        nsCOMPtr<nsIDOMHTMLCollection> forms;
        htmldoc->GetForms(getter_AddRefs(forms));
        if (forms)
        {
          PRUint32 numForms;
          forms->GetLength(&numForms);
          for (PRUint32 formX = 0; (formX < numForms) && !gEncryptionFailure; formX++)
          {
            nsCOMPtr<nsIDOMNode> formNode;
            forms->Item(formX, getter_AddRefs(formNode));
            if (formNode)
            {
              nsCOMPtr<nsIDOMHTMLFormElement> formElement = do_QueryInterface(formNode);
              if (formElement)
              {
                nsCOMPtr<nsIDOMHTMLCollection> elements;
                result = formElement->GetElements(getter_AddRefs(elements));
                if (elements)
                {
                  /* got to the form elements at long last */
                  /* now find out how many text fields are on the form */
                  PRUint32 numElements;
                  elements->GetLength(&numElements);
                  for (PRUint32 elementY = 0; (elementY < numElements) && !gEncryptionFailure; elementY++)
                  {
                    nsCOMPtr<nsIDOMNode> elementNode;
                    elements->Item(elementY, getter_AddRefs(elementNode));
                    if (elementNode)
                    {
                      nsCOMPtr<nsIDOMHTMLInputElement> inputElement = do_QueryInterface(elementNode);  
                      if (inputElement)
                      {
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
                              nsCOMPtr<nsIDOMElement> element = do_QueryInterface(elementNode);
                              if (element)
                              {
                                nsAutoString vcardName; vcardName.AssignWithConversion("VCARD_NAME");
                                result = element->GetAttribute(vcardName, vcardValue);
                              }
                              if (wallet_Capture(doc, field, value, vcardValue)) {
                                captureCount++;
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  PRUnichar * message;

  if (gEncryptionFailure) {
    message = Wallet_Localize("UnableToCapture");
    *status = 0;
  } else if (captureCount) {
    /* give caveat if this is the first time data is being captured */
    Wallet_GiveCaveat(win, nsnull);
    message = Wallet_Localize("Captured");
    *status = 0;
  } else {
    message = Wallet_Localize("NotCaptured");
    *status = +1;
  }
  wallet_Alert(message, win);
  Recycle(message);
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
WLLT_OnSubmit(nsIContent* currentForm, nsIDOMWindow* window) {

  nsCOMPtr<nsIDOMHTMLFormElement> currentFormNode(do_QueryInterface(currentForm));

  /* get url name as ascii string */
  char *URLName = nsnull;
  char *strippedURLName = nsnull;
  nsAutoString strippedURLNameAutoString;
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
  wallet_GetHostFile(docURL, strippedURLNameAutoString);
  strippedURLName = strippedURLNameAutoString.ToNewCString();

  /* get to the form elements */
  nsCOMPtr<nsIDOMHTMLDocument> htmldoc(do_QueryInterface(doc));
  if (htmldoc == nsnull) {
    nsCRT::free(URLName);
    nsCRT::free(strippedURLName);
    return;
  }

  nsCOMPtr<nsIDOMHTMLCollection> forms;
  nsresult rv = htmldoc->GetForms(getter_AddRefs(forms));
  if (NS_FAILED(rv) || (forms == nsnull)) {
    nsCRT::free(URLName);
    nsCRT::free(strippedURLName);
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
          PRInt32 passwordcount = 0;
          PRInt32 hits = 0;
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
#ifdef AutoCapture
                  if (isPassword) {
                    passwordcount++;
                    OKToPrompt = PR_FALSE;
                  }
#else
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
                        if (passwordcount == 0) {
                          /* get schema from field */
                          wallet_Initialize(PR_FALSE, PR_FALSE);
                          wallet_InitializeCurrentURL(doc);
                          nsAutoString schema;
                          nsVoidArray* dummy;
                          nsString stripField;
                          if (schema.Length() ||
                              (wallet_ReadFromList(field, schema, dummy, wallet_specificURLFieldToSchema_list, PR_FALSE)) ||
                              (wallet_ReadFromList(Strip(field, stripField), schema, dummy, wallet_FieldToSchema_list, PR_FALSE))) {
                          }
                          /* see if schema is in distinguished list */
                          schema.ToLowerCase();
                          wallet_MapElement * ptr;
                          PRInt32 count = LIST_COUNT(wallet_DistinguishedSchema_list);
                          /* test for at least two distinguished schemas and no passwords */
                          for (PRInt32 i=0; i<count; i++) {
                            ptr = NS_STATIC_CAST
                              (wallet_MapElement*, wallet_DistinguishedSchema_list->ElementAt(i));
                            if (ptr->item1 == schema && value.Length() > 0) {
                              hits++;
                              if (hits > 1) {
                                OKToPrompt = PR_TRUE;
                                break;
                              }
                            }
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
            NS_WITH_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, &rv);
            if (NS_SUCCEEDED(rv)) {
              SINGSIGN_RememberSignonData(dialog, URLName, signonData, window);
            }
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
            wallet_Alert(notification, window);
            Recycle(notification);
          }
#else
          /* save form if it meets all necessary conditions */
          if (wallet_GetFormsCapturingPref() &&
              (OKToPrompt) && wallet_OKToCapture(strippedURLName, window)) {

            /* give caveat if this is the first time data is being captured */
            Wallet_GiveCaveat(window, nsnull);

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
  nsCRT::free(strippedURLName);
}

PUBLIC void
WLLT_FetchFromNetCenter() {
//  wallet_FetchFromNetCenter();
}

