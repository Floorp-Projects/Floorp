/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#define alphabetize 1

#include "singsign.h"

#ifdef XP_MAC
#include "prpriv.h"             /* for NewNamedMonitor */
#include "prinrval.h"           /* for PR_IntervalNow */
#ifdef APPLE_KEYCHAIN                                   /* APPLE */
#include "Keychain.h"                                   /* APPLE */
#define kNetscapeProtocolType   'form'  /* APPLE */
#endif                                                                  /* APPLE */
#else
#include "private/prpriv.h"     /* for NewNamedMonitor */
#endif

#include "nsIPref.h"
#include "nsFileStream.h"
#include "nsSpecialSystemDirectory.h"
#include "nsINetSupportDialogService.h"
#include "nsIServiceManager.h"
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsIDOMHTMLDocument.h"
#include "prmem.h"
#include "prprf.h"  
#include "nsVoidArray.h"
#include "nsXPIDLString.h"

typedef PRInt32 nsKeyType;

static NS_DEFINE_IID(kIPrefServiceIID, NS_IPREF_IID);
static NS_DEFINE_IID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kStandardUrlCID, NS_STANDARDURL_CID);


/********************
 * Global Variables *
 ********************/

/* locks for signon cache */

static PRMonitor * signon_lock_monitor = NULL;
static PRThread  * signon_lock_owner = NULL;
static int signon_lock_count = 0;

/* load states */

static PRBool si_PartiallyLoaded = PR_FALSE;
static PRBool si_FullyLoaded = PR_FALSE;
static PRBool si_UserHasBeenSelected = PR_FALSE;

/* apple keychain stuff */

#ifdef APPLE_KEYCHAIN
static PRBool     si_list_invalid = PR_FALSE;
static KCCallbackUPP si_kcUPP = NULL;
PRIVATE int
si_SaveSignonDataInKeychain();
#endif

#define USERNAMEFIELD "\\=username=\\"
#define PASSWORDFIELD "\\=password=\\"

/*************************************
 * Externs to Routines in Wallet.cpp *
 *************************************/

extern nsresult Wallet_ProfileDirectory(nsFileSpec& dirSpec);
extern PRUnichar * Wallet_Localize(char * genericString);
extern void Wallet_StreamGeneratorReset();


/***********
 * Dialogs *
 ***********/

extern PRBool Wallet_ConfirmYN(PRUnichar * szMessage);
extern void Wallet_Alert(PRUnichar * szMessage);

PRIVATE void
si_Alert(PRUnichar * szMessage) {
  Wallet_Alert(szMessage);
}

PRIVATE PRBool
si_ConfirmYN(PRUnichar * szMessage) {
  return Wallet_ConfirmYN(szMessage);
}

extern PRInt32 Wallet_3ButtonConfirm(PRUnichar * szMessage);

PRIVATE PRInt32
si_3ButtonConfirm(PRUnichar * szMessage) {
  return Wallet_3ButtonConfirm(szMessage);
}

// This will go away once select is passed a prompter interface
#include "nsAppShellCIDs.h" // TODO remove later
#include "nsIAppShellService.h" // TODO remove later
#include "nsIXULWindow.h" // TODO remove later
static NS_DEFINE_CID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);

PRIVATE PRBool
si_SelectDialog(const PRUnichar* szMessage, PRUnichar** pList, PRInt32* pCount) {
  if (si_UserHasBeenSelected) {
    /* a user was already selected for this form, use same one again */
    *pCount = 0; /* last user selected is now at head of list */
    return PR_TRUE;
  }

  nsresult rv;
  NS_WITH_SERVICE(nsIAppShellService, appshellservice, kAppShellServiceCID, &rv);
  if(NS_FAILED(rv)) {
    return PR_FALSE;
  }
  nsCOMPtr<nsIXULWindow> xulWindow;
  appshellservice->GetHiddenWindow(getter_AddRefs(xulWindow));
  nsCOMPtr<nsIPrompt> prompter(do_QueryInterface(xulWindow));
  PRInt32 selectedIndex;
  PRBool rtnValue;
  PRUnichar * title_string = Wallet_Localize("SelectUserTitleLine");
  rv = prompter->Select( title_string, szMessage, *pCount, NS_CONST_CAST(const PRUnichar**, pList), &selectedIndex, &rtnValue );
  Recycle(title_string);
  *pCount = selectedIndex;
  si_UserHasBeenSelected = PR_TRUE;
  return rtnValue;  
}

nsresult
si_CheckGetPassword
  (PRUnichar ** password,
  const PRUnichar * szMessage,
  PRBool* checkValue)
{
  nsresult res;  
  NS_WITH_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, &res);
  if (NS_FAILED(res)) {
    return res;
  }

  PRInt32 buttonPressed = 1; /* in case user exits dialog by clickin X */
  PRUnichar * prompt_string = Wallet_Localize("PromptForPassword");
  PRUnichar * check_string = Wallet_Localize("SaveThisValue");

  res = dialog->UniversalDialog(
    NULL, /* title message */
    prompt_string, /* title text in top line of window */
    szMessage, /* this is the main message */
    check_string, /* This is the checkbox message */
    NULL, /* first button text, becomes OK by default */
    NULL, /* second button text, becomes CANCEL by default */
    NULL, /* third button text */
    NULL, /* fourth button text */
    NULL, /* first edit field label */
    NULL, /* second edit field label */
    password, /* first edit field initial and final value */
    NULL, /* second edit field initial and final value */
    NULL,  /* icon: question mark by default */
    checkValue, /* initial and final value of checkbox */
    2, /* number of buttons */
    1, /* number of edit fields */
    1, /* is first edit field a password field */
    &buttonPressed);

  Recycle(prompt_string);
  Recycle(check_string);

  if (NS_FAILED(res)) {
    return res;
  }
  if (buttonPressed == 0) {
    return NS_OK;
  } else {
    return NS_ERROR_FAILURE; /* user pressed cancel */
  }
}

nsresult
si_CheckGetData
  (PRUnichar ** data,
  const PRUnichar * szMessage,
  PRBool* checkValue)
{
  nsresult res;  
  NS_WITH_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, &res);
  if (NS_FAILED(res)) {
    return res;
  }

  PRInt32 buttonPressed = 1; /* in case user exits dialog by clickin X */
  PRUnichar * prompt_string = Wallet_Localize("PromptForData");
  PRUnichar * check_string = Wallet_Localize("SaveThisValue");

  res = dialog->UniversalDialog(
    NULL, /* title message */
    prompt_string, /* title text in top line of window */
    szMessage, /* this is the main message */
    check_string, /* This is the checkbox message */
    NULL, /* first button text, becomes OK by default */
    NULL, /* second button text, becomes CANCEL by default */
    NULL, /* third button text */
    NULL, /* fourth button text */
    NULL, /* first edit field label */
    NULL, /* second edit field label */
    data, /* first edit field initial and final value */
    NULL, /* second edit field initial and final value */
    NULL,  /* icon: question mark by default */
    checkValue, /* initial and final value of checkbox */
    2, /* number of buttons */
    1, /* number of edit fields */
    0, /* is first edit field a password field */
    &buttonPressed);

  Recycle(prompt_string);
  Recycle(check_string);

  if (NS_FAILED(res)) {
    return res;
  }
  if (buttonPressed == 0) {
    return NS_OK;
  } else {
    return NS_ERROR_FAILURE; /* user pressed cancel */
  }
}

nsresult
si_CheckGetUsernamePassword
  (PRUnichar ** username,
  PRUnichar ** password,
  const PRUnichar * szMessage,
  PRBool* checkValue)
{
  nsresult res;  
  NS_WITH_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, &res);
  if (NS_FAILED(res)) {
    return res;
  }

  PRInt32 buttonPressed = 1; /* in case user exits dialog by clickin X */
  PRUnichar * prompt_string = Wallet_Localize("PromptForPassword");
  PRUnichar * check_string = Wallet_Localize("SaveTheseValues");
  PRUnichar * user_string = Wallet_Localize("UserName");
  PRUnichar * password_string = Wallet_Localize("Password");

  res = dialog->UniversalDialog(
    NULL, /* title message */
    prompt_string, /* title text in top line of window */
    szMessage, /* this is the main message */
    check_string, /* This is the checkbox message */
    NULL, /* first button text, becomes OK by default */
    NULL, /* second button text, becomes CANCEL by default */
    NULL, /* third button text */
    NULL, /* fourth button text */
    user_string, /* first edit field label */
    password_string, /* second edit field label */
    username, /* first edit field initial and final value */
    password, /* second edit field initial and final value */
    NULL,  /* icon: question mark by default */
    checkValue, /* initial and final value of checkbox */
    2, /* number of buttons */
    2, /* number of edit fields */
    0, /* is first edit field a password field */
    &buttonPressed);

  Recycle(prompt_string);
  Recycle(check_string);
  Recycle(user_string);
  Recycle(password_string);

  if (NS_FAILED(res)) {
    return res;
  }
  if (buttonPressed == 0) {
    return NS_OK;
  } else {
    return NS_ERROR_FAILURE; /* user pressed cancel */
  }
}


/******************
 * Key Management *
 ******************/

extern PRUnichar Wallet_GetKey(nsKeyType saveCount, nsKeyType writeCount);
extern PRBool Wallet_IsKeySet();
extern void Wallet_KeyResetTime();
extern PRBool Wallet_KeyTimedOut();
extern PRBool Wallet_SetKey(PRBool newkey);
extern PRInt32 Wallet_KeySize();
extern PRBool Wallet_CancelKey();

char* signonFileNameP = nsnull;
char* signonFileNameU = nsnull;

PRIVATE PRUnichar
si_GetKey(nsKeyType saveCount, nsKeyType writeCount) {
  return Wallet_GetKey(saveCount, writeCount);
}

PRIVATE PRBool
si_KeySet() {
  return Wallet_IsKeySet();
}

PRIVATE PRBool
si_SetKey() {
  return Wallet_SetKey(PR_FALSE);
}

PRIVATE void
si_KeyResetTime() {
  Wallet_KeyResetTime();
}

PRIVATE PRBool
si_KeyTimedOut() {
  return Wallet_KeyTimedOut();
}


/***************************
 * Locking the Signon List *
 ***************************/

PRIVATE void
si_lock_signon_list(void) {
  if(!signon_lock_monitor) {
    signon_lock_monitor = PR_NewNamedMonitor("signon-lock");
  }
  PR_EnterMonitor(signon_lock_monitor);
  while(PR_TRUE) {

    /* no current owner or owned by this thread */
    PRThread * t = PR_CurrentThread();
    if(signon_lock_owner == NULL || signon_lock_owner == t) {
      signon_lock_owner = t;
      signon_lock_count++;
      PR_ExitMonitor(signon_lock_monitor);
      return;
    }

    /* owned by someone else -- wait till we can get it */
    PR_Wait(signon_lock_monitor, PR_INTERVAL_NO_TIMEOUT);
  }
}

PRIVATE void
si_unlock_signon_list(void) {
    PR_EnterMonitor(signon_lock_monitor);

#ifdef DEBUG
    /* make sure someone doesn't try to free a lock they don't own */
    PR_ASSERT(signon_lock_owner == PR_CurrentThread());
#endif

    signon_lock_count--;
    if(signon_lock_count == 0) {
        signon_lock_owner = NULL;
        PR_Notify(signon_lock_monitor);
    }
    PR_ExitMonitor(signon_lock_monitor);
}


/********************************
 * Preference Utility Functions *
 ********************************/

typedef int (*PrefChangedFunc) (const char *, void *);

PUBLIC void
SI_RegisterCallback(const char* domain, PrefChangedFunc callback, void* instance_data) {
  nsresult ret;
  nsCOMPtr<nsIPref> pPrefService = do_GetService(kPrefServiceCID, &ret);
  if (!NS_FAILED(ret)) {
    ret = pPrefService->RegisterCallback(domain, callback, instance_data);
  }
}

PUBLIC void
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

PUBLIC PRBool
SI_GetBoolPref(const char * prefname, PRBool defaultvalue) {
  nsresult ret;
  PRBool prefvalue = defaultvalue;
  nsCOMPtr<nsIPref> pPrefService = do_GetService(kPrefServiceCID, &ret);
  if (!NS_FAILED(ret)) {
    ret = pPrefService->GetBoolPref(prefname, &prefvalue);
  }
  return prefvalue;
}

PUBLIC void
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

PUBLIC void
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


/*********************************
 * Preferences for Single Signon *
 *********************************/

static const char *pref_rememberSignons = "signon.rememberSignons";
#ifdef DefaultIsOff
static const char *pref_Notified = "signon.Notified";
#endif
static const char *pref_SignonFileNameP = "signon.SignonPasswordFileName";
static const char *pref_SignonFileNameU = "signon.SignonURLFileName";

PRIVATE PRBool si_RememberSignons = PR_FALSE;
#ifdef DefaultIsOff
PRIVATE PRBool si_Notified = PR_FALSE;
#endif

PRIVATE int
si_SaveSignonDataLocked(PRBool fullSave);

PUBLIC int
SI_LoadSignonData(PRBool fullLoad);

PUBLIC void
SI_RemoveAllSignonData();

#ifdef DefaultIsOff
PRIVATE PRBool
si_GetNotificationPref(void) {
  return si_Notified;
}

PRIVATE void
si_SetNotificationPref(PRBool x) {
  SI_SetBoolPref(pref_Notified, x);
  si_Notified = x;
}
#endif

PRIVATE void
si_SetSignonRememberingPref(PRBool x) {
#ifdef APPLE_KEYCHAIN
  if (x == 0) {
    /* We no longer need the Keychain callback installed */
    KCRemoveCallback( si_kcUPP );
    DisposeRoutineDescriptor( si_kcUPP );
    si_kcUPP = NULL;
  }
#endif
  si_RememberSignons = x;
}

MODULE_PRIVATE int PR_CALLBACK
si_SignonRememberingPrefChanged(const char * newpref, void * data) {
    PRBool x;
    x = SI_GetBoolPref(pref_rememberSignons, PR_TRUE);
    si_SetSignonRememberingPref(x);
    return 0; /* this is PREF_NOERROR but we no longer include prefapi.h */
}

PRIVATE void
si_RegisterSignonPrefCallbacks(void) {
  PRBool x;
  static PRBool first_time = PR_TRUE;
  if(first_time) {
    first_time = PR_FALSE;
    SI_LoadSignonData(PR_FALSE);
#ifdef DefaultIsOff
    x = SI_GetBoolPref(pref_Notified, PR_FALSE);
    si_SetNotificationPref(x);        
#endif
    x = SI_GetBoolPref(pref_rememberSignons, PR_FALSE);
    si_SetSignonRememberingPref(x);
    SI_RegisterCallback(pref_rememberSignons, si_SignonRememberingPrefChanged, NULL);
  }
}

PRIVATE PRBool
si_GetSignonRememberingPref(void) {
#ifdef APPLE_KEYCHAIN
  /* If the Keychain has been locked or an item deleted or updated,
   * we need to reload the signon data
   */
  if (si_list_invalid) {
    /*
     * set si_list_invalid to PR_FALSE first because SI_RemoveAllSignonData
     * calls si_GetSignonRememberingPref
     */
    si_list_invalid = PR_FALSE;
    SI_LoadSignonData(PR_FALSE);
  }
#endif

  si_RegisterSignonPrefCallbacks();

#ifdef DefaultIsOff
  /*
   * We initially want the rememberSignons pref to be PR_FALSE.  But this will
   * prevent the notification message from ever occurring.  To get around
   * this problem, if the signon pref is PR_FALSE and no notification has
   * ever been given, we will treat this as if the signon pref were PR_TRUE.
   */
  if (!si_RememberSignons && !si_GetNotificationPref()) {
    return PR_TRUE;
  } else {
    return si_RememberSignons;
  }
#else
  return si_RememberSignons;
#endif
}

extern char* Wallet_RandomName(char* suffix);

PUBLIC void
SI_InitSignonFileName() {
  SI_GetCharPref(pref_SignonFileNameP, &signonFileNameP);
  if (!signonFileNameP) {
    signonFileNameP = Wallet_RandomName("p");
    SI_SetCharPref(pref_SignonFileNameP, signonFileNameP);
  }
  SI_GetCharPref(pref_SignonFileNameU, &signonFileNameU);
  if (!signonFileNameU) {
    signonFileNameU = Wallet_RandomName("u");
    SI_SetCharPref(pref_SignonFileNameU, signonFileNameU);
  }
}


/********************
 * Utility Routines *
 ********************/

/* StrAllocCopy should really be defined elsewhere */
#include "plstr.h"
#include "prmem.h"

#undef StrAllocCopy
#define StrAllocCopy(dest, src) Local_SACopy (&(dest), src)
PRIVATE char *
Local_SACopy(char **destination, const char *source) {
  if(*destination) {
    PL_strfree(*destination);
    *destination = 0;
  }
  *destination = PL_strdup(source);
  return *destination;
}

/* Remove misleading portions from URL name */

const char* empty = "empty";

PRIVATE char*
si_StrippedURL (const char* URLName) {
  char *result = 0;
  char *s, *t;

  /* check for null URLName */
  if (URLName == NULL || PL_strlen(URLName) == 0) {
    return NULL;
  }

  /* remove protocol */
  s = (char*) PL_strchr(URLName +1, '/');
  if (s && *s == '/' && *(s+1) == '/') {
    s += 2;
  }
  if (s) {
    StrAllocCopy(result, s);
  } else {
    StrAllocCopy(result, URLName);
  }

  /* remove everything after hostname */
  s = result;
  s = (char*) PL_strchr(s, '/');
  if (s) {
    *s = '\0';
  }

  /* remove everything after and including first question mark */
  s = result;
  s = (char*) PL_strchr(s, '?');
  if (s) {
    *s = '\0';
  }

  /* remove socket number from result */
  s = result;
  while ((s = (char*) PL_strchr(s+1, ':'))) {
    /* s is at next colon */
    if (*(s+1) != '/') {
      /* and it's not :// so it must be start of socket number */
      if ((t = (char*) PL_strchr(s+1, '/'))) {
        /* t is at slash terminating the socket number */
        do {
          /* copy remainder of t to s */
          *s++ = *t;
        } while (*(t++));
      }
      break;
    }
  }

  /* all done */
  if (PL_strlen(result)) {
    return result;
  } else {
    return PL_strdup(empty);
  }
}

/* remove terminating CRs or LFs */
PRIVATE void
si_StripLF(nsAutoString buffer) {
  PRUnichar c;
  while ((c=buffer.CharAt(buffer.Length())-1) == '\n' || c == '\r' ) {
    buffer.SetLength(buffer.Length()-1);
  }
}

/* If user-entered password is "********", then generate a random password */
PRIVATE void
si_Randomize(nsAutoString& password) {
  PRIntervalTime randomNumber;
  int i;
  const char * hexDigits = "0123456789AbCdEf";
  if (password == nsAutoString("********")) {
    randomNumber = PR_IntervalNow();
    for (i=0; i<8; i++) {
      password.SetCharAt(hexDigits[randomNumber%16], i);
      randomNumber = randomNumber/16;
    }
  }
}


/************************
 * Managing Signon List *
 ************************/

class si_SignonDataStruct {
public:
  si_SignonDataStruct() : name(""), value(""), isPassword(PR_FALSE) {}
  nsAutoString name;
  nsAutoString value;
  PRBool isPassword;
};

class si_SignonUserStruct {
public:
  si_SignonUserStruct() : signonData_list(NULL) {}
  nsVoidArray * signonData_list;
};

class si_SignonURLStruct {
public:
  si_SignonURLStruct() : URLName(NULL), chosen_user(NULL), signonUser_list(NULL) {}
  char * URLName;
  si_SignonUserStruct* chosen_user; /* this is a state variable */
  nsVoidArray * signonUser_list;
};


class si_Reject {
public:
  si_Reject() : URLName(NULL), userName("") {}
  char * URLName;
  nsAutoString userName;
};

//typedef struct _SignonDataStruct {
//  nsAutoString name;
//  nsAutoString value;
//  PRBool isPassword;
//} si_SignonDataStruct;

//typedef struct _SignonUserStruct {
//  nsVoidArray * signonData_list;
//} si_SignonUserStruct;

//typedef struct _SignonURLStruct {
//  char * URLName;
//  si_SignonUserStruct* chosen_user; /* this is a state variable */
//  nsVoidArray * signonUser_list;
//} si_SignonURLStruct;


//typedef struct _RejectStruct {
//  char * URLName;
//  nsAutoString userName;
//} si_Reject;

PRIVATE nsVoidArray * si_signon_list=0;
PRIVATE nsVoidArray * si_reject_list=0;
#define LIST_COUNT(list) (list ? list->Count() : 0)
PRIVATE PRBool si_signon_list_changed = PR_FALSE;

/*
 * Get the URL node for a given URL name
 *
 * This routine is called only when holding the signon lock!!!
 */
PRIVATE si_SignonURLStruct *
si_GetURL(const char * URLName) {
  si_SignonURLStruct * url;
  char *strippedURLName = 0;
  if (!URLName) {
    /* no URLName specified, return first URL (returns NULL if not URLs) */
    if (LIST_COUNT(si_signon_list)==0) {
      return NULL;
    }
    return (si_SignonURLStruct *) (si_signon_list->ElementAt(0));
  }
  strippedURLName = si_StrippedURL(URLName);
  PRInt32 urlCount = LIST_COUNT(si_signon_list);
  for (PRInt32 i=0; i<urlCount; i++) {
    url = NS_STATIC_CAST(si_SignonURLStruct*, si_signon_list->ElementAt(i));
    if(url->URLName && !PL_strcmp(strippedURLName, url->URLName)) {
      PR_Free(strippedURLName);
      return url;
    }
  }
  PR_Free(strippedURLName);
  return (NULL);
}

static nsresult MangleUrl(const char *url, char **result)
{
	if (!url || !result) return NS_ERROR_FAILURE;

	nsCAutoString temp(url);

	temp.ReplaceSubstring("@","-");
	temp.ReplaceSubstring("/","-");
	temp.ReplaceSubstring(":","-");
	temp.ReplaceSubstring("#","-");
	temp.ReplaceSubstring("&","-");
	temp.ReplaceSubstring("?","-");
	temp.ReplaceSubstring(".","-");

	*result = PL_strdup((const char *)temp);

	printf("mangling %s into %s\n", url, *result);
	return NS_OK;
}

/* Remove a user node from a given URL node */
PRIVATE PRBool
si_RemoveUser(const char *URLName, nsAutoString userName, PRBool save, PRBool strip) {
  nsresult res;
  si_SignonURLStruct * url;
  si_SignonUserStruct * user;
  si_SignonDataStruct * data;

  /* do nothing if signon preference is not enabled */
  if (!si_GetSignonRememberingPref()) {
    return PR_FALSE;
  }

  /* convert URLName to a uri so we can parse out the username and hostname */
  nsXPIDLCString host;
  if (strip) {
    if (URLName) {
      nsCOMPtr<nsIURL> uri;
      nsComponentManager::CreateInstance(kStandardUrlCID, nsnull, NS_GET_IID(nsIURL), (void **) getter_AddRefs(uri));
      res = uri->SetSpec(URLName);
      if (NS_FAILED(res)) return PR_FALSE;

      /* uri is of the form <scheme>://<username>:<password>@<host>:<portnumber>/<pathname>) */

      /* get host part of the uri */
      res = uri->GetHost(getter_Copies(host));
      if (NS_FAILED(res)) {
        return PR_FALSE;
      }

      /* if no username given, extract it from uri -- note: prehost is <username>:<password> */
      if (userName.Length() == 0) {
        nsXPIDLCString userName2;
        res = uri->GetPreHost(getter_Copies(userName2));
        if (NS_FAILED(res)) {
          return PR_FALSE;
        }
        if ((const char *)userName2 && (PL_strlen((const char *)userName2))) {
          userName = nsAutoString((const char *)userName2);
          PRInt32 colon = userName.FindChar(':');
          if (colon != -1) {
            userName.Truncate(colon);
          }
        }
      }
    }
  } else {
    res = MangleUrl(URLName, getter_Copies(host));
    if (NS_FAILED(res)) return PR_FALSE;
  }

  si_lock_signon_list();

  /* get URL corresponding to host */
  url = si_GetURL((const char *)host);
  if (!url) {
    /* URL not found */
    si_unlock_signon_list();
    return PR_FALSE;
  }

  /* free the data in each node of the specified user node for this URL */
  if (userName.Length() == 0) {

    /* no user specified so remove the first one */
    user = (si_SignonUserStruct *) (url->signonUser_list->ElementAt(0));

  } else {

    /* find the specified user */
    PRInt32 userCount = LIST_COUNT(url->signonUser_list);
    for (PRInt32 i=0; i<userCount; i++) {
      user = NS_STATIC_CAST(si_SignonUserStruct*, url->signonUser_list->ElementAt(i));
      PRInt32 dataCount = LIST_COUNT(user->signonData_list);
      for (PRInt32 ii=0; ii<dataCount; ii++) {
        data = NS_STATIC_CAST(si_SignonDataStruct*, user->signonData_list->ElementAt(ii));
        if (data->value == userName) {
          goto foundUser;
        }
      }
    }
    si_unlock_signon_list();
    return PR_FALSE; /* user not found so nothing to remove */
    foundUser: ;
  }

  /* free the data list */
  delete user->signonData_list;

  /* free the user node */
  url->signonUser_list->RemoveElement(user);

  /* remove this URL if it contains no more users */
  if (LIST_COUNT(url->signonUser_list) == 0) {
    PR_Free(url->URLName);
    si_signon_list->RemoveElement(url);
  }

  /* write out the change to disk */
  if (save) {
    si_signon_list_changed = PR_TRUE;
    si_SaveSignonDataLocked(PR_TRUE);
  }

  si_unlock_signon_list();
  return PR_TRUE;
}

PUBLIC PRBool
SINGSIGN_RemoveUser(const char *URLName, const PRUnichar *userName, PRBool strip) {
  return si_RemoveUser(URLName, nsAutoString(userName), PR_TRUE, strip);
}


/* Determine if a specified url/user exists */
PRIVATE PRBool
si_CheckForUser(char *URLName, nsAutoString userName) {
  si_SignonURLStruct * url;
  si_SignonUserStruct * user;
  si_SignonDataStruct * data;

  /* do nothing if signon preference is not enabled */
  if (!si_GetSignonRememberingPref()) {
    return PR_FALSE;
  }

  si_lock_signon_list();

  /* get URL corresponding to URLName */
  url = si_GetURL(URLName);
  if (!url) {
    /* URL not found */
    si_unlock_signon_list();
    return PR_FALSE;
  }

  /* find the specified user */
  PRInt32 userCount = LIST_COUNT(url->signonUser_list);
  for (PRInt32 i=0; i<userCount; i++) {
    user = NS_STATIC_CAST(si_SignonUserStruct*, url->signonUser_list->ElementAt(i));
    PRInt32 dataCount = LIST_COUNT(user->signonData_list);
    for (PRInt32 ii=0; ii<dataCount; ii++) {
      data = NS_STATIC_CAST(si_SignonDataStruct*, user->signonData_list->ElementAt(ii));
      if (data->value == userName) {
        si_unlock_signon_list();
        return PR_TRUE;
      }
    }
  }
  si_unlock_signon_list();
  return PR_FALSE; /* user not found */
}

/*
 * Get the user node for a given URL
 *
 * This routine is called only when holding the signon lock!!!
 *
 * This routine is called only if signon pref is enabled!!!
 */
PRIVATE si_SignonUserStruct*
si_GetUser(const char* URLName, PRBool pickFirstUser, nsAutoString userText) {
  si_SignonURLStruct* url;
  si_SignonUserStruct* user = nsnull;
  si_SignonDataStruct* data;

  /* get to node for this URL */
  url = si_GetURL(URLName);
  if (url != NULL) {

    /* node for this URL was found */
    PRInt32 user_count;
    if ((user_count = LIST_COUNT(url->signonUser_list)) == 1) {

      /* only one set of data exists for this URL so select it */
      user = (si_SignonUserStruct *) (url->signonUser_list->ElementAt(0));
      url->chosen_user = user;

    } else if (pickFirstUser) {
      PRInt32 userCount = LIST_COUNT(url->signonUser_list);
      for (PRInt32 i=0; i<userCount; i++) {
        user = NS_STATIC_CAST(si_SignonUserStruct*, url->signonUser_list->ElementAt(i));
        /* consider first data node to be the identifying item */
        data = (si_SignonDataStruct *) (user->signonData_list->ElementAt(0));
        if (data->name != userText) {
          /* name of current data item does not match name in data node */
          continue;
        }
        break;
      }
      url->chosen_user = user;

    } else {
      /* multiple users for this URL so a choice needs to be made */
      PRUnichar ** list;
      PRUnichar ** list2;
      si_SignonUserStruct** users;
      si_SignonUserStruct** users2;
      list = (PRUnichar**)PR_Malloc(user_count*sizeof(PRUnichar*));
      users = (si_SignonUserStruct **) PR_Malloc(user_count*sizeof(si_SignonUserStruct*));
      list2 = list;
      users2 = users;

      /* if there are multiple users, we need to load in the password file
       * before creating the list of usernames that we will present for 
       * selection.  Otherwise the ordering of the users in the file and the
       * ordering of users in the list we present for selection will disagree
       */
      user_count = 0;
      PRInt32 userCount = LIST_COUNT(url->signonUser_list);
      for (PRInt32 i=0; i<userCount; i++) {
        user = NS_STATIC_CAST(si_SignonUserStruct*, url->signonUser_list->ElementAt(i));
        /* consider first data node to be the identifying item */
        data = (si_SignonDataStruct *) (user->signonData_list->ElementAt(0));
          if (data->name != userText) {
          /* name of current data item does not match name in data node */
          continue;
        }
        user_count++;
      }
      if (user_count > 1) {
        SI_LoadSignonData(PR_TRUE);
        url = si_GetURL(URLName);
        if (url == NULL) {
          /* This will happen if user fails to unlock database in SI_LoadSignonData above */
          PR_Free(list);
          PR_Free(users);
          return NULL;
        }
      }

      /* step through set of user nodes for this URL and create list of
       * first data node of each (presumably that is the user name).
       * Note that the user nodes are already ordered by
       * most-recently-used so the first one in the list is the most
       * likely one to be chosen.
       */
      PRInt32 userCount2 = LIST_COUNT(url->signonUser_list);
      for (PRInt32 i2=0; i2<userCount2; i2++) {
        user = NS_STATIC_CAST(si_SignonUserStruct*, url->signonUser_list->ElementAt(i2));
        /* consider first data node to be the identifying item */
        data = (si_SignonDataStruct *) (user->signonData_list->ElementAt(0));
        if (data->name != userText) {
          /* name of current data item does not match name in data node */
          continue;
        }
        *(list2++) = (data->value).ToNewUnicode();
        *(users2++) = user;
      }

      /* have user select a username from the list */
      PRUnichar * selectUser = Wallet_Localize("SelectUser");
      if (user_count == 0) {
        /* not first data node for any saved user, so simply pick first user */
        if (url->chosen_user) {
          user = url->chosen_user;
        } else {
          /* no user selection had been made for first data node */
          user = NULL;
        } 
      } else if (user_count == 1) {
        /* only one user for this form at this url, so select it */
        user = users[0];
      } else if ((user_count > 1) && si_SelectDialog(selectUser, list, &user_count)) {
        /* user pressed OK */
        if (user_count == -1) {
          user_count = 0; /* user didn't select, so use first one */
        }
        user = users[user_count]; /* this is the selected item */
        /* item selected is now most-recently used, put at head of list */
        url->signonUser_list->RemoveElement(user);
        url->signonUser_list->InsertElementAt(user, 0);
      } else {
        user = NULL;
      }
      Recycle(selectUser);
      url->chosen_user = user;
      while (--list2 > list) {
        Recycle(*list2);
      }
      PR_Free(list);
      PR_Free(users);

      /* if we don't remove the URL from the cache at this point, the
       * cached copy will be brought containing the last-used username
       * rather than the username that was just selected
       */

#ifdef junk
      NET_RemoveURLFromCache(NET_CreateURLStruct((char *)URLName, NET_DONT_RELOAD));
#endif

    }
  } else {
    user = NULL;
  }
  return user;
}

/*
 * Get a specific user node for a given URL
 *
 * This routine is called only when holding the signon lock!!!
 *
 * This routine is called only if signon pref is enabled!!!
 */
PRIVATE si_SignonUserStruct*
si_GetSpecificUser(const char* URLName, nsAutoString userName, nsAutoString userText) {
  si_SignonURLStruct* url;
  si_SignonUserStruct* user;
  si_SignonDataStruct* data;

  /* get to node for this URL */
  url = si_GetURL(URLName);
  if (url != NULL) {

    /* step through set of user nodes for this URL looking for specified username */
    PRInt32 userCount2 = LIST_COUNT(url->signonUser_list);
    for (PRInt32 i2=0; i2<userCount2; i2++) {
      user = NS_STATIC_CAST(si_SignonUserStruct*, url->signonUser_list->ElementAt(i2));
      /* consider first data node to be the identifying item */
      data = (si_SignonDataStruct *) (user->signonData_list->ElementAt(0));
      if (data->name != userText) {
        /* desired username text does not match name in data node */
        continue;
      }
      if (data->value != userName) {
        /* desired username value does not match value in data node */
        continue;
      }
      return user;
    }

    /* if we don't remove the URL from the cache at this point, the
     * cached copy will be brought containing the last-used username
     * rather than the username that was just selected
     */

#ifdef junk
    NET_RemoveURLFromCache(NET_CreateURLStruct((char *)URLName, NET_DONT_RELOAD));
#endif

  }
  return NULL;
}

/*
 * Get the url and user for which a change-of-password is to be applied
 *
 * This routine is called only when holding the signon lock!!!
 *
 * This routine is called only if signon pref is enabled!!!
 */
PRIVATE si_SignonUserStruct*
si_GetURLAndUserForChangeForm(nsAutoString password)
{
  si_SignonURLStruct* url;
  si_SignonUserStruct* user;
  si_SignonDataStruct * data;
  PRInt32 user_count;

  PRUnichar ** list;
  PRUnichar ** list2;
  si_SignonUserStruct** users;
  si_SignonUserStruct** users2;
  si_SignonURLStruct** urls;
  si_SignonURLStruct** urls2;

  /* get count of total number of user nodes at all url nodes */
  user_count = 0;
  PRInt32 urlCount = LIST_COUNT(si_signon_list);
  for (PRInt32 i=0; i<urlCount; i++) {
    url = NS_STATIC_CAST(si_SignonURLStruct*, si_signon_list->ElementAt(i));
    PRInt32 userCount = LIST_COUNT(url->signonUser_list);
    for (PRInt32 ii=0; ii<userCount; ii++) {
      user = NS_STATIC_CAST(si_SignonUserStruct*, url->signonUser_list->ElementAt(ii));
      user_count++;
    }
  }

  /* allocate lists for maximumum possible url and user names */
  list = (PRUnichar**)PR_Malloc(user_count*sizeof(PRUnichar*));
  users = (si_SignonUserStruct **) PR_Malloc(user_count*sizeof(si_SignonUserStruct*));
  urls = (si_SignonURLStruct **)PR_Malloc(user_count*sizeof(si_SignonUserStruct*));
  list2 = list;
  users2 = users;
  urls2 = urls;
    
  /* step through set of URLs and users and create list of each */
  user_count = 0;
  PRInt32 urlCount2 = LIST_COUNT(si_signon_list);
  for (PRInt32 i2=0; i2<urlCount2; i2++) {
    url = NS_STATIC_CAST(si_SignonURLStruct*, si_signon_list->ElementAt(i2));
    PRInt32 userCount = LIST_COUNT(url->signonUser_list);
    for (PRInt32 i3=0; i3<userCount; i3++) {
      user = NS_STATIC_CAST(si_SignonUserStruct*, url->signonUser_list->ElementAt(i3));
      /* find saved password and see if it matches password user just entered */
      PRInt32 dataCount = LIST_COUNT(user->signonData_list);
      for (PRInt32 i4=0; i4<dataCount; i4++) {
        data = NS_STATIC_CAST(si_SignonDataStruct*, user->signonData_list->ElementAt(i4));
        if (data->isPassword && data->value == password) {
          /* passwords match so add entry to list */
          /* consider first data node to be the identifying item */
          data = (si_SignonDataStruct *) (user->signonData_list->ElementAt(0));
          nsAutoString temp = nsAutoString(url->URLName) + ":" + data->value;
          *list2 = temp.ToNewUnicode();
          list2++;
          *(users2++) = user;
          *(urls2++) = url;
          user_count++;
          break;
        }
      }
    }
  }

  /* query user */
  PRUnichar * msg = Wallet_Localize("SelectUserWhosePasswordIsBeingChanged");
  if (user_count && si_SelectDialog(msg, list, &user_count)) {
    user = users[user_count];
    url = urls[user_count];
    /*
     * since this user node is now the most-recently-used one, move it
     * to the head of the user list so that it can be favored for
     * re-use the next time this form is encountered
     */
    url->signonUser_list->RemoveElement(user);
    url->signonUser_list->InsertElementAt(user, 0);
    si_signon_list_changed = PR_TRUE;
    si_SaveSignonDataLocked(PR_TRUE);
  } else {
    user = NULL;
  }
  Recycle(msg);

  /* free allocated strings */
  while (--list2 > list) {
    Recycle(*list2);
  }
  PR_Free(list);
  PR_Free(users);
  PR_Free(urls);
  return user;
}

PRIVATE void
si_FreeReject(si_Reject * reject);

/*
 * Remove all the signons and free everything
 */

PUBLIC void
SI_RemoveAllSignonData() {
  if (si_PartiallyLoaded) {
    /* repeatedly remove first user node of first URL node */
    while (si_RemoveUser(NULL, nsAutoString(""), PR_FALSE, PR_TRUE)) {
    }
  }
  si_PartiallyLoaded = PR_FALSE;
  si_FullyLoaded = PR_FALSE;

  si_Reject * reject;
  while (LIST_COUNT(si_reject_list)>0) {
    reject = NS_STATIC_CAST(si_Reject*, si_reject_list->ElementAt(0));
    if (reject) {
      si_FreeReject(reject);
      si_signon_list_changed = PR_TRUE;
    }
  }
}

/****************************
 * Managing the Reject List *
 ****************************/

PRIVATE void
si_FreeReject(si_Reject * reject) {

  /*
   * This routine should only be called while holding the
   * signon list lock
   */

  if(!reject) {
      return;
  }
  si_reject_list->RemoveElement(reject);
  PR_FREEIF(reject->URLName);
  PR_Free(reject);
}

PRIVATE PRBool
si_CheckForReject(char * URLName, nsAutoString userName) {
  si_Reject * reject;

  si_lock_signon_list();
  if (si_reject_list) {
    PRInt32 rejectCount = LIST_COUNT(si_reject_list);
    for (PRInt32 i=0; i<rejectCount; i++) {
      reject = NS_STATIC_CAST(si_Reject*, si_reject_list->ElementAt(i));
      if(!PL_strcmp(URLName, reject->URLName)) {
// No need for username check on a rejectlist entry.  URL check is sufficient
//    if(!PL_strcmp(userName, reject->userName) && !PL_strcmp(URLName, reject->URLName)) {
        si_unlock_signon_list();
        return PR_TRUE;
      }
    }
  }
  si_unlock_signon_list();
  return PR_FALSE;
}

PRIVATE void
si_PutReject(char * URLName, nsAutoString userName, PRBool save) {
  char * URLName2=NULL;
  nsAutoString userName2;
  si_Reject * reject = new si_Reject;

  if (reject) {
    /*
     * lock the signon list
     *  Note that, for efficiency, SI_LoadSignonData already sets the lock
     *  before calling this routine whereas none of the other callers do.
     *  So we need to determine whether or not we were called from
     *  SI_LoadSignonData before setting or clearing the lock.  We can
     *  determine this by testing "save" since only SI_LoadSignonData
     *  passes in a value of PR_FALSE for "save".
     */
    if (save) {
      si_lock_signon_list();
    }

    StrAllocCopy(URLName2, URLName);
    userName2 = userName;
    reject->URLName = URLName2;
    reject->userName = userName2;

    if(!si_reject_list) {
      si_reject_list = new nsVoidArray();
      if(!si_reject_list) {
        PR_Free(reject->URLName);
        PR_Free(reject);
        if (save) {
          si_unlock_signon_list();
        }
        return;
      }
    }
#ifdef alphabetize
    /* add it to the list in alphabetical order */
    si_Reject * tmp_reject;
    PRBool rejectAdded = PR_FALSE;
    PRInt32 rejectCount = LIST_COUNT(si_reject_list);
    for (PRInt32 i = 0; i<rejectCount; ++i) {
      tmp_reject = NS_STATIC_CAST(si_Reject *, si_reject_list->ElementAt(i));
      if (tmp_reject) {
        if (PL_strcasecmp(reject->URLName, tmp_reject->URLName)<0) {
          si_reject_list->InsertElementAt(reject, i);
          rejectAdded = PR_TRUE;
          break;
        }
      }
    }
    if (!rejectAdded) {
      si_reject_list->AppendElement(reject);
    }
#else
    /* add it to the end of the list */
    si_reject_list->AppendElement(reject);
#endif

    if (save) {
      si_signon_list_changed = PR_TRUE;
      si_lock_signon_list();
      si_SaveSignonDataLocked(PR_FALSE);
      si_unlock_signon_list();
    }
  }
}

/*
 * Put data obtained from a submit form into the data structure for
 * the specified URL
 *
 * See comments below about state of signon lock when routine is called!!!
 *
 * This routine is called only if signon pref is enabled!!!
 */
PRIVATE void
si_PutData(const char * URLName, nsVoidArray * signonData, PRBool save) {
  PRBool added_to_list = PR_FALSE;
  si_SignonURLStruct * url;
  si_SignonUserStruct * user;
  si_SignonDataStruct * data;
  si_SignonDataStruct * data2;
  PRBool mismatch = PR_FALSE;

  /* discard this if the password is empty */
  PRInt32 count = signonData->Count();
  for (PRInt32 i=0; i<count; i++) {
    data2 = NS_STATIC_CAST(si_SignonDataStruct*, signonData->ElementAt(i));
    if (data2->isPassword && data2->value.Length()==0) {
      return;
    }
  }

  /*
   * lock the signon list
   *   Note that, for efficiency, SI_LoadSignonData already sets the lock
   *   before calling this routine whereas none of the other callers do.
   *   So we need to determine whether or not we were called from
   *   SI_LoadSignonData before setting or clearing the lock.  We can
   *   determine this by testing "save" since only SI_LoadSignonData passes
   *   in a value of PR_FALSE for "save".
   */
  if (save) {
    si_lock_signon_list();
  }

  /* make sure the signon list exists */
  if(!si_signon_list) {
    si_signon_list = new nsVoidArray();
    if(!si_signon_list) {
      if (save) {
        si_unlock_signon_list();
      }
      return;
    }
  }

  /* find node in signon list having the same URL */
  if ((url = si_GetURL(URLName)) == NULL) {

    /* doesn't exist so allocate new node to be put into signon list */
    url = new si_SignonURLStruct;
    if (!url) {
      if (save) {
        si_unlock_signon_list();
      }
      return;
    }

    /* fill in fields of new node */
    url->URLName = si_StrippedURL(URLName);
    if (!url->URLName) {
      PR_Free(url);
      if (save) {
        si_unlock_signon_list();
      }
      return;
    }

    url->signonUser_list = new nsVoidArray();
    if(!url->signonUser_list) {
      PR_Free(url->URLName);
      PR_Free(url);
    }

    /* put new node into signon list */

#ifdef alphabetize
    /* add it to the list in alphabetical order */
    si_SignonURLStruct * tmp_URL;
    PRInt32 urlCount = LIST_COUNT(si_signon_list);
    for (PRInt32 ii = 0; ii<urlCount; ++ii) {
      tmp_URL = NS_STATIC_CAST(si_SignonURLStruct *, si_signon_list->ElementAt(ii));
      if (tmp_URL) {
        if (PL_strcasecmp(url->URLName, tmp_URL->URLName)<0) {
          si_signon_list->InsertElementAt(url, ii);
          added_to_list = PR_TRUE;
          break;
        }
      }
    }
    if (!added_to_list) {
      si_signon_list->AppendElement(url);
    }
#else
    /* add it to the end of the list */
    si_signon_list->AppendElement(url);
#endif
  }

  /* initialize state variables in URL node */
  url->chosen_user = NULL;

  /*
   * see if a user node with data list matching new data already exists
   * (password fields will not be checked for in this matching)
   */
  PRInt32 userCount = LIST_COUNT(url->signonUser_list);
  for (PRInt32 i2=0; i2<userCount; i2++) {
    user = NS_STATIC_CAST(si_SignonUserStruct*, url->signonUser_list->ElementAt(i2));
    PRInt32 j = 0;
    PRInt32 dataCount = LIST_COUNT(user->signonData_list);
    for (PRInt32 i3=0; i3<dataCount; i3++) {
      data = NS_STATIC_CAST(si_SignonDataStruct*, user->signonData_list->ElementAt(i3));
      mismatch = PR_FALSE;

      /* check for match on name field and type field */
      if (j < signonData->Count()) {
        data2 = NS_STATIC_CAST(si_SignonDataStruct*, signonData->ElementAt(j));
        if ((data->isPassword == data2->isPassword) &&
            (data->name == data2->name)) {

          /* success, now check for match on value field if not password */
          if (data->value == data2->value || data->isPassword) {
            j++; /* success */
          } else {
            mismatch = PR_TRUE;
            break; /* value mismatch, try next user */
          }
        } else {
          mismatch = PR_TRUE;
          break; /* name or type mismatch, try next user */
        }
      }
    }
    if (!mismatch) {

      /* all names and types matched and all non-password values matched */

      /*
       * note: it is ok for password values not to match; it means
       * that the user has either changed his password behind our
       * back or that he previously mis-entered the password
       */

      /* update the saved password values */
      j = 0;
      PRInt32 dataCount2 = LIST_COUNT(user->signonData_list);
      for (PRInt32 i4=0; i4<dataCount2; i4++) {
        data = NS_STATIC_CAST(si_SignonDataStruct*, user->signonData_list->ElementAt(i4));

        /* update saved password */
        if ((j < signonData->Count()) && data->isPassword) {
          data2 = NS_STATIC_CAST(si_SignonDataStruct*, signonData->ElementAt(j));
          if (data->value != data2->value) {
            si_signon_list_changed = PR_TRUE;
            data->value = data2->value;
            si_Randomize(data->value);
          }
        }
        j++;
      }

      /*
       * since this user node is now the most-recently-used one, move it
       * to the head of the user list so that it can be favored for
       * re-use the next time this form is encountered
       */
      url->signonUser_list->RemoveElement(user);
      url->signonUser_list->InsertElementAt(user, 0);

      /* return */
      if (save) {
        si_signon_list_changed = PR_TRUE;
        si_SaveSignonDataLocked(PR_TRUE);
        si_unlock_signon_list();
      }
      return; /* nothing more to do since data already exists */
    }
  }

  /* user node with current data not found so create one */
  user = new si_SignonUserStruct;
  if (!user) {
    if (save) {
      si_unlock_signon_list();
    }
    return;
  }
  user->signonData_list = new nsVoidArray();
  if(!user->signonData_list) {
    PR_Free(user);
    if (save) {
      si_unlock_signon_list();
    }
    return;
  }

  /* create and fill in data nodes for new user node */
  for (PRInt32 k=0; k<signonData->Count(); k++) {

    /* create signon data node */
    data = new si_SignonDataStruct;
    if (!data) {
      delete user->signonData_list;
      PR_Free(user);
      if (save) {
        si_unlock_signon_list();
      }
      return;
    }
    data2 = NS_STATIC_CAST(si_SignonDataStruct*, signonData->ElementAt(k));
    data->isPassword = data2->isPassword;
    data->name = data2->name;
    data->value = data2->value;
    if (data->isPassword) {
      si_Randomize(data->value);
    }
    /* append new data node to end of data list */
    user->signonData_list->AppendElement(data);
  }

  /* append new user node to front of user list for matching URL */
    /*
     * Note that by appending to the front, we assure that if there are
     * several users, the most recently used one will be favored for
     * reuse the next time this form is encountered.  But don't do this
     * when reading in signons.txt (i.e., when save is PR_FALSE), otherwise
     * we will be reversing the order when reading in.
     */
  if (save) {
    url->signonUser_list->InsertElementAt(user, 0);
    si_signon_list_changed = PR_TRUE;
    si_SaveSignonDataLocked(PR_TRUE);
    if (!si_KeySet()) {
      url->signonUser_list->RemoveElementAt(0);
    }
    si_unlock_signon_list();
  } else {
    url->signonUser_list->AppendElement(user);
  }
}


/*****************************
 * Managing the Signon Files *
 *****************************/

#define HEADER_VERSION_1 "#1"

extern void
Wallet_UTF8Put(nsOutputFileStream strm, PRUnichar c);

extern PRUnichar
Wallet_UTF8Get(nsInputFileStream strm);

#define BUFFER_SIZE 4096

/*
 * get a line from a file
 * return -1 if end of file reached
 * strip carriage returns and line feeds from end of line
 */
PRIVATE PRInt32
si_ReadLine
  (nsInputFileStream strmu, nsInputFileStream strmp, nsAutoString& lineBuffer,PRBool obscure,
    nsKeyType saveCount = 0, nsKeyType * readCount = 0, PRBool inHeader = PR_FALSE) {

  lineBuffer = nsAutoString("");

  /* read the line */
  PRUnichar c, c2;
  for (;;) {
    if (inHeader) {
      c = Wallet_UTF8Get(strmu);
      if (obscure) {
        c2 = Wallet_UTF8Get(strmp);
        if (c != c2) {
          NS_ASSERTION(0, ".p and .u files differ in header");
          return -1;
        }
      }
    } else if (obscure) {
      c = Wallet_UTF8Get(strmu); /* get past the asterisk */
      if ((c != '*') && (c != '\n') && (c != '\r')) {
        return -1;
      }
      c = Wallet_UTF8Get(strmp)^si_GetKey(saveCount, (*readCount)++); /* get the real character */
    } else {
      c = Wallet_UTF8Get(strmu);
    }

    /* note that eof is not set until we read past the end of the file */
    if (strmu.eof()) {
      return -1;
    }

    if (c == '\n') {
      break;
    }
    if (c != '\r') {
      lineBuffer += c;
    }
  }
  return 0;
}

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
#define INITIAL_SAVECOUNTP 1
static nsKeyType saveCountP = INITIAL_SAVECOUNTP;

/*
 * Load signon data from disk file
 * Return value is:
 *   -1: fatal error
 *    0: successfully load
 *   +1: user aborted the load (by failing to open the database)
 */
PUBLIC int
SI_LoadSignonData(PRBool fullLoad) {
  /*
   * This routine is called initially with fullLoad set to PR_FALSE.  That will cause
   * the main file (consisting of URLs and usernames but having dummy passwords) to
   * be read in.  Later, when a password is needed for the first time, the internal
   * table is flushed and this routine is called again to read in the main file
   * along with the file containing the real passwords.
   *
   * This is done because the reading of the password file requires the user to
   * give a password to unlock the file.  We don't want to bother the user until
   * we absolutely have to.  So if he never goes to fill out a password form, he
   * should never have to give his password to unlock the password file
   */
  char * URLName;
  nsAutoString buffer;
  PRBool badInput = PR_FALSE;

  nsKeyType readCount = 0;

  if (si_FullyLoaded && fullLoad && !si_KeyTimedOut()) {
    return 0;
  }

#ifdef APPLE_KEYCHAIN
  if (KeychainManagerAvailable()) {
    SI_RemoveAllSignonData();
    return si_LoadSignonDataFromKeychain();
  }
#endif

  /* open the signon file */
  nsFileSpec dirSpec;
  nsresult rv = Wallet_ProfileDirectory(dirSpec);
  if (NS_FAILED(rv)) {
    return -1;
  }

  SI_InitSignonFileName();
  nsInputFileStream strmu(dirSpec+signonFileNameU);

  if (!strmu.is_open()) {
    /* this means no data was previously saved, so we must be fully loaded at this time */
    si_FullyLoaded = fullLoad;
    return 0;
  }

  SI_RemoveAllSignonData();

  if (fullLoad) {
    PRUnichar * message = Wallet_Localize("IncorrectKey_TryAgain?");
    if (si_KeyTimedOut()) {
      SI_RemoveAllSignonData();
    }
    while (!si_SetKey()) {
      if ((Wallet_CancelKey() || Wallet_KeySize() < 0) || !si_ConfirmYN(message)) {
        Recycle(message);
        return 1;
      }
    }
    Recycle(message);
    si_KeyResetTime();
  }

  nsInputFileStream strmp(fullLoad ? (dirSpec+signonFileNameP) : dirSpec); // not used if !fullLoad
  if (fullLoad && !strmp.is_open()) {
    fullLoad = PR_FALSE;
  }

  /* read the header information */
  nsAutoString format;
  nsKeyType temp;
  PRInt32 error;

  /* format */

  if (NS_FAILED(si_ReadLine(strmu, strmp, format, fullLoad, 0, 0, PR_TRUE))) {
    return -1;
  }
  if (format != HEADER_VERSION_1) {
    /* something's wrong */
    return -1;
  }

  /* saveCount */

  if (NS_FAILED(si_ReadLine(strmu, strmp, buffer, fullLoad, 0, 0, PR_TRUE))) {
    return -1;
  }
  if (NS_FAILED(si_ReadLine(strmu, strmp, buffer, fullLoad, 0, 0, PR_TRUE))) {
    return -1;
  }
  temp = (nsKeyType)(buffer.ToInteger(&error));
  if (error) {
    return -1;
  }
  saveCountP = temp;

  /* readCount */

  if (NS_FAILED(si_ReadLine(strmu, strmp, buffer, fullLoad, 0, 0, PR_TRUE))) {
    return -1;
  }
  if (NS_FAILED(si_ReadLine(strmu, strmp, buffer, fullLoad, 0, 0, PR_TRUE))) {
    return -1;
  }
  temp = (nsKeyType)(buffer.ToInteger(&error));
  if (error) {
    return -1;
  }
  readCount = temp;

  Wallet_StreamGeneratorReset();

  /* read the reject list */
  si_lock_signon_list();
  while (!NS_FAILED(si_ReadLine(strmu, strmp, buffer, PR_FALSE))) {
    if (buffer.CharAt(0) == '.') {
      break; /* end of reject list */
    }
    si_StripLF(buffer);
    URLName = buffer.ToNewCString();
    si_PutReject(URLName, buffer, PR_FALSE); /* middle parameter is obsolete */
    Recycle (URLName);
  }

  /* read the URL line */
  while(!NS_FAILED(si_ReadLine(strmu, strmp, buffer, PR_FALSE))) {
    si_StripLF(buffer);
    URLName = buffer.ToNewCString();

    /* prepare to read the name/value pairs */
    badInput = PR_FALSE;

    nsVoidArray * signonData = new nsVoidArray();
    si_SignonDataStruct * data;
    while(!NS_FAILED(si_ReadLine(strmu, strmp, buffer, PR_FALSE))) {

      /* line starting with . terminates the pairs for this URL entry */
      if (buffer.CharAt(0) == '.') {
        break; /* end of URL entry */
      }

      /* line just read is the name part */

      /* save the name part and determine if it is a password */
      PRBool ret;
      si_StripLF(buffer);
      nsAutoString name;
      nsAutoString value;
      PRBool isPassword;
      if (buffer.CharAt(0) == '*') {
        isPassword = PR_TRUE;
        nsAutoString temp;
        buffer.Mid(name, 1, buffer.Length()-1);
        ret = si_ReadLine(strmu, strmp, buffer, fullLoad, saveCountP, &readCount);
      } else {
        isPassword = PR_FALSE;
        name = buffer;
        ret = si_ReadLine(strmu, strmp, buffer, fullLoad, saveCountP, &readCount);
      }

      /* read in and save the value part */
      if(NS_FAILED(ret)) {
        /* error in input file so give up */
        badInput = PR_TRUE;
        break;
      }
      si_StripLF(buffer);
      value = buffer;

      data = new si_SignonDataStruct;
      data->name = name;
      data->value = value;
      data->isPassword = isPassword;
      signonData->AppendElement(data);
    }

    /* store the info for this URL into memory-resident data structure */
    if (!URLName || PL_strlen(URLName) == 0) {
      badInput = PR_TRUE;
    }
    if (!badInput) {
      si_PutData(URLName, signonData, PR_FALSE);
    }

    /* free up all the allocations done for processing this URL */
    Recycle(URLName);
    if (badInput) {
      si_unlock_signon_list();
      return -1;
    }

    PRInt32 count = signonData->Count();
    for (PRInt32 i=count-1; i>=0; i--) {
      data = NS_STATIC_CAST(si_SignonDataStruct*, signonData->ElementAt(i));
      delete data;
    }
    delete signonData;
  }

  si_unlock_signon_list();
  si_PartiallyLoaded = PR_TRUE;
  si_FullyLoaded = fullLoad;
  return 0;
}

/*
 * Save signon data to disk file
 * The parameter passed in on entry is ignored
 *
 * This routine is called only when holding the signon lock!!!
 *
 * This routine is called only if signon pref is enabled!!!
 */

PRIVATE void
si_WriteChar(nsOutputFileStream strm, PRUnichar c) {
  Wallet_UTF8Put(strm, c);
}

PRIVATE void
si_WriteLine(nsOutputFileStream strmu, nsOutputFileStream strmp, 
    nsAutoString lineBuffer, PRBool obscure, PRBool fullSave, 
    nsKeyType saveCount = 0, nsKeyType *writeCount = 0, PRBool inHeader = PR_FALSE) {

  for (int i=0; i<lineBuffer.Length(); i++) {
    if (inHeader) {
      Wallet_UTF8Put(strmu, lineBuffer.CharAt(i));
      if (fullSave) {
        Wallet_UTF8Put(strmp, lineBuffer.CharAt(i));
      }
    } else if (obscure) {
      Wallet_UTF8Put(strmu, '*');
      if (fullSave) {
        Wallet_UTF8Put(strmp, lineBuffer.CharAt(i)^si_GetKey(saveCount, (*writeCount)++));
      }
    } else {
      Wallet_UTF8Put(strmu, lineBuffer.CharAt(i));
    }
  }
  Wallet_UTF8Put(strmu, '\n');
  if (inHeader && fullSave) {
    Wallet_UTF8Put(strmp, '\n');
  } else if (obscure && fullSave) {
    Wallet_UTF8Put(strmp, '\n'^si_GetKey(saveCount, (*writeCount)++));
  }
}

PRIVATE int
si_SaveSignonDataLocked(PRBool fullSave) {
  si_SignonURLStruct * url;
  si_SignonUserStruct * user;
  si_SignonDataStruct * data;
  si_Reject * reject;
  nsKeyType writeCount = 0;

  /* do nothing if signon list has not changed */
  if(!si_signon_list_changed) {
    return(-1);
  }

  if (fullSave) {
    PRUnichar * message = Wallet_Localize("IncorrectKey_TryAgain?");
    if (si_KeyTimedOut()) {
      SI_RemoveAllSignonData();
    }
    while (!si_SetKey()) {
      if (Wallet_CancelKey() || (Wallet_KeySize() < 0) || !si_ConfirmYN(message)) {
        Recycle(message);
        return 1;
      }
    }
    Recycle(message);
    si_KeyResetTime();

    /* do not save signons if user didn't know the key */
    if (!si_KeySet()) {
      return(-1);
    }
  }

#ifdef APPLE_KEYCHAIN
  if (KeychainManagerAvailable()) {
    return si_SaveSignonDataInKeychain();
  }
#endif

  /* do nothing if we are unable to open file that contains signon list */
  nsFileSpec dirSpec;
  nsresult rv = Wallet_ProfileDirectory(dirSpec);
  if (NS_FAILED(rv)) {
    return 0;
  }

  nsOutputFileStream strmu(dirSpec + signonFileNameU);
  if (!strmu.is_open()) {
    return 0;
  }
  nsOutputFileStream strmp
    (fullSave ? (dirSpec+signonFileNameP) : dirSpec); // not used if !fullSave
  if (fullSave && !strmp.is_open()) {
    return 0;
  }


  /* format for head of file shall be:
   * format revision number
   * unused at present
   * saveCount
   * unused at present
   * writeCount
   */

  /* write out the header info */

  /* format revision number */

  si_WriteLine(strmu, strmp, nsAutoString(HEADER_VERSION_1), PR_FALSE, fullSave, 0, 0, PR_TRUE);

  /* saveCount */

  nsAutoString buffer;
  buffer = "";
  if (fullSave) {
    saveCountP += 16; /* preserve low order four bits which designate the file type */
  }
  buffer.Append(PRInt32(saveCountP),10);
  si_WriteLine(strmu, strmp, buffer, PR_FALSE, fullSave, 0, 0, PR_TRUE);
  si_WriteLine(strmu, strmp, buffer, PR_FALSE, fullSave, 0, 0, PR_TRUE);

  /* writeCount */

  buffer = "";
  buffer.Append(PRInt32(writeCount),10);
  si_WriteLine(strmu, strmp, buffer, PR_FALSE, fullSave, 0, 0, PR_TRUE);
  si_WriteLine(strmu, strmp, buffer, PR_FALSE, fullSave, 0, 0, PR_TRUE);

  Wallet_StreamGeneratorReset();

  /* format for next part of file shall be:
   * URLName -- first url/username on reject list
   * userName
   * URLName -- second url/username on reject list
   * userName
   * ...     -- etc.
   * .       -- end of list
   */

  /* write out reject list */
  if (si_reject_list) {
    PRInt32 rejectCount = LIST_COUNT(si_reject_list);
    for (PRInt32 i=0; i<rejectCount; i++) {
      reject = NS_STATIC_CAST(si_Reject*, si_reject_list->ElementAt(i));
      si_WriteLine(strmu, strmp, nsAutoString(reject->URLName), PR_FALSE, fullSave);
    }
  }
  si_WriteLine(strmu, strmp, nsAutoString("."), PR_FALSE, fullSave);

  /* format for cached logins shall be:
   * url LINEBREAK {name LINEBREAK value LINEBREAK}*  . LINEBREAK
   * if type is password, name is preceded by an asterisk (*)
   */

  /* write out each URL node */
  if((si_signon_list)) {
    PRInt32 urlCount = LIST_COUNT(si_signon_list);
    for (PRInt32 i2=0; i2<urlCount; i2++) {
      url = NS_STATIC_CAST(si_SignonURLStruct*, si_signon_list->ElementAt(i2));

      /* write out each user node of the URL node */
      PRInt32 userCount = LIST_COUNT(url->signonUser_list);
      for (PRInt32 i3=0; i3<userCount; i3++) {
        user = NS_STATIC_CAST(si_SignonUserStruct*, url->signonUser_list->ElementAt(i3));
        si_WriteLine
          (strmu, strmp, nsAutoString(url->URLName), PR_FALSE, fullSave);

        /* write out each data node of the user node */
        PRInt32 dataCount = LIST_COUNT(user->signonData_list);
        for (PRInt32 i4=0; i4<dataCount; i4++) {
          data = NS_STATIC_CAST(si_SignonDataStruct*, user->signonData_list->ElementAt(i4));
          if (data->isPassword) {
            si_WriteChar(strmu, '*');
          }
          si_WriteLine(strmu, strmp, nsAutoString(data->name), PR_FALSE, fullSave);
          si_WriteLine(strmu, strmp, nsAutoString(data->value), PR_TRUE,
            fullSave, saveCountP, &writeCount);
        }
        si_WriteLine(strmu, strmp, nsAutoString("."), PR_FALSE, fullSave);
      }
    }
  }
  si_signon_list_changed = PR_FALSE;
  strmu.flush();
  strmu.close();
  if (fullSave) {
    strmp.flush();
    strmp.close();
  }
  return 0;
}

/*
 * Save signon data to disk file
 * The parameter passed in on entry is ignored
 */

PUBLIC int
SI_SaveSignonData() {
  int retval;

  /* do nothing if signon preference is not enabled */
  if (!si_GetSignonRememberingPref()) {
    return PR_FALSE;
  }

  /* lock and call common save routine */
  si_lock_signon_list();
  si_signon_list_changed = PR_TRUE; /* force saving to occur */
  retval = si_SaveSignonDataLocked(PR_TRUE);
  si_unlock_signon_list();
  return retval;
}


/***************************
 * Processing Signon Forms *
 ***************************/

/* Ask user if it is ok to save the signon data */
PRIVATE PRBool
si_OkToSave(char *URLName, nsAutoString userName) {
  char *strippedURLName = 0;

  /* if url/user already exists, then it is safe to save it again */
  if (si_CheckForUser(URLName, userName)) {
    return PR_TRUE;
  }

#ifdef DefaultIsOff
  if (!si_RememberSignons && !si_GetNotificationPref()) {
    PRUnichar * notification = Wallet_Localize("PasswordNotification");
    si_SetNotificationPref(PR_TRUE);
    if (!si_ConfirmYN(notification)) {
      Recycle(notification);
      SI_SetBoolPref(pref_rememberSignons, PR_FALSE);
      return PR_FALSE;
    }
    Recycle(notification);
    SI_SetBoolPref(pref_rememberSignons, PR_TRUE);
  }
#endif

  strippedURLName = si_StrippedURL(URLName);
  if (si_CheckForReject(strippedURLName, userName)) {
    PR_Free(strippedURLName);
    return PR_FALSE;
  }

  PRUnichar * message = Wallet_Localize("WantToSavePassword?");
  PRInt32 button;
  if ((button = si_3ButtonConfirm(message)) != 1) {
    if (button == -1) {
      si_PutReject(strippedURLName, userName, PR_TRUE);
    }
    PR_Free(strippedURLName);
    Recycle(message);
    return PR_FALSE;
  }
  Recycle(message);
  PR_Free(strippedURLName);
  return PR_TRUE;
}

/*
 * Check for a signon submission and remember the data if so
 */
PUBLIC void
SINGSIGN_RememberSignonData (char* URLName, nsVoidArray * signonData)
{
  int passwordCount = 0;
  int pswd[3];
  si_SignonDataStruct * data;
  si_SignonDataStruct * data0;
  si_SignonDataStruct * data1;
  si_SignonDataStruct * data2;

  /* do nothing if signon preference is not enabled */
  if (!si_GetSignonRememberingPref()){
    return;
  }

  /* determine how many passwords are in the form and where they are */
  for (PRInt32 i=0; i<signonData->Count(); i++) {
    data = NS_STATIC_CAST(si_SignonDataStruct*, signonData->ElementAt(i));
    if (data->isPassword) {
      if (passwordCount < 3 ) {
        pswd[passwordCount] = i;
      }
      passwordCount++;
    }
  }

  /* process the form according to how many passwords it has */
  if (passwordCount == 1) {
    /* one-password form is a log-in so remember it */

    /* obtain the index of the first input field (that is the username) */
    PRInt32 j;
    for (j=0; j<signonData->Count(); j++) {
      data = NS_STATIC_CAST(si_SignonDataStruct*, signonData->ElementAt(j));
      if (!data->isPassword) {
        break;
      }
    }

    if (j<signonData->Count()) {
      data2 = NS_STATIC_CAST(si_SignonDataStruct*, signonData->ElementAt(j));
      if (si_OkToSave(URLName, data2->value /* username */)) {
        if (SI_LoadSignonData(PR_TRUE) == 0) {
          /* user succeeded in unlocking the database */
          si_PutData(URLName, signonData, PR_TRUE);
        }
      }
    }
  } else if (passwordCount == 2) {
    /* two-password form is a registration */

  } else if (passwordCount == 3) {
    /* three-password form is a change-of-password request */

    si_SignonUserStruct* user;

    /* make sure all passwords are non-null and 2nd and 3rd are identical */
    data0 = NS_STATIC_CAST(si_SignonDataStruct*, signonData->ElementAt(pswd[0]));
    data1 = NS_STATIC_CAST(si_SignonDataStruct*, signonData->ElementAt(pswd[1]));
    data2 = NS_STATIC_CAST(si_SignonDataStruct*, signonData->ElementAt(pswd[2]));
    if (data0->value.Length() == 0 || data1->value.Length() == 0 ||
        data2->value.Length() == 0 || data1->value != data2->value) {
      return;
    }

    /* ask user if this is a password change */
    si_lock_signon_list();
    user = si_GetURLAndUserForChangeForm(data0->value);

    /* return if user said no */
    if (!user) {
      si_unlock_signon_list();
      return;
    }

    /* get to password being saved */
    SI_LoadSignonData(PR_TRUE); /* this destroys "user" so we need to recalculate it */
    user = si_GetURLAndUserForChangeForm(data0->value);
    if (!user) { /* this should never happen but just in case */
      si_unlock_signon_list();
      return;
    }
    PRInt32 dataCount = LIST_COUNT(user->signonData_list);
    for (PRInt32 k=0; k<dataCount; k++) {
      data = NS_STATIC_CAST(si_SignonDataStruct*, user->signonData_list->ElementAt(k));
      if (data->isPassword) {
        break;
      }
    }

    /*
     * if second password is "********" then generate a random
     * password for it and use same random value for third password
     * as well (Note: this all works because we already know that
     * second and third passwords are identical so third password
     * must also be "********".  Furthermore si_Randomize() will
     * create a random password of exactly eight characters -- the
     * same length as "********".)
     */

/* commenting out because I don't see how such randomizing could ever have worked. */
//    si_Randomize(data1->value);
//    data2->value = data1->value;

    data->value = data1->value;
    si_signon_list_changed = PR_TRUE;
    si_SaveSignonDataLocked(PR_TRUE);
    si_unlock_signon_list();
  }
}

PUBLIC void
SINGSIGN_RestoreSignonData (char* URLName, PRUnichar* name, PRUnichar** value, PRUint32 elementNumber) {
  si_SignonUserStruct* user;
  si_SignonDataStruct* data;
  nsAutoString correctedName;

  /* do nothing if signon preference is not enabled */
  if (!si_GetSignonRememberingPref()){
    return;
  }

  si_lock_signon_list();
  if (elementNumber == 0) {
    si_UserHasBeenSelected = PR_FALSE;
  }

  /* Correct the field name to avoid mistaking for fields in browser-generated form
   *
   *   Note that data saved for browser-generated logins (e.g. http authentication)
   *   use artificial field names starting with * \= (see USERNAMEFIELD and PASSWORDFIELD.
   *   To avoid mistakes whereby saved logins for http authentication is then prefilled
   *   into a field on the html form at the same URL, we will prevent html field names
   *   from starting with \=.  We do that by doubling up a backslash if it appears in the
   *   first character position
   */
  if (*name == '\\') {
    correctedName = nsAutoString('\\') + name;
  } else {
    correctedName = name;
  }

  /* determine if name has been saved (avoids unlocking the database if not) */
  PRBool nameFound = PR_FALSE;
  user = si_GetUser(URLName, PR_FALSE, correctedName);
  if (user) {
    PRInt32 dataCount = LIST_COUNT(user->signonData_list);
    for (PRInt32 i=0; i<dataCount; i++) {
      data = NS_STATIC_CAST(si_SignonDataStruct*, user->signonData_list->ElementAt(i));
      if(correctedName.Length() && (data->name == correctedName)) {
        nameFound = PR_TRUE;
      }
    }
  }
  if (!nameFound) {
    si_unlock_signon_list();
    return;
  }

#ifdef xxx
  /*
   * determine if it is a change-of-password field
   *    the heuristic that we will use is that if this is the first
   *    item on the form and it is a password, this is probably a
   *    change-of-password form
   */
  /* see if this is first item in form and is a password */
  /* get first saved user just so we can see the name of the first item on the form */
  user = si_GetUser(URLName, PR_TRUE, NULL); /* this is the first saved user */
  if (user) {
    SI_LoadSignonData(PR_TRUE); /* this destroys "user" so need to recalculate it */
    user = si_GetUser(URLName, PR_TRUE, NULL);
    data = (si_SignonDataStruct *) (user->signonData_list->ElementAt(0)); /* 1st item on form */
    if(data->isPassword && correctedName.Length() && (data->name == correctedName)) {
      /* current item is first item on form and is a password */
      user = (URLName, MK_SIGNON_PASSWORDS_FETCH);
      if (user) {
        /* user has confirmed it's a change-of-password form */
        PRInt32 dataCount = LIST_COUNT(user->signonData_list);
        for (PRInt32 i=1; i<dataCount; i++) {
          data = NS_STATIC_CAST(si_SignonDataStruct*, user->signonData_list->ElementAt(i));
          if (data->isPassword) {
            *value = data->value;
            si_unlock_signon_list();
            return;
          }
        }
      }
    }
  }
#endif

  /* restore the data from previous time this URL was visited */

  user = si_GetUser(URLName, PR_FALSE, correctedName);
  if (user) {
    SI_LoadSignonData(PR_TRUE); /* this destroys user so need to recaculate it */
    user = si_GetUser(URLName, PR_FALSE, correctedName);
    if (user) { /* will be 0 if user failed to unlock database in SI_LoadSignonData above */
      PRInt32 dataCount = LIST_COUNT(user->signonData_list);
      for (PRInt32 i=0; i<dataCount; i++) {
        data = NS_STATIC_CAST(si_SignonDataStruct*, user->signonData_list->ElementAt(i));
        if(correctedName.Length() && (data->name == correctedName)) {
          *value = (data->value).ToNewUnicode();
          si_unlock_signon_list();
          return;
        }
      }
    }
  }
  si_unlock_signon_list();
}

/*
 * Remember signon data from a browser-generated password dialog
 */
PRIVATE void
si_RememberSignonDataFromBrowser(const char* URLName, nsAutoString username, nsAutoString password) {
  /* do nothing if signon preference is not enabled */
  if (!si_GetSignonRememberingPref()){
    return;
  }

  nsVoidArray * signonData = new nsVoidArray();
  si_SignonDataStruct * data1 = new si_SignonDataStruct;
  data1 -> name = nsAutoString(USERNAMEFIELD);
  data1 -> value = nsAutoString(username);
  data1 -> isPassword = PR_FALSE;
  signonData->AppendElement(data1);
  si_SignonDataStruct * data2 = new si_SignonDataStruct;
  data2 -> name = nsAutoString(PASSWORDFIELD);
  data2 -> value = nsAutoString(password);
  data2 -> isPassword = PR_TRUE;
  signonData->AppendElement(data2);

  /* Save the signon data */
  if (SI_LoadSignonData(PR_TRUE) == 0) {
    /* user succeeded in unlocking the database */
    si_PutData(URLName, signonData, PR_TRUE);
  }

  /* Deallocate */
  delete data1;
  delete data2;
  delete signonData;
}

/*
 * Check for remembered data from a previous browser-generated password dialog
 * restore it if so
 */
PRIVATE void
si_RestoreOldSignonDataFromBrowser
    (const char* URLName, PRBool pickFirstUser, nsAutoString& username, nsAutoString& password) {
  si_SignonUserStruct* user;
  si_SignonDataStruct* data;

  /* get the data from previous time this URL was visited */
  si_lock_signon_list();
  if (username.Length() != 0) {
    user = si_GetSpecificUser(URLName, username, nsAutoString(USERNAMEFIELD));
  } else {
    user = si_GetUser(URLName, pickFirstUser, nsAutoString(USERNAMEFIELD));
  }
  if (!user) {
    /* leave original username and password from caller unchanged */
    /* username = 0; */
    /* *password = 0; */
    si_unlock_signon_list();
    return;
  }
  SI_LoadSignonData(PR_TRUE); /* this destroys "user" so need to recalculate it */
  if (username.Length() != 0) {
    user = si_GetSpecificUser(URLName, username, nsAutoString(USERNAMEFIELD));
  } else {
    user = si_GetUser(URLName, pickFirstUser, nsAutoString(USERNAMEFIELD));
  }
  if (!user) {
    /* user failed to unlock the database in SI_LoadSignonData above */
    si_unlock_signon_list();
    return;
  }

  /* restore the data from previous time this URL was visited */
  PRInt32 dataCount = LIST_COUNT(user->signonData_list);
  for (PRInt32 i=0; i<dataCount; i++) {
    data = NS_STATIC_CAST(si_SignonDataStruct*, user->signonData_list->ElementAt(i));
    if(data->name == USERNAMEFIELD) {
      username = data->value;
    } else if(data->name == PASSWORDFIELD) {
      password = data->value;
    }
  }
  si_unlock_signon_list();
}

PUBLIC PRBool
SINGSIGN_StorePassword(const char *URLName, const PRUnichar *user, const PRUnichar *password, PRBool strip) {
  nsresult res;

  nsAutoString userName(user);

  /* convert URLName to a uri so we can parse out the username and hostname */
  nsXPIDLCString host;
  if (strip) {
    if (URLName) {
      nsCOMPtr<nsIURL> uri;
      nsComponentManager::CreateInstance(kStandardUrlCID, nsnull, NS_GET_IID(nsIURL), (void **) getter_AddRefs(uri));
      res = uri->SetSpec(URLName);
      if (NS_FAILED(res)) return PR_FALSE;

      /* uri is of the form <scheme>://<username>:<password>@<host>:<portnumber>/<pathname>) */

      /* get host part of the uri */
      res = uri->GetHost(getter_Copies(host));
      if (NS_FAILED(res)) {
        return PR_FALSE;
      }

      /* if no username given, extract it from uri -- note: prehost is <username>:<password> */
      if (userName.Length() == 0) {
        nsXPIDLCString userName2;
        res = uri->GetPreHost(getter_Copies(userName2));
        if (NS_FAILED(res)) {
          return PR_FALSE;
        }
        if ((const char *)userName2 && (PL_strlen((const char *)userName2))) {
          userName = nsAutoString((const char *)userName2);
          PRInt32 colon = userName.FindChar(':');
          if (colon != -1) {
            userName.Truncate(colon);
          }
        }
      }
    }
  } else {
    res = MangleUrl(URLName, getter_Copies(host));
    if (NS_FAILED(res)) return PR_FALSE;
  } 

  si_RememberSignonDataFromBrowser ((const char *)host, userName, nsAutoString(password));
  return PR_TRUE;
}

/* The following comments apply to the three prompt routines that follow
 *
 * If a password was successfully obtain (either from the single-signon
 * database or from a dialog with the user), we return NS_OK for the
 * function value and PR_TRUE for the boolean argument "pressedOK".
 *
 * If the user presses cancel from the dialog, we return NS_OK for the
 * function value and PR_FALSE for the boolean argument "pressedOK".
 *
 * If a password is not collected for any other reason, we return the
 * failure code for the function value and the boolean argument
 * "pressedOK" is undefined.
 */

PUBLIC nsresult
SINGSIGN_PromptUsernameAndPassword
    (const PRUnichar *text, PRUnichar **user, PRUnichar **pwd,
     const char *urlname, nsIPrompt* dialog, PRBool *pressedOK, PRBool strip) {

  nsresult res;

  /* do only the dialog if signon preference is not enabled */
  if (!si_GetSignonRememberingPref()){
    return dialog->PromptUsernameAndPassword(text, user, pwd, pressedOK);
  }

  /* convert to a uri so we can parse out the hostname */
  nsCOMPtr<nsIURL> uri;
  nsComponentManager::CreateInstance(kStandardUrlCID, nsnull, NS_GET_IID(nsIURL), (void **) getter_AddRefs(uri));
  res = uri->SetSpec(urlname);
  if (NS_FAILED(res)) {
    return res;
  }

  /* uri is of the form <scheme>://<username>:<password>@<host>:<portnumber>/<pathname>) */

  /* get host part of the uri */
  nsXPIDLCString host;
  if (strip) {
    res = uri->GetHost(getter_Copies(host));
    if (NS_FAILED(res)) {
      return res;
    }
  } else {
    res = MangleUrl(urlname, getter_Copies(host));
    if (NS_FAILED(res)) {
      return res;
    }
  }

  /* prefill with previous username/password if any */
  nsAutoString username, password;
  si_RestoreOldSignonDataFromBrowser((const char*)host, PR_FALSE, username, password);

  /* get new username/password from user */
  *user = username.ToNewUnicode();
  *pwd = password.ToNewUnicode();
  PRBool checked = PR_FALSE;
  res = si_CheckGetUsernamePassword(user, pwd, text, &checked);
  if (NS_FAILED(res)) {
    /* user pressed Cancel */
    PR_FREEIF(*user);
    PR_FREEIF(*pwd);
    *pressedOK = PR_FALSE;
    return NS_OK;
  }
  if (checked) {
    si_RememberSignonDataFromBrowser ((const char*)host, nsAutoString(*user), nsAutoString(*pwd));
  }

  /* cleanup and return */
  *pressedOK = PR_TRUE;
  return NS_OK;
}

PUBLIC nsresult
SINGSIGN_PromptPassword
    (const PRUnichar *text, PRUnichar **pwd, const char *urlname,
    nsIPrompt* dialog, PRBool *pressedOK, PRBool strip) 
{

  nsresult res;
  nsAutoString password, username;

  /* do only the dialog if signon preference is not enabled */
  if (!si_GetSignonRememberingPref()){
    PRUnichar * prompt_string = Wallet_Localize("PromptForPassword");
    res = dialog->PromptPassword(text, prompt_string, pwd, pressedOK);
    Recycle(prompt_string);
    return res;
  }

  /* convert to a uri so we can parse out the username and hostname */
  nsCOMPtr<nsIURL> uri;
  nsComponentManager::CreateInstance(kStandardUrlCID, nsnull, NS_GET_IID(nsIURL), (void **) getter_AddRefs(uri));
  res = uri->SetSpec(urlname);
  if (NS_FAILED(res)) {
    return res;
  }

  /* uri is of the form <scheme>://<username>:<password>@<host>:<portnumber>/<pathname>) */

  /* get host part of the uri */
  nsXPIDLCString host;
  if (strip) {
    res = uri->GetHost(getter_Copies(host));
    if (NS_FAILED(res)) {
      return res;
    }
  } else {
    res = MangleUrl(urlname, getter_Copies(host));
    if (NS_FAILED(res)) {
      return res;
    }
  }

  /* extract username from uri -- note: prehost is <username>:<password> */
  if (strip) {
	  nsXPIDLCString prehostCString;
	  res = uri->GetPreHost(getter_Copies(prehostCString));
	  if (NS_FAILED(res)) {
	    return res;
	  }
	  nsAutoString prehost = nsAutoString((const char *)prehostCString);
	  PRInt32 colon = prehost.FindChar(':');
	  if (colon == -1) {
	    username = prehost;
	  } else {
	    prehost.Left(username, colon);  
	  }
  }

  /* get previous password used with this username, pick first user if no username found */
  si_RestoreOldSignonDataFromBrowser((const char *)host, (username.Length() == 0), username, password);

  /* return if a password was found */
  if (password.Length() != 0) {
    *pwd = password.ToNewUnicode();
    *pressedOK = PR_TRUE;
    return NS_OK;
  }

  /* no password found, get new password from user */
  *pwd = password.ToNewUnicode();
  PRBool checked = PR_FALSE;
  res = si_CheckGetPassword(pwd, text, &checked);
  if (NS_FAILED(res)) {
    /* user pressed Cancel */
    PR_FREEIF(*pwd);
    *pressedOK = PR_FALSE;
    return NS_OK;
  }
  if (checked) {
    si_RememberSignonDataFromBrowser ((const char *)host, username, nsAutoString(*pwd));
  }

  /* cleanup and return */
  *pressedOK = PR_TRUE;
  return NS_OK;
}

PUBLIC nsresult
SINGSIGN_Prompt
    (const PRUnichar *text, const PRUnichar *defaultText, PRUnichar **resultText,
    const char *urlname, nsIPrompt* dialog, PRBool *pressedOK, PRBool strip) 
{
  nsresult res;
  nsAutoString data, emptyUsername("");

  /* do only the dialog if signon preference is not enabled */
  if (!si_GetSignonRememberingPref()){
    PRUnichar * prompt_string = Wallet_Localize("PromptForData");
    res = dialog->Prompt(text, prompt_string, resultText, pressedOK);
    Recycle(prompt_string);
    return res;
  }

  /* convert to a uri so we can parse out the hostname */
  nsCOMPtr<nsIURL> uri;
  nsComponentManager::CreateInstance(kStandardUrlCID, nsnull, NS_GET_IID(nsIURL), (void **) getter_AddRefs(uri));
  res = uri->SetSpec(urlname);
  if (NS_FAILED(res)) {
    return res;
  }

  /* get host part of the uri */
  nsXPIDLCString host;
  if (strip) {
    res = uri->GetHost(getter_Copies(host));
    if (NS_FAILED(res)) {
      return res;
    }
  } else {
    res = MangleUrl(urlname, getter_Copies(host));
    if (NS_FAILED(res)) {
      return res;
    }
  }

  /* get previous data used with this hostname */
  si_RestoreOldSignonDataFromBrowser((const char *)host, PR_TRUE, emptyUsername, data);

  /* return if data was found */
  if (data.Length() != 0) {
    *resultText = data.ToNewUnicode();
    *pressedOK = PR_TRUE;
    return NS_OK;
  }

  /* no data found, get new data from user */
  *resultText = data.ToNewUnicode();
  PRBool checked = PR_FALSE;
  res = si_CheckGetData(resultText, text, &checked);
  if (NS_FAILED(res)) {
    /* user pressed Cancel */
    PR_FREEIF(*resultText);
    *pressedOK = PR_FALSE;
    return NS_OK;
  }
  if (checked) {
    si_RememberSignonDataFromBrowser ((const char *)host, emptyUsername, nsAutoString(*resultText));
  }

  /* cleanup and return */
  *pressedOK = PR_TRUE;
  return NS_OK;
}

/*****************
 * Signon Viewer *
 *****************/

/* return PR_TRUE if "number" is in sequence of comma-separated numbers */
PUBLIC PRBool
SI_InSequence(nsAutoString sequence, PRInt32 number) {
  nsAutoString tail = sequence;
  nsAutoString head, temp;
  PRInt32 separator;

  for (;;) {
    /* get next item in list */
    separator = tail.FindChar(',');
    if (-1 == separator) {
      return PR_FALSE;
    }
    tail.Left(head, separator);
    tail.Mid(temp, separator+1, tail.Length() - (separator+1));
    tail = temp;

    /* test item to see if it equals our number */
    PRInt32 error;
    PRInt32 numberInList = head.ToInteger(&error);
    if (!error && numberInList == number) {
      return PR_TRUE;
    }
  }
}

PUBLIC PRUnichar*
SI_FindValueInArgs(nsAutoString results, nsAutoString name) {
  /* note: name must start and end with a vertical bar */
  nsAutoString value;
  PRInt32 start, length;
  start = results.Find(name);
  PR_ASSERT(start >= 0);
  if (start < 0) {
    return nsAutoString("").ToNewUnicode();
  }
  start += name.Length(); /* get passed the |name| part */
  length = results.FindChar('|', PR_FALSE,start) - start;
  results.Mid(value, start, length);
  return value.ToNewUnicode();
}

extern void
Wallet_SignonViewerReturn (nsAutoString results);

PUBLIC void
SINGSIGN_SignonViewerReturn (nsAutoString results) {
  si_SignonURLStruct *url;
  si_SignonUserStruct* user;
  si_SignonDataStruct* data;
  si_Reject* reject;
  PRInt32 signonNum;

  /* get total number of signons so we can go through list backwards */
  signonNum = 0;
  PRInt32 count = LIST_COUNT(si_signon_list);
  for (PRInt32 i=0; i<count; i++) {
    url = NS_STATIC_CAST(si_SignonURLStruct*, si_signon_list->ElementAt(i));
    signonNum += LIST_COUNT(url->signonUser_list);
  }

  /*
   * step backwards through all users and delete those that are in the sequence */
  nsAutoString gone;
  gone = SI_FindValueInArgs(results, nsAutoString("|goneS|"));
  PRInt32 urlCount = LIST_COUNT(si_signon_list);
  while (urlCount>0) {
    urlCount--;
    url = NS_STATIC_CAST(si_SignonURLStruct*, si_signon_list->ElementAt(urlCount));
    PRInt32 userCount = LIST_COUNT(url->signonUser_list);
    while (userCount>0) {
      userCount--;
      signonNum--;
      user = NS_STATIC_CAST(si_SignonUserStruct*, url->signonUser_list->ElementAt(userCount));
      if (user && SI_InSequence(gone, signonNum)) {
        /* get to first data item -- that's the user name */
        data = (si_SignonDataStruct *) (user->signonData_list->ElementAt(0));
        /* do the deletion */
        si_RemoveUser(url->URLName, data->value, PR_TRUE, PR_TRUE);
        si_signon_list_changed = PR_TRUE;
      }
    }
  }
  si_SaveSignonDataLocked(PR_TRUE);

  /* step backwards through all rejects and delete those that are in the sequence */
  gone = SI_FindValueInArgs(results, nsAutoString("|goneR|"));
  si_lock_signon_list();
  PRInt32 rejectCount = LIST_COUNT(si_reject_list);
  while (rejectCount>0) {
    rejectCount--;
    reject = NS_STATIC_CAST(si_Reject*, si_reject_list->ElementAt(rejectCount));
    if (reject && SI_InSequence(gone, rejectCount)) {
      si_FreeReject(reject);
      si_signon_list_changed = PR_TRUE;
    }
  }
  si_SaveSignonDataLocked(PR_FALSE);
  si_unlock_signon_list();

  /* now give wallet a chance to do its deletions */
  Wallet_SignonViewerReturn(results);
}

#define BUFLEN2 5000
#define BREAK '\001'

PUBLIC void
SINGSIGN_GetSignonListForViewer(nsAutoString& aSignonList)
{
  /* force loading of the signons file */
  si_RegisterSignonPrefCallbacks();

  /* unlock the database */
  if ((SI_LoadSignonData(PR_TRUE) != 0) || (Wallet_KeySize() < 0)) {
    if (Wallet_KeySize() < 0) {
      PRUnichar * message = Wallet_Localize("NoPasswordsEverSaved");
      si_Alert(message);
      Recycle(message);
    }
    aSignonList = "."; /* a list of length 1 tells viewer that database was not unlocked */ 
    /* don't display saved signons if user couldn't unlock the database */
    return;
  }
  nsAutoString buffer = "";
  int signonNum = 0;
  si_SignonURLStruct *url;
  si_SignonUserStruct * user;
  si_SignonDataStruct* data = nsnull;

  PRInt32 urlCount = LIST_COUNT(si_signon_list);
  for (PRInt32 i=0; i<urlCount; i++) {
    url = NS_STATIC_CAST(si_SignonURLStruct*, si_signon_list->ElementAt(i));
    PRInt32 userCount = LIST_COUNT(url->signonUser_list);
    for (PRInt32 j=0; j<userCount; j++) {
      user = NS_STATIC_CAST(si_SignonUserStruct*, url->signonUser_list->ElementAt(j));

      /* first non-password data item for user is the username */
      PRInt32 dataCount = LIST_COUNT(user->signonData_list);
      for (PRInt32 k=0; k<dataCount; k++) {
        data = (si_SignonDataStruct *) (user->signonData_list->ElementAt(k));
        if (!(data->isPassword)) {
          break;
        }
      }
      buffer += BREAK;
      buffer += "<OPTION value=";
      buffer.Append(signonNum, 10);
      buffer += ">";
      buffer += url->URLName;
      buffer += ":";
      buffer += data->isPassword ? nsAutoString("") : data->value; // in case all fields are passwords
      buffer += "</OPTION>\n";
      signonNum++;
    }
  }
  aSignonList = buffer;
}

PUBLIC void
SINGSIGN_GetRejectListForViewer(nsAutoString& aRejectList)
{
  nsAutoString buffer = "";
  int rejectNum = 0;
  si_Reject *reject;

  /* force loading of the signons file */
  si_RegisterSignonPrefCallbacks();

  PRInt32 rejectCount = LIST_COUNT(si_reject_list);
  for (PRInt32 i=0; i<rejectCount; i++) {
    reject = NS_STATIC_CAST(si_Reject*, si_reject_list->ElementAt(i));
    buffer += BREAK;
    buffer += "<OPTION value=";
    buffer.Append(rejectNum, 10);
    buffer += ">";
    buffer += reject->URLName;
    buffer += ":";
    buffer += reject->userName;
    buffer += "</OPTION>\n";
    rejectNum++;
  }
  aRejectList = buffer;
}

PUBLIC nsresult
SINGSIGN_HaveData(const char *url, const PRUnichar *userName, PRBool strip, PRBool *retval)
{
  nsresult res;
  nsAutoString data, usernameForLookup;

  *retval = PR_FALSE;

  if (!si_GetSignonRememberingPref()) {
    return NS_OK;
  }

  NS_ASSERTION(strip == PR_FALSE, "this code needs to be finished for the strip case");

  /* convert to a uri so we can parse out the username and hostname */
  nsCOMPtr<nsIURL> uri;
  nsComponentManager::CreateInstance(kStandardUrlCID, nsnull, NS_GET_IID(nsIURL), (void **) getter_AddRefs(uri));
  res = uri->SetSpec(url);
  if (NS_FAILED(res)) return res;

  /* get host part of the uri */
  nsXPIDLCString host;
  if (strip) {
    res = uri->GetHost(getter_Copies(host));
    if (NS_FAILED(res)) {
      return res;
    }
  } else {
    res = MangleUrl(url, getter_Copies(host));
    if (NS_FAILED(res)) return res;
  }

  if (strip) {
      /* convert url to a uri so we can parse out the username and hostname */
      /* if no username given, extract it from uri -- note: prehost is <username>:<password> */
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  /* get previous data used with this username, pick first user if no username found */
  si_RestoreOldSignonDataFromBrowser((const char *)host, (usernameForLookup.Length() == 0), usernameForLookup, data);

  if (data.Length()) {
    *retval = PR_TRUE;
  }

  return NS_OK;
}


#ifdef APPLE_KEYCHAIN
/************************************
 * Apple Keychain Specific Routines *
 ************************************/

/*
 * APPLE
 * The Keychain callback.  This routine will be called whenever a lock,
 * delete, or update event occurs in the Keychain.  The only action taken
 * is to make the signon list invalid, so it will be read in again the
 * next time it is accessed.
 */
OSStatus PR_CALLBACK
si_KeychainCallback( KCEvent keychainEvent, KCCallbackInfo *info, void *userContext) {
  PRBool    *listInvalid = (PRBool*)userContext;
  *listInvalid = PR_TRUE;
}

/*
 * APPLE
 * Get the signon data from the keychain
 *
 * This routine is called only if signon pref is enabled!!!
 */
PRIVATE int
si_LoadSignonDataFromKeychain() {
  char * URLName;
  si_FormSubmitData submit;
  nsAutoString name_array[MAX_ARRAY_SIZE];
  nsAutoString value_array[MAX_ARRAY_SIZE];
  uint8 type_array[MAX_ARRAY_SIZE];
  char buffer[BUFFER_SIZE];
  PRBool badInput = PR_FALSE;
  int i;
  KCItemRef   itemRef;
  KCAttributeList attrList;
  KCAttribute attr[2];
  KCItemClass itemClass = kInternetPasswordKCItemClass;
  KCProtocolType protocol = kNetscapeProtocolType;
  OSStatus status = noErr;
  KCSearchRef searchRef = NULL;
  /* initialize the submit structure */
  submit.name_array = name_array;
  submit.value_array = value_array;
  submit.type_array = (PRUnichar *)type_array;

  /* set up the attribute list */
  attrList.count = 2;
  attrList.attr = attr;
  attr[0].tag = kClassKCItemAttr;
  attr[0].data = &itemClass;
  attr[0].length = sizeof(itemClass);

  attr[1].tag = kProtocolKCItemAttr;
  attr[1].data = &protocol;
  attr[1].length = sizeof(protocol);

  status = KCFindFirstItem( &attrList, &searchRef, &itemRef );

#ifdef DefaultIsOff
  if (status == noErr) {
    /* if we found a Netscape item, let's assume notice has been given */
    si_SetNotificationPref(PR_TRUE);
  } else {
    si_SetNotificationPref(PR_FALSE);
  }
#endif

  si_lock_signon_list();
  while(status == noErr) {
    char *value;
    uint16 i = 0;
    uint32 actualSize;
    KCItemFlags flags;
    PRBool reject = PR_FALSE;
    submit.value_cnt = 0;

    /* first find out if it is a reject entry */
    attr[0].tag = kFlagsKCItemAttr;
    attr[0].length = sizeof(KCItemFlags);
    attr[0].data = &flags;
    status = KCGetAttribute( itemRef, attr, nil );
    if (status != noErr) {
      break;
    }
    if (flags & kNegativeKCItemFlag) {
      reject = PR_TRUE;
    }

    /* get the server name */
    attr[0].tag = kServerKCItemAttr;
    attr[0].length = BUFFER_SIZE;
    attr[0].data = buffer;
    status = KCGetAttribute( itemRef, attr, &actualSize );
    if (status != noErr) {
      break;
    }

    /* null terminate */
    buffer[actualSize] = 0;
    URLName = NULL;
    StrAllocCopy(URLName, buffer);
    if (!reject) {
      /* get the password data */
      status = KCGetData(itemRef, BUFFER_SIZE, buffer, &actualSize);
      if (status != noErr) {
        break;
      }

      /* null terminate */
      buffer[actualSize] = 0;

      /* parse for '=' which separates the name and value */
      for (i = 0; i < PL_strlen(buffer); i++) {
        if (buffer[i] == '=') {
          value = &buffer[i+1];
          buffer[i] = 0;
          break;
        }
      }
      name_array[submit.value_cnt] = NULL;
      value_array[submit.value_cnt] = NULL;
      type_array[submit.value_cnt] = FORM_TYPE_PASSWORD;
      StrAllocCopy(name_array[submit.value_cnt], buffer);
      StrAllocCopy(value_array[submit.value_cnt], value);
    }

    /* get the account attribute */
    attr[0].tag = kAccountKCItemAttr;
    attr[0].length = BUFFER_SIZE;
    attr[0].data = buffer;
    status = KCGetAttribute( itemRef, attr, &actualSize );
    if (status != noErr) {
      break;
    }

    /* null terminate */
    buffer[actualSize] = 0;
    if (!reject) {
      /* parse for '=' which separates the name and value */
      for (i = 0; i < PL_strlen(buffer); i++) {
        if (buffer[i] == '=') {
          value = &buffer[i+1];
          buffer[i] = 0;
          break;
        }
      }
      submit.value_cnt++;
      name_array[submit.value_cnt] = NULL;
      value_array[submit.value_cnt] = NULL;
      type_array[submit.value_cnt] = FORM_TYPE_TEXT;
      StrAllocCopy(name_array[submit.value_cnt], buffer);
      StrAllocCopy(value_array[submit.value_cnt], value);

      /* check for overruning of the arrays */
      if (submit.value_cnt >= MAX_ARRAY_SIZE) {
        break;
      }
      submit.value_cnt++;
      /* store the info for this URL into memory-resident data structure */
      if (!URLName || PL_strlen(URLName) == 0) {
        badInput = PR_TRUE;
      }
      if (!badInput) {
        si_PutData(URLName, &submit, PR_FALSE);
      }

    } else {
      /* reject */
      si_PutReject(URLName, nsAutoString(buffer), PR_FALSE);
    }
    reject = PR_FALSE; /* reset reject flag */
    PR_Free(URLName);
    KCReleaseItemRef( &itemRef );
    status = KCFindNextItem( searchRef, &itemRef);
  }
  si_unlock_signon_list();

  if (searchRef) {
    KCReleaseSearchRef( &searchRef );
  }

  /* Register a callback with the Keychain if we haven't already done so. */

  if (si_kcUPP == NULL) {
    si_kcUPP = NewKCCallbackProc( si_KeychainCallback );
    if (!si_kcUPP) {
      return memFullErr;
    }

    KCAddCallback( si_kcUPP, kLockKCEventMask + kDeleteKCEventMask + kUpdateKCEventMask, &si_list_invalid );
    /*
     * Note that the callback is not necessarily removed.  We take advantage
     * of the fact that the Keychain will clean up the callback when the app
     * goes away. It is explicitly removed when the signon preference is turned off.
     */
  }
  if (status == errKCItemNotFound) {
    status = 0;
  }
  return (status);
}

/*
 * APPLE
 * Save signon data to Apple Keychain
 *
 * This routine is called only if signon pref is enabled!!!
 */
PRIVATE int
si_SaveSignonDataInKeychain() {
  char* account = nil;
  char* password = nil;
  si_SignonURLStruct * URL;
  si_SignonUserStruct * user;
  si_SignonDataStruct * data;
  si_Reject * reject;
  OSStatus status;
  KCItemRef itemRef;
  KCAttribute attr;
  KCItemFlags flags = kInvisibleKCItemFlag + kNegativeKCItemFlag;
  uint32 actualLength;

  /* save off the reject list */
  if (si_reject_list) {
    PRInt32 rejectCount = LIST_COUNT(si_reject_list);
    for (PRInt32 i=0; i<rejectCount; i++) {
      reject = NS_STATIC_CAST(si_Reject*, si_reject_list->ElementAt(i));
      status = kcaddinternetpassword
        (reject->URLName, nil,
         reject->userName,
         kAnyPort,
         kNetscapeProtocolType,
         kAnyAuthType,
         0,
         nil,
         &itemRef);
      if (status != noErr && status != errKCDuplicateItem) {
        return(status);
      }
      if (status == noErr) {
        /*
         * make the item invisible so the user doesn't see it and
         * negative so we know that it is a reject entry
         */
        attr.tag = kFlagsKCItemAttr;
        attr.data = &flags;
        attr.length = sizeof( flags );

        status = KCSetAttribute( itemRef, &attr );
        if (status != noErr) {
          return(status);
        }
        status = KCUpdateItem(itemRef);
        if (status != noErr) {
          return(status);
        }
        KCReleaseItemRef(&itemRef);
      }
    }
  }

  /* save off the passwords */
  if((si_signon_list)) {
    PRInt32 urlCount = LIST_COUNT(si_signon_list);
    for (PRInt32 i=0; i<urlCount; i++) {
      URL = NS_STATIC_CAST(si_SignonURLStruct*, si_signon_list->ElementAt(i));

      /* add each user node of the URL node */
      PRInt32 userCount = LIST_COUNT(URL->signonUser_list);
      for (PRInt32 i=0; i<userCount; i++) {
        user = NS_STATIC_CAST(si_SignonUserStruct*, URL->signonUser_list->ElementAt(i));

        /* write out each data node of the user node */
        PRInt32 count = LIST_COUNT(user->signonData_list);
        for (PRInt32 i=0; i<count; i++) {
          data = NS_STATIC_CAST(si_SignonDataStruct*, user->signonData_list->ElementAt(i));
          char* attribute = nil;
          if (data->isPassword) {
            password = PR_Malloc(PL_strlen(data->value) + PL_strlen(data->name) + 2);
            if (!password) {
              return (-1);
            }
            attribute = password;
          } else {
            account = PR_Malloc( PL_strlen(data->value) + PL_strlen(data->name) + 2);
            if (!account) {
              PR_Free(password);
              return (-1);
            }
            attribute = account;
          }
          PL_strcpy(attribute, data->name);
          PL_strcat(attribute, "=");
          PL_strcat(attribute, data->value);
        }
        /* if it's already there, we just want to change the password */
        status = kcfindinternetpassword
            (URL->URLName,
             nil,
             account,
             kAnyPort,
             kNetscapeProtocolType,
             kAnyAuthType,
             0,
             nil,
             &actualLength,
             &itemRef);
        if (status == noErr) {
          status = KCSetData(itemRef, PL_strlen(password), password);
          if (status != noErr) {
            return(status);
          }
          status = KCUpdateItem(itemRef);
          KCReleaseItemRef(&itemRef);
        } else {
          /* wasn't there, let's add it */
          status = kcaddinternetpassword
            (URL->URLName,
             nil,
             account,
             kAnyPort,
             kNetscapeProtocolType,
             kAnyAuthType,
             PL_strlen(password),
             password,
             nil);
        }
        if (account) {
          PR_Free(account);
        }
        if (password) {
          PR_Free(password);
        } 
        account = password = nil;
        if (status != noErr) {
          return(status);
        }
      }
    }
  }
  si_signon_list_changed = PR_FALSE;
  return (0);
}

#endif
