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

#include "xp_list.h"
#include "nsFileStream.h"
#include "nsSpecialSystemDirectory.h"

#include "nsINetSupportDialogService.h"
#include "nsIStringBundle.h"
#include "nsILocale.h"
#include "nsIFileLocator.h"
#include "nsIFileSpec.h"
#include "nsFileLocations.h"
#include "xp_mem.h"
#include "prmem.h"
#include "prprf.h"  
#include "nsIProfile.h"
#include "nsIContent.h"

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

/***************************************************/
/* The following declarations define the data base */
/***************************************************/

enum PlacementType {DUP_IGNORE, DUP_OVERWRITE, DUP_BEFORE, DUP_AFTER, AT_END};

typedef struct _wallet_MapElement {
  nsAutoString * item1;
  nsAutoString * item2;
  XP_List * itemList;
} wallet_MapElement;

typedef struct _wallet_Sublist {
  nsAutoString * item;
} wallet_Sublist;

PRIVATE XP_List * wallet_URLFieldToSchema_list=0;
PRIVATE XP_List * wallet_specificURLFieldToSchema_list=0;
PRIVATE XP_List * wallet_FieldToSchema_list=0;
PRIVATE XP_List * wallet_SchemaToValue_list=0;
PRIVATE XP_List * wallet_SchemaConcat_list=0;
PRIVATE XP_List * wallet_URL_list = 0;

#define NO_CAPTURE 0
#define NO_PREVIEW 1

typedef struct _wallet_PrefillElement {
  nsIDOMHTMLInputElement* inputElement;
  nsIDOMHTMLSelectElement* selectElement;
  nsAutoString * schema;
  nsAutoString * value;
  PRInt32 selectIndex;
  PRUint32 count;
  XP_List * resume;
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
wallet_Dump(XP_List * list) {
  XP_List * list_ptr;
  wallet_MapElement * ptr;

  char item1[100];
  char item2[100];
  char item[100];
  list_ptr = list;
  while((ptr = (wallet_MapElement *) XP_ListNextObject(list_ptr))!=0) {
    ptr->item1->ToCString(item1, 100);
    ptr->item2->ToCString(item2, 100);
    fprintf(stdout, "%s %s \n", item1, item2);
    XP_List * list_ptr1;
    wallet_Sublist * ptr1;
    list_ptr1 = ptr->itemList;
    while((ptr1=(wallet_Sublist *) XP_ListNextObject(list_ptr1))!=0) {
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

extern void
SI_SetBoolPref(char * prefname, PRBool prefvalue);


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
Wallet_CheckConfirm(char * szMessage, char * szCheckMessage, PRBool* checkValue)
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
  res = dialog->ConfirmCheck(message.GetUnicode(), checkMessage.GetUnicode(), checkValue, &retval);
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
    dialog->ConfirmCheck(message, checkMessage, &retval, checkValue);
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
wallet_Clear(XP_List ** list) {
  XP_List * list_ptr;
  wallet_MapElement * ptr;

  list_ptr = *list;
  while((ptr = (wallet_MapElement *) XP_ListNextObject(list_ptr))!=0) {
    delete ptr->item1;
    delete ptr->item2;

    XP_List * list_ptr1;
    wallet_Sublist * ptr1;
    list_ptr1 = ptr->itemList;
    while((ptr1=(wallet_Sublist *) XP_ListNextObject(list_ptr1))!=0) {
      delete ptr1->item;
    }
    delete ptr->itemList;
    XP_ListRemoveObject(*list, ptr);
    list_ptr = *list;
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
    XP_List* itemList,
    XP_List*& list,
    PlacementType placement = DUP_BEFORE) {

  XP_List * list_ptr;
  wallet_MapElement * ptr;
  PRBool added_to_list = PR_FALSE;

  wallet_MapElement * mapElement;
  mapElement = XP_NEW(wallet_MapElement);

  mapElement->item1 = &item1;
  mapElement->item2 = &item2;
  mapElement->itemList = itemList;

  /* make sure the list exists */
  if(!list) {
      list = XP_ListNew();
      if(!list) {
          return;
      }
  }

  /*
   * Add new entry to the list in alphabetical order by item1.
   * If identical value of item1 exists, use "placement" parameter to 
   * determine what to do
   */
  list_ptr = list;
  item1.ToLowerCase();
  if (AT_END==placement) {
    XP_ListAddObjectToEnd (list, mapElement);
    return;
  }
  while((ptr = (wallet_MapElement *) XP_ListNextObject(list_ptr))!=0) {
    if((ptr->item1->Compare(item1))==0) {
      if (DUP_OVERWRITE==placement) {
        delete ptr->item1;
        delete ptr->item2;
        delete mapElement;
        ptr->item1 = &item1;
        ptr->item2 = &item2;
      } else if (DUP_BEFORE==placement) {
        XP_ListInsertObject(list, ptr, mapElement);
      }
      if (DUP_AFTER!=placement) {
        added_to_list = PR_TRUE;
        break;
      }
    } else if((ptr->item1->Compare(item1))>=0) {
      XP_ListInsertObject(list, ptr, mapElement);
      added_to_list = PR_TRUE;
      break;
    }
  }
  if (!added_to_list) {
    XP_ListAddObjectToEnd (list, mapElement);
  }
}

/*
 * fetch an entry from the designated list
 */
PRBool
wallet_ReadFromList(
  nsAutoString item1,
  nsAutoString& item2,
  XP_List*& itemList,
  XP_List*& list_ptr)
{
  wallet_MapElement * ptr;

  wallet_MapElement * mapElement;
  mapElement = XP_NEW(wallet_MapElement);

  /* make sure the list exists */
  if(!list_ptr) {
    return PR_FALSE;
  }

  /* find item1 in the list */
  item1.ToLowerCase();
  while((ptr = (wallet_MapElement *) XP_ListNextObject(list_ptr))!=0) {
    if((ptr->item1->Compare(item1))==0) {
      item2 = nsAutoString(*ptr->item2);
      itemList = ptr->itemList;
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

/*
 * given a sublist element, advance to the next sublist element and get its value
 */
PRInt32
wallet_ReadFromSublist(nsAutoString& value, XP_List*& resume)
{
  wallet_Sublist * ptr;
  if((ptr=(wallet_Sublist *) XP_ListNextObject(resume))!=0) {
    value = *ptr->item;
    return 0;
  }
  return -1;
}

/************************************************************/
/* The following routines are for unlocking the stored data */
/************************************************************/

#define maxKeySize 100
char key[maxKeySize+1];
PRUint32 keyPosition = 0;
PRBool keyFailure = PR_FALSE;
#ifndef NECKO
/* avoid bug 9326 */
PUBLIC
#endif
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
Wallet_BadKey() {
  return keyFailure;
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
    char c = strm.get();
    PRInt32 rv = (strm.eof() ? 0 : 1);
    strm.close();
    return rv;
  }
}

PUBLIC PRBool
Wallet_SetKey(PRBool isNewkey) {
  if (keySet && !isNewkey) {
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
  if (newkey == NULL) { /* user hit cancel button */
    if (wallet_KeySize() < 0) { /* no password file existed before */
      newkey  = PL_strdup(""); /* use zero-length password */
    } else {
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
      keyFailure = PR_TRUE;
      return PR_FALSE;
    }
    nsOutputFileStream strm2(dirSpec + "key");
    if (!strm2.is_open()) {
      keyFailure = PR_TRUE;
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
    keyFailure = PR_FALSE;
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
      keyFailure = PR_FALSE;
      return PR_TRUE;
    }

    nsFileSpec dirSpec;
    nsresult rval = Wallet_ProfileDirectory(dirSpec);
    if (NS_FAILED(rval)) {
      keyFailure = PR_TRUE;
      return PR_FALSE;
    }
    nsInputFileStream strm(dirSpec + "key");
    Wallet_RestartKey();
    char* p = key+1;
    while (*p) {
      if (strm.get() != (*(p++)^Wallet_GetKey()) || strm.eof()) {
        strm.close();
        keyFailure = PR_TRUE;
        *key = '\0';
        return PR_FALSE;
      }
    }
    if (strm.get() != ((*key)^Wallet_GetKey()) || strm.eof()) {
      strm.close();
      keyFailure = PR_TRUE;
      *key = '\0';
      return PR_FALSE;
    }
    strm.get(); /* to get past the end of the file so eof() will get set */
    PRBool rv = strm.eof();
    strm.close();
    if (rv) {
      Wallet_RestartKey();
      keySet = PR_TRUE;
      keyFailure = PR_FALSE;
      return PR_TRUE;
    } else {
      keyFailure = PR_TRUE;
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
wallet_WriteToFile(char* filename, XP_List* list, PRBool obscure) {
  XP_List * list_ptr;
  wallet_MapElement * ptr;

  if (obscure && Wallet_BadKey()) {
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
  list_ptr = list;
  while((ptr = (wallet_MapElement *) XP_ListNextObject(list_ptr))!=0) {
    if (NS_FAILED(wallet_PutLine(strm, *ptr->item1, obscure))) {
      break;
    }
    if (*ptr->item2 != "") {
      if (NS_FAILED(wallet_PutLine(strm, *ptr->item2, obscure))) {
        break;
      }
    } else {
      XP_List * list_ptr1;
      wallet_Sublist * ptr1;
      list_ptr1 = ptr->itemList;
      while((ptr1=(wallet_Sublist *) XP_ListNextObject(list_ptr1))!=0) {
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
    (char* filename, XP_List*& list, PRBool obscure, PlacementType placement = DUP_AFTER) {

  /* open input stream */
  nsFileSpec dirSpec;
  nsresult rv = Wallet_ProfileDirectory(dirSpec);
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
      strm.close();
      return;
    }

    nsAutoString * aItem2;
    if (NS_FAILED(wallet_GetLine(strm, aItem2, obscure))) {
      /* unexpected end of file reached */
      delete aItem1;
      strm.close();
      return;
    }

    nsAutoString * aItem3;
    if (NS_FAILED(wallet_GetLine(strm, aItem3, obscure))) {
      /* end of file reached */
      XP_List* dummy = NULL;
      wallet_WriteToList(*aItem1, *aItem2, dummy, list, placement);
      strm.close();
      return;
    }

    if (aItem3->Length()==0) {
      /* just a pair of values, no need for a sublist */
      XP_List* dummy = NULL;
      wallet_WriteToList(*aItem1, *aItem2, dummy, list, placement);
    } else {
      /* need to create a sublist and put item2 and item3 onto it */
      XP_List * itemList = XP_ListNew();
      wallet_Sublist * sublist;
      sublist = XP_NEW(wallet_Sublist);
      sublist->item = new nsAutoString (*aItem2);
      XP_ListAddObjectToEnd (itemList, sublist);
      delete aItem2;
      sublist = XP_NEW(wallet_Sublist);
      sublist->item = new nsAutoString (*aItem3);
      XP_ListAddObjectToEnd (itemList, sublist);
      delete aItem3;
      /* add any following items to sublist up to next blank line */
      nsAutoString * dummy2 = new nsAutoString("");
      if (!dummy2) {
        strm.close();
        return;
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
        sublist = XP_NEW(wallet_Sublist);
        sublist->item = new nsAutoString (*aItem3);
        XP_ListAddObjectToEnd (itemList, sublist);
        delete aItem3;
      }
    }
  }
}

/*
 * Read contents of designated URLFieldToSchema file into designated list
 */
void
wallet_ReadFromURLFieldToSchemaFile
    (char* filename, XP_List*& list, PlacementType placement = DUP_AFTER) {

  /* open input stream */
  nsFileSpec dirSpec;
  nsresult rv = Wallet_ProfileDirectory(dirSpec);
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
    list = XP_ListNew();
    if(!list) {
      strm.close();
      return;
    }
  }

  for (;;) {

    nsAutoString * aItem;
    if (NS_FAILED(wallet_GetLine(strm, aItem, PR_FALSE))) {
      /* end of file reached */
      strm.close();
      return;
    }

    XP_List * itemList = XP_ListNew();
    nsAutoString * dummy = new nsAutoString("");
    if (!dummy) {
      strm.close();
      return;
    }
    wallet_WriteToList(*aItem, *dummy, itemList, list, placement);

    for (;;) {
      nsAutoString * aItem1;
      if (NS_FAILED(wallet_GetLine(strm, aItem1, PR_FALSE))) {
        /* end of file reached */
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
        strm.close();
        return;
      }

      XP_List* dummy = NULL;
      wallet_WriteToList(*aItem1, *aItem2, dummy, itemList, placement);

      nsAutoString * aItem3;
      if (NS_FAILED(wallet_GetLine(strm, aItem3, PR_FALSE))) {
        /* end of file reached */
        strm.close();
        return;
      }

      if (aItem3->Length()!=0) {
        /* invalid file format */
        strm.close();
        delete aItem3;
        return;
      }
      delete aItem3;
    }
  }
}

/***************************************************************/
/* The following routines are for fetching data from NetCenter */
/***************************************************************/

void
wallet_FetchFromNetCenter(char* from, char* to) {
return;
    nsIInputStream* newStream;
    nsIInputStream* *aNewStream = &newStream;
    nsresult rv;
#ifndef NECKO
    nsIURI * url;
    if (!NS_FAILED(NS_NewURL(&url, from))) {
        NS_WITH_SERVICE(nsINetService, inet, kNetServiceCID, &rv);
        if (NS_FAILED(rv)) return;

        rv = inet->OpenBlockingStream(url, nsnull, aNewStream);
#else
    NS_WITH_SERVICE(nsIIOService, inet, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return;


    nsIChannel *channel = nsnull;
    // XXX NECKO what verb do we want here?
    rv = inet->NewChannel("load", from, nsnull, nsnull, &channel);
    if (NS_FAILED(rv)) return;

    rv = channel->OpenInputStream(0, -1, aNewStream);
#endif // NECKO

        /* open network stream */
        if (NS_SUCCEEDED(rv)) {

          /* open output file */
          nsFileSpec dirSpec;
          rv = Wallet_ProfileDirectory(dirSpec);
          if (NS_FAILED(rv)) {
            return;
          }
          nsOutputFileStream strm(dirSpec + to);
          if (!strm.is_open()) {
            NS_ERROR("unable to open file");
          } else {

            /* place contents of network stream in output file */
            char buff[1001];
            PRUint32 count;
            while (NS_SUCCEEDED((*aNewStream)->Read(buff,1000,&count))) {
              buff[count] = '\0';
              strm.write(buff, count);
            }
            strm.flush();
            strm.close();
          }
        }
#ifndef NECKO
    }
#endif // NECKO
}

/*
 * fetch URL-specific field/schema mapping from netcenter and put into local copy of file
 * at URLFieldSchema.tbl
 */
void
wallet_FetchURLFieldSchemaFromNetCenter() {
  wallet_FetchFromNetCenter
    ("http://people.netscape.com/morse/wallet/URLFieldSchema.tbl","URLFieldSchema.tbl");
}

/*
 * fetch generic field/schema mapping from netcenter and put into
 * local copy of file at FieldSchema.tbl
 */
void
wallet_FetchFieldSchemaFromNetCenter() {
  wallet_FetchFromNetCenter
    ("http://people.netscape.com/morse/wallet/FieldSchema.tbl","FieldSchema.tbl");
}

/*
 * fetch generic schema-concatenation rules from netcenter and put into
 * local copy of file at SchemaConcat.tbl
 */
void
wallet_FetchSchemaConcatFromNetCenter() {
  wallet_FetchFromNetCenter
    ("http://people.netscape.com/morse/wallet/SchemaConcat.tbl","SchemaConcat.tbl");
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
    XP_List*& itemList,
    XP_List*& resume)
{
  /* return if no SchemaToValue list exists */
  if (!wallet_SchemaToValue_list) {
    return -1;
  }

  /* if no schema name is given, fetch schema name from field/schema tables */
  XP_List* FieldToSchema_list = wallet_FieldToSchema_list;
  XP_List* URLFieldToSchema_list = wallet_specificURLFieldToSchema_list;
  XP_List* SchemaToValue_list;
  XP_List* dummy;
  if (nsnull == resume) {
    resume = wallet_SchemaToValue_list;
  }
  if ((schema.Length() > 0) ||
      wallet_ReadFromList(field, schema, dummy, URLFieldToSchema_list) ||
      wallet_ReadFromList(field, schema, dummy, FieldToSchema_list)) {
    /* schema name found, now fetch value from schema/value table */ 
    SchemaToValue_list = resume;
    if (wallet_ReadFromList(schema, value, itemList, SchemaToValue_list)) {
      /* value found, prefill it into form */
      resume = SchemaToValue_list;
      return 0;
    } else {
      /* value not found, see if concatenation rule exists */
      XP_List * itemList2;

      XP_List * SchemaConcat_list = wallet_SchemaConcat_list;
      nsAutoString dummy2;
      if (wallet_ReadFromList(schema, dummy2, itemList2, SchemaConcat_list)) {
        /* concatenation rules exist, generate value as a concatenation */
        XP_List * list_ptr1;
        wallet_Sublist * ptr1;
        list_ptr1 = itemList2;
        value = nsAutoString("");
        nsAutoString value2;
        while((ptr1=(wallet_Sublist *) XP_ListNextObject(list_ptr1))!=0) {
          SchemaToValue_list = wallet_SchemaToValue_list;
          if (wallet_ReadFromList(*(ptr1->item), value2, dummy, SchemaToValue_list)) {
            if (value.Length()>0) {
              value += " ";
            }
            value += value2;
          }
        }
        resume = nsnull;
        itemList = nsnull;
        if (value.Length()>0) {
          return 0;
        }
      }
    }
  } else {
    /* schema name not found, use field name as schema name and fetch value */
    SchemaToValue_list = resume;
    if (wallet_ReadFromList(field, value, itemList, SchemaToValue_list)) {
      /* value found, prefill it into form */
      schema = nsAutoString(field);
      resume = SchemaToValue_list;
      return 0;
    }
  }
  resume = nsnull;
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
  XP_List*& resume)
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
        XP_List* itemList;

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
        if (FieldToValue(field, schema, value, itemList, resume) == 0) {
          if (value == "" && nsnull != itemList) {
            /* pick first of a set of synonymous values */
            wallet_ReadFromSublist(value, itemList);
          }
          valuePtr = new nsAutoString(value);
          schemaPtr = new nsAutoString(schema);
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
      XP_List* itemList;
      if (FieldToValue(field, schema, value, itemList, resume) == 0) {
        if (value != "") {
          /* no synonym list, just one value to try */
          result = wallet_GetSelectIndex(selectElement, value, selectIndex);
          if (NS_SUCCEEDED(result)) {
            /* value matched one of the values in the drop-down list */
            valuePtr = new nsAutoString(value);
            schemaPtr = new nsAutoString(schema);
            inputElement = nsnull;
            return NS_OK;
          }
        } else {
          /* synonym list exists, try each value */
          while (wallet_ReadFromSublist(value, itemList) == 0) {
            result = wallet_GetSelectIndex(selectElement, value, selectIndex);
            if (NS_SUCCEEDED(result)) {
              /* value matched one of the values in the drop-down list */
              valuePtr = new nsAutoString(value);
              schemaPtr = new nsAutoString(schema);
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
  if (!wallet_Initialized) {
    wallet_FetchFieldSchemaFromNetCenter();
    wallet_FetchURLFieldSchemaFromNetCenter();
    wallet_FetchSchemaConcatFromNetCenter();

    wallet_ReadFromFile("FieldSchema.tbl", wallet_FieldToSchema_list, PR_FALSE);
    wallet_ReadFromURLFieldToSchemaFile("URLFieldSchema.tbl", wallet_URLFieldToSchema_list);
    wallet_ReadFromFile("SchemaConcat.tbl", wallet_SchemaConcat_list, PR_FALSE);

    wallet_Initialized = PR_TRUE;

    Wallet_RestartKey();
    char * message = Wallet_Localize("IncorrectKey_TryAgain?");
    char * failed = Wallet_Localize("KeyFailure");
    while (!Wallet_SetKey(PR_FALSE)) {
      if (!Wallet_Confirm(message)) {
        Wallet_Alert(failed);
        PR_FREEIF(message);
        PR_FREEIF(failed);
        return;
      }
    }
    PR_FREEIF(message);
    PR_FREEIF(failed);


    wallet_ReadFromFile("SchemaValue.tbl", wallet_SchemaToValue_list, PR_TRUE);
  }

#if DEBUG
//    fprintf(stdout,"Field to Schema table \n");
//    wallet_Dump(wallet_FieldToSchema_list);

//    fprintf(stdout,"SchemaConcat table \n");
//    wallet_Dump(wallet_SchemaConcat_list);

//    fprintf(stdout,"URL Field to Schema table \n");
//    XP_List * list_ptr;
//    char item1[100];
//    wallet_MapElement * ptr;
//    list_ptr = wallet_URLFieldToSchema_list;
//    while((ptr = (wallet_MapElement *) XP_ListNextObject(list_ptr))!=0) {
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
  if (Wallet_BadKey()) {
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
    wallet_ReadFromFile("URL.tbl", wallet_URL_list, PR_FALSE);
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
  url->GetHost(&host);
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
  XP_List * list_ptr;
  wallet_MapElement * ptr;
  list_ptr = wallet_URLFieldToSchema_list;
  while((ptr = (wallet_MapElement *) XP_ListNextObject(list_ptr))!=0) {
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

#define SEPARATOR "#*%&"

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
wallet_ReleasePrefillElementList(XP_List * wallet_PrefillElement_list) {
  if (wallet_PrefillElement_list) {
    wallet_PrefillElement * ptr;
    XP_List * list_ptr = wallet_PrefillElement_list;
    while((ptr = (wallet_PrefillElement *) XP_ListNextObject(list_ptr))!=0) {
      if (ptr->inputElement) {
        NS_RELEASE(ptr->inputElement);
      } else {
        NS_RELEASE(ptr->selectElement);
      }
      delete ptr->schema;
      delete ptr->value;
      XP_ListRemoveObject(wallet_PrefillElement_list, ptr);
      list_ptr = wallet_PrefillElement_list;
      delete ptr;
    }
  }
}

#define BUFLEN3 5000
#define BREAK '\001'

XP_List * wallet_list;
nsString wallet_url;

PUBLIC void
WLLT_GetPrefillListForViewer(nsString& aPrefillList)
{
  char *buffer = (char*)XP_ALLOC(BUFLEN3);
  int g = 0, prefillNum = 0;
  XP_List *list_ptr = wallet_list;
  wallet_PrefillElement * ptr;
  buffer[0] = '\0';
  char * schema;
  char * value;

  while((ptr = (wallet_PrefillElement *) XP_ListNextObject(list_ptr))!=0) {
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
    XP_ListRemoveObject(wallet_URL_list, url);
    PR_FREEIF(url->item1);
    PR_FREEIF(url->item2);
    PR_Free(url);
}

PUBLIC void
Wallet_SignonViewerReturn (nsAutoString results) {
    XP_List *url_ptr;
    wallet_MapElement *url;
    wallet_MapElement *URLToDelete;
    int urlNumber;
    char * gone;

    /*
     * step through all nopreviews and delete those that are in the sequence
     * Note: we can't delete nopreview while "url_ptr" is pointing to it because
     * that would destroy "url_ptr". So we do a lazy deletion
     */
    URLToDelete = 0;
    gone = SI_FindValueInArgs(results, "|goneP|");
    urlNumber = 0;
    url_ptr = wallet_URL_list;
    while ((url = (wallet_MapElement *) XP_ListNextObject(url_ptr))) {
      if (url->item2->CharAt(NO_PREVIEW) == 'y') {
        if (SI_InSequence(gone, urlNumber)) {
          url->item2->SetCharAt('n', NO_PREVIEW);
          if (url->item2->CharAt(NO_CAPTURE) == 'n') {
            if (URLToDelete) {
                wallet_FreeURL(URLToDelete);
            }
            URLToDelete = url;
          }
        }
        urlNumber++;
      }
    }
    if (URLToDelete) {
        wallet_FreeURL(URLToDelete);
        wallet_WriteToFile("URL.tbl", wallet_URL_list, PR_FALSE);
    }
    delete[] gone;

    /*
     * step through all nocaptures and delete those that are in the sequence
     * Note: we can't delete nocapture while "url_ptr" is pointing to it because
     * that would destroy "url_ptr". So we do a lazy deletion
     */
    URLToDelete = 0;
    gone = SI_FindValueInArgs(results, "|goneC|");
    urlNumber = 0;
    url_ptr = wallet_URL_list;
    while ((url = (wallet_MapElement *) XP_ListNextObject(url_ptr))) {
      if (url->item2->CharAt(NO_CAPTURE) == 'y') {
        if (SI_InSequence(gone, urlNumber)) {
          url->item2->SetCharAt('n', NO_CAPTURE);
          if (url->item2->CharAt(NO_PREVIEW) == 'n') {
            if (URLToDelete) {
                wallet_FreeURL(URLToDelete);
            }
            URLToDelete = url;
          }
        }
        urlNumber++;
      }
    }
    if (URLToDelete) {
        wallet_FreeURL(URLToDelete);
        wallet_WriteToFile("URL.tbl", wallet_URL_list, PR_FALSE);
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
  XP_List* URL_list = wallet_URL_list;
  XP_List* dummy;
  nsAutoString * value = new nsAutoString("nn");
  if (!url || !value) {
    return PR_FALSE;
  }
  if (wallet_ReadFromList(*url, *value, dummy, URL_list)) {
    if (value->CharAt(NO_CAPTURE) == 'y') {
      return PR_FALSE;
    }
  }

  /* ask user if we should capture the values on this form */
  char * message = Wallet_Localize("WantToCaptureForm?");
  char * checkMessage = Wallet_Localize("NeverSave");
  PRBool checkValue;
  PRBool result = Wallet_CheckConfirm(message, checkMessage, &checkValue);
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
    if (Wallet_BadKey()) {
      return;
    }
  }

  nsAutoString oldValue;

  /* is there a mapping from this field name to a schema name */
  nsAutoString schema(vcard);
  XP_List* FieldToSchema_list = wallet_FieldToSchema_list;
  XP_List* URLFieldToSchema_list = wallet_specificURLFieldToSchema_list;
  XP_List* SchemaToValue_list = wallet_SchemaToValue_list;
  XP_List* dummy;

  if (schema.Length() ||
      (wallet_ReadFromList(field, schema, dummy, URLFieldToSchema_list)) ||
      (wallet_ReadFromList(field, schema, dummy, FieldToSchema_list))) {

    /* field to schema mapping already exists */

    /* is this a new value for the schema */
    if (!(wallet_ReadFromList(schema, oldValue, dummy, SchemaToValue_list)) || 
        (oldValue != value)) {

      /* this is a new value so store it */
      nsAutoString * aValue = new nsAutoString(value);
      nsAutoString * aSchema = new nsAutoString(schema);
      dummy = 0;
      wallet_WriteToList(*aSchema, *aValue, dummy, wallet_SchemaToValue_list);
      wallet_WriteToFile("SchemaValue.tbl", wallet_SchemaToValue_list, PR_TRUE);
    }
  } else {

    /* no field to schema mapping so assume schema name is same as field name */

    /* is this a new value for the schema */
    if (!(wallet_ReadFromList(field, oldValue, dummy, SchemaToValue_list)) ||
        (oldValue != value)) {

      /* this is a new value so store it */
      nsAutoString * aField = new nsAutoString(field);
      nsAutoString * aValue = new nsAutoString(value);
      dummy = 0;
      wallet_WriteToList(*aField, *aValue, dummy, wallet_SchemaToValue_list);
      wallet_WriteToFile("SchemaValue.tbl", wallet_SchemaToValue_list, PR_TRUE);
    }
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
  char *buffer = (char*)XP_ALLOC(BUFLEN2);
  int g = 0, nopreviewNum;
  XP_List *url_ptr;
  wallet_MapElement *url;
  char* urlCString;

  wallet_InitializeURLList();
  buffer[0] = '\0';
  url_ptr = wallet_URL_list;
  nopreviewNum = 0;
  while ( (url=(wallet_MapElement *) XP_ListNextObject(url_ptr)) ) {
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
  char *buffer = (char*)XP_ALLOC(BUFLEN2);
  int g = 0, nocaptureNum;
  XP_List *url_ptr;
  wallet_MapElement *url;
  char* urlCString;

  wallet_InitializeURLList();
  buffer[0] = '\0';
  url_ptr = wallet_URL_list;
  nocaptureNum = 0;
  while ( (url=(wallet_MapElement *) XP_ListNextObject(url_ptr)) ) {
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
  if (Wallet_BadKey()) {
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
  wallet_ReadFromFile("SchemaValue.tbl", wallet_SchemaToValue_list, PR_TRUE);
  delete []walletListAsCString;
}

PUBLIC void
WLLT_PreEdit(nsAutoString& walletList) {
  wallet_Initialize();
  walletList = BREAK;
  XP_List * list_ptr;
  wallet_MapElement * ptr;
  list_ptr = wallet_SchemaToValue_list;

  while((ptr = (wallet_MapElement *) XP_ListNextObject(list_ptr))!=0) {
    walletList += *(ptr->item1) + BREAK;
    if (*ptr->item2 != "") {
      walletList += *(ptr->item2) + BREAK;
    } else {
      XP_List * list_ptr1;
      wallet_Sublist * ptr1;
      list_ptr1 = ptr->itemList;
      while((ptr1=(wallet_Sublist *) XP_ListNextObject(list_ptr1))!=0) {
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
    XP_List* URL_list = wallet_URL_list;
    XP_List* dummy;
    nsAutoString * value = new nsAutoString("nn");
    if (!url || !value) {
      wallet_ReadFromList(*url, *value, dummy, URL_list);
      value->SetCharAt('y', NO_PREVIEW);
      wallet_WriteToList(*url, *value, dummy, wallet_URL_list, DUP_OVERWRITE);
      wallet_WriteToFile("URL.tbl", wallet_URL_list, PR_FALSE);
    }
  }

  /* process the list, doing the fillins */
  XP_List * list;
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

  XP_List * list_ptr = list;
  wallet_PrefillElement * ptr;
  char * ptr2;
  ptr2 = fillins;
  /* step through pre-fill list */
  PRBool first = PR_TRUE;
  while((ptr = (wallet_PrefillElement *) XP_ListNextObject(list_ptr))!=0) {
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
      if (ptr->resume && (ptr->count == 0)) {
        wallet_MapElement * mapElement = (wallet_MapElement *) (ptr->resume->object);
        XP_ListRemoveObject(wallet_SchemaToValue_list, mapElement);
        wallet_WriteToList(
          *(mapElement->item1),
          *(mapElement->item2),
          mapElement->itemList, 
          wallet_SchemaToValue_list);
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
  XP_List *wallet_PrefillElement_list=XP_ListNew();
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
      nsresult result = doc->QueryInterface(kIDOMHTMLDocumentIID, (void**)&htmldoc);
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
                      XP_List * resume = nsnull;
                      wallet_PrefillElement * firstElement = nsnull;
                      PRUint32 numberOfElements = 0;
                      for (;;) {
                        /* loop to allow for multiple values */
                        /* first element in multiple-value group will have its count
                         * field set to the number of elements in group.  All other
                         * elements in group will have count field set to 0
                         */
                        prefillElement = XP_NEW(wallet_PrefillElement);
                        if (wallet_GetPrefills
                            (elementNode,
                            prefillElement->inputElement,
                            prefillElement->selectElement,
                            prefillElement->schema,
                            prefillElement->value,
                            prefillElement->selectIndex,
                            resume) != -1) {
                          /* another value found */
                          prefillElement->resume = resume;
                          if (nsnull == firstElement) {
                            firstElement = prefillElement;
                          }
                          numberOfElements++;
                          prefillElement->count = 0;
                          XP_ListAddObjectToEnd(wallet_PrefillElement_list, prefillElement);
                          if (nsnull == resume) {
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
  if (!wallet_PrefillElement_list || !XP_ListCount(wallet_PrefillElement_list)) {
    return NS_ERROR_FAILURE; // indicates to caller not to display preview screen
  }

  /* prefill each element using the list */

  /* determine if url is on list of urls that should not be previewed */
  PRBool noPreview = PR_FALSE;
  if (!quick) {
    wallet_InitializeURLList();
    XP_List* URL_list = wallet_URL_list;
    XP_List* dummy;
    nsAutoString * value = new nsAutoString("nn");
    nsAutoString * urlPtr = new nsAutoString(url);
    if (!value || !urlPtr) {
      wallet_ReadFromList(*urlPtr, *value, dummy, URL_list);
      noPreview = (value->CharAt(NO_PREVIEW) == 'y');
      delete value;
      delete urlPtr;
    }
  }

  /* determine if preview is necessary */
  if (noPreview || quick) {
    /* prefill each element without any preview for user verification */
    XP_List * list_ptr = wallet_PrefillElement_list;
    wallet_PrefillElement * ptr;
    while((ptr = (wallet_PrefillElement *) XP_ListNextObject(list_ptr))!=0) {
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
SI_RememberSignonDat2
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
      SI_RememberSignonDat2
        (URLName, (char**)name_array, (char**)value_array, (char**)type_array, value_cnt);

      /* save form if it meets all necessary conditions */
      if (wallet_GetFormsCapturingPref() && (count>=3) && wallet_OKToCapture(URLName)) {

        /* conditions all met, now save it */
        for (PRUint32 elementX = 0; elementX < numElements; elementX++) {
          nsIDOMNode* elementNode = nsnull;
          elements->Item(elementX, &elementNode);
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
