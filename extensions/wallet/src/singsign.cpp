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

/* the following is from the now-defunct lo_ele.h */

struct si_FormSubmitData_struct {
  int32 value_cnt;
  nsAutoString* name_array;
  nsAutoString* value_array;
  PRUnichar ** type_array;
};
typedef struct si_FormSubmitData_struct si_FormSubmitData;

#define FORM_TYPE_TEXT          1
#define FORM_TYPE_PASSWORD      7


/*************************************
 * Externs to Routines in Wallet.cpp *
 *************************************/

extern nsresult Wallet_ProfileDirectory(nsFileSpec& dirSpec);


/***********
 * Dialogs *
 ***********/

extern PRBool Wallet_ConfirmYN(PRUnichar * szMessage);

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
#include "nsIWebShellWindow.h" // TODO remove later
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
  nsCOMPtr<nsIWebShellWindow> webshellwindow;
  appshellservice->GetHiddenWindow(getter_AddRefs(webshellwindow));
  nsCOMPtr<nsIPrompt> prompter(do_QueryInterface(webshellwindow));
  PRInt32 selectedIndex;
  PRBool rtnValue;
  rv = prompter->Select( NULL, szMessage, *pCount, (const PRUnichar **)pList, &selectedIndex, &rtnValue );
  *pCount = selectedIndex;
  si_UserHasBeenSelected = PR_TRUE;
  return rtnValue;  
}


/******************
 * Key Management *
 ******************/

extern void Wallet_RestartKey();
extern PRUnichar Wallet_GetKey();
extern PRBool Wallet_KeySet();
extern void Wallet_KeyResetTime();
extern PRBool Wallet_KeyTimedOut();
extern PRBool Wallet_SetKey(PRBool newkey);
extern PRInt32 Wallet_KeySize();
extern PRUnichar * Wallet_Localize(char * genericString);
extern PRBool Wallet_CancelKey();

char* signonFileName = nsnull;

PRIVATE void
si_RestartKey() {
  Wallet_RestartKey();
}

PRIVATE PRUnichar
si_GetKey() {
  return Wallet_GetKey();
}

PRIVATE PRBool
si_KeySet() {
  return Wallet_KeySet();
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
  nsIPref* pPrefService = nsnull;
  ret = nsServiceManager::GetService(kPrefServiceCID, kIPrefServiceIID,
    (nsISupports**) &pPrefService);
  if (!NS_FAILED(ret)) {
    ret = pPrefService->RegisterCallback(domain, callback, instance_data);
    if (!NS_FAILED(ret)) {
      ret = pPrefService->SavePrefFile(); 
    }
    nsServiceManager::ReleaseService(kPrefServiceCID, pPrefService);
  }
}

PUBLIC void
SI_SetBoolPref(const char * prefname, PRBool prefvalue) {
  nsresult ret;
  nsIPref* pPrefService = nsnull;
  ret = nsServiceManager::GetService(kPrefServiceCID, kIPrefServiceIID,
    (nsISupports**) &pPrefService);
  if (!NS_FAILED(ret)) {
    ret = pPrefService->SetBoolPref(prefname, prefvalue);
    if (!NS_FAILED(ret)) {
      ret = pPrefService->SavePrefFile(); 
    }
    nsServiceManager::ReleaseService(kPrefServiceCID, pPrefService);
  }
}

PUBLIC PRBool
SI_GetBoolPref(const char * prefname, PRBool defaultvalue) {
  nsresult ret;
  PRBool prefvalue = defaultvalue;
  nsIPref* pPrefService = nsnull;
  ret = nsServiceManager::GetService(kPrefServiceCID, kIPrefServiceIID,
    (nsISupports**) &pPrefService);
  if (!NS_FAILED(ret)) {
    ret = pPrefService->GetBoolPref(prefname, &prefvalue);
    if (!NS_FAILED(ret)) {
      ret = pPrefService->SavePrefFile(); 
    }
    nsServiceManager::ReleaseService(kPrefServiceCID, pPrefService);
  }
  return prefvalue;
}

PUBLIC void
SI_SetCharPref(const char * prefname, const char * prefvalue) {
  nsresult ret;
  nsIPref* pPrefService = nsnull;
  ret = nsServiceManager::GetService(kPrefServiceCID, kIPrefServiceIID,
    (nsISupports**) &pPrefService);
  if (!NS_FAILED(ret)) {
    ret = pPrefService->SetCharPref(prefname, prefvalue);
    if (!NS_FAILED(ret)) {
      ret = pPrefService->SavePrefFile(); 
    }
    nsServiceManager::ReleaseService(kPrefServiceCID, pPrefService);
  }
}

PUBLIC void
SI_GetCharPref(const char * prefname, char** aPrefvalue) {
  nsresult ret;
  nsIPref* pPrefService = nsnull;
  ret = nsServiceManager::GetService(kPrefServiceCID, kIPrefServiceIID,
    (nsISupports**) &pPrefService);
  if (!NS_FAILED(ret)) {
    ret = pPrefService->CopyCharPref(prefname, aPrefvalue);
    if (!NS_FAILED(ret)) {
      ret = pPrefService->SavePrefFile(); 
    } else {
      *aPrefvalue = nsnull;
    }
    nsServiceManager::ReleaseService(kPrefServiceCID, pPrefService);
  } else {
    *aPrefvalue = nsnull;
  }
}


/*********************************
 * Preferences for Single Signon *
 *********************************/

static const char *pref_rememberSignons = "signon.rememberSignons";
static const char *pref_Notified = "signon.Notified";
static const char *pref_SignonFileName = "signon.SignonFileName";

PRIVATE PRBool si_RememberSignons = PR_FALSE;
PRIVATE PRBool si_Notified = PR_FALSE;

PRIVATE int
si_SaveSignonDataLocked(PRBool fullSave);

PUBLIC int
SI_LoadSignonData(PRBool fullLoad);

PRIVATE void
si_RemoveAllSignonData();

PRIVATE PRBool
si_GetNotificationPref(void) {
  return si_Notified;
}

PRIVATE void
si_SetNotificationPref(PRBool x) {
  SI_SetBoolPref(pref_Notified, x);
  si_Notified = x;
}

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
    x = SI_GetBoolPref(pref_Notified, PR_FALSE);
    si_SetNotificationPref(x);        
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
     * set si_list_invalid to PR_FALSE first because si_RemoveAllSignonData
     * calls si_GetSignonRememberingPref
     */
    si_list_invalid = PR_FALSE;
    SI_LoadSignonData(PR_FALSE);
  }
#endif

  si_RegisterSignonPrefCallbacks();

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
}

extern char* Wallet_RandomName(char* suffix);

PUBLIC void
SI_InitSignonFileName() {
  SI_GetCharPref(pref_SignonFileName, &signonFileName);
  if (!signonFileName) {
    signonFileName = Wallet_RandomName("psw");
    SI_SetCharPref(pref_SignonFileName, signonFileName);
  }
}


/********************
 * Utility Routines *
 ********************/

/* StrAllocCopy and StrAllocCat should really be defined elsewhere */
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

#undef StrAllocCat
#define StrAllocCat(dest, src) Local_SACat (&(dest), src)
PRIVATE char *
Local_SACat(char **destination, const char *source) {
  if (source && *source) {
    if (*destination) {
      int length = PL_strlen (*destination);
      *destination = (char *) PR_Realloc(*destination, length + PL_strlen(source) + 1);
      if (*destination == NULL) {
        return(NULL);
      }
      PL_strcpy (*destination + length, source);
    } else {
      *destination = PL_strdup(source);
    }
  }
  return *destination;
}
  
/* Remove misleading portions from URL name */

const char* empty = "empty";

PRIVATE char*
si_StrippedURL (char* URLName) {
  char *result = 0;
  char *s, *t;

  /* check for null URLName */
  if (URLName == NULL || PL_strlen(URLName) == 0) {
    return NULL;
  }

  /* remove protocol */
  s = URLName;
  s = (char*) PL_strchr(s+1, '/');
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
si_GetURL(char * URLName) {
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

/* Remove a user node from a given URL node */
PRIVATE PRBool
si_RemoveUser(char *URLName, nsAutoString userName, PRBool save) {
  nsresult res;
  si_SignonURLStruct * url;
  si_SignonUserStruct * user;
  si_SignonDataStruct * data;

  /* do nothing if signon preference is not enabled */
  if (!si_GetSignonRememberingPref()) {
    return PR_FALSE;
  }

  /* convert URLName to a uri so we can parse out the username and hostname */
  char* host = nsnull;
  if (URLName) {
    nsCOMPtr<nsIURL> uri;
    nsComponentManager::CreateInstance(kStandardUrlCID, nsnull, NS_GET_IID(nsIURL), (void **) getter_AddRefs(uri));
    uri->SetSpec((char *)URLName);

    /* uri is of the form <scheme>://<username>:<password>@<host>:<portnumber>/<pathname>) */

    /* get host part of the uri */
    res = uri->GetHost(&host);
    if (NS_FAILED(res)) {
      return PR_FALSE;
    }

    /* if no username given, extract it from uri -- note: prehost is <username>:<password> */
    if (userName.Length() == 0) {
      char * userName2 = nsnull;
      res = uri->GetPreHost(&userName2);
      if (NS_FAILED(res)) {
        PR_FREEIF(host);
        return PR_FALSE;
      }
      if (userName2) {
        userName = nsAutoString(userName2);
        PR_FREEIF(userName2);
        PRInt32 colon = userName.FindChar(':');
        if (colon != -1) {
          userName.Truncate(colon);
        }
      }
    }
  }

  si_lock_signon_list();

  /* get URL corresponding to host */
  url = si_GetURL(host);
  if (!url) {
    /* URL not found */
    si_unlock_signon_list();
    PR_FREEIF(host);
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
    PR_FREEIF(host);
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
  PR_FREEIF(host);
  return PR_TRUE;
}

PUBLIC PRBool
SINGSIGN_RemoveUser(const char *URLName, PRUnichar *userName) {
  return si_RemoveUser((char *)URLName, userName, PR_TRUE);
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
si_GetUser(char* URLName, PRBool pickFirstUser, nsAutoString userText) {
  si_SignonURLStruct* url;
  si_SignonUserStruct* user;
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
si_GetSpecificUser(char* URLName, nsAutoString userName, nsAutoString userText) {
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

/*
 * Remove all the signons and free everything
 */

PRIVATE void
si_RemoveAllSignonData() {
  if (si_PartiallyLoaded) {
    /* repeatedly remove first user node of first URL node */
    while (si_RemoveUser(NULL, nsAutoString(""), PR_FALSE)) {
    }
  }
  si_PartiallyLoaded = PR_FALSE;
  si_FullyLoaded = PR_FALSE;
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
    if (save) {
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
si_PutData(char * URLName, si_FormSubmitData * submit, PRBool save) {
  PRBool added_to_list = PR_FALSE;
  si_SignonURLStruct * url;
  si_SignonUserStruct * user;
  si_SignonDataStruct * data;
  PRBool mismatch;

  /* discard this if the password is empty */
  for (PRInt32 i=0; i<submit->value_cnt; i++) {
    if ((((uint8*)submit->type_array)[i] == FORM_TYPE_PASSWORD) &&
        (((submit->value_array)) [i]).Length == 0 ) {
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

      /* skip non text/password fields */
      while ((j < submit->value_cnt) &&
            (((uint8*)submit->type_array)[j]!=FORM_TYPE_TEXT) &&
            (((uint8*)submit->type_array)[j]!=FORM_TYPE_PASSWORD)) {
        j++;
      }

      /* check for match on name field and type field */
      if ((j < submit->value_cnt) &&
          (data->isPassword ==
            (((uint8*)submit->type_array)[j]==FORM_TYPE_PASSWORD)) &&
            (data->name == (submit->name_array)[j])) {

        /* success, now check for match on value field if not password */
        if (data->value == (submit->value_array)[j] || data->isPassword) {
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

        /* skip non text/password fields */
        while ((j < submit->value_cnt) &&
              (((uint8*)submit->type_array)[j]!=FORM_TYPE_TEXT) &&
              (((uint8*)submit->type_array)[j]!=FORM_TYPE_PASSWORD)) {
          j++;
        }

        /* update saved password */
        if ((j < submit->value_cnt) && data->isPassword) {
          if (data->value != (submit->value_array)[j]) {
            si_signon_list_changed = PR_TRUE;
            data->value = (submit->value_array)[j];
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
  for (PRInt32 k=0; k<submit->value_cnt; k++) {

    /* skip non text/password fields */
    if((((uint8*)submit->type_array)[k]!=FORM_TYPE_TEXT) &&
        (((uint8*)submit->type_array)[k]!=FORM_TYPE_PASSWORD)) {
      continue;
    }

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
    data->isPassword = (((uint8 *)submit->type_array)[k] == FORM_TYPE_PASSWORD);
    data->name = (submit->name_array)[k];
    data->value = (submit->value_array)[k];
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

extern void
Wallet_UTF8Put(nsOutputFileStream strm, PRUnichar c);

extern PRUnichar
Wallet_UTF8Get(nsInputFileStream strm);

#define BUFFER_SIZE 4096
#define MAX_ARRAY_SIZE 500

/*
 * get a line from a file
 * return -1 if end of file reached
 * strip carriage returns and line feeds from end of line
 */
PRIVATE PRInt32
si_ReadLine
  (nsInputFileStream strm, nsInputFileStream strmx, nsAutoString& lineBuffer, PRBool obscure) {

  int count = 0;
  lineBuffer = nsAutoString("");

  /* read the line */
  PRUnichar c;
  for (;;) {
    if (obscure) {
      c = Wallet_UTF8Get(strm); /* get past the asterisk */
      c = Wallet_UTF8Get(strmx)^si_GetKey(); /* get the real character */
    } else {
      c = Wallet_UTF8Get(strm);
    }

    /* note that eof is not set until we read past the end of the file */
    if (strm.eof()) {
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
 * Load signon data from disk file
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
  si_FormSubmitData submit;
  nsAutoString name_array[MAX_ARRAY_SIZE];
  nsAutoString value_array[MAX_ARRAY_SIZE];
  uint8 type_array[MAX_ARRAY_SIZE];
  nsAutoString buffer;
  PRBool badInput;

  if (si_FullyLoaded && fullLoad) {
    return 0;
  }

#ifdef APPLE_KEYCHAIN
  if (KeychainManagerAvailable()) {
    si_RemoveAllSignonData();
    return si_LoadSignonDataFromKeychain();
  }
#endif

  /* open the signon file */
  nsFileSpec dirSpec;
  nsresult rv = Wallet_ProfileDirectory(dirSpec);
  if (NS_FAILED(rv)) {
    return -1;
  }
  nsInputFileStream strm(dirSpec + "signon.tbl");
  if (!strm.is_open()) {
    return -1;
  }

  si_RemoveAllSignonData();

  if (fullLoad) {
    si_RestartKey();
    PRUnichar * message = Wallet_Localize("IncorrectKey_TryAgain?");
    if (si_KeyTimedOut()) {
      si_RemoveAllSignonData();
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

  nsInputFileStream strmx(fullLoad ? (dirSpec+signonFileName) : dirSpec); // not used if !fullLoad
  if (fullLoad && !strmx.is_open()) {
    return -1;
  }

  /* read the reject list */
  si_lock_signon_list();
  while(!NS_FAILED(si_ReadLine(strm, strmx, buffer, PR_FALSE))) {
    if (buffer.CharAt(0) == '.') {
      break; /* end of reject list */
    }
    si_StripLF(buffer);
    URLName = buffer.ToNewCString();
    if (NS_FAILED(si_ReadLine(strm, strmx, buffer, PR_FALSE))) {
      /* error in input file so give up */
      badInput = PR_TRUE;
      break;
    }
    si_StripLF(buffer);
    si_PutReject(URLName, buffer, PR_FALSE);
    Recycle (URLName);
  }

  /* initialize the submit structure */
  submit.name_array = name_array;
  submit.value_array = value_array;
  submit.type_array = (PRUnichar **)type_array;

  /* read the URL line */
  while(!NS_FAILED(si_ReadLine(strm, strmx, buffer, PR_FALSE))) {
    si_StripLF(buffer);
    URLName = buffer.ToNewCString();

    /* prepare to read the name/value pairs */
    submit.value_cnt = 0;
    badInput = PR_FALSE;

    while(!NS_FAILED(si_ReadLine(strm, strmx, buffer, PR_FALSE))) {

      /* line starting with . terminates the pairs for this URL entry */
      if (buffer.CharAt(0) == '.') {
        break; /* end of URL entry */
      }

      /* line just read is the name part */

      /* save the name part and determine if it is a password */
      PRBool ret;
      si_StripLF(buffer);
      if (buffer.CharAt(0) == '*') {
        type_array[submit.value_cnt] = FORM_TYPE_PASSWORD;
        nsAutoString temp;
        buffer.Mid(temp, 1, buffer.Length()-1);
        name_array[submit.value_cnt] = temp;
        ret = si_ReadLine(strm, strmx, buffer, fullLoad);
      } else {
        type_array[submit.value_cnt] = FORM_TYPE_TEXT;
        name_array[submit.value_cnt] = buffer;
        ret = si_ReadLine(strm, strmx, buffer, PR_FALSE);
      }

      /* read in and save the value part */
      if(NS_FAILED(ret)) {
        /* error in input file so give up */
        badInput = PR_TRUE;
        break;
      }
      si_StripLF(buffer);
      value_array[submit.value_cnt++] = buffer;

      /* check for overruning of the arrays */
      if (submit.value_cnt >= MAX_ARRAY_SIZE) {
        break;
      }
    }

    /* store the info for this URL into memory-resident data structure */
    if (!URLName || PL_strlen(URLName) == 0) {
      badInput = PR_TRUE;
    }
    if (!badInput) {
      si_PutData(URLName, &submit, PR_FALSE);
    }

    /* free up all the allocations done for processing this URL */
    Recycle(URLName);
    if (badInput) {
      si_unlock_signon_list();
      return (1);
    }
  }

  si_unlock_signon_list();
  si_PartiallyLoaded = PR_TRUE;
  si_FullyLoaded = fullLoad;
  return(0);
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
si_WriteLine(nsOutputFileStream strm, nsOutputFileStream strmx, nsAutoString lineBuffer, PRBool obscure, PRBool fullSave) {
  for (int i=0; i<lineBuffer.Length(); i++) {
    if (obscure) {
      Wallet_UTF8Put(strm, '*');
      if (fullSave) {
        Wallet_UTF8Put(strmx, lineBuffer.CharAt(i)^si_GetKey());
      }
    } else {
      Wallet_UTF8Put(strm, lineBuffer.CharAt(i));
    }
  }
  Wallet_UTF8Put(strm, '\n');
  if (obscure && fullSave) {
    Wallet_UTF8Put(strmx, '\n'^si_GetKey());
  }
}

PRIVATE int
si_SaveSignonDataLocked(PRBool fullSave) {
  si_SignonURLStruct * url;
  si_SignonUserStruct * user;
  si_SignonDataStruct * data;
  si_Reject * reject;

  /* do nothing if signon list has not changed */
  if(!si_signon_list_changed) {
    return(-1);
  }

  if (fullSave) {
    si_RestartKey();
    PRUnichar * message = Wallet_Localize("IncorrectKey_TryAgain?");
    if (si_KeyTimedOut()) {
      si_RemoveAllSignonData();
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
  nsOutputFileStream strm(dirSpec + "signon.tbl");
  if (!strm.is_open()) {
    return 0;
  }
  nsOutputFileStream strmx(dirSpec + signonFileName);
  if (fullSave) {
    if (!strmx.is_open()) {
      return 0;
    }
    si_RestartKey();
  }

  /* format for head of file shall be:
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
      si_WriteLine(strm, strmx, nsAutoString(reject->URLName), PR_FALSE, fullSave);
      si_WriteLine(strm, strmx, nsAutoString(reject->userName), PR_FALSE, fullSave);
    }
  }
  si_WriteLine(strm, strmx, nsAutoString("."), PR_FALSE, fullSave);

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
        si_WriteLine(strm, strmx, nsAutoString(url->URLName), PR_FALSE, fullSave);

        /* write out each data node of the user node */
        PRInt32 dataCount = LIST_COUNT(user->signonData_list);
        for (PRInt32 i4=0; i4<dataCount; i4++) {
          data = NS_STATIC_CAST(si_SignonDataStruct*, user->signonData_list->ElementAt(i4));
          if (data->isPassword) {
            si_WriteChar(strm, '*');
            si_WriteLine(strm, strmx, nsAutoString(data->name), PR_FALSE, fullSave);
            si_WriteLine(strm, strmx, nsAutoString(data->value), PR_TRUE, fullSave);
          } else {
            si_WriteLine(strm, strmx, nsAutoString(data->name), PR_FALSE, fullSave);
            si_WriteLine(strm, strmx, nsAutoString(data->value), PR_FALSE, fullSave);
          }
        }
        si_WriteLine(strm, strmx, nsAutoString("."), PR_FALSE, fullSave);
      }
    }
  }
  si_signon_list_changed = PR_FALSE;
  strm.flush();
  strm.close();
  if (fullSave) {
    strmx.flush();
    strmx.close();
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
SINGSIGN_RememberSignonData
       (char* URLName, nsAutoString* name_array, nsAutoString* value_array, char** type_array, PRInt32 value_cnt)
{
  int passwordCount = 0;
  int pswd[3];

  si_FormSubmitData submit;
  submit.name_array = name_array;
  submit.value_array = value_array;
  submit.type_array = (PRUnichar **)type_array;
  submit.value_cnt = value_cnt;

  /* do nothing if signon preference is not enabled */
  if (!si_GetSignonRememberingPref()){
    return;
  }

  /* determine how many passwords are in the form and where they are */
  for (PRInt32 i=0; i<submit.value_cnt; i++) {
    if (((uint8 *)submit.type_array)[i] == FORM_TYPE_PASSWORD) {
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
    for (j=0; j<submit.value_cnt; j++) {
      if (((uint8 *)submit.type_array)[j] == FORM_TYPE_TEXT) {
        break;
      }
    }

    if ((j<submit.value_cnt) && si_OkToSave(URLName, /* urlname */
        (submit.value_array)[j] /* username */)) {
      SI_LoadSignonData(PR_TRUE);
      si_PutData(URLName, &submit, PR_TRUE);
    }
  } else if (passwordCount == 2) {
    /* two-password form is a registration */

  } else if (passwordCount == 3) {
    /* three-password form is a change-of-password request */

    si_SignonDataStruct* data;
    si_SignonUserStruct* user;

    /* make sure all passwords are non-null and 2nd and 3rd are identical */
    if ((submit.value_array[pswd[0]]).Length() == 0 ||
        (submit.value_array[pswd[1]]).Length() == 0 ||
        (submit.value_array[pswd[2]]).Length() == 0 ||
        (submit.value_array)[pswd[1]] != (submit.value_array)[pswd[2]]) {
      return;
    }

    /* ask user if this is a password change */
    si_lock_signon_list();
    user = si_GetURLAndUserForChangeForm((submit.value_array)[pswd[0]]);

    /* return if user said no */
    if (!user) {
      si_unlock_signon_list();
      return;
    }

    /* get to password being saved */
    SI_LoadSignonData(PR_TRUE); /* this destroys "user" so we need to recalculate it */
    user = si_GetURLAndUserForChangeForm((submit.value_array)[pswd[0]]);
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
     * if second password is "generate" then generate a random
     * password for it and use same random value for third password
     * as well (Note: this all works because we already know that
     * second and third passwords are identical so third password
     * must also be "generate".  Furthermore si_Randomize() will
     * create a random password of exactly eight characters -- the
     * same length as "generate".)
     */
    si_Randomize((submit.value_array)[pswd[1]]);
    (submit.value_array)[pswd[2]] = (submit.value_array)[pswd[1]];
    data->value = (submit.value_array)[pswd[1]];
    si_signon_list_changed = PR_TRUE;
    si_SaveSignonDataLocked(PR_TRUE);
    si_unlock_signon_list();
  }
}

PUBLIC void
SINGSIGN_RestoreSignonData (char* URLName, PRUnichar* name, PRUnichar** value, PRUint32 elementNumber) {
  si_SignonUserStruct* user;
  si_SignonDataStruct* data;

  /* do nothing if signon preference is not enabled */
  if (!si_GetSignonRememberingPref()){
    return;
  }

  si_lock_signon_list();
  if (elementNumber == 0) {
    si_UserHasBeenSelected = PR_FALSE;
  }

  /* determine if name has been saved (avoids unlocking the database if not) */
  PRBool nameFound = PR_FALSE;
  user = si_GetUser(URLName, PR_FALSE, nsAutoString(name));
  if (user) {
    PRInt32 dataCount = LIST_COUNT(user->signonData_list);
    for (PRInt32 i=0; i<dataCount; i++) {
      data = NS_STATIC_CAST(si_SignonDataStruct*, user->signonData_list->ElementAt(i));
      if(name && data->name == name) {
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
    if(data->isPassword && name && PL_strcmp(data->name, name)==0) {
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

  user = si_GetUser(URLName, PR_FALSE, nsAutoString(name));
  if (user) {
    SI_LoadSignonData(PR_TRUE); /* this destroys user so need to recaculate it */
    user = si_GetUser(URLName, PR_FALSE, nsAutoString(name));
    if (user) { /* this should always be true but just in case */
      PRInt32 dataCount = LIST_COUNT(user->signonData_list);
      for (PRInt32 i=0; i<dataCount; i++) {
        data = NS_STATIC_CAST(si_SignonDataStruct*, user->signonData_list->ElementAt(i));
        if(name && data->name == name) {
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
si_RememberSignonDataFromBrowser(char* URLName, nsAutoString username, nsAutoString password) {
  si_FormSubmitData submit;
  nsAutoString name_array[2];
  nsAutoString value_array[2];
  uint8 type_array[2];

  /* do nothing if signon preference is not enabled */
  if (!si_GetSignonRememberingPref()){
    return;
  }

  /* initialize a temporary submit structure */
  submit.name_array = name_array;
  submit.value_array = value_array;
  submit.type_array = (PRUnichar **)type_array;
  submit.value_cnt = 2;

  name_array[0] = nsAutoString("username");
  value_array[0] = nsAutoString(username);
  type_array[0] = FORM_TYPE_TEXT;

  name_array[1] = nsAutoString("password");
  value_array[1] = nsAutoString(password);
  type_array[1] = FORM_TYPE_PASSWORD;

  /* Save the signon data */
  SI_LoadSignonData(PR_TRUE);
  si_PutData(URLName, &submit, PR_TRUE);
}

/*
 * Check for remembered data from a previous browser-generated password dialog
 * restore it if so
 */
PRIVATE void
si_RestoreOldSignonDataFromBrowser
    (char* URLName, PRBool pickFirstUser, nsAutoString& username, nsAutoString& password) {
  si_SignonUserStruct* user;
  si_SignonDataStruct* data;

  /* get the data from previous time this URL was visited */
  si_lock_signon_list();
  if (username.Length() != 0) {
    user = si_GetSpecificUser(URLName, username, nsAutoString("username"));
  } else {
    user = si_GetUser(URLName, pickFirstUser, nsAutoString("username"));
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
    user = si_GetSpecificUser(URLName, username, nsAutoString("username"));
  } else {
    user = si_GetUser(URLName, pickFirstUser, nsAutoString("username"));
  }
  if (!user) {
    si_unlock_signon_list();
    return;
  }

  /* restore the data from previous time this URL was visited */
  PRInt32 dataCount = LIST_COUNT(user->signonData_list);
  for (PRInt32 i=0; i<dataCount; i++) {
    data = NS_STATIC_CAST(si_SignonDataStruct*, user->signonData_list->ElementAt(i));
    if(data->name == "username") {
      username = data->value;
    } else if(data->name == "password") {
      password = data->value;
    }
  }
  si_unlock_signon_list();
}

PUBLIC nsresult
SINGSIGN_PromptUsernameAndPassword
    (const PRUnichar *text, PRUnichar **user, PRUnichar **pwd,
     const char *urlname, nsIPrompt* dialog, PRBool *returnValue) {

  nsresult res;


  /* do only the dialog if signon preference is not enabled */
  if (!si_GetSignonRememberingPref()){
    return dialog->PromptUsernameAndPassword(text, user, pwd, returnValue);
  }

  /* convert to a uri so we can parse out the hostname */
  nsCOMPtr<nsIURL> uri;
  nsComponentManager::CreateInstance(kStandardUrlCID, nsnull, NS_GET_IID(nsIURL), (void **) getter_AddRefs(uri));
  uri->SetSpec((char *)urlname);

  /* uri is of the form <scheme>://<username>:<password>@<host>:<portnumber>/<pathname>) */

  /* get host part of the uri */
  char* host = nsnull;
  res = uri->GetHost(&host);
  if (NS_FAILED(res)) {
    return res;
  }

  /* prefill with previous username/password if any */
  nsAutoString username, password;
  si_RestoreOldSignonDataFromBrowser(host, PR_FALSE, username, password);
  *user = username.ToNewUnicode();
  *pwd = password.ToNewUnicode();

  /* get new username/password from user */
  res = dialog->PromptUsernameAndPassword(text, user, pwd, returnValue);
  if (NS_FAILED(res) || !(*returnValue)) {
    /* user pressed Cancel */
    PR_FREEIF(*user);
    PR_FREEIF(*pwd);
    PR_FREEIF(host);
    return res;
  }

  /* remember these values for next time */
  if (si_OkToSave(host, nsAutoString(username))) {
    si_RememberSignonDataFromBrowser (host, username, password);
  }

  /* cleanup and return */
  PR_FREEIF(host);
  return NS_OK;
}

PUBLIC nsresult
SINGSIGN_PromptPassword
    (const PRUnichar *text, PRUnichar **pwd, const char *urlname, nsIPrompt* dialog, PRBool *returnValue) {

  nsresult res;
  nsAutoString password, username;

  /* do only the dialog if signon preference is not enabled */
  if (!si_GetSignonRememberingPref()){
    return dialog->PromptPassword(text, nsnull /* window title */, pwd, returnValue);
  }

  /* convert to a uri so we can parse out the username and hostname */
  nsCOMPtr<nsIURL> uri;
  nsComponentManager::CreateInstance(kStandardUrlCID, nsnull, NS_GET_IID(nsIURL), (void **) getter_AddRefs(uri));
  uri->SetSpec((char *)urlname);

  /* uri is of the form <scheme>://<username>:<password>@<host>:<portnumber>/<pathname>) */

  /* get host part of the uri */
  char* host;
  res = uri->GetHost(&host);
  if (NS_FAILED(res)) {
    return res;
  }

  /* extract username from uri -- note: prehost is <username>:<password> */
  char * prehostCString;
  res = uri->GetPreHost(&prehostCString);
  if (NS_FAILED(res)) {
    PR_FREEIF(host);
    return res;
  }
  nsAutoString prehost = nsAutoString(prehostCString);
  PRInt32 colon = prehost.Find(prehost, ':');
  prehost.Left(username, colon);  

  PR_FREEIF(prehostCString);

  /* get previous password used with this username, pick first user if no username found */
  si_RestoreOldSignonDataFromBrowser(host, (username.Length() == 0), username, password);

  /* return if a password was found */
  if (password.Length() != 0) {
    *pwd = password.ToNewUnicode();
    *returnValue = PR_TRUE;
    PR_FREEIF(host);
    return NS_OK;
  }

  /* if no password found, get new password from user */
  res = dialog->PromptPassword(text, nsnull /* window title */, pwd, returnValue);
  if (NS_FAILED(res) || !(*returnValue)) {
    /* user pressed Cancel */
    PR_FREEIF(host);
    return res;
  }
        
  /* remember these values for next time */
  nsAutoString temp = *pwd;
  if (temp.Length() != 0 && si_OkToSave(host, username)) {
    si_RememberSignonDataFromBrowser (host, username, nsAutoString(*pwd));
  }

  /* cleanup and return */
  PR_FREEIF(host);
  return NS_OK;
}

PUBLIC nsresult
SINGSIGN_Prompt
    (const PRUnichar *text, const PRUnichar *defaultText,
     PRUnichar **resultText, const char *urlname,nsIPrompt* dialog, PRBool *returnValue) {
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
        si_RemoveUser(url->URLName, data->value, PR_TRUE);
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
  nsAutoString buffer = "";
  int signonNum = 0;
  si_SignonURLStruct *url;
  si_SignonUserStruct * user;
  si_SignonDataStruct* data;

  /* force loading of the signons file */
  si_RegisterSignonPrefCallbacks();

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

  if (status == noErr) {
    /* if we found a Netscape item, let's assume notice has been given */
    si_SetNotificationPref(PR_TRUE);
  } else {
    si_SetNotificationPref(PR_FALSE);
  }

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
