/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *  thayes@netscape.com
 */

#include "stdlib.h"
#include "plstr.h"
#include "nsMemory.h"
#include "nsIServiceManager.h"

#include "nsISecretDecoderRing.h"

#include "cmtcmn.h"
#include "nsIPSMComponent.h"

#include "nsFSDR.h"

/*********************************************************************************
**************************** MASTER PASSWORD FUNCTIONS ***************************
*********************************************************************************/

#include "nsNetUtil.h"
#include "nsFileStream.h"
#include "nsINetSupportDialogService.h"
#include "nsIStringBundle.h"
#include "nsIFileSpec.h"
#include "nsFileLocations.h"
#include "prmem.h"
#include "prprf.h"  
#include "ntypes.h"
#include "nsIPref.h"

static NS_DEFINE_IID(kIIOServiceIID, NS_IIOSERVICE_IID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_IID(kIStringBundleServiceIID, NS_ISTRINGBUNDLESERVICE_IID);
static NS_DEFINE_IID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_IID(kIPrefServiceIID, NS_IPREF_IID);
static NS_DEFINE_IID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);

#define HEADER_VERSION_1 "#2b"

#define PROPERTIES_URL "chrome://communicator/locale/wallet/wallet.properties"

/**************************** LOCALIZATION ***************************/

PRUnichar *
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

/**************************** DIALOGS ***************************/

PRBool
Wallet_Confirm(PRUnichar * szMessage)
{
  PRBool retval = PR_TRUE; /* default value */

  nsresult res;  
  NS_WITH_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, &res);
  if (NS_FAILED(res)) {
    return retval;
  }

  const nsAutoString message(szMessage);
  retval = PR_FALSE; /* in case user exits dialog by clicking X */
  res = dialog->Confirm(nsnull, message.GetUnicode(), &retval);
  return retval;
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

/************************ PREFERENCES ***************************************/

static const char *pref_WalletKeyFileName = "wallet.KeyFileName";

void
SI_GetCharPref(const char * prefname, char** aPrefvalue) {
  nsresult ret;
  nsCOMPtr<nsIPref> pPrefService = do_GetService(kPrefServiceCID, &ret);
  if (!NS_FAILED(ret)) {
    ret = pPrefService->CopyCharPref(prefname, aPrefvalue);
    if (NS_FAILED(ret)) {
      *aPrefvalue = nsnull;
    }
  } else {
    *aPrefvalue = nsnull;
  }
}

 void
SI_SetCharPref(const char * prefname, const char * prefvalue) {
  nsresult ret;
  nsCOMPtr<nsIPref> pPrefService = do_GetService(kPrefServiceCID, &ret);
  if (!NS_FAILED(ret)) {
    ret = pPrefService->SetCharPref(prefname, prefvalue);
    if (!NS_FAILED(ret)) {
      ret = pPrefService->SavePrefFile(); 
    }
  }
}

 PRBool
SI_GetBoolPref(const char * prefname, PRBool defaultvalue) {
  nsresult ret;
  PRBool prefvalue = defaultvalue;
  nsCOMPtr<nsIPref> pPrefService = do_GetService(kPrefServiceCID, &ret);
  if (!NS_FAILED(ret)) {
    ret = pPrefService->GetBoolPref(prefname, &prefvalue);
  }
  return prefvalue;
}

 void
SI_SetBoolPref(const char * prefname, PRBool prefvalue) {
  nsresult ret;
  nsCOMPtr<nsIPref> pPrefService = do_GetService(kPrefServiceCID, &ret);
  if (!NS_FAILED(ret)) {
    ret = pPrefService->SetBoolPref(prefname, prefvalue);
    if (!NS_FAILED(ret)) {
      ret = pPrefService->SavePrefFile(); 
    }
  }
}


/************************ UTILITIES *****************************************/

nsresult Wallet_ProfileDirectory(nsFileSpec& dirSpec) {
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

char *
Wallet_RandomName(char* suffix)
{
  /* pick the current time as the random number */
  time_t curTime = time(NULL);

  /* take 8 least-significant digits + three-digit suffix as the file name */
  char name[13];
  PR_snprintf(name, 13, "%lu.%s", (((int)curTime)%100000000), suffix);
  return PL_strdup(name);
}

void
Wallet_Put(nsOutputFileStream strm, PRUnichar c) {
    strm.put((char)c);
}

PRUnichar
Wallet_Get(nsInputFileStream strm) {
  return (strm.get() & 0xFF);
}

PRInt32
wallet_GetLine(nsInputFileStream strm, nsAutoString& line) {

  /* read the line */
  line.SetLength(0);
  PRUnichar c;
  for (;;) {
    c = Wallet_Get(strm);
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
wallet_PutLine(nsOutputFileStream strm, const nsAutoString& line)
{
  for (PRUint32 i=0; i<line.Length(); i++) {
    Wallet_Put(strm, line.CharAt(i));
  }
  Wallet_Put(strm, '\n');
}

/************************ MASTER PASSWORD CONTROL ***************************/

nsAutoString gKey;
PRBool gIsKeySet = PR_FALSE;
PRBool gKeyCancel = PR_FALSE;
char* gKeyFileName = nsnull;

// 30 minute duration (60*30=1800 seconds)
#define keyDuration 1800
time_t gKeyExpiresTime;

void
Wallet_InitKeySet(PRBool b) {
  gIsKeySet = b;
}

PRBool
Wallet_IsKeySet() {
  return gIsKeySet;
}

PRUnichar
Wallet_GetKey(PRInt32 writeCount) {
  nsresult rv = NS_OK;
  PRUint8 keyByte1 = 0, keyByte2 = 0;
  NS_ASSERTION(gKey.Length()>0, "Master Password was never established");
  if (gKey.Length() > 0 ) {
    return gKey.CharAt((PRInt32)(writeCount % gKey.Length()));
  } else {
    return '~'; /* What else can we do?  We can't recover from this. */
  }
}

PRBool
Wallet_KeyTimedOut() {
  time_t curTime = time(NULL);
  if (Wallet_IsKeySet() && (curTime >= gKeyExpiresTime)) {
    Wallet_InitKeySet(PR_FALSE);
    return PR_TRUE;
  }
  return PR_FALSE;
}

void
Wallet_KeyResetTime() {
  if (Wallet_IsKeySet()) {
    gKeyExpiresTime = time(NULL) + keyDuration;
  }
}

void
WLLT_ExpirePassword() {
  if (Wallet_IsKeySet()) {
    gKeyExpiresTime = time(NULL);
  }
}

PRBool
Wallet_CancelKey() {
  return gKeyCancel;
}

 void
wallet_InitKeyFileName() {
  static PRBool namesInitialized = PR_FALSE;
  if (!namesInitialized) {
    SI_GetCharPref(pref_WalletKeyFileName, &gKeyFileName);
    if (!gKeyFileName) {
      gKeyFileName = Wallet_RandomName("k");
      SI_SetCharPref(pref_WalletKeyFileName, gKeyFileName);
    }
    namesInitialized = PR_TRUE;
  }
}

/* returns -1 if key does not exist, 0 if key is of length 0, 1 otherwise */
PRInt32
Wallet_KeySize() {
  wallet_InitKeyFileName();
  nsFileSpec dirSpec;
  nsresult rv = Wallet_ProfileDirectory(dirSpec);
  if (NS_FAILED(rv)) {
    return -1;
  }
  nsInputFileStream strm(dirSpec + gKeyFileName);
  if (!strm.is_open()) {
    return -1;
  } else {
#define BUFSIZE 128
    char buffer[BUFSIZE];
    PRInt32 count;
    count = strm.read(buffer, BUFSIZE);
    strm.close();
    nsAutoString temp; temp.AssignWithConversion(buffer);
    PRInt32 start = 0;
    for (PRInt32 i=0; i<1; i++) { /* skip over the one-line header */
      start = temp.FindChar('\n', PR_FALSE, start);
      if (start == -1) {
        return -1; /* this should never happen, but just in case */
      }
      start++;
    }
    return ((start < count) ? 1 : 0);
  }
}

PRBool
wallet_ReadKeyFile(PRBool useDefaultKey) {
  PRInt32 writeCount = 0;

  if (useDefaultKey && (Wallet_KeySize() == 0) ) {
    Wallet_InitKeySet(PR_TRUE);
    return PR_TRUE;
  }

  nsFileSpec dirSpec;
  nsresult rval = Wallet_ProfileDirectory(dirSpec);
  if (NS_FAILED(rval)) {
    gKeyCancel = PR_TRUE;
    return PR_FALSE;
  }
  nsInputFileStream strm(dirSpec + gKeyFileName);
  writeCount = 0;

  /* read the header */
  nsAutoString format;
  if (NS_FAILED(wallet_GetLine(strm, format))) {
    return PR_FALSE;
  }
  if (!format.EqualsWithConversion(HEADER_VERSION_1)) {
    /* something's wrong */
    return PR_FALSE;
  }

  /*
   * Note that eof() is not set until after we read past the end of the file.  That
   * is why the following code reads a character and immediately after the read
   * checks for eof()
   */

  for (PRUint32 j = 1; j < gKey.Length(); j++) {
    if (Wallet_Get(strm) != ((gKey.CharAt(j))^Wallet_GetKey(writeCount++))
        || strm.eof()) {
      strm.close();
      Wallet_InitKeySet(PR_FALSE);
      gKey.SetLength(0);
      gKeyCancel = PR_FALSE;
      return PR_FALSE;
    }
  }
  if (gKey.Length() != 0) {
    if (Wallet_Get(strm) != ((gKey.CharAt(0))^Wallet_GetKey(writeCount++))
        || strm.eof()) {
      strm.close();
      Wallet_InitKeySet(PR_FALSE);
      gKey.SetLength(0);
      gKeyCancel = PR_FALSE;
      return PR_FALSE;
    }
  }

  Wallet_Get(strm); /* to get past the end of the file so eof() will get set */
  PRBool rv = strm.eof();
  strm.close();
  if (rv) {
    Wallet_InitKeySet(PR_TRUE);
    gKeyExpiresTime = time(NULL) + keyDuration;
    return PR_TRUE;
  } else {
    Wallet_InitKeySet(PR_FALSE);
    gKey.SetLength(0);
    gKeyCancel = PR_FALSE;
    return PR_FALSE;
  }
}

PRBool
wallet_WriteKeyFile(PRBool useDefaultKey) {
  PRInt32 writeCount = 0;

  nsFileSpec dirSpec;
  nsresult rval = Wallet_ProfileDirectory(dirSpec);
  if (NS_FAILED(rval)) {
    gKeyCancel = PR_TRUE;
    return PR_FALSE;
  }

  nsOutputFileStream strm2(dirSpec + gKeyFileName);
  if (!strm2.is_open()) {
    gKey.SetLength(0);
    gKeyCancel = PR_TRUE;
    return PR_FALSE;
  }

  /* write out the header information */
  {
    nsAutoString temp1;
    temp1.AssignWithConversion(HEADER_VERSION_1);
    wallet_PutLine(strm2, temp1);
  }

  /* If we store the key obscured by the key itself, then the result will be zero
   * for all keys (since we are using XOR to obscure).  So instead we store
   * key[1..n],key[0] obscured by the actual key.
   */

  if (!useDefaultKey && (gKey.Length() != 0)) {
    for (PRUint32 i = 1; i < gKey.Length(); i++) {
      Wallet_Put(strm2, (gKey.CharAt(i))^Wallet_GetKey(writeCount++));
    }
    Wallet_Put(strm2, (gKey.CharAt(0))^Wallet_GetKey(writeCount++));
  }
  strm2.flush();
  strm2.close();
  Wallet_InitKeySet(PR_TRUE);
  return PR_TRUE;
}

PRBool
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
        gKeyCancel = PR_TRUE;
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
            gKeyCancel = PR_TRUE;
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
        gKeyCancel = PR_TRUE;
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
  gKey = newkey;

  if (isNewkey || (Wallet_KeySize() < 0)) {
    /* Either key is to be changed or the file containing the saved key doesn't exist */
    /* In either case we need to (re)create and re(write) the file */
    return wallet_WriteKeyFile(useDefaultKey);
  } else {
    /* file of saved key existed so see if it matches the key the user typed in */

    if (!wallet_ReadKeyFile(useDefaultKey)) {
      return PR_FALSE;
    }

    /* it matched */
    return PR_TRUE;
  }
}

/*********************************************************************************
************************ END OF MASTER PASSWORD FUNCTIONS ************************
*********************************************************************************/


NS_IMPL_ISUPPORTS1(nsFSecretDecoderRing, nsISecretDecoderRing)

nsFSecretDecoderRing::nsFSecretDecoderRing()
{
  NS_INIT_ISUPPORTS();

  mPSM = NULL;
}

nsFSecretDecoderRing::~nsFSecretDecoderRing()
{
  if (mPSM) mPSM->Release();
}

/* Init the new instance */
nsresult nsFSecretDecoderRing::
init()
{
  nsresult rv;
  nsISupports *psm;

  rv = nsServiceManager::GetService(kPSMComponentProgID, NS_GET_IID(nsIPSMComponent),
                                    &psm);
  if (rv != NS_OK) goto loser;  /* Should promote error */

  mPSM = (nsIPSMComponent *)psm;

loser:
  return rv;
}

PRBool
encryptCheck() {
  static PRBool wallet_keyInitialized = PR_FALSE;

  /* see if key has timed out */
  if (Wallet_KeyTimedOut()) {
    wallet_keyInitialized = PR_FALSE;
  }

  if (!wallet_keyInitialized) {
    PRUnichar * message = Wallet_Localize("IncorrectKey_TryAgain?");
    while (!Wallet_SetKey(PR_FALSE)) {
      if (Wallet_CancelKey() || (Wallet_KeySize() < 0) || !Wallet_Confirm(message)) {
        Recycle(message);
        return PR_FALSE;
      }
    }
    Recycle(message);
    wallet_keyInitialized = PR_TRUE;
  }

  /* restart key timeout period */
  Wallet_KeyResetTime();
  return PR_TRUE;
}


/* [noscript] long encrypt (in buffer data, in long dataLen, out buffer result); */
NS_IMETHODIMP nsFSecretDecoderRing::
Encrypt(unsigned char * data, PRInt32 dataLen, unsigned char * *result, PRInt32 *_retval)
{
    nsresult rv = NS_OK;
    unsigned char *r = 0;
    CMT_CONTROL *control;
    CMTStatus status;
    CMUint32 cLen;

    if (data == nsnull || result == nsnull || _retval == nsnull) {
       rv = NS_ERROR_INVALID_POINTER;
       goto loser;
    }

    /* Check object initialization */
    NS_ASSERTION(mPSM != nsnull, "SDR object not initialized");
    if (mPSM == nsnull) { rv = NS_ERROR_NOT_INITIALIZED; goto loser; }




    /* Get the control connect to use for the request */
    rv = mPSM->GetControlConnection(&control);
    if (rv != NS_OK) { rv = NS_ERROR_NOT_AVAILABLE; goto loser; }
   
    status = CMT_SDREncrypt(control, (void *)0, (const unsigned char *)0, 0, 
               data, dataLen, result, &cLen);
    if (status != CMTSuccess) { rv = NS_ERROR_FAILURE; goto loser; } /* XXX */

    /* Copy returned data to nsMemory buffer ? */
    *_retval = cLen;

loser:
    return rv;
}

/* [noscript] long decrypt (in buffer data, in long dataLen, out buffer result); */
NS_IMETHODIMP nsFSecretDecoderRing::
Decrypt(unsigned char * data, PRInt32 dataLen, unsigned char * *result, PRInt32 *_retval)
{
    nsresult rv = NS_OK;
    CMTStatus status;
    CMT_CONTROL *control;
    CMUint32 len;

    if (data == nsnull || result == nsnull || _retval == nsnull) {
       rv = NS_ERROR_INVALID_POINTER;
       goto loser;
    }

    /* Check object initialization */
    NS_ASSERTION(mPSM != nsnull, "SDR object not initialized");
    if (mPSM == nsnull) { rv = NS_ERROR_NOT_INITIALIZED; goto loser; }

    /* Get the control connection */
    rv = mPSM->GetControlConnection(&control);
    if (rv != NS_OK) { rv = NS_ERROR_NOT_AVAILABLE; goto loser; }
    
    /* Call PSM to decrypt the value */
    status = CMT_SDRDecrypt(control, (void *)0, data, dataLen, result, &len);
    if (status != CMTSuccess) { rv = NS_ERROR_FAILURE; goto loser; } /* Promote? */

    /* Copy returned data to nsMemory buffer ? */
    *_retval = len;

loser:
    return rv;
}

/* string encryptString (in string text); */
NS_IMETHODIMP nsFSecretDecoderRing::
EncryptString(const char *text, char **_retval)
{
  if (!encryptCheck()) {
    return  NS_ERROR_FAILURE;
  }

////  *_retval = (char *)PR_Malloc( PL_strlen("Encrypted:") + PL_strlen(text) + 1);
////  PL_strcpy(*_retval, "Encrypted:");
////  PL_strcat(*_retval, text);
//  *_retval = PL_strdup(text);
//  for (PRUint32 i=0; i<PL_strlen(text); i++) {
//      *_retval[i] = text[i] + 1;
//  }

  PRInt32 writeCount = 0;
  *_retval = (char *)PR_Malloc(2*PL_strlen(text) + 1);
  for (PRUint32 i=0; i<PL_strlen(text); i++) {
    char ret = text[i]^Wallet_GetKey(writeCount++);
    (*_retval)[2*i] = '0' | (ret >> 4);
    (*_retval)[2*i+1] = '0' | (ret & 0x0F);
  }
  (*_retval)[2*PL_strlen(text)] = '\0';

  return NS_OK;

    nsresult rv = NS_OK;
    unsigned char *encrypted = 0;
    PRInt32 eLen;

    if (text == nsnull || _retval == nsnull) {
        rv = NS_ERROR_INVALID_POINTER;
        goto loser;
    }

    rv = Encrypt((unsigned char *)text, PL_strlen(text), &encrypted, &eLen);
    if (rv != NS_OK) { goto loser; }

    rv = encode(encrypted, eLen, _retval);

loser:
    if (encrypted) nsMemory::Free(encrypted);

    return rv;
}

/* string decryptString (in string crypt); */
NS_IMETHODIMP nsFSecretDecoderRing::
DecryptString(const char *crypt, char **_retval)
{
  if (!encryptCheck()) {
    return  NS_ERROR_FAILURE;
  }

////  if (PL_strncmp(crypt, "Encrypted:", PL_strlen("Encrypted:")) != 0) {
////    return NS_ERROR_FAILURE;
////  }
//  *_retval = PL_strdup(crypt);
//  for (PRUint32 i=0; i<PL_strlen(crypt); i++) {
//      *_retval[i] = crypt[i] - 1;
//  }

  PRInt32 writeCount = 0;
  *_retval = (char *)PR_Malloc((PL_strlen(crypt)/2) + 1);
  for (PRUint32 i=0; i<(PL_strlen(crypt)/2); i++) {
    (*_retval)[i] =
      (((crypt[2*i] & 0x0f)<<4) + (crypt[2*i+1] & 0x0f))^Wallet_GetKey(writeCount++);
  }
  (*_retval)[PL_strlen(crypt)/2] = '\0';

  return NS_OK;

    nsresult rv = NS_OK;
    char *r = 0;
    unsigned char *decoded = 0;
    PRInt32 decodedLen;
    unsigned char *decrypted = 0;
    PRInt32 decryptedLen;

    if (crypt == nsnull || _retval == nsnull) {
      rv = NS_ERROR_INVALID_POINTER;
      goto loser;
    }

    rv = decode(crypt, &decoded, &decodedLen);
    if (rv != NS_OK) goto loser;

    rv = Decrypt(decoded, decodedLen, &decrypted, &decryptedLen);
    if (rv != NS_OK) goto loser;

    // Convert to NUL-terminated string
    r = (char *)nsMemory::Alloc(decryptedLen+1);
    if (!r) { rv = NS_ERROR_OUT_OF_MEMORY; goto loser; }

    memcpy(r, decrypted, decryptedLen);
    r[decryptedLen] = 0;

    *_retval = r;
    r = 0;

loser:
    if (r) nsMemory::Free(r);
    if (decrypted) nsMemory::Free(decrypted);
    if (decoded) nsMemory::Free(decoded);
 
    return rv;
}

/* void changePassword(); */
NS_IMETHODIMP nsFSecretDecoderRing::
ChangePassword()
{
  if (Wallet_KeySize() < 0) {

    /* have user create database key if one was never created */
    PRUnichar * message = Wallet_Localize("IncorrectKey_TryAgain?");
    while (!Wallet_SetKey(PR_FALSE)) {
      if (Wallet_CancelKey() || (Wallet_KeySize() < 0) || !Wallet_Confirm(message)) {
        Recycle(message);
        return NS_ERROR_FAILURE;
      }
    }
    Recycle(message);
    return NS_OK;
  }

  /* obscure the data if it is encrypted */

  PRBool encrypted = SI_GetBoolPref("wallet.crypto", PR_TRUE);
  if (encrypted) {
    SI_SetBoolPref("wallet.crypto", PR_FALSE);
  }

  /* force the user to supply old database key, for security */
  nsresult rv = NS_OK;
  WLLT_ExpirePassword();
  Wallet_InitKeySet(PR_FALSE);
  if (!Wallet_SetKey(PR_FALSE)) {
    rv = NS_ERROR_FAILURE;
  }

  /* establish new key */
  if (!Wallet_SetKey(PR_TRUE)) {
    rv = NS_ERROR_FAILURE;
  }

  if (encrypted) {
    SI_SetBoolPref("wallet.crypto", PR_TRUE);
  }

  return rv;

}

/* void logout(); */
NS_IMETHODIMP nsFSecretDecoderRing::
Logout()
{
  WLLT_ExpirePassword();
  return NS_OK;
}

nsresult nsFSecretDecoderRing::
encode(const unsigned char *data, PRInt32 dataLen, char **_retval)
{
    nsresult rv = NS_OK;
    char *r = 0;

    // Allocate space for encoded string (with NUL)
    r = (char *)nsMemory::Alloc(dataLen+1);
    if (!r) { rv = NS_ERROR_OUT_OF_MEMORY; goto loser; }

    memcpy(r, data, dataLen);
    r[dataLen] = 0;

    *_retval = r;
    r = 0;

loser:
    if (r) nsMemory::Free(r);

    return rv;
}

nsresult nsFSecretDecoderRing::
decode(const char *data, unsigned char **result, PRInt32 * _retval)
{
    nsresult rv = NS_OK;
    unsigned char *r = 0;
    PRInt32 rLen;

    // Allocate space for decoded string (missing NUL)
    rLen = PL_strlen(data);
    r = (unsigned char *)nsMemory::Alloc(rLen);
    if (!r) { rv = NS_ERROR_OUT_OF_MEMORY; goto loser; }

    memcpy(r, data, rLen);

    *result = r;
    r = 0;
    *_retval = rLen;

loser:
    if (r) nsMemory::Free(r);

    return rv;
}

const char * nsFSecretDecoderRing::kPSMComponentProgID = PSM_COMPONENT_PROGID;
