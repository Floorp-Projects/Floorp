/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
   wallet.cpp
*/

#define AutoCapture
//#define IgnoreFieldNames

#include "wallet.h"
#include "singsign.h"

#include "nsNetUtil.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"

#include "nsIServiceManager.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIURL.h"
#include "nsIDOMWindowCollection.h"
#include "nsIPrompt.h"
#include "nsIWindowWatcher.h"

#include "nsFileStream.h"
#include "nsAppDirectoryServiceDefs.h"

#include "nsIStringBundle.h"
#include "nsIFileSpec.h"
#include "prmem.h"
#include "prprf.h"  
#include "nsIContent.h"
#include "nsISecurityManagerComponent.h"

#include "nsIWalletService.h"

#ifdef DEBUG_morse
#define morseAssert NS_ASSERTION
#else
#define morseAssert(x,y) 0
#endif 

static NS_DEFINE_IID(kIDOMHTMLOptionElementIID, NS_IDOMHTMLOPTIONELEMENT_IID);

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


/***************************************************/
/* The following declarations define the data base */
/***************************************************/


enum PlacementType {DUP_IGNORE, DUP_OVERWRITE, DUP_BEFORE, DUP_AFTER, AT_END, BY_LENGTH};
#define LIST_COUNT(list)  ((list) ? (list)->Count() : 0)

MOZ_DECL_CTOR_COUNTER(wallet_Sublist)

class wallet_Sublist {
public:
  wallet_Sublist()
  {
    MOZ_COUNT_CTOR(wallet_Sublist);
  }
  ~wallet_Sublist()
  {
    MOZ_COUNT_DTOR(wallet_Sublist);
  }
  nsString item;
};

/*
 * The data structure below consists of mapping tables that map one item into another.  
 * The actual interpretation of the items depend on which table we are in.  For 
 * example, if in the field-to-schema table, item1 is a field name and item2 is a 
 * schema name.  Whereas in the schema-to-value table, item1 is a schema name and 
 * item2 is a value.  Therefore this generic data structure refers to them simply as 
 * item1 and item2.
 */
MOZ_DECL_CTOR_COUNTER(wallet_MapElement)

class wallet_MapElement {
public:
  wallet_MapElement() : itemList(nsnull)
  {
    MOZ_COUNT_CTOR(wallet_MapElement);
  }
  ~wallet_MapElement()
  {
    if (itemList) {
      PRInt32 count = LIST_COUNT(itemList);
      wallet_Sublist * sublistPtr;
      for (PRInt32 i=0; i<count; i++) {
        sublistPtr = NS_STATIC_CAST(wallet_Sublist*, itemList->ElementAt(i));
        delete sublistPtr;
      }
      delete itemList;
    }
    MOZ_COUNT_DTOR(wallet_MapElement);
  }
  nsString item1;
  nsString item2;
  nsVoidArray * itemList;
};

/* Purpose of this class is to speed up startup time on the mac
 *
 * These strings are used over and over again inside an inner loop.  Rather
 * then allocating them and then deallocating them, they will be allocated
 * only once and left sitting on the heap
 */

MOZ_DECL_CTOR_COUNTER(wallet_HelpMac)

class wallet_HelpMac {
public:
  wallet_HelpMac() {
    MOZ_COUNT_CTOR(wallet_HelpMac);
  }
  ~wallet_HelpMac() {
    MOZ_COUNT_DTOR(wallet_HelpMac);
  }
  nsAutoString item1;
  nsAutoString item2;
  nsAutoString item3;
  nsAutoString dummy;
};
wallet_HelpMac * helpMac;

PRIVATE nsVoidArray * wallet_FieldToSchema_list=0;
PRIVATE nsVoidArray * wallet_VcardToSchema_list=0;
PRIVATE nsVoidArray * wallet_SchemaToValue_list=0;
PRIVATE nsVoidArray * wallet_SchemaConcat_list=0;
PRIVATE nsVoidArray * wallet_SchemaStrings_list=0;
PRIVATE nsVoidArray * wallet_PositionalSchema_list=0;
PRIVATE nsVoidArray * wallet_StateSchema_list=0;
PRIVATE nsVoidArray * wallet_URL_list=0;
#ifdef AutoCapture
PRIVATE nsVoidArray * wallet_DistinguishedSchema_list=0;
#endif

#define NO_CAPTURE 0
#define NO_PREVIEW 1

MOZ_DECL_CTOR_COUNTER(wallet_PrefillElement)

class wallet_PrefillElement {
public:
  wallet_PrefillElement() : inputElement(nsnull), selectElement(nsnull)
  {
    MOZ_COUNT_CTOR(wallet_PrefillElement);
  }
  ~wallet_PrefillElement()
  {
    NS_IF_RELEASE(inputElement);
    NS_IF_RELEASE(selectElement);
    MOZ_COUNT_DTOR(wallet_PrefillElement);
  }
  nsIDOMHTMLInputElement* inputElement;
  nsIDOMHTMLSelectElement* selectElement;
  nsString schema;
  nsString value;
  PRInt32 selectIndex;
  PRUint32 count;
};

nsIURI * wallet_lastUrl = NULL;

/***********************************************************/
/* The following routines are for diagnostic purposes only */
/***********************************************************/

#ifdef DEBUG_morse

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
  wallet_MapElement * mapElementPtr;
  char item1[100];
  char item2[100];
  char item[100];
  PRInt32 count = LIST_COUNT(list);
  for (PRInt32 i=0; i<count; i++) {
    mapElementPtr = NS_STATIC_CAST(wallet_MapElement*, list->ElementAt(i));
    mapElementPtr->item1.ToCString(item1, sizeof(item1));
    mapElementPtr->item2.ToCString(item2, sizeof(item2));
    fprintf(stdout, "%s %s \n", item1, item2);
    wallet_Sublist * sublistPtr;
    PRInt32 count2 = LIST_COUNT(mapElementPtr->itemList);
    for (PRInt32 i2=0; i2<count2; i2++) {
      sublistPtr = NS_STATIC_CAST(wallet_Sublist*, mapElementPtr->itemList->ElementAt(i2));
      sublistPtr->item.ToCString(item, sizeof(item));
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
#endif /* DEBUG_morse */


/*************************************************************************/
/* The following routines are used for accessing strings to be localized */
/*************************************************************************/

#define PROPERTIES_URL "chrome://communicator/locale/wallet/wallet.properties"

PUBLIC PRUnichar *
Wallet_Localize(char* genericString) {
  nsresult ret;
  nsAutoString v;

  /* create a bundle for the localization */
  nsCOMPtr<nsIStringBundleService> pStringService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &ret);
  if (NS_FAILED(ret)) {
#ifdef DEBUG
    printf("cannot get string service\n");
#endif
    return ToNewUnicode(v);
  }
  nsCOMPtr<nsIStringBundle> bundle;
  ret = pStringService->CreateBundle(PROPERTIES_URL, getter_AddRefs(bundle));
  if (NS_FAILED(ret)) {
#ifdef DEBUG
    printf("cannot create instance\n");
#endif
    return ToNewUnicode(v);
  }

  /* localize the given string */
  nsAutoString   strtmp; strtmp.AssignWithConversion(genericString);
  const PRUnichar *ptrtmp = strtmp.get();
  PRUnichar *ptrv = nsnull;
  ret = bundle->GetStringFromName(ptrtmp, &ptrv);
  if (NS_FAILED(ret)) {
#ifdef DEBUG
    printf("cannot get string from name\n");
#endif
    return ToNewUnicode(v);
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

  return ToNewUnicode(v);
}

/**********************/
/* Modal dialog boxes */
/**********************/

PUBLIC PRBool
Wallet_Confirm(PRUnichar * szMessage, nsIDOMWindowInternal* window)
{
  PRBool retval = PR_TRUE; /* default value */

  nsresult res;  
  nsCOMPtr<nsIPrompt> dialog; 
  window->GetPrompter(getter_AddRefs(dialog)); 
  if (!dialog) {
    return retval;
  } 

  const nsAutoString message( szMessage );
  retval = PR_FALSE; /* in case user exits dialog by clicking X */
  res = dialog->Confirm(nsnull, message.get(), &retval);
  return retval;
}

PUBLIC PRBool
Wallet_ConfirmYN(PRUnichar * szMessage, nsIDOMWindowInternal* window) {
  nsresult res;  
  nsCOMPtr<nsIPrompt> dialog; 
  window->GetPrompter(getter_AddRefs(dialog)); 
  if (!dialog) {
    return PR_FALSE;
  } 

  PRInt32 buttonPressed = 1; /* in case user exits dialog by clickin X */
  PRUnichar * confirm_string = Wallet_Localize("Confirm");

  res = dialog->ConfirmEx(confirm_string, szMessage,
                          (nsIPrompt::BUTTON_TITLE_YES * nsIPrompt::BUTTON_POS_0) +
                          (nsIPrompt::BUTTON_TITLE_NO * nsIPrompt::BUTTON_POS_1),
                          nsnull, nsnull, nsnull, nsnull, nsnull, &buttonPressed);

  Recycle(confirm_string);
  return (buttonPressed == 0);
}

PUBLIC PRInt32
Wallet_3ButtonConfirm(PRUnichar * szMessage, nsIDOMWindowInternal* window)
{
  nsresult res;  
  nsCOMPtr<nsIPrompt> dialog; 
  window->GetPrompter(getter_AddRefs(dialog)); 
  if (!dialog) {
    return 0; /* default value is NO */
  } 

  PRInt32 buttonPressed = 1; /* default of NO if user exits dialog by clickin X */
  PRUnichar * never_string = Wallet_Localize("Never");
  PRUnichar * confirm_string = Wallet_Localize("Confirm");

  res = dialog->ConfirmEx(confirm_string, szMessage,
                          (nsIPrompt::BUTTON_TITLE_YES * nsIPrompt::BUTTON_POS_0) +
                          (nsIPrompt::BUTTON_TITLE_NO * nsIPrompt::BUTTON_POS_1) +
                          (nsIPrompt::BUTTON_TITLE_IS_STRING * nsIPrompt::BUTTON_POS_2),
                          nsnull, nsnull, never_string, nsnull, nsnull, &buttonPressed);

  Recycle(never_string);
  Recycle(confirm_string);

  return buttonPressed;
}

PRIVATE void
wallet_Alert(PRUnichar * szMessage, nsIDOMWindowInternal* window)
{
  nsresult res;
  nsCOMPtr<nsIPrompt> dialog; 
  window->GetPrompter(getter_AddRefs(dialog)); 
  if (!dialog) {
    return;     // XXX should return the error
  } 

  const nsAutoString message( szMessage );
  PRUnichar * title = Wallet_Localize("CaveatTitle");
  res = dialog->Alert(title, message.get());
  Recycle(title);
  return;     // XXX should return the error
}

PRIVATE void
wallet_Alert(PRUnichar * szMessage, nsIPrompt* dialog)
{
  nsresult res;  
  const nsAutoString message( szMessage );
  PRUnichar * title = Wallet_Localize("CaveatTitle");
  res = dialog->Alert(title, message.get());
  Recycle(title);
  return;     // XXX should return the error
}

PUBLIC PRBool
Wallet_CheckConfirmYN
    (PRUnichar * szMessage, PRUnichar * szCheckMessage, PRBool* checkValue,
     nsIDOMWindowInternal* window) {
  nsresult res;
  nsCOMPtr<nsIPrompt> dialog; 
  window->GetPrompter(getter_AddRefs(dialog)); 
  if (!dialog) {
    return PR_FALSE;
  } 

  PRInt32 buttonPressed = 1; /* in case user exits dialog by clickin X */
  PRUnichar * confirm_string = Wallet_Localize("Confirm");

  res = dialog->ConfirmEx(confirm_string, szMessage,
                          (nsIPrompt::BUTTON_TITLE_YES * nsIPrompt::BUTTON_POS_0) +
                          (nsIPrompt::BUTTON_TITLE_NO * nsIPrompt::BUTTON_POS_1),
                          nsnull, nsnull, nsnull, szCheckMessage, checkValue, &buttonPressed);

  if (NS_FAILED(res)) {
    *checkValue = 0;
  }
  if (*checkValue!=0 && *checkValue!=1) {
    NS_ASSERTION(PR_FALSE, "Bad result from checkbox");
    *checkValue = 0; /* this should never happen but it is happening!!! */
  }
  Recycle(confirm_string);
  return (buttonPressed == 0);
}


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
      = do_CreateInstance("@mozilla.org/security/sdr;1", &rv);
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
  PRUint32 PREFIX_len = sizeof (PREFIX) - 1;
  PRUint32 crypt0_len = PL_strlen(crypt0);
  crypt = (char *)PR_Malloc(PREFIX_len + crypt0_len + 1);
  PRUint32 i;
  for (i=0; i<PREFIX_len; i++) {
    crypt[i] = PREFIX[i];
  }
  for (i=0; i<crypt0_len; i++) {
    crypt[PREFIX_len+i] = crypt0[i];
  }
  crypt[PREFIX_len + crypt0_len] = '\0';
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

  PRUint32 PREFIX_len = sizeof(PREFIX) - 1;
  if (PL_strlen(crypt) == PREFIX_len) {
    text = (char *)PR_Malloc(1);
    text[0] = '\0';
    return NS_OK;
  }
  text = PL_Base64Decode(&crypt[PREFIX_len], 0, NULL);
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
  char * UTF8textCString = ToNewCString(UTF8text);
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
  char * cryptCString = ToNewCString(crypt);
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
  
  PRUint32 UTF8textCString_len = PL_strlen(UTF8textCString);
  for (PRUint32 i=0; i<UTF8textCString_len; ) {
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
  if (*list == wallet_SchemaToValue_list || *list == wallet_URL_list) {
    /* the other lists were allocated in blocks and need to be deallocated the same way */
    wallet_MapElement * mapElementPtr;
    PRInt32 count = LIST_COUNT((*list));
    for (PRInt32 i=count-1; i>=0; i--) {
      mapElementPtr = NS_STATIC_CAST(wallet_MapElement*, (*list)->ElementAt(i));
      delete mapElementPtr;
    }
  }
  delete (*list);
  *list = 0;
}

/*
 * allocate another mapElement
 * We are going to buffer up allocations because it was found that alocating one
 * element at a time was very inefficient on the mac
 */

PRIVATE nsVoidArray * wallet_MapElementAllocations_list=0;
const PRInt32 kAllocBlockElems = 500;
static PRInt32 wallet_NextAllocSlot = kAllocBlockElems;

static wallet_MapElement *
wallet_AllocateMapElement() {
  static wallet_MapElement* mapElementTable;
  if (wallet_NextAllocSlot >= kAllocBlockElems) {
    mapElementTable = new wallet_MapElement[kAllocBlockElems];
    if (!mapElementTable) {
      return nsnull;
    }
    if(!wallet_MapElementAllocations_list) {
      wallet_MapElementAllocations_list = new nsVoidArray();
    }
    if(wallet_MapElementAllocations_list) {
      wallet_MapElementAllocations_list->AppendElement(mapElementTable);
    }
    wallet_NextAllocSlot = 0;
  }
  return &mapElementTable[wallet_NextAllocSlot++];
}

static void
wallet_DeallocateMapElements() {
  wallet_MapElement * mapElementPtr;
  PRInt32 count = LIST_COUNT(wallet_MapElementAllocations_list);
  for (PRInt32 i=count-1; i>=0; i--) {
    mapElementPtr =
      NS_STATIC_CAST(wallet_MapElement*, (wallet_MapElementAllocations_list)->ElementAt(i));
    delete [] mapElementPtr;
  }  
  delete wallet_MapElementAllocations_list;
  wallet_MapElementAllocations_list = 0;
  wallet_NextAllocSlot = kAllocBlockElems;
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

  wallet_MapElement * mapElementPtr;
  PRBool added_to_list = PR_FALSE;

  wallet_MapElement * mapElement;
  if (list == wallet_FieldToSchema_list || list == wallet_SchemaStrings_list ||
      list == wallet_PositionalSchema_list || list == wallet_StateSchema_list ||
      list == wallet_SchemaConcat_list  || list == wallet_DistinguishedSchema_list ||
      list == wallet_VcardToSchema_list) {
    mapElement = wallet_AllocateMapElement();
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
    mapElementPtr = NS_STATIC_CAST(wallet_MapElement*, list->ElementAt(i));
    if (BY_LENGTH==placement) {
      if (LIST_COUNT(mapElementPtr->itemList) < LIST_COUNT(itemList)) {
        list->InsertElementAt(mapElement, i);
        added_to_list = PR_TRUE;
        break;
      } else if (LIST_COUNT(mapElementPtr->itemList) == LIST_COUNT(itemList)) {
        if (itemList) {
          wallet_Sublist * sublistPtr;
          wallet_Sublist * sublistPtr2;
          sublistPtr = NS_STATIC_CAST(wallet_Sublist*, mapElementPtr->itemList->ElementAt(0));
          sublistPtr2 = NS_STATIC_CAST(wallet_Sublist*, itemList->ElementAt(0));
          if(sublistPtr->item.Length() < sublistPtr2->item.Length()) {
            list->InsertElementAt(mapElement, i);
            added_to_list = PR_TRUE;
            break;
          }
        } else if (mapElementPtr->item2.Length() < item2.Length()) {
          list->InsertElementAt(mapElement, i);
          added_to_list = PR_TRUE;
          break;
        }
      }
    } else if(mapElementPtr->item1.Equals(item1)) {
      if (DUP_OVERWRITE==placement) {
        delete mapElement;
        mapElementPtr->item1 = item1;
        mapElementPtr->item2 = item2;
        mapElementPtr->itemList = itemList;
      } else if (DUP_BEFORE==placement) {
        list->InsertElementAt(mapElement, i);
      }
      if (DUP_AFTER!=placement) {
        added_to_list = PR_TRUE;
        break;
      }
    } else if(Compare(mapElementPtr->item1,item1)>=0) {
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
  wallet_MapElement * mapElementPtr;
  item1.ToLowerCase();
  PRInt32 count = LIST_COUNT(list);
  for (PRInt32 i=index; i<count; i++) {
    mapElementPtr = NS_STATIC_CAST(wallet_MapElement*, list->ElementAt(i));
    if(mapElementPtr->item1.Equals(item1)) {
      if (obscure) {
        if (NS_FAILED(Wallet_Decrypt(mapElementPtr->item2, item2))) {
          return PR_FALSE;
        }
      } else {
        item2 = nsAutoString(mapElementPtr->item2);
      }
      itemList = mapElementPtr->itemList;
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
const char allFileName[] = "wallet.tbl";
const char fieldSchemaFileName[] = "FieldSchema.tbl";
const char vcardSchemaFileName[] = "VcardSchema.tbl";
const char schemaConcatFileName[] = "SchemaConcat.tbl";
const char schemaStringsFileName[] = "SchemaStrings.tbl";
const char positionalSchemaFileName[] = "PositionalSchema.tbl";
const char stateSchemaFileName[] = "StateSchema.tbl";
#ifdef AutoCapture
const char distinguishedSchemaFileName[] = "DistinguishedSchema.tbl";
#endif


/******************************************************/
/* The following routines are for accessing the files */
/******************************************************/

PUBLIC nsresult Wallet_ProfileDirectory(nsFileSpec& dirSpec) {
  /* return the profile */
  
  nsresult res;
  nsCOMPtr<nsIFile> aFile;
  nsXPIDLCString pathBuf;
  nsCOMPtr<nsIFileSpec> tempSpec;
  
  res = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(aFile));
  if (NS_FAILED(res)) return res;
  res = aFile->GetPath(getter_Copies(pathBuf));
  if (NS_FAILED(res)) return res;
  
  // TODO: Change the function to return an nsIFile
  // and not do this conversion
  res = NS_NewFileSpec(getter_AddRefs(tempSpec));
  if (NS_FAILED(res)) return res;
  res = tempSpec->SetNativePath(pathBuf);
  if (NS_FAILED(res)) return res;
  res = tempSpec->GetFileSpec(&dirSpec);
  
  return res;
}

PUBLIC nsresult Wallet_DefaultsDirectory(nsFileSpec& dirSpec) {

  nsresult res;
  nsCOMPtr<nsIFile> aFile;
  nsXPIDLCString pathBuf;
  nsCOMPtr<nsIFileSpec> tempSpec;
  
  res = NS_GetSpecialDirectory(NS_APP_DEFAULTS_50_DIR, getter_AddRefs(aFile));
  if (NS_FAILED(res)) return res;
  res = aFile->Append("wallet");
  if (NS_FAILED(res)) return res;
  res = aFile->GetPath(getter_Copies(pathBuf));
  if (NS_FAILED(res)) return res;
  
  // TODO: Change the function to return an nsIFile
  // and not do this conversion
  res = NS_NewFileSpec(getter_AddRefs(tempSpec));
  if (NS_FAILED(res)) return res;
  res = tempSpec->SetNativePath(pathBuf);
  if (NS_FAILED(res)) return res;
  res = tempSpec->GetFileSpec(&dirSpec);
  
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
  wallet_MapElement * mapElementPtr;

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
    mapElementPtr = NS_STATIC_CAST(wallet_MapElement*, list->ElementAt(i));
    wallet_PutLine(strm, (*mapElementPtr).item1);
    if (!(*mapElementPtr).item2.IsEmpty()) {
      wallet_PutLine(strm, (*mapElementPtr).item2);
    } else {
      wallet_Sublist * sublistPtr;
      PRInt32 count2 = LIST_COUNT(mapElementPtr->itemList);
      for (PRInt32 j=0; j<count2; j++) {
        sublistPtr = NS_STATIC_CAST(wallet_Sublist*, mapElementPtr->itemList->ElementAt(j));
        wallet_PutLine(strm, (*sublistPtr).item);
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
  if (localFile) {
    rv = Wallet_ProfileDirectory(dirSpec);
  } else {
    rv = Wallet_DefaultsDirectory(dirSpec);
  }
  if (NS_FAILED(rv)) {
    return;
  }
  nsInputFileStream strm(dirSpec + filename);
  if (!strm.is_open()) {
    return;
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
    if (NS_FAILED(wallet_GetLine(strm, helpMac->item1))) {
      /* end of file reached */
      break;
    }

#ifdef AutoCapture
    /* Distinguished schema list is a list of single entries, not name/value pairs */
    if (PL_strcmp(filename, distinguishedSchemaFileName) == 0) {
      nsVoidArray* dummy = NULL;
      wallet_WriteToList(helpMac->item1, helpMac->item1, dummy, list, PR_FALSE, placement);
      continue;
    }
#endif

    if (NS_FAILED(wallet_GetLine(strm, helpMac->item2))) {
      /* unexpected end of file reached */
      break;
    }
    if (helpMac->item2.Length()==0) {
      /* the value must have been deleted */
      nsVoidArray* dummy = NULL;
      wallet_WriteToList(helpMac->item1, helpMac->item2, dummy, list, PR_FALSE, placement);
      continue;
    }

    if (NS_FAILED(wallet_GetLine(strm, helpMac->item3))) {
      /* end of file reached */
      nsVoidArray* dummy = NULL;
      wallet_WriteToList(helpMac->item1, helpMac->item2, dummy, list, PR_FALSE, placement);
      strm.close();
      return;
    }

    if (helpMac->item3.Length()==0) {
      /* just a pair of values, no need for a sublist */
      nsVoidArray* dummy = NULL;
      wallet_WriteToList(helpMac->item1, helpMac->item2, dummy, list, PR_FALSE, placement);
    } else {
      /* need to create a sublist and put item2 and item3 onto it */

      // don't we leak itemList here?

      nsVoidArray * itemList = new nsVoidArray();
      if (!itemList) {
        break;
      }

      // Don't we leak sublist too?
      wallet_Sublist * sublist = new wallet_Sublist;
      if (!sublist) {
        break;
      }
      sublist->item = helpMac->item2;
      itemList->AppendElement(sublist);
      sublist = new wallet_Sublist;
      if (!sublist) {
        break;
      }
      sublist->item = helpMac->item3;
      itemList->AppendElement(sublist);
      /* add any following items to sublist up to next blank line */
      helpMac->dummy.Truncate(0);
      for (;;) {
        /* get next item for sublist */
        if (NS_FAILED(wallet_GetLine(strm, helpMac->item3))) {
          /* end of file reached */
          wallet_WriteToList(helpMac->item1, helpMac->dummy, itemList, list, PR_FALSE, placement);
          strm.close();
          return;
        }
        if (helpMac->item3.Length()==0) {
          /* blank line reached indicating end of sublist */
          wallet_WriteToList(helpMac->item1, helpMac->dummy, itemList, list, PR_FALSE, placement);
          break;
        }
        /* add item to sublist */
        sublist = new wallet_Sublist;
        if (!sublist) {
          break;
        }
        sublist->item = helpMac->item3;
        itemList->AppendElement(sublist);
      }
    }
  }
  strm.close();
}


/*********************************************************************/
/* The following are utility routines for the main wallet processing */
/*********************************************************************/

PUBLIC void
Wallet_GiveCaveat(nsIDOMWindowInternal* window, nsIPrompt* dialog) {
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
    if (nsCRT::IsAsciiAlpha(c) || nsCRT::IsAsciiDigit(c) || c>'~') {
      stripText += c;
    }
  }
  return stripText;
}

/*
 * given a displayable text, get the schema
 */
static void TextToSchema(
    const nsString& text,
    nsString& schema)
{
  /* return if no SchemaStrings list exists */
  if (!wallet_SchemaStrings_list) {
    return;
  }

  /* try each schema entry in schemastring table to see if it's acceptable */
  wallet_MapElement * mapElementPtr;
  PRInt32 count = LIST_COUNT(wallet_SchemaStrings_list);
  for (PRInt32 i=0; i<count; i++) {

    /* get each string associated with this schema */
    PRBool isSubstring = PR_TRUE;
    mapElementPtr = NS_STATIC_CAST(wallet_MapElement*, wallet_SchemaStrings_list->ElementAt(i));
    wallet_Sublist * sublistPtr;
    PRInt32 count2 = LIST_COUNT(mapElementPtr->itemList);

    if (count2) {
      for (PRInt32 i2=0; i2<count2; i2++) {

        /* see if displayable text contains this string */
        sublistPtr = NS_STATIC_CAST(wallet_Sublist*, mapElementPtr->itemList->ElementAt(i2));
        if (text.Find(sublistPtr->item, PR_TRUE) == -1) {
 
          /* displayable text does not contain this string, reject this schema */
          isSubstring = PR_FALSE;
          break;
        }
      }
    } else if (text.Find(mapElementPtr->item2, PR_TRUE) == -1) {
 
      /* displayable text does not contain this string, reject this schema */
      isSubstring = PR_FALSE;
    }

    if (isSubstring) {

      /* all strings were contained in the displayable text, accept this schema */
      schema = mapElementPtr->item1;
      return;
    }
  }
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

  /* return if no SchemaToValue list exists or if all values previous used */
  if (!wallet_SchemaToValue_list || index == -1) {
    return -1;
  }

  /* if no schema name is given, fetch schema name from field/schema tables */
  nsVoidArray* dummy;
  nsString stripField;
  if ((schema.Length() > 0) ||
      wallet_ReadFromList(Strip(field, stripField), schema, dummy, wallet_FieldToSchema_list, PR_FALSE)) {

    /* schema name found, now attempt to fetch value from schema/value table */ 
    PRInt32 index2 = index;
    if ((index >= 0) &&
        wallet_ReadFromList(schema, value, itemList, wallet_SchemaToValue_list, PR_TRUE, index2)) {
      /* value found, prefill it into form and return */
      index = index2;
      return 0;

    } else {

      /* value not found, see if concatenation rule exists */
      nsVoidArray * itemList2;
      nsAutoString dummy2;
      if (index > 0) {
        index = 0;
      }
      PRInt32 index0 = index;
      PRInt32 index00 = index;
      PRInt32 index4 = 0;
      while (wallet_ReadFromList(schema, dummy2, itemList2, wallet_SchemaConcat_list, PR_FALSE, index4)) {

        /* concatenation rules exist, generate value as a concatenation */
        wallet_Sublist * sublistPtr;
        value.SetLength(0);
        nsAutoString value2;
        PRInt32 index00max = index0;

        if (dummy2.Length() > 0) {

          /* single item on rhs of concatenation rule */
          PRInt32 index5 = 0;
          PRInt32 j;
          PRBool failed = PR_FALSE;
          for (j=0; j>index0; j -= 2) {
            if (!wallet_ReadFromList(dummy2, value2, dummy, wallet_SchemaToValue_list, PR_TRUE, index5)) {
              failed = PR_TRUE;
              break;
            }
            index00 += 2;
          }

          if (!failed && wallet_ReadFromList(dummy2, value2, dummy, wallet_SchemaToValue_list, PR_TRUE, index5)) {

            /* found an unused value for the single rhs item */
            value += value2;
            index00 += 2;
          }
          index00max = index00;
        }

        /* process each item in a multi-rhs rule */
        PRInt32 count = LIST_COUNT(itemList2);
        for (PRInt32 i=0; i<count; i++) {
          sublistPtr = NS_STATIC_CAST(wallet_Sublist*, itemList2->ElementAt(i));

          /* skip over values found previously */
          /*   note: a returned index of -1 means not-found.  So we will use the
           *   negative even numbers (-2, -4, -6) to designate found as a concatenation
           *   where -2 means first value of each concatenation, -4 means second value, etc.
           */
          index00 = index0;
          PRInt32 index3 = 0;
          PRBool failed = PR_FALSE;
          for (PRInt32 j=0; j>index0; j -= 2) {
            if (!wallet_ReadFromList(sublistPtr->item, value2, dummy, wallet_SchemaToValue_list, PR_TRUE, index3)) {

              /* all values of next multi-rhs item were used previously */
              failed = PR_TRUE;
              break;
            }
            index00 += 2;
          }

          if (!failed && wallet_ReadFromList(sublistPtr->item, value2, dummy, wallet_SchemaToValue_list, PR_TRUE, index3)) {
            if (value.Length()>0) {
              value.Append(NS_LITERAL_STRING(" "));
            }

            /* found an unused value for the multi-rhs item */
            value += value2;
            index00 += 2;
          }
          if (index00 > index00max) {
            index00max = index00;
          }
        }

        itemList = nsnull;
        if (value.Length()>0) {

          /* a new value was found */
          index -= 2;
          return 0;
        }

        /* all values from this concat rule were used, go on to next concat rule */
        index0 = index00max;
      }

      /* no more concat rules, indicate failure */
      index = -1;
      return -1;
    }
  } else {
    /* schema name not found, use field name as schema name and fetch value */
    PRInt32 index2 = index;

    nsAutoString temp;
    wallet_GetHostFile(wallet_lastUrl, temp);
    temp.Append(NS_LITERAL_STRING(":"));
    temp.Append(field);

    if (wallet_ReadFromList(temp, value, itemList, wallet_SchemaToValue_list, PR_TRUE, index2)) {
      /* value found, prefill it into form */
      schema = temp;
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
  PRUint32 length;
  selectElement->GetLength(&length);
  nsCOMPtr<nsIDOMHTMLCollection> options;
  selectElement->GetOptions(getter_AddRefs(options));
  if (options) {
    PRUint32 numOptions;
    options->GetLength(&numOptions);
    for (PRUint32 optionX = 0; optionX < numOptions; optionX++) {
      nsCOMPtr<nsIDOMNode> optionNode;
      options->Item(optionX, getter_AddRefs(optionNode));

      if (optionNode) {
        nsCOMPtr<nsIDOMHTMLOptionElement> optionElement(do_QueryInterface(optionNode));

        if (optionElement) {
          nsAutoString optionValue;
          nsAutoString optionText;
          optionElement->GetValue(optionValue);
          optionElement->GetText(optionText);
          nsAutoString valueLC( value );
          valueLC.ToLowerCase();
          optionValue.ToLowerCase();
          optionText.ToLowerCase();
          optionText.Trim(" \n\t\r");
          if (valueLC==optionValue || valueLC==optionText) {
            index = optionX;
            return 0;
          }
        }
      }
    }
  }
  return -1;
}

void
wallet_StepForwardOrBack
    (nsIDOMNode*& elementNode, nsString& text, PRBool& atInputOrSelect, PRBool& atEnd, PRBool goForward) {
  nsresult result;
  atInputOrSelect = PR_FALSE;
  atEnd = PR_FALSE;

 /* try getting next/previous sibling */
  nsCOMPtr<nsIDOMNode> sibling;
  if (goForward) {
    result = elementNode->GetNextSibling(getter_AddRefs(sibling));
  } else {
    result = elementNode->GetPreviousSibling(getter_AddRefs(sibling));
  }
  if ((NS_FAILED(result)) || !sibling) {
    /* no next/previous siblings, try getting parent */
    nsCOMPtr<nsIDOMNode> parent;
    result = elementNode->GetParentNode(getter_AddRefs(parent));
    if ((NS_FAILED(result)) || !parent) {
      /* no parent, we've reached the top of the tree */
      atEnd = PR_TRUE;
    } else {
      /* parent obtained */
      elementNode = parent;
    }
    return;
  }
  /* sibling obtained */
  elementNode = sibling;

  while (PR_TRUE) {

    /* if we've reached a SELECT or non-hidden INPUT tag, we're done */
    /*
     *    There is a subtle difference here between going forward and going backwards.
     *
     *    When going forward we are trying to find out how many consecutive <input> elements are not separated
     *    by displayed text.  That is important for determing, for example, if we have a three-input phone-number
     *    field.  In that case, we want to consider only input tags have type="text" or no type ("text" by default).
     *
     *    When going backwards we want to find the text between the current <input> element and any preceding
     *    visible <input> element.  That would include such things as type="button", type="submit" etc.  The
     *    only thing it would exclude is type="hidden".
     */
    nsCOMPtr<nsIDOMHTMLInputElement> inputElement(do_QueryInterface(elementNode, &result));
    if ((NS_SUCCEEDED(result)) && (inputElement)) {
      nsAutoString type;
      result = inputElement->GetType(type);
      if (goForward) {
        if (NS_SUCCEEDED(result) &&
            (type.IsEmpty() ||
             (Compare(type, NS_LITERAL_STRING("text"), 
                      nsCaseInsensitiveStringComparator()) == 0))) {
          /* at <input> element and it's type is either "text" or is missing ("text" by default) */
          atInputOrSelect = PR_TRUE;
          return;
        }
      } else {
        if (NS_SUCCEEDED(result) && (type.CompareWithConversion("hidden", PR_TRUE) != 0)) {
          /* at <input> element and it's type is not "hidden" */
          atInputOrSelect = PR_TRUE;
          return;
        }
      }
    } else {
      nsCOMPtr<nsIDOMHTMLSelectElement> selectElement(do_QueryInterface(elementNode));

      if (selectElement) {
        atInputOrSelect = PR_TRUE;
        return;
      }
    }

    /* if we've reached a #text node, append it to accumulated text */
    nsAutoString siblingName;
    result = elementNode->GetNodeName(siblingName);
    nsCAutoString siblingCName; siblingCName.AssignWithConversion(siblingName);
//    if (siblingName.EqualsIgnoreCase(NS_LITERAL_STRING("#text")) {
    if (siblingCName.EqualsIgnoreCase("#text")) {
      nsAutoString siblingValue;
      result = elementNode->GetNodeValue(siblingValue);
      text.Append(siblingValue);      
    }

    /* if we've reached a SCRIPT node, don't fetch its siblings */
//    if (siblingName.EqualsIgnoreCase(NS_LITERAL_STRING("SCRIPT")) {
    if (siblingCName.EqualsIgnoreCase("SCRIPT")) {
      return;
    }

    /* try getting first/last child */
    nsCOMPtr<nsIDOMNode> child;
    if (goForward) {
      result = elementNode->GetFirstChild(getter_AddRefs(child));
    } else {
      result = elementNode->GetLastChild(getter_AddRefs(child));
    }
    if ((NS_FAILED(result)) || !child) {
      /* no children, we're done with this node */
      return;
    }
    /* child obtained */
    elementNode = child;
  }

  return;
}

//#include "nsIUGenCategory.h"
//#include "nsUnicharUtilCIID.h"
//static NS_DEFINE_IID(kUnicharUtilCID, NS_UNICHARUTIL_CID);
//static NS_DEFINE_IID(kIUGenCategoryIID, NS_IUGENCATEGORY_IID);

//#include "nsICaseConversion.h"
//static NS_DEFINE_IID(kICaseConversionIID, NS_ICASECONVERSION_IID);
//static nsICaseConversion* gCaseConv =  nsnull;

static void
wallet_ResolvePositionalSchema(nsIDOMNode* elementNode, nsString& schema) {
  static PRInt32 numerator = 0;
  static PRInt32 denominator = 0;
  static nsString lastPositionalSchema;

  /* return if no PositionalSchema list exists */
  if (!wallet_PositionalSchema_list) {
    schema.SetLength(0);
    return;
  }

  if (schema.Length()) {
    numerator = 0;
    denominator = 0;
    lastPositionalSchema = schema;
  } else if (numerator < denominator) {
    schema = lastPositionalSchema;
  } else {
    schema.SetLength(0);
    return;
  }

  /* search PositionalSchema list for our positional schema */
  wallet_MapElement * mapElementPtr;
  PRInt32 count = LIST_COUNT(wallet_PositionalSchema_list);
  for (PRInt32 i=0; i<count; i++) {
    mapElementPtr = NS_STATIC_CAST(wallet_MapElement*, wallet_PositionalSchema_list->ElementAt(i));
    if (mapElementPtr->item1.EqualsIgnoreCase(schema)) {
      /* found our positional schema in the list */

      /* A "position set" is a set of continuous <input> or <select> fields
       * with no displayable text between them.  For example: zipcode [     ]-[    ].
       * We need to determine how many elements are in the current set (denominator)
       * and which of those elements we are currently up to (numerator).  From that
       * we can identify our position with the fraction x/y meaning the xth element
       * out of a set of y.  We use that fraction when consulting the positionalSchema list
       * to determine which schema should be used.
       *
       * So for example, the positionalSchema list for %zip might be:
       *
       *    1/1  Home.PostalCode
       *    1/2  Home.PostalCode.Prefix
       *    2/2  Home.PostalCode.Suffix
       *
       * The positionalSchema list also contains fractions with no denominators, for example x/.
       * That means the xth element out of a set of any length.  These entries come last in
       * the positionalSchema list so they can match only if no match for a specific length is
       * found.  As an example, the positionalSchema list for %phone might be:
       *
       *    1/1 Home.Phone
       *    1/2 Home.Phone.LocCode
       *    2/2 Home.Phone.Number
       *    1/  Home.Phone.LocCode
       *    2/  Home.Phone.Number.Prefix
       *    3/  Home.Phone.Number.Suffix
       */

      if (numerator < denominator) {

        /* this is a continuation of previous position set */
        numerator++;

      } else {

        /* start a new position set */
        numerator = 1; /* start with first element */

        /* determine how many elements in current position set (denominator) */
        denominator = 1; /* assume that's the only element */
        PRBool atInputOrSelect = PR_FALSE;
        PRBool charFound = PR_FALSE;
        while (!charFound) {
          nsAutoString text;
          PRBool atEnd;
          wallet_StepForwardOrBack
            (elementNode, text, atInputOrSelect, atEnd, PR_TRUE); /* step forward */
          if (atEnd) {
            break;
          }
          PRUnichar c;
          for (PRUint32 j=0; j<text.Length(); j++) {
            c = text.CharAt(j);

            /* break out if an alphanumeric character is found */

//          nsresult res = nsServiceManager::GetService(kUnicharUtilCID, kICaseConversionIID,
//                                      (nsISupports**)&gCaseConv);
//
//          nsIUGenCategory* intl =  nsnull;
//          nsresult rv = nsServiceManager::GetService(kUnicharUtilCID, kIUGenCategoryIID,
//                                      (nsISupports**)&intl);
//          Whaaaaaa, intl is never released here!
//          if (NS_SUCCEEDED(rv) && intl) {
//            PRBool accept;
//            rv = intl->Is(c, intl->kUGenCategory_Number, &accept);
//            if (NS_FAILED(rv) || !accept) {
//              rv = intl->Is(c, intl->kUGenCategory_Letter, &accept);
//            }
//            if (NS_OK(rv) && accept) {
//              charFound = PR_TRUE;
//              break;
//            }
//          } else {
//            /* failed to get the i18n interfaces, so just treat latin characters */
              if (nsCRT::IsAsciiAlpha(c) || nsCRT::IsAsciiDigit(c)) {
                charFound = PR_TRUE;
                break;
//            }
            }
          }
          if (!charFound && atInputOrSelect) {
            /* add one more element to position set */
            denominator++;
          }
        }
      }

      nsAutoString fractionString; /* of form 2/5 meaning 2nd in a 5-element set */
      nsAutoString fractionStringWithoutDenominator; /* of form 2/ meaning 2nd in any-length set */
      fractionString.SetLength(0);
      fractionString.AppendInt(numerator);
      fractionString.Append(NS_LITERAL_STRING("/"));
      fractionStringWithoutDenominator.Assign(fractionString);
      fractionString.AppendInt(denominator);

      /* use positionalSchema list to obtain schema */
      wallet_Sublist * sublistPtr;
      PRInt32 count2 = LIST_COUNT(mapElementPtr->itemList);
      for (PRInt32 j=0; j<count2; j=j+2) {
        sublistPtr = NS_STATIC_CAST(wallet_Sublist*, mapElementPtr->itemList->ElementAt(j));

        if (sublistPtr->item.EqualsWithConversion(fractionString) ||
            sublistPtr->item.EqualsWithConversion(fractionStringWithoutDenominator)) {
          sublistPtr = NS_STATIC_CAST(wallet_Sublist*, mapElementPtr->itemList->ElementAt(j+1));
          schema = sublistPtr->item;
          return;
        }
      }
    }
  }
  schema.SetLength(0);
}

static nsString previousElementState;
static nsIDOMNode* previousElementNode;

static void
wallet_InitializeStateTesting() {
  previousElementNode = nsnull;
  previousElementState.SetLength(0);
}

static void
wallet_ResolveStateSchema(nsIDOMNode* elementNode, nsString& schema) {

  /* return if no StateSchema list exists */
  if (!wallet_StateSchema_list) {
    return;
  }

  /* search state schema list for our state schema */
  wallet_MapElement * mapElementPtr;
  PRInt32 count = LIST_COUNT(wallet_StateSchema_list);
  for (PRInt32 i=0; i<count; i++) {
    mapElementPtr = NS_STATIC_CAST(wallet_MapElement*, wallet_StateSchema_list->ElementAt(i));
    if (mapElementPtr->item1.EqualsIgnoreCase(schema)) {
      /* found our state schema in the list */

      /* A state-schema entry consists of a set of possible states and the schema associated
       * with each state.  For example, for the state-schema $phone we might have
       *
       *    ship  ShipTo.Phone
       *    bill  BillTo.Phone
       *    *     Home.Phone
       *
       * This means that if we are in the "ship" state, the schema is ShipTo.Phone, if in the
       * "bill" state it is BillTo.Phone, and if in no identifiable state it is Home.Phone.
       *
       * So we will start stepping backwards through the dom tree
       * obtaining text at each step.  If the text contains a substring for one of
       * the states, then that is the state we are in and we take the associated
       * schema.  If the text does not contain any of the states, we continue
       * stepping back until we get to a preceding node for which we knew the state.
       * If none is found, stop when we get to the beginning of the tree.
       */

      nsIDOMNode* localElementNode = elementNode;
      PRBool atEnd = PR_FALSE;
      PRBool atInputOrSelect = PR_FALSE;
      while (!atEnd) {

        /* get next text in the dom */
        nsAutoString text;
        wallet_StepForwardOrBack(localElementNode, text, atInputOrSelect, atEnd, PR_FALSE);

        /* see if its a node we already saved the state for */
        if (localElementNode == previousElementNode) {
          previousElementNode = elementNode;

          /* step through the list of states to see if any are the state of the previous Node */
          wallet_Sublist * sublistPtr;
          PRInt32 count2 = LIST_COUNT(mapElementPtr->itemList);
          PRInt32 j;
          for (j=0; j<count2; j=j+2) {
            sublistPtr = NS_STATIC_CAST(wallet_Sublist*, mapElementPtr->itemList->ElementAt(j));

            /* next state in list obtained, test to see if it is the state of the previous node */
            if (sublistPtr->item.EqualsIgnoreCase(previousElementState)) {
              previousElementState = sublistPtr->item;
              sublistPtr = NS_STATIC_CAST(wallet_Sublist*, mapElementPtr->itemList->ElementAt(j+1));
              schema = sublistPtr->item;
              return;
            }

            /* test to see if we obtained the catch-all (*) state.
             *   Note: the catch-all must be the last entry in the list
             */
            if (sublistPtr->item.Equals(NS_LITERAL_STRING("*"))) {
              sublistPtr = NS_STATIC_CAST(wallet_Sublist*, mapElementPtr->itemList->ElementAt(j+1));
              schema = sublistPtr->item;
              return;
            }
          }

          /* no catch-all state specified, return no schema */
          schema.SetLength(0);
          return;
        }

        /* step through the list of states to see if any are in the text */
        wallet_Sublist * sublistPtr;
        PRInt32 count2 = LIST_COUNT(mapElementPtr->itemList);
        for (PRInt32 j=0; j<count2; j=j+2) {
          sublistPtr = NS_STATIC_CAST(wallet_Sublist*, mapElementPtr->itemList->ElementAt(j));

          /* next state obtained, test to see if it is in the text */
          if (text.Find(sublistPtr->item, PR_TRUE) != -1) {
            previousElementState = sublistPtr->item;
            previousElementNode = elementNode;
            sublistPtr = NS_STATIC_CAST(wallet_Sublist*, mapElementPtr->itemList->ElementAt(j+1));
            schema = sublistPtr->item;
            return;
          }
        }
      }

      /* state not found, so take the catch-all (*) state */
      wallet_Sublist * sublistPtr;
      PRInt32 count2 = LIST_COUNT(mapElementPtr->itemList);
      for (PRInt32 j=0; j<count2; j=j+2) {
        sublistPtr = NS_STATIC_CAST(wallet_Sublist*, mapElementPtr->itemList->ElementAt(j));
        if (sublistPtr->item.Equals(NS_LITERAL_STRING("*"))) {
          previousElementNode = localElementNode;
          sublistPtr = NS_STATIC_CAST(wallet_Sublist*, mapElementPtr->itemList->ElementAt(j+1));
          schema = sublistPtr->item;
          previousElementNode = elementNode;
          return;
        }
      }

      /* no catch-all state specified, return no schema */
      schema.SetLength(0);
      previousElementNode = elementNode;
      return;
    }
  }

  /* This is an error.  It means that a state-schema (entry starting with a $)
   * was obtained from the SchemaStrings table or the PositionalSchema table
   * but there was no entry for that state-schema in the StateSchema table.
   */
  NS_ASSERTION(PR_FALSE, "Undefined state in SchemaStrings table");
}

static void
wallet_GetSchemaFromDisplayableText(nsIDOMNode* elementNode, nsString& schema, PRBool skipStateChecking) {

  static nsString lastSchema;
  static nsIDOMNode* lastElementNode;

  /* return if this is the same as the last element node */
  if (elementNode == lastElementNode) {
    schema = lastSchema;
    return;
  }
  lastElementNode = elementNode;

  nsIDOMNode* localElementNode = elementNode;
  PRBool atInputOrSelect = PR_FALSE;
  PRBool atEnd = PR_FALSE;
  PRBool someTextFound = PR_FALSE;
  while (!atEnd && !atInputOrSelect) {

    /* step back and get text found in a preceding node */
    nsAutoString text;
    wallet_StepForwardOrBack(localElementNode, text, atInputOrSelect, atEnd, PR_FALSE);

    /* strip off non-alphanumerics */
    PRUint32 i;
    PRUnichar c;
    nsAutoString temp;
    for (i=0; i<text.Length(); i++) {
      c = text.CharAt(i);
      if (nsCRT::IsAsciiAlpha(c) || nsCRT::IsAsciiDigit(c)) {
        temp.Append(c);
      }
    }
    text = temp;

    /* done if we've obtained enough text from which to determine the schema */
    if (text.Length()) {
      someTextFound = PR_TRUE;

      TextToSchema(text, schema);
      if (schema.Length()) {

        /* schema found, process positional schema if any */
        if (schema.Length() && schema.CharAt(0) == '%') {
          wallet_ResolvePositionalSchema(elementNode, schema);
        }

        /* process state schema if any */
        if (!skipStateChecking && schema.Length() && schema.CharAt(0) == '$') {
          wallet_ResolveStateSchema(elementNode, schema); 
        }
        lastSchema = schema;
        return;
      }

    }
  }

  /* no displayable text found, see if we are inside a position set */
  if (!someTextFound) {
    wallet_ResolvePositionalSchema(elementNode, schema);
  }

  /* process state schema if any */

  /* The current routine is called for each field whose value is to be captured,
   * even if there is no value entered for that field.  We do this because we need
   * to call ResolvePositionalSchema above even for null values.  If we didn't
   * make that call, we would fail to recognize fields in a positional set if any
   * preceding fields in that set were blank.  For example:
   *
   *    name (first, middle, last): [William] [  ] [Clinton] 
   *
   * With that said, at least we can skip the call to ResolveStateSchema in this
   * case.  That call could be very time consuming because it involves looking
   * looking backwards through all preceding text (possibly all the way to the
   * beginning of the document) just to determine the state.  That is the purpose
   * of the skipStateChecking argument.
   */

  if (!skipStateChecking && schema.Length() && schema.CharAt(0) == '$') {
    wallet_ResolveStateSchema(elementNode, schema); 
  }

  lastSchema = schema;
  return;
}

nsresult
wallet_GetPrefills(
  nsIDOMNode* elementNode,
  nsIDOMHTMLInputElement*& inputElement,  
  nsIDOMHTMLSelectElement*& selectElement,
  nsString& schema,
  nsString& value,
  PRInt32& selectIndex,
  PRInt32& index)
{
  nsresult result;

  /* get prefills for input element */
  result = elementNode->QueryInterface(NS_GET_IID(nsIDOMHTMLInputElement), (void**)&inputElement);

  // The below code looks really suspicious leak-wize

  if ((NS_SUCCEEDED(result)) && (nsnull != inputElement)) {
    nsAutoString type;
    result = inputElement->GetType(type);
    if ((NS_SUCCEEDED(result)) && ((type.IsEmpty()) || (type.CompareWithConversion("text", PR_TRUE) == 0))) {
      nsAutoString field;
      result = inputElement->GetName(field);
      if (NS_SUCCEEDED(result)) {
        nsVoidArray* itemList;

        /* try to get schema name from vcard attribute if it exists */
        if (schema.Length() == 0) {
          nsCOMPtr<nsIDOMElement> element = do_QueryInterface(elementNode);
          if (element) {
            nsAutoString vcard; vcard.Assign(NS_LITERAL_STRING("VCARD_NAME"));
            nsAutoString vcardValue;
            result = element->GetAttribute(vcard, vcardValue);
            if (NS_OK == result) {
              nsVoidArray* dummy;
              wallet_ReadFromList(vcardValue, schema, dummy, wallet_VcardToSchema_list, PR_FALSE);
            }
          }
        }

        /* try to get schema name from displayable text if possible */
        if (schema.Length() == 0) {
          wallet_GetSchemaFromDisplayableText(inputElement, schema, PR_FALSE);
        }

#ifdef IgnoreFieldNames
// use displayable text instead of field names
if (schema.Length()) {
#endif

        /*
         * if schema name was obtained then get value from schema name,
         * otherwise get value from field name by using mapping tables to get schema name
         */
        if (FieldToValue(field, schema, value, itemList, index) == 0) {
          if (value.IsEmpty() && nsnull != itemList) {
            /* pick first of a set of synonymous values */
            nsAutoString encryptedValue( ((wallet_Sublist *)itemList->ElementAt(0))->item );
            if (NS_FAILED(Wallet_Decrypt(encryptedValue, value))) {
              NS_RELEASE(inputElement);
              return NS_ERROR_FAILURE;
            }
          }
          selectElement = nsnull;
          selectIndex = -1;
          return NS_OK;
        }

#ifdef IgnoreFieldNames
// use displayable text instead of field names
}
#endif

      }
    }
    NS_RELEASE(inputElement);
    return NS_ERROR_FAILURE;
  }

  /* get prefills for dropdown list */
  result = elementNode->QueryInterface(NS_GET_IID(nsIDOMHTMLSelectElement), (void**)&selectElement);
  if ((NS_SUCCEEDED(result)) && (nsnull != selectElement)) {
    nsAutoString field;
    result = selectElement->GetName(field);
    if (NS_SUCCEEDED(result)) {

      /* try to get schema name from displayable text if possible */
      wallet_GetSchemaFromDisplayableText(selectElement, schema, PR_FALSE);

#ifdef IgnoreFieldNames
// use displayable text instead of field names
if (schema.Length()) {
#endif

      nsVoidArray* itemList;
      if (FieldToValue(field, schema, value, itemList, index) == 0) {
        if (!value.IsEmpty()) {
          /* no synonym list, just one value to try */
          result = wallet_GetSelectIndex(selectElement, value, selectIndex);
          if (NS_SUCCEEDED(result)) {
            /* value matched one of the values in the drop-down list */

            // No Release() here?

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

              // No Release() here?

              inputElement = nsnull;
              return NS_OK;
            }
          }
        }
      }


#ifdef IgnoreFieldNames
// use displayable text instead of field names
}
#endif

    }
    NS_RELEASE(selectElement);
  }
  return NS_ERROR_FAILURE;
}

/*
 * termination for wallet session
 */
PUBLIC void
Wallet_ReleaseAllLists() {
    wallet_Clear(&wallet_FieldToSchema_list); /* otherwise we will duplicate the list */
    wallet_Clear(&wallet_VcardToSchema_list); /* otherwise we will duplicate the list */
    wallet_Clear(&wallet_SchemaConcat_list); /* otherwise we will duplicate the list */
    wallet_Clear(&wallet_SchemaStrings_list); /* otherwise we will duplicate the list */
    wallet_Clear(&wallet_PositionalSchema_list); /* otherwise we will duplicate the list */
    wallet_Clear(&wallet_StateSchema_list); /* otherwise we will duplicate the list */
#ifdef AutoCapture
    wallet_Clear(&wallet_DistinguishedSchema_list); /* otherwise we will duplicate the list */
#endif
    wallet_DeallocateMapElements();
    delete helpMac;
    helpMac = 0;
}

/*
 * initialization for wallet session (done only once)
 */

static PRBool wallet_tablesInitialized = PR_FALSE;
static PRBool wallet_ValuesReadIn = PR_FALSE;
static PRBool namesInitialized = PR_FALSE;
static PRBool wallet_URLListInitialized = PR_FALSE;

static void
wallet_Initialize(PRBool unlockDatabase=PR_TRUE) {

#ifdef DEBUG_morse
//wallet_ClearStopwatch();
//wallet_ResumeStopwatch();
#endif

  if (!wallet_tablesInitialized) {
#ifdef DEBUG_morse
//wallet_PauseStopwatch();
//wallet_DumpStopwatch();
#endif
//    printf("******** start profile\n");
//             ProfileStart();

    Wallet_ReleaseAllLists();
    helpMac = new wallet_HelpMac; /* to speed up startup time on the mac */
#ifdef AutoCapture
    wallet_ReadFromFile(distinguishedSchemaFileName, wallet_DistinguishedSchema_list, PR_FALSE);
#endif
    wallet_ReadFromFile(fieldSchemaFileName, wallet_FieldToSchema_list, PR_FALSE);
    wallet_ReadFromFile(vcardSchemaFileName, wallet_VcardToSchema_list, PR_FALSE);
    wallet_ReadFromFile(schemaConcatFileName, wallet_SchemaConcat_list, PR_FALSE);
    wallet_ReadFromFile(schemaStringsFileName, wallet_SchemaStrings_list, PR_FALSE, BY_LENGTH);
    wallet_ReadFromFile(positionalSchemaFileName, wallet_PositionalSchema_list, PR_FALSE);
    wallet_ReadFromFile(stateSchemaFileName, wallet_StateSchema_list, PR_FALSE);

    /* Note that we sort the SchemaString list by length instead of alphabetically.  To see
     * why that's necessary, consider the following example:
     *
     *    Card.Name: requires "card" and "name" both be present
     *    Name: requires "name"
     *
     * So we want to check for a match on one with more strings (Card.Name in this case) before
     * checking for a match with the one containing less strings.
     */
 
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

//    fprintf(stdout,"Vcard to Schema table \n");
//    wallet_Dump(wallet_VcardToSchema_list);

//    fprintf(stdout,"SchemaConcat table \n");
//    wallet_Dump(wallet_SchemaConcat_list);

//    fprintf(stdout,"SchemaStrings table \n");
//    wallet_Dump(wallet_SchemaStrings_list);

//    fprintf(stdout,"PositionalSchema table \n");
//    wallet_Dump(wallet_PositionalSchema_list);

//    fprintf(stdout,"StateSchema table \n");
//    wallet_Dump(wallet_StateSchema_list);

//    fprintf(stdout,"Schema to Value table \n");
//    wallet_Dump(wallet_SchemaToValue_list);
#endif

}

static void
wallet_InitializeURLList() {
  if (!wallet_URLListInitialized) {
    wallet_Clear(&wallet_URL_list);
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
  nsCOMPtr<nsIURI> url;
  doc->GetDocumentURL(getter_AddRefs(url));
  if (wallet_lastUrl == url) {
    return;
  } else {
    if (wallet_lastUrl) {
//??      NS_RELEASE(lastUrl);
    }
    wallet_lastUrl = url;
  }

}

#define SEPARATOR "#*%$"

static PRInt32
wallet_GetNextInString(const nsString& str, nsString& head, nsString& tail) {
  PRInt32 separator = str.Find(SEPARATOR);
  if (separator == -1) {
    return -1;
  }
  str.Left(head, separator);
  str.Mid(tail, separator+sizeof(SEPARATOR)-1, str.Length() - (separator+sizeof(SEPARATOR)-1));
  return 0;
}

static void
wallet_ReleasePrefillElementList(nsVoidArray * wallet_PrefillElement_list) {
  if (wallet_PrefillElement_list) {
    wallet_PrefillElement * prefillElementPtr;
    PRInt32 count = LIST_COUNT(wallet_PrefillElement_list);
    for (PRInt32 i=count-1; i>=0; i--) {
      prefillElementPtr = NS_STATIC_CAST(wallet_PrefillElement*, wallet_PrefillElement_list->ElementAt(i));
      delete prefillElementPtr;
    }
    delete wallet_PrefillElement_list;
    wallet_PrefillElement_list = 0;
  }
}

#define BREAK '\001'

nsVoidArray * wallet_list;
nsAutoString wallet_url;

PUBLIC void
WLLT_GetPrefillListForViewer(nsString& aPrefillList)
{
  wallet_Initialize(PR_FALSE); /* to initialize helpMac */
  wallet_PrefillElement * mapElementPtr;
  nsAutoString buffer;
  PRInt32 count = LIST_COUNT(wallet_list);
  for (PRInt32 i=0; i<count; i++) {
    mapElementPtr = NS_STATIC_CAST(wallet_PrefillElement*, wallet_list->ElementAt(i));
    buffer.AppendWithConversion(BREAK);
    buffer.AppendInt(mapElementPtr->count,10);
    buffer.AppendWithConversion(BREAK);
    buffer.Append(mapElementPtr->schema);
    buffer.AppendWithConversion(BREAK);
    buffer.Append(mapElementPtr->value);
  }

  PRUnichar * urlUnichar = ToNewUnicode(wallet_url);
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
      SI_FindValueInArgs(results, NS_LITERAL_STRING("|goneP|"), gone);
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
      SI_FindValueInArgs(results, NS_LITERAL_STRING("|goneC|"), gone);
    }
    PRInt32 count2 = LIST_COUNT(wallet_URL_list);
    while (count2>0) {
      count2--;
      url = NS_STATIC_CAST(wallet_MapElement*, wallet_URL_list->ElementAt(count2));
      if (url && SI_InSequence(gone, count2)) {
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
wallet_OKToCapture(char* urlName, nsIDOMWindowInternal* window) {
  nsAutoString url; url.AssignWithConversion(urlName);

  /* exit if pref is not set */
  if (!wallet_GetFormsCapturingPref() || !wallet_GetEnabledPref()) {
    return PR_FALSE;
  }

  /* see if this url is already on list of url's for which we don't want to capture */
  wallet_InitializeURLList();
  nsVoidArray* dummy;
  nsAutoString value; value.Assign(NS_LITERAL_STRING("nn"));
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
wallet_Capture(nsIDocument* doc, const nsString& field, const nsString& value, const nsString& schema)
{
  /* do nothing if there is no value */
  if (!value.Length()) {
    return PR_FALSE;
  }

  /* read in the mappings if they are not already present */
  wallet_Initialize();
  wallet_InitializeCurrentURL(doc);

#ifdef IgnoreFieldNames
// use displayable text instead of field names
  if (!schema.Length()) {
    return PR_FALSE;
  }
#endif

  nsAutoString oldValue;

  /* is there a mapping from this field name to a schema name */
  nsAutoString localSchema(schema);
  nsVoidArray* dummy;
  nsString stripField;
  if (localSchema.Length() ||
      (wallet_ReadFromList(Strip(field, stripField), localSchema, dummy, wallet_FieldToSchema_list, PR_FALSE))) {

    /* field to schema mapping already exists */

    /* is this a new value for the schema */
    PRInt32 index = 0;
    PRInt32 lastIndex = index;
    while(wallet_ReadFromList(localSchema, oldValue, dummy, wallet_SchemaToValue_list, PR_TRUE, index)) {
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
    if (wallet_WriteToList(localSchema, value, dummy, wallet_SchemaToValue_list, PR_TRUE)) {
      wallet_WriteToFile(schemaValueFileName, wallet_SchemaToValue_list);
    }

  } else {

    /* no field to schema mapping so assume schema name is same as field name */

    /* is this a new value for the schema */
    PRInt32 index = 0;
    PRInt32 lastIndex = index;

    nsAutoString concat_param;
    wallet_GetHostFile(wallet_lastUrl, concat_param);
    concat_param.Append(NS_LITERAL_STRING(":"));
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
      concat_param.Append(NS_LITERAL_STRING(":"));
      concat_param.Append(field);
    }

    /* this is a new value so store it */
    dummy = 0;
    nsAutoString hostFileField;
    wallet_GetHostFile(wallet_lastUrl, hostFileField);
    hostFileField.Append(NS_LITERAL_STRING(":"));
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

#define BREAK '\001'

PUBLIC void
WLLT_GetNopreviewListForViewer(nsString& aNopreviewList)
{
  wallet_Initialize(PR_FALSE); /* to initialize helpMac */
  nsAutoString buffer;
  int nopreviewNum = 0;
  wallet_MapElement *url;

  wallet_InitializeURLList();
  PRInt32 count = LIST_COUNT(wallet_URL_list);
  for (PRInt32 i=0; i<count; i++) {
    url = NS_STATIC_CAST(wallet_MapElement*, wallet_URL_list->ElementAt(i));
    if (url->item2.CharAt(NO_PREVIEW) == 'y') {
      buffer.AppendWithConversion(BREAK);
      buffer.Append(NS_LITERAL_STRING("<OPTION value="));
      buffer.AppendInt(nopreviewNum, 10);
      buffer.Append(NS_LITERAL_STRING(">"));
      buffer += url->item1;
      buffer.Append(NS_LITERAL_STRING("</OPTION>\n"));
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
      buffer.Append(NS_LITERAL_STRING("<OPTION value="));
      buffer.AppendInt(nocaptureNum, 10);
      buffer.Append(NS_LITERAL_STRING(">"));
      buffer += url->item1;
      buffer.Append(NS_LITERAL_STRING("</OPTION>\n"));
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

  nsAutoString tail( walletList );
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
  if (!head.Equals(NS_LITERAL_STRING("OK"))) {
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
  wallet_Initialize();
  walletList.AssignWithConversion(BREAK);
  wallet_MapElement * mapElementPtr;
  PRInt32 count = LIST_COUNT(wallet_SchemaToValue_list);
  for (PRInt32 i=0; i<count; i++) {
    mapElementPtr = NS_STATIC_CAST(wallet_MapElement*, wallet_SchemaToValue_list->ElementAt(i));

    walletList += mapElementPtr->item1; walletList.AppendWithConversion(BREAK);
    if (!mapElementPtr->item2.IsEmpty()) {
      walletList += mapElementPtr->item2; walletList.AppendWithConversion(BREAK);
    } else {
      wallet_Sublist * sublistPtr;
      PRInt32 count2 = LIST_COUNT(mapElementPtr->itemList);
      for (PRInt32 i2=0; i2<count2; i2++) {
        sublistPtr = NS_STATIC_CAST(wallet_Sublist*, mapElementPtr->itemList->ElementAt(i2));
        walletList += sublistPtr->item; walletList.AppendWithConversion(BREAK);

      }
    }
    walletList.AppendWithConversion(BREAK);
  }
}

PUBLIC void
WLLT_DeleteAll() {
  wallet_Initialize();
  wallet_Clear(&wallet_SchemaToValue_list);
  wallet_WriteToFile(schemaValueFileName, wallet_SchemaToValue_list);
  SI_DeleteAll();
}

PUBLIC void
WLLT_ClearUserData() {
  wallet_ValuesReadIn = PR_FALSE;
  namesInitialized = PR_FALSE;
  wallet_URLListInitialized = PR_FALSE;
}

PUBLIC void
WLLT_DeletePersistentUserData() {

  if (schemaValueFileName && schemaValueFileName[0]) {
    nsFileSpec fileSpec;
    nsresult rv = Wallet_ProfileDirectory(fileSpec);
    if (NS_SUCCEEDED(rv)) {
      fileSpec += schemaValueFileName;
      if (fileSpec.Valid() && fileSpec.IsFile())
        fileSpec.Delete(PR_FALSE);
    }
  }
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
  wallet_Initialize();
}
  wallet_MapElement * mapElementPtr;
  gEncryptionFailure = PR_FALSE;
  for (i=0; i<count && !gEncryptionFailure; i++) {
    mapElementPtr = NS_STATIC_CAST(wallet_MapElement*, wallet_SchemaToValue_list->ElementAt(i));
    if (!mapElementPtr->item2.IsEmpty()) {
      if (NS_FAILED(Wallet_Decrypt(mapElementPtr->item2, value))) {
        goto fail;
      }
      if (NS_FAILED(Wallet_Encrypt(value, mapElementPtr->item2))) {
        goto fail;
      }
    } else {
      wallet_Sublist * sublistPtr;
      PRInt32 count2 = LIST_COUNT(mapElementPtr->itemList);
      for (PRInt32 i2=0; i2<count2; i2++) {
        sublistPtr = NS_STATIC_CAST(wallet_Sublist*, mapElementPtr->itemList->ElementAt(i2));
        if (NS_FAILED(Wallet_Decrypt(sublistPtr->item, value))) {
          goto fail;
        }
        if (NS_FAILED(Wallet_Encrypt(value, sublistPtr->item))) {
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
//  wallet_Alert(message, (nsIDOMWindowInternal *)window);
//  Recycle(message);
  level--;
  return 0; /* this is PREF_NOERROR but we no longer include prefapi.h */
fail:
  /* toggle the pref back to its previous value */
  SI_SetBoolPref(pref_Crypto, !SI_GetBoolPref(pref_Crypto, PR_TRUE));

  /* alert the user to the failure */
  message = Wallet_Localize("NotConverted");
  wallet_Alert(message, (nsIDOMWindowInternal *)window);
  Recycle(message);
  level--;
  return 1;
}

PUBLIC void
WLLT_InitReencryptCallback(nsIDOMWindowInternal* window) {
  static PRBool registered = PR_FALSE;
  static nsIDOMWindowInternal* lastWindow;
  if (registered) {
    SI_UnregisterCallback(pref_Crypto, wallet_ReencryptAll, lastWindow);
  }
  SI_RegisterCallback(pref_Crypto, wallet_ReencryptAll, window);
  lastWindow = window;
  registered = PR_TRUE;
}

PRIVATE void
wallet_DecodeVerticalBars(nsString& s) {
  s.ReplaceSubstring(NS_LITERAL_STRING("^2").get(), NS_LITERAL_STRING("|").get());
  s.ReplaceSubstring(NS_LITERAL_STRING("^1").get(), NS_LITERAL_STRING("^").get());
}

/*
 * return after previewing a set of prefills
 */
PUBLIC void
WLLT_PrefillReturn(const nsString& results)
{
  nsAutoString fillins;
  nsAutoString urlName;
  nsAutoString skip;
  nsAutoString next;

  /* get values that are in environment variables */
  SI_FindValueInArgs(results, NS_LITERAL_STRING("|fillins|"), fillins);
  SI_FindValueInArgs(results, NS_LITERAL_STRING("|skip|"), skip);
  SI_FindValueInArgs(results, NS_LITERAL_STRING("|url|"), urlName);
  wallet_DecodeVerticalBars(fillins);
  wallet_DecodeVerticalBars(urlName);

  /* add url to url list if user doesn't want to preview this page in the future */
  if (skip.Equals(NS_LITERAL_STRING("true"))) {
    nsAutoString url = nsAutoString(urlName);
    nsVoidArray* dummy;
    nsAutoString value; value.Assign(NS_LITERAL_STRING("nn"));
    wallet_ReadFromList(url, value, dummy, wallet_URL_list, PR_FALSE);
    value.SetCharAt('y', NO_PREVIEW);
    if (wallet_WriteToList(url, value, dummy, wallet_URL_list, PR_FALSE, DUP_OVERWRITE)) {
      wallet_WriteToFile(URLFileName, wallet_URL_list);
    }
  }

  /* process the list, doing the fillins */
  if (fillins.Length() == 0) { /* user pressed CANCEL */
    wallet_ReleasePrefillElementList(wallet_list);
    wallet_list = nsnull;
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

  wallet_PrefillElement * mapElementPtr;
  /* step through pre-fill list */
  PRInt32 count = LIST_COUNT(wallet_list);
  for (PRInt32 i=0; i<count; i++) {
    mapElementPtr = NS_STATIC_CAST(wallet_PrefillElement*, wallet_list->ElementAt(i));

    /* advance in fillins list each time a new schema name in pre-fill list is encountered */
    if (mapElementPtr->count != 0) {
      /* count != 0 indicates a new schema name */
      nsAutoString tail;
      if (wallet_GetNextInString(fillins, next, tail) == -1) {
        break;
      }
      fillins = tail;
      if (next != mapElementPtr->schema) {
        break; /* something's wrong so stop prefilling */
      }
      wallet_GetNextInString(fillins, next, tail);
      fillins = tail;
    }
    if (next == mapElementPtr->value) {
      /*
       * Remove entry from wallet_SchemaToValue_list and then reinsert.  This will
       * keep multiple values in that list for the same field ordered with
       * most-recently-used first.  That's useful since the first such entry
       * is the default value used for pre-filling.
       */
      /*
       * Test for mapElementPtr->count being zero is an optimization that avoids us from doing a
       * reordering if the current entry already was first
       */
      if (mapElementPtr->count == 0) {
        nsAutoString oldValue;
        PRInt32 index = 0;
        PRInt32 lastIndex = index;
        nsVoidArray* dummy;
        while(wallet_ReadFromList(mapElementPtr->schema, oldValue, dummy, wallet_SchemaToValue_list, PR_TRUE, index)) {
          if (oldValue == mapElementPtr->value) {
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

    if (!next.IsEmpty()) {
      if (mapElementPtr->inputElement) {
        mapElementPtr->inputElement->SetValue(next);
      } else {
        nsresult result;
        result = wallet_GetSelectIndex(mapElementPtr->selectElement, next, mapElementPtr->selectIndex);
        if (NS_SUCCEEDED(result)) {
          mapElementPtr->selectElement->SetSelectedIndex(mapElementPtr->selectIndex);
        } else {
          mapElementPtr->selectElement->SetSelectedIndex(0);
        }
      }
    }
  }

  /* Release the prefill list that was generated when we walked thru the html content */
  wallet_ReleasePrefillElementList(wallet_list);
  wallet_list = nsnull;
}

/*
 * get the form elements on the current page and prefill them if possible
 */
PRIVATE void
wallet_TraversalForPrefill
    (nsIDOMWindow* win, nsVoidArray* wallet_PrefillElement_list, nsString& urlName) {

  nsresult result;
  if (nsnull != win) {
    nsCOMPtr<nsIDOMDocument> domdoc;
    result = win->GetDocument(getter_AddRefs(domdoc));
    if (NS_SUCCEEDED(result)) {
      nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);
      if (doc) {
        nsCOMPtr<nsIURI> url;
        doc->GetDocumentURL(getter_AddRefs(url));
        if (url) {
          wallet_GetHostFile(url, urlName);
        }
        wallet_Initialize();
        wallet_InitializeCurrentURL(doc);

        nsCOMPtr<nsIDOMHTMLDocument> htmldoc = do_QueryInterface(doc);
        if (htmldoc) {
          nsCOMPtr<nsIDOMHTMLCollection> forms;
          htmldoc->GetForms(getter_AddRefs(forms));
          if (forms) {
            wallet_InitializeStateTesting();
            PRUint32 numForms;
            forms->GetLength(&numForms);
            for (PRUint32 formX = 0; (formX < numForms) && !gEncryptionFailure; formX++) {
              nsCOMPtr<nsIDOMNode> formNode;
              forms->Item(formX, getter_AddRefs(formNode));
              if (formNode) {
                nsCOMPtr<nsIDOMHTMLFormElement> formElement = do_QueryInterface(formNode);
                if (formElement) {
                  nsCOMPtr<nsIDOMHTMLCollection> elements;
                  result = formElement->GetElements(getter_AddRefs(elements));
                  if (elements) {
                    /* got to the form elements at long last */
                    PRUint32 numElements;
                    elements->GetLength(&numElements);
                    for (PRUint32 elementX = 0; (elementX < numElements) && !gEncryptionFailure; elementX++) {
                      nsCOMPtr<nsIDOMNode> elementNode;
                      elements->Item(elementX, getter_AddRefs(elementNode));
                      if (elementNode) {
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
                          prefillElement = new wallet_PrefillElement;
                          if (NS_SUCCEEDED(wallet_GetPrefills
                              (elementNode,
                              prefillElement->inputElement,
                              prefillElement->selectElement,
                              prefillElement->schema,
                              prefillElement->value,
                              prefillElement->selectIndex,
                              index))) {
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
  }

  nsCOMPtr<nsIDOMWindowCollection> frames;
  win->GetFrames(getter_AddRefs(frames));
  if (frames) {
    PRUint32 numFrames;
    frames->GetLength(&numFrames);
    for (PRUint32 frameX = 0; (frameX < numFrames) && !gEncryptionFailure; frameX++) {
      nsCOMPtr<nsIDOMWindow> frameNode;
      frames->Item(frameX, getter_AddRefs(frameNode));
      if (frameNode) {
        wallet_TraversalForPrefill(frameNode, wallet_PrefillElement_list, urlName);
      }
    }
  }
}

PUBLIC nsresult
WLLT_PrefillOneElement
  (nsIDOMWindowInternal* win, nsIDOMNode* elementNode, nsString& compositeValue)
{
  nsIDOMHTMLInputElement* inputElement;
  nsIDOMHTMLSelectElement* selectElement;
  nsString schema;
  nsString value;
  PRInt32 selectIndex = 0;
  PRInt32 index = 0;

  if (nsnull != win) {
    nsCOMPtr<nsIDOMDocument> domdoc;
    nsresult result = win->GetDocument(getter_AddRefs(domdoc));
    if (NS_SUCCEEDED(result)) {
      nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);
      if (doc) {
        wallet_Initialize(PR_TRUE);
        wallet_InitializeCurrentURL(doc);
        wallet_InitializeStateTesting();
        while (NS_SUCCEEDED(wallet_GetPrefills
            (elementNode,
            inputElement,
            selectElement,
            schema,
            value,
            selectIndex,
            index))) {
              compositeValue.AppendWithConversion(BREAK);
              compositeValue.Append(value);
        }
      }
    }
  }
  return NS_OK;
}

PUBLIC nsresult
WLLT_Prefill(nsIPresShell* shell, PRBool quick, nsIDOMWindowInternal* win)
{
  /* do not prefill if preview window is open in some other browser window */
  if (wallet_list) {
    return NS_ERROR_FAILURE;
  }

  /* create list of elements that can be prefilled */
  nsVoidArray *wallet_PrefillElement_list=new nsVoidArray();
  if (!wallet_PrefillElement_list) {
    return NS_ERROR_FAILURE;
  }

  nsAutoString urlName;
  gEncryptionFailure = PR_FALSE;
  wallet_TraversalForPrefill(win, wallet_PrefillElement_list, urlName);

  /* return if no elements were put into the list */
  if (LIST_COUNT(wallet_PrefillElement_list) == 0) {
    if (!gEncryptionFailure) {
      PRUnichar * message = Wallet_Localize("noPrefills");
      wallet_Alert(message, win);
      Recycle(message);
    }

    // Shouldn't wallet_PrefillElement_list be deleted here?

    return NS_ERROR_FAILURE; // indicates to caller not to display preview screen
  }

  /* prefill each element using the list */

  /* determine if url is on list of urls that should not be previewed */
  PRBool noPreview = PR_FALSE;
  if (!quick) {
    wallet_InitializeURLList();
    nsVoidArray* dummy;
    nsAutoString value; value.Assign(NS_LITERAL_STRING("nn"));
    if (urlName.Length() != 0) {
      wallet_ReadFromList(urlName, value, dummy, wallet_URL_list, PR_FALSE);
      noPreview = (value.CharAt(NO_PREVIEW) == 'y');
    }
  }

  /* determine if preview is necessary */
  if (noPreview || quick) {
    /* prefill each element without any preview for user verification */
    wallet_PrefillElement * mapElementPtr;
    PRInt32 count = LIST_COUNT(wallet_PrefillElement_list);
    for (PRInt32 i=0; i<count; i++) {
      mapElementPtr = NS_STATIC_CAST(wallet_PrefillElement*, wallet_PrefillElement_list->ElementAt(i));
      if (mapElementPtr->count) {
        if (mapElementPtr->inputElement) {
          mapElementPtr->inputElement->SetValue(mapElementPtr->value);
        } else {
          mapElementPtr->selectElement->SetSelectedIndex(mapElementPtr->selectIndex);
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
#ifdef DEBUG_morse
////wallet_DumpStopwatch();
////wallet_ClearStopwatch();
//wallet_DumpTiming();
//wallet_ClearTiming();
#endif
    return NS_OK; // indicates that caller is to display preview screen
  }
}

PRIVATE PRBool
wallet_CaptureInputElement(nsIDOMNode* elementNode, nsIDocument* doc) {
  nsresult result;
  PRBool captured = PR_FALSE;
  nsCOMPtr<nsIDOMHTMLInputElement> inputElement = do_QueryInterface(elementNode);  
  if (inputElement) {
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
          nsAutoString schema;
          nsCOMPtr<nsIDOMElement> element = do_QueryInterface(elementNode);
          if (element) {
            nsAutoString vcardName; vcardName.Assign(NS_LITERAL_STRING("VCARD_NAME"));
            nsAutoString vcardValue;
            result = element->GetAttribute(vcardName, vcardValue);
            if (NS_OK == result) {
              nsVoidArray* dummy;
              wallet_ReadFromList(vcardValue, schema, dummy, wallet_VcardToSchema_list, PR_FALSE);
            }
          }

#ifdef IgnoreFieldNames
// use displayable text instead of vcard names
schema.SetLength(0);
#endif

          if (!schema.Length()) {
            /* get schema from displayable text if possible */
            wallet_GetSchemaFromDisplayableText(inputElement, schema, (value.Length()==0));
          }
          if (wallet_Capture(doc, field, value, schema)) {
            captured = PR_TRUE;
          }
        }
      }
    }
  }
  return captured;
}

PRIVATE PRBool
wallet_CaptureSelectElement(nsIDOMNode* elementNode, nsIDocument* doc) {
  nsresult result;
  PRBool captured = PR_FALSE;
  nsCOMPtr<nsIDOMHTMLSelectElement> selectElement = do_QueryInterface(elementNode);  
  if (selectElement) {
    /* it's a dropdown list */
    nsAutoString field;
    result = selectElement->GetName(field);

    if (NS_SUCCEEDED(result)) {
      PRUint32 length;
      selectElement->GetLength(&length);

      nsCOMPtr<nsIDOMHTMLCollection> options;
      selectElement->GetOptions(getter_AddRefs(options));

      if (options) {
        PRInt32 selectedIndex;
        result = selectElement->GetSelectedIndex(&selectedIndex);

        if (NS_SUCCEEDED(result)) {
          nsCOMPtr<nsIDOMNode> optionNode;

          options->Item(selectedIndex, getter_AddRefs(optionNode));

          if (optionNode) {
            nsCOMPtr<nsIDOMHTMLOptionElement> optionElement(do_QueryInterface(optionNode));

            if (optionElement) {
              nsAutoString optionValue;
              nsAutoString optionText;

              PRBool valueOK = NS_SUCCEEDED(optionElement->GetValue(optionValue))
                               && optionValue.Length();
              PRBool textOK = NS_SUCCEEDED(optionElement->GetText(optionText))
                              && optionText.Length();
              if (valueOK || textOK) {
                /* get schema name from vcard attribute if it exists */
                nsAutoString schema;
                nsCOMPtr<nsIDOMElement> element = do_QueryInterface(elementNode);
                if (element) {
                  nsAutoString vcardName; vcardName.Assign(NS_LITERAL_STRING("VCARD_NAME"));
                  nsAutoString vcardValue;
                  result = element->GetAttribute(vcardName, vcardValue);
                  if (NS_OK == result) {
                    nsVoidArray* dummy;
                    wallet_ReadFromList(vcardValue, schema, dummy, wallet_VcardToSchema_list, PR_FALSE);
                  }
                }

#ifdef IgnoreFieldNames
// use displayable text instead of vcard names
schema.SetLength(0);
#endif

                if (!schema.Length()) {
                  /* get schema from displayable text if possible */
                  wallet_GetSchemaFromDisplayableText(selectElement, schema, (!valueOK && !textOK));
                }
                if (valueOK && wallet_Capture(doc, field, optionValue, schema)) {
                  captured = PR_TRUE;
                }
                optionText.Trim(" \n\t\r");
                if (textOK && wallet_Capture(doc, field, optionText, schema)) {
                  captured = PR_TRUE;
                }
              }
            }
          }
        }
      }
    }
  }
  return captured;
}

PRIVATE void
wallet_TraversalForRequestToCapture(nsIDOMWindow* win, PRInt32& captureCount) {

  nsresult result;
  if (nsnull != win) {
    nsCOMPtr<nsIDOMDocument> domdoc;
    result = win->GetDocument(getter_AddRefs(domdoc));
    if (NS_SUCCEEDED(result)) {
      nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);
      if (doc) {
        wallet_Initialize();
        wallet_InitializeCurrentURL(doc);
        nsCOMPtr<nsIDOMHTMLDocument> htmldoc = do_QueryInterface(doc);
        if (htmldoc) {
          nsCOMPtr<nsIDOMHTMLCollection> forms;
          htmldoc->GetForms(getter_AddRefs(forms));
          if (forms) {
          wallet_InitializeStateTesting();
            PRUint32 numForms;
            forms->GetLength(&numForms);
            for (PRUint32 formX = 0; (formX < numForms) && !gEncryptionFailure; formX++) {
              nsCOMPtr<nsIDOMNode> formNode;
              forms->Item(formX, getter_AddRefs(formNode));
              if (formNode) {
                nsCOMPtr<nsIDOMHTMLFormElement> formElement = do_QueryInterface(formNode);
                if (formElement) {
                  nsCOMPtr<nsIDOMHTMLCollection> elements;
                  result = formElement->GetElements(getter_AddRefs(elements));
                  if (elements) {
                    /* got to the form elements at long last */
                    /* now find out how many text fields are on the form */
                    PRUint32 numElements;
                    elements->GetLength(&numElements);
                    for (PRUint32 elementY = 0; (elementY < numElements) && !gEncryptionFailure; elementY++) {
                      nsCOMPtr<nsIDOMNode> elementNode;
                      elements->Item(elementY, getter_AddRefs(elementNode));
                      if (elementNode) {
                        if (wallet_CaptureInputElement(elementNode, doc)) {
                          captureCount++;
                        }
                        if (wallet_CaptureSelectElement(elementNode, doc)) {
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

  nsCOMPtr<nsIDOMWindowCollection> frames;
  win->GetFrames(getter_AddRefs(frames));
  if (frames) {
    PRUint32 numFrames;
    frames->GetLength(&numFrames);
    for (PRUint32 frameX = 0; (frameX < numFrames) && !gEncryptionFailure; frameX++)
    {
      nsCOMPtr<nsIDOMWindow> frameNode;
      frames->Item(frameX, getter_AddRefs(frameNode));
      if (frameNode) {
        wallet_TraversalForRequestToCapture(frameNode, captureCount);
      }
    }
  }
}

PUBLIC void
WLLT_RequestToCapture(nsIPresShell* shell, nsIDOMWindowInternal* win, PRUint32* status) {

  PRInt32 captureCount = 0;
  gEncryptionFailure = PR_FALSE;
  wallet_TraversalForRequestToCapture(win, captureCount);

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

MOZ_DECL_CTOR_COUNTER(si_SignonDataStruct)

/* should move this to an include file */
class si_SignonDataStruct {
public:
  si_SignonDataStruct() : isPassword(PR_FALSE)
  {
    MOZ_COUNT_CTOR(si_SignonDataStruct);
  }
  ~si_SignonDataStruct()
  {
    MOZ_COUNT_DTOR(si_SignonDataStruct);
  }
  nsAutoString name;
  nsAutoString value;
  PRBool isPassword;
};

PRIVATE PRBool
wallet_IsFromCartman(nsIURI* aURL) {
  PRBool retval = PR_FALSE;
  nsXPIDLCString host;
  if (NS_SUCCEEDED(aURL->GetHost(getter_Copies(host))) && host) {
    if (PL_strncasecmp(host, "127.0.0.1",  9) == 0) {
      /* submit is to server on local machine */
      nsresult res;
      nsCOMPtr<nsISecurityManagerComponent> psm = 
               do_GetService(PSM_COMPONENT_CONTRACTID, &res);
      if (NS_SUCCEEDED(res)) { 
        nsXPIDLCString password;
        if (NS_SUCCEEDED(aURL->GetPassword(getter_Copies(password))) && password) {
          nsXPIDLCString secmanPassword;
          if (NS_SUCCEEDED(psm->GetPassword(getter_Copies(secmanPassword))) && secmanPassword) {
            if (PL_strncasecmp(password, secmanPassword,  9) == 0) {
              /* password for submit is cartman's password */
              retval = PR_TRUE;
            }
          }
        }
      }
    }
  }
  return retval;
}

#ifdef AutoCapture
PRIVATE PRBool
wallet_IsNewValue(nsIDOMNode* elementNode, nsString valueOnForm) {
  if (valueOnForm.Equals(NS_LITERAL_STRING(""))) {
    return PR_FALSE;
  }
  nsIDOMHTMLInputElement* inputElement;
  nsIDOMHTMLSelectElement* selectElement;
  nsAutoString schema;
  nsAutoString valueSaved;
  PRInt32 selectIndex = 0;
  PRInt32 index = 0;
  while (NS_SUCCEEDED(wallet_GetPrefills
      (elementNode, inputElement, selectElement, schema, valueSaved, selectIndex, index))) {
    if (valueOnForm.Equals(valueSaved)) {
      return PR_FALSE;
    }
  }
  return PR_TRUE;
}
#endif

PUBLIC void
WLLT_OnSubmit(nsIContent* currentForm, nsIDOMWindowInternal* window) {

  nsCOMPtr<nsIDOMHTMLFormElement> currentFormNode(do_QueryInterface(currentForm));

  /* get url name as ascii string */
  char *URLName = nsnull;
  char *strippedURLName = nsnull;
  nsAutoString strippedURLNameAutoString;
  nsCOMPtr<nsIDocument> doc;
  currentForm->GetDocument(*getter_AddRefs(doc));
  if (!doc) {
    return;
  }
  nsCOMPtr<nsIURI> docURL;
  doc->GetDocumentURL(getter_AddRefs(docURL));
  if (!docURL || wallet_IsFromCartman(docURL)) {
    return;
  }
  (void)docURL->GetSpec(&URLName);
  wallet_GetHostFile(docURL, strippedURLNameAutoString);
  strippedURLName = ToNewCString(strippedURLNameAutoString);

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
          wallet_Initialize(PR_FALSE);
          wallet_InitializeCurrentURL(doc);
          wallet_InitializeStateTesting();
          PRBool newValueFound = PR_FALSE;
#endif
          for (PRUint32 elementX = 0; elementX < numElements; elementX++) {
            nsCOMPtr<nsIDOMNode> elementNode;
            elements->Item(elementX, getter_AddRefs(elementNode));
            if (nsnull != elementNode) {
              nsCOMPtr<nsIDOMHTMLSelectElement> selectElement(do_QueryInterface(elementNode));
              if ((NS_SUCCEEDED(rv)) && (nsnull != selectElement)) {
                if (passwordcount == 0 && !newValueFound && !OKToPrompt) {
                  nsAutoString valueOnForm;
                  rv = selectElement->GetValue(valueOnForm);
                  if (NS_SUCCEEDED(rv) && wallet_IsNewValue (elementNode, valueOnForm)) {
                    newValueFound = PR_TRUE;
                    if (hits > 1) {
                      OKToPrompt = PR_TRUE;
                    }
                  }
                }
              }
              nsCOMPtr<nsIDOMHTMLInputElement> inputElement(do_QueryInterface(elementNode));
              if ((NS_SUCCEEDED(rv)) && (nsnull != inputElement)) {
                nsAutoString type;
                rv = inputElement->GetType(type);
                if (NS_SUCCEEDED(rv)) {

                  PRBool isText = (type.IsEmpty() || (type.CompareWithConversion("text", PR_TRUE)==0));
                  PRBool isPassword = (type.CompareWithConversion("password", PR_TRUE)==0);

                  // don't save password if field was left blank
                  if (isPassword) {
                    nsAutoString val;
                    (void) inputElement->GetValue(val);
                    if (val.IsEmpty()) {
                      isPassword = PR_FALSE;
                    }
                  }
#define WALLET_DONT_CACHE_ALL_PASSWORDS
#ifdef WALLET_DONT_CACHE_ALL_PASSWORDS
                  if (isPassword) {
                    nsAutoString val;
                    (void) inputElement->GetAttribute(NS_LITERAL_STRING("autocomplete"), val);
                    if (val.EqualsIgnoreCase("off")) {
                      isPassword = PR_FALSE;
                    } else {
                      (void) formElement->GetAttribute(NS_LITERAL_STRING("autocomplete"), val);
                      if (val.EqualsIgnoreCase("off")) {
                        isPassword = PR_FALSE;
                      }
                    }
                  }
#endif
#ifdef AutoCapture
                  if (isPassword) {
                    passwordcount++;
                    OKToPrompt = PR_FALSE;
                  }
                  if (isText) {
                    if (passwordcount == 0 && !newValueFound && !OKToPrompt) {
                      nsAutoString valueOnForm;
                      rv = inputElement->GetValue(valueOnForm);
                      if (NS_SUCCEEDED(rv) && wallet_IsNewValue (elementNode, valueOnForm)) {
                        newValueFound = PR_TRUE;
                        if (hits > 1) {
                          OKToPrompt = PR_TRUE;
                        }
                      }
                    }
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
                        if (field.Length() && field.CharAt(0) == '\\') {
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
                        if (passwordcount == 0 && !OKToPrompt) {
                          /* get schema from field */
                          nsAutoString schema;
                          nsVoidArray* dummy;
                          nsString stripField;

                          /* try to get schema from displayable text */
                          if (schema.Length() == 0) {
                            wallet_GetSchemaFromDisplayableText(inputElement, schema, PR_FALSE);
                          }

#ifndef IgnoreFieldNames
                          /* no schema found, so try to get it from field name */
                          if (schema.Length() == 0) {
                            wallet_ReadFromList(Strip(field, stripField), schema, dummy, wallet_FieldToSchema_list, PR_FALSE);
                          }
#endif

                          /* if schema found, see if it is in distinguished schema list */
                          if (schema.Length()) {
                            /* see if schema is in distinguished list */
                            wallet_MapElement * mapElementPtr;
                            PRInt32 count = LIST_COUNT(wallet_DistinguishedSchema_list);
                            /* test for at least two distinguished schemas and no passwords */
                            for (PRInt32 i=0; i<count; i++) {
                              mapElementPtr = NS_STATIC_CAST
                                (wallet_MapElement*, wallet_DistinguishedSchema_list->ElementAt(i));
                              if (mapElementPtr->item1.EqualsIgnoreCase(schema) && value.Length() > 0) {
                                hits++;
                                if (hits > 1 && newValueFound) {
                                  OKToPrompt = PR_TRUE;
                                  break;
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
              }
            }
          }

          /* save login if appropriate */
          if (currentFormNode == formNode) {
            nsCOMPtr<nsIPrompt> dialog;
            nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
            if (wwatch)
              wwatch->GetNewPrompter(0, getter_AddRefs(dialog));

            if (dialog) {
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
                wallet_CaptureInputElement(elementNode, doc);
                wallet_CaptureSelectElement(elementNode, doc);
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
