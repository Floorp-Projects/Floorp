/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#define alphabetize 1

#include "singsign.h"
#include "libi18n.h"            /* For the character code set conversions */

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
#include "nsIDOMHTMLDocument.h"
#include "prmem.h"
#include "prprf.h"  
#include "nsVoidArray.h"

static NS_DEFINE_IID(kIPrefServiceIID, NS_IPREF_IID);
static NS_DEFINE_IID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);


/********************
 * Global Variables *
 ********************/

/* locks for signon cache */

static PRMonitor * signon_lock_monitor = NULL;
static PRThread  * signon_lock_owner = NULL;
static int signon_lock_count = 0;
static PRBool si_anonymous = PR_FALSE;

/* load states */

static PRBool si_PartiallyLoaded = FALSE;
static PRBool si_FullyLoaded = FALSE;

/* apple keychain stuff */

#ifdef APPLE_KEYCHAIN
static PRBool     si_list_invalid = PR_FALSE;
static KCCallbackUPP si_kcUPP = NULL;
PRIVATE int
si_SaveSignonDataInKeychain();
#endif

/* the following is from the now-defunct lo_ele.h */

struct LO_FormSubmitData_struct {
  int32 value_cnt;
  PA_Block name_array;
  PA_Block value_array;
  PA_Block type_array;
};
#define FORM_TYPE_TEXT          1
#define FORM_TYPE_PASSWORD      7


/*************************************
 * Externs to Routines in Wallet.cpp *
 *************************************/

extern nsresult Wallet_ProfileDirectory(nsFileSpec& dirSpec);


/***********
 * Dialogs *
 ***********/

extern PRBool Wallet_ConfirmYN(char * szMessage);

PRIVATE PRBool
si_ConfirmYN(char * szMessage) {
  return Wallet_ConfirmYN(szMessage);
}

PRIVATE PRBool
si_PromptUsernameAndPassword(char *szMessage, char **szUsername, char **szPassword) {
#ifdef NECKO
  nsString username;
  nsString password;
  PRBool retval;
  nsresult res;
  NS_WITH_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, &res);
  if (NS_FAILED(res)) {
    return NULL; /* failure value */
  }
  const nsString message = szMessage;
  PRUnichar* usr;
  PRUnichar* pwd;
  res = dialog->PromptUsernameAndPassword(message.GetUnicode(), &usr, &pwd, &retval);
  if (NS_FAILED(res)) {
    return NULL; /* failure value */
  }
  username = usr;
  delete[] usr;
  password = pwd;
  delete[] pwd;
  *szUsername = username.ToNewCString();
  *szPassword = password.ToNewCString();
  return retval;
#else
    nsString username;
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
      dialog->PromptUserAndPassword(message, username, password, &retval);
  }
  nsServiceManager::ReleaseService(kNetSupportDialogCID, dialog);
  *szUsername = username.ToNewCString();
  *szPassword = password.ToNewCString();
  return retval;
#endif
}

PRIVATE char*
si_PromptPassword(char *szMessage) {
#ifdef NECKO
  nsString password;
  PRBool retval;
  nsresult res;  
  NS_WITH_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, &res);
  if (NS_FAILED(res)) {
    return NULL; /* failure value */
  }
  const nsString message = szMessage;
  PRUnichar* pwd;
  res = dialog->PromptPassword(message.GetUnicode(), &pwd, &retval);
  if (NS_FAILED(res)) {
    return NULL; /* failure value */
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

PRIVATE char*
si_Prompt(char *szMessage, char* szDefaultUsername) {
#ifdef NECKO
  nsString defaultUsername = szDefaultUsername;
  nsString username;
  PRBool retval;
  nsresult res;  
  NS_WITH_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, &res);
  if (NS_FAILED(res)) {
    return NULL; /* failure value */
  }
  const nsString message = szMessage;
  PRUnichar* usr;
  res = dialog->Prompt(message.GetUnicode(), defaultUsername.GetUnicode(),
                       &usr, &retval);
  if (NS_FAILED(res)) {
    return NULL; /* failure value */
  }
  username = usr;
  delete[] usr;
  if (retval) {
    return username.ToNewCString();
  } else {
    return NULL; /* user pressed cancel */
  }
#else
  nsString defaultUsername = szDefaultUsername;
  nsString username;
  PRBool retval;
  nsINetSupportDialogService* dialog = NULL;
  nsresult res = nsServiceManager::GetService(kNetSupportDialogCID,
  nsINetSupportDialogService::GetIID(), (nsISupports**)&dialog);
  if (NS_FAILED(res)) {
    return NULL; /* failure value */
  }
  if (dialog) {
    const nsString message = szMessage;
    dialog->Prompt(message, defaultUsername, username, &retval);
  }
  nsServiceManager::ReleaseService(kNetSupportDialogCID, dialog);
  if (retval) {
    return username.ToNewCString();
  } else {
    return NULL; /* user pressed cancel */
  }
#endif
}

PRIVATE PRBool
si_SelectDialog(const char* szMessage, char** pList, PRInt32* pCount) {
#ifdef NECKO
  PRBool retval = PR_TRUE; /* default value */
  nsresult res;  
  NS_WITH_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, &res);
  if (NS_FAILED(res)) {
    return NULL; /* failure value */
  }
  const nsString message = szMessage;
#ifdef xxx
  nsString users[10]; // need to make this dynamic ?????
  for (int i=0; i<*pCount; i++) {
    users[i] = pList[i];
  }
  dialog->Select(message, users, pCount, &retval);
#else
  for (int i=0; i<*pCount; i++) {
    nsString msg = "user = ";
    msg += pList[i];
    msg += "?";
    res = dialog->ConfirmYN(msg.GetUnicode(), &retval);
    if (NS_SUCCEEDED(res) && retval) {
      *pCount = i;
      break;
    }
  }
#endif
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
#ifdef xxx
    dialog->Select(message, pList, pCount, &retval);
#else
    for (int i=0; i<*pCount; i++) {
      nsString msg = "user = ";
      msg += pList[i];
      msg += "?";
      dialog->ConfirmYN(msg, &retval);
      if (retval) {
        *pCount = i;
        break;
      }
    }
#endif
  }
  nsServiceManager::ReleaseService(kNetSupportDialogCID, dialog);
  return retval;
#endif
}


/******************
 * Key Management *
 ******************/

extern void Wallet_RestartKey();
extern char Wallet_GetKey();
extern PRBool Wallet_KeySet();
extern PRBool Wallet_SetKey(PRBool newkey);
extern char * Wallet_Localize(char * genericString);

PRIVATE void
si_RestartKey() {
  Wallet_RestartKey();
}

PRIVATE char
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


/*********************************
 * Preferences for Single Signon *
 *********************************/

static const char *pref_rememberSignons = "signon.rememberSignons";
static const char *pref_Notified = "signon.Notified";

PRIVATE PRBool si_RememberSignons = PR_FALSE;
PRIVATE PRBool si_Notified = PR_FALSE;

PRIVATE int
si_SaveSignonDataLocked(PRBool fullSave);

PUBLIC int
SI_SaveSignonData();

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
    SI_LoadSignonData(FALSE);
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
    SI_LoadSignonData(FALSE);
  }
#endif

  si_RegisterSignonPrefCallbacks();

  /*
   * We initially want the rememberSignons pref to be FALSE.  But this will
   * prevent the notification message from ever occurring.  To get around
   * this problem, if the signon pref is FALSE and no notification has
   * ever been given, we will treat this as if the signon pref were TRUE.
   */
  if (!si_RememberSignons && !si_GetNotificationPref()) {
    return PR_TRUE;
  } else {
    return si_RememberSignons;
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
si_StripLF(char* buffer) {
  while ((buffer[PL_strlen(buffer)-1] == '\n') || (buffer[PL_strlen(buffer)-1] == '\r')) {
    buffer[PL_strlen(buffer)-1] = '\0';
  }
}

/* If user-entered password is "********", then generate a random password */
PRIVATE void
si_Randomize(char * password) {
  PRIntervalTime randomNumber;
  int i;
  const char * hexDigits = "0123456789AbCdEf";
  if (PL_strcmp(password, "********") == 0) {
    randomNumber = PR_IntervalNow();
    for (i=0; i<8; i++) {
      password[i] = hexDigits[randomNumber%16];
      randomNumber = randomNumber/16;
    }
  }
}


/************************
 * Managing Signon List *
 ************************/

typedef struct _SignonDataStruct {
  char * name;
  char * value;
  PRBool isPassword;
} si_SignonDataStruct;

typedef struct _SignonUserStruct {
  nsVoidArray * signonData_list;
} si_SignonUserStruct;

typedef struct _SignonURLStruct {
  char * URLName;
  si_SignonUserStruct* chosen_user; /* this is a state variable */
  nsVoidArray * signonUser_list;
} si_SignonURLStruct;


typedef struct _RejectStruct {
  char * URLName;
  char * userName;
} si_Reject;

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
si_RemoveUser(char *URLName, char *userName, PRBool save) {
  si_SignonURLStruct * url;
  si_SignonUserStruct * user;
  si_SignonDataStruct * data;

  /* do nothing if signon preference is not enabled */
  if (!si_GetSignonRememberingPref()) {
    return PR_FALSE;
  }

  si_lock_signon_list();

  /* get URL corresponding to URLName (or first URL if URLName is NULL) */
  url = si_GetURL(URLName);
  if (!url) {
    /* URL not found */
    si_unlock_signon_list();
    return PR_FALSE;
  }

  /* free the data in each node of the specified user node for this URL */
  if (userName == NULL) {

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
        if (PL_strcmp(data->value, userName)==0) {
          goto foundUser;
        }
      }
    }
    si_unlock_signon_list();
    return PR_FALSE; /* user not found so nothing to remove */
    foundUser: ;
  }

  /* free the items in the data list */
  PRInt32 dataCount = LIST_COUNT(user->signonData_list);
  for (PRInt32 j=0; j<dataCount; j++) {
    data = NS_STATIC_CAST(si_SignonDataStruct*, user->signonData_list->ElementAt(j));
    PR_Free(data->name);
    PR_Free(data->value);
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

/* Determine if a specified url/user exists */
PRIVATE PRBool
si_CheckForUser(char *URLName, char *userName) {
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
      if (PL_strcmp(data->value, userName)==0) {
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
si_GetUser(char* URLName, PRBool pickFirstUser, char* userText) {
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
      user = (si_SignonUserStruct *) (url->signonUser_list->ElementAt(0));
      url->chosen_user = user;

    } else {
      /* multiple users for this URL so a choice needs to be made */
      char ** list;
      char ** list2;
      si_SignonUserStruct** users;
      si_SignonUserStruct** users2;
      list = (char**)PR_Malloc(user_count*sizeof(char*));
      users = (struct _SignonUserStruct **) PR_Malloc(user_count*sizeof(si_SignonUserStruct*));
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
        if (PL_strcmp(data->name, userText)) {
          /* name of current data item does not match name in data node */
          continue;
        }
        user_count++;
      }
      if (user_count > 1) {
        SI_LoadSignonData(TRUE);
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
        if (PL_strcmp(data->name, userText)) {
          /* name of current data item does not match name in data node */
          continue;
        }
        *(list2++) = data->value;
        *(users2++) = user;
      }

      /* have user select a username from the list */
      char * selectUser = Wallet_Localize("SelectUser");
      if (user_count == 0) {
        /* not first data node for any saved user, so simply pick first user */
        if (url->chosen_user) {
          user = (si_SignonUserStruct *) (url->signonUser_list->ElementAt(0));
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
      PR_FREEIF(selectUser);
      url->chosen_user = user;
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
 * Get the url and user for which a change-of-password is to be applied
 *
 * This routine is called only when holding the signon lock!!!
 *
 * This routine is called only if signon pref is enabled!!!
 */
PRIVATE si_SignonUserStruct*
si_GetURLAndUserForChangeForm(char* password)
{
  si_SignonURLStruct* url;
  si_SignonUserStruct* user;
  si_SignonDataStruct * data;
  PRInt32 user_count;

  char ** list;
  char ** list2;
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
  list = (char**)PR_Malloc(user_count*sizeof(char*));
  users = (struct _SignonUserStruct **) PR_Malloc(user_count*sizeof(si_SignonUserStruct*));
  urls = (struct _SignonURLStruct **)PR_Malloc(user_count*sizeof(si_SignonUserStruct*));
  list2 = list;
  users2 = users;
  urls2 = urls;
    
  /* step through set of URLs and users and create list of each */
  user_count = 0;
  PRInt32 urlCount2 = LIST_COUNT(si_signon_list);
  for (PRInt32 i2=0; i2<urlCount2; i2++) {
    url = NS_STATIC_CAST(si_SignonURLStruct*, si_signon_list->ElementAt(i2));
    PRInt32 userCount = LIST_COUNT(url->signonUser_list);
    for (PRInt32 i=0; i<userCount; i++) {
      user = NS_STATIC_CAST(si_SignonUserStruct*, url->signonUser_list->ElementAt(i));
      /* find saved password and see if it matches password user just entered */
      PRInt32 dataCount = LIST_COUNT(user->signonData_list);
      for (PRInt32 i=0; i<dataCount; i++) {
        data = NS_STATIC_CAST(si_SignonDataStruct*, user->signonData_list->ElementAt(i));
        if (data->isPassword && !PL_strcmp(data->value, password)) {
          /* passwords match so add entry to list */
          /* consider first data node to be the identifying item */
          data = (si_SignonDataStruct *) (user->signonData_list->ElementAt(0));
          *list2 = 0;
          StrAllocCopy(*list2, url->URLName);
          StrAllocCat(*list2,": ");
          StrAllocCat(*list2, data->value);
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
  char * msg = Wallet_Localize("SelectUserWhosePasswordIsBeingChanged");
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
  PR_FREEIF(msg);

  /* free allocated strings */
  while (--list2 > list) {
    PR_Free(*list2);
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
    while (si_RemoveUser(NULL, NULL, PR_FALSE)) {
    }
  }
  si_PartiallyLoaded = FALSE;
  si_FullyLoaded = FALSE;
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
  PR_FREEIF(reject->userName);
  PR_Free(reject);
}

PRIVATE PRBool
si_CheckForReject(char * URLName, char * userName) {
  si_Reject * reject;

  si_lock_signon_list();
  if (si_reject_list) {
    PRInt32 rejectCount = LIST_COUNT(si_reject_list);
    for (PRInt32 i=0; i<rejectCount; i++) {
      reject = NS_STATIC_CAST(si_Reject*, si_reject_list->ElementAt(i));
      if(!PL_strcmp(userName, reject->userName) && !PL_strcmp(URLName, reject->URLName)) {
        si_unlock_signon_list();
        return PR_TRUE;
      }
    }
  }
  si_unlock_signon_list();
  return PR_FALSE;
}

PRIVATE void
si_PutReject(char * URLName, char * userName, PRBool save) {
  si_Reject * reject;
  char * URLName2=NULL;
  char * userName2=NULL;

  reject = PR_NEW(si_Reject);
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
    StrAllocCopy(userName2, userName);
    reject->URLName = URLName2;
    reject->userName = userName2;

    if(!si_reject_list) {
      si_reject_list = new nsVoidArray();
      if(!si_reject_list) {
        PR_Free(reject->URLName);
        PR_Free(reject->userName);
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
si_PutData(char * URLName, LO_FormSubmitData * submit, PRBool save) {
  char * name;
  char * value;
  PRBool added_to_list = PR_FALSE;
  si_SignonURLStruct * url;
  si_SignonUserStruct * user;
  si_SignonDataStruct * data;
  PRBool mismatch;

  /* discard this if the password is empty */
  for (PRInt32 i=0; i<submit->value_cnt; i++) {
    if ((((uint8*)submit->type_array)[i] == FORM_TYPE_PASSWORD) &&
        (!((char **)(submit->value_array))[i] ||
        !PL_strlen( ((char **)(submit->value_array)) [i])) ) {
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
    url = PR_NEW(si_SignonURLStruct);
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
    for (PRInt32 i=0; i<dataCount; i++) {
      data = NS_STATIC_CAST(si_SignonDataStruct*, user->signonData_list->ElementAt(i));


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
            (!PL_strcmp(data->name, ((char **)submit->name_array)[j]))) {

        /* success, now check for match on value field if not password */
        if (!submit->value_array[j]) {
          /* special case a null value */
          if (!PL_strcmp(data->value, "")) {
            j++; /* success */
          } else {
            mismatch = PR_TRUE;
            break; /* value mismatch, try next user */
          }
        } else if (!PL_strcmp(data->value, ((char **)submit->value_array)[j])
            || data->isPassword) {
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
      PRInt32 dataCount = LIST_COUNT(user->signonData_list);
      for (PRInt32 i=0; i<dataCount; i++) {
        data = NS_STATIC_CAST(si_SignonDataStruct*, user->signonData_list->ElementAt(i));

        /* skip non text/password fields */
        while ((j < submit->value_cnt) &&
              (((uint8*)submit->type_array)[j]!=FORM_TYPE_TEXT) &&
              (((uint8*)submit->type_array)[j]!=FORM_TYPE_PASSWORD)) {
          j++;
        }

        /* update saved password */
        if ((j < submit->value_cnt) && data->isPassword) {
          if (PL_strcmp(data->value, ((char **)submit->value_array)[j])) {
            si_signon_list_changed = PR_TRUE;
            StrAllocCopy(data->value, ((char **)submit->value_array)[j]);
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
  user = PR_NEW(si_SignonUserStruct);
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
    data = PR_NEW(si_SignonDataStruct);
    if (!data) {
      delete user->signonData_list;
      PR_Free(user);
    }
    data->isPassword = (((uint8 *)submit->type_array)[k] == FORM_TYPE_PASSWORD);
    name = 0; /* so that StrAllocCopy doesn't free previous name */
    StrAllocCopy(name, ((char **)submit->name_array)[k]);
    data->name = name;
    value = 0; /* so that StrAllocCopy doesn't free previous name */
    if (submit->value_array[k]) {
      StrAllocCopy(value, ((char **)submit->value_array)[k]);
    } else {
      StrAllocCopy(value, ""); /* insures that value is not null */
    }
    data->value = value;
    if (data->isPassword) {
      si_Randomize(data->value);
    }
    /* append new data node to end of data list */
    user->signonData_list->InsertElementAt(data, 0);
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
    si_unlock_signon_list();
  } else {
    url->signonUser_list->AppendElement(user);
  }
}


/*****************************
 * Managing the Signon Files *
 *****************************/

#define BUFFER_SIZE 4096
#define MAX_ARRAY_SIZE 500

/*
 * get a line from a file
 * return -1 if end of file reached
 * strip carriage returns and line feeds from end of line
 */
PRIVATE PRInt32
si_ReadLine
  (nsInputFileStream strm, nsInputFileStream strmx, char * lineBuffer, PRBool obscure) {

  int count = 0;

  /* read the line */
  char c;
  for (;;) {
    if (obscure) {
      c = strm.get(); /* get past the asterisk */
      c = strmx.get()^si_GetKey(); /* get the real character */
    } else {
      c = strm.get();
    }

    /* note that eof is not set until we read past the end of the file */
    if (strm.eof()) {
      return -1;
    }

    if (c == '\n') {
      lineBuffer[count] = '\0';
      break;
    }
    if (c != '\r') {
      lineBuffer[count++] = c;
      if (count == BUFFER_SIZE) {
        return -1;
      }
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
   * This routine is called initially with fullLoad set to FALSE.  That will cause
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
  LO_FormSubmitData submit;
  char* name_array[MAX_ARRAY_SIZE];
  char* value_array[MAX_ARRAY_SIZE];
  uint8 type_array[MAX_ARRAY_SIZE];
  char buffer[BUFFER_SIZE+1];
  PRBool badInput;
  int i;

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
  nsInputFileStream strmx(dirSpec + "signonx.tbl");
  if (!strmx.is_open()) {
    return -1;
  }

  si_RemoveAllSignonData();

  if (fullLoad) {
    si_RestartKey();
    char * message = Wallet_Localize("IncorrectKey_TryAgain?");
    while (!si_SetKey()) {
      if (!si_ConfirmYN(message)) {
        return 1;
      }
    }
  }

  /* read the reject list */
  si_lock_signon_list();
  while(!NS_FAILED(si_ReadLine(strm, strmx, buffer, FALSE))) {
    if (buffer[0] == '.') {
      break; /* end of reject list */
    }
    si_StripLF(buffer);
    URLName = NULL;
    StrAllocCopy(URLName, buffer);
    if (NS_FAILED(si_ReadLine(strm, strmx, buffer, FALSE))) {
      /* error in input file so give up */
      badInput = PR_TRUE;
      break;
    }
    si_StripLF(buffer);
    si_PutReject(URLName, buffer, PR_FALSE);
    PR_Free (URLName);
  }

  /* initialize the submit structure */
  submit.name_array = (PA_Block)name_array;
  submit.value_array = (PA_Block)value_array;
  submit.type_array = (PA_Block)type_array;

  /* read the URL line */
  while(!NS_FAILED(si_ReadLine(strm, strmx, buffer, FALSE))) {
    si_StripLF(buffer);
    URLName = NULL;
    StrAllocCopy(URLName, buffer);

    /* prepare to read the name/value pairs */
    submit.value_cnt = 0;
    badInput = PR_FALSE;

    while(!NS_FAILED(si_ReadLine(strm, strmx, buffer, FALSE))) {

      /* line starting with . terminates the pairs for this URL entry */
      if (buffer[0] == '.') {
        break; /* end of URL entry */
      }

      /* line just read is the name part */

      /* save the name part and determine if it is a password */
      PRBool ret;
      si_StripLF(buffer);
      name_array[submit.value_cnt] = NULL;
      if (buffer[0] == '*') {
        type_array[submit.value_cnt] = FORM_TYPE_PASSWORD;
        StrAllocCopy(name_array[submit.value_cnt], buffer+1);
        ret = si_ReadLine(strm, strmx, buffer, fullLoad);
      } else {
        type_array[submit.value_cnt] = FORM_TYPE_TEXT;
        StrAllocCopy(name_array[submit.value_cnt], buffer);
        ret = si_ReadLine(strm, strmx, buffer, FALSE);
      }

      /* read in and save the value part */
      if(!ret) {
        /* error in input file so give up */
        badInput = PR_TRUE;
        break;
      }
      si_StripLF(buffer);
      value_array[submit.value_cnt] = NULL;
      StrAllocCopy(value_array[submit.value_cnt++], buffer);

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
    PR_Free(URLName);
    for (i=0; i<submit.value_cnt; i++) {
      PR_Free(name_array[i]);
      PR_Free(value_array[i]);
    }
    if (badInput) {
      si_unlock_signon_list();
      return (1);
    }
  }

  si_unlock_signon_list();
  si_PartiallyLoaded = TRUE;
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
si_WriteChar(nsOutputFileStream strm, char c) {
  strm.put(c);
}

PRIVATE void
si_WriteLine(nsOutputFileStream strm, nsOutputFileStream strmx, char * lineBuffer, PRBool obscure) {
  char* p = lineBuffer;
  while (*p) {
    if (obscure) {
      strm.put('*');
      strmx.put(*(p++)^si_GetKey());
    } else {
      strm.put(*p++);
    }
  }
  strm.put('\n');
  if (obscure) {
    strmx.put('\n'^si_GetKey());
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

  /* do not save signons if in anonymous mode */
  if(si_anonymous) {
    return(-1);
  }

  if (fullSave) {
    si_RestartKey();
    char * message = Wallet_Localize("IncorrectKey_TryAgain?");
    while (!si_SetKey()) {
      if (!si_ConfirmYN(message)) {
        return 1;
      }
    }

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
  nsOutputFileStream strmx(dirSpec + "signonx.tbl");
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
      si_WriteLine(strm, strmx, reject->URLName, FALSE);
      si_WriteLine(strm, strmx, reject->userName, FALSE);
    }
  }
  si_WriteLine(strm, strmx, ".", FALSE);

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
        si_WriteLine(strm, strmx, url->URLName, FALSE);

        /* write out each data node of the user node */
        PRInt32 dataCount = LIST_COUNT(user->signonData_list);
        for (PRInt32 i4=0; i4<dataCount; i4++) {
          data = NS_STATIC_CAST(si_SignonDataStruct*, user->signonData_list->ElementAt(i4));
          if (data->isPassword) {
            si_WriteChar(strm, '*');
            si_WriteLine(strm, strmx, data->name, FALSE);
            if (fullSave) {
              si_WriteLine(strm, strmx, data->value, TRUE);
            }
          } else {
            si_WriteLine(strm, strmx, data->name, FALSE);
            si_WriteLine(strm, strmx, data->value, FALSE);
          }
        }
        si_WriteLine(strm, strmx, ".", FALSE);
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
  retval = si_SaveSignonDataLocked(PR_TRUE);
  si_unlock_signon_list();
  return retval;
}


/***************************
 * Processing Signon Forms *
 ***************************/

/* Ask user if it is ok to save the signon data */
PRIVATE PRBool
si_OkToSave(char *URLName, char *userName) {
  char *strippedURLName = 0;

  /* if url/user already exists, then it is safe to save it again */
  if (si_CheckForUser(URLName, userName)) {
    return PR_TRUE;
  }

  if (!si_RememberSignons && !si_GetNotificationPref()) {
    char* notification = 0;
    char * message = Wallet_Localize("PasswordNotification1");
    StrAllocCopy(notification, message);
    PR_FREEIF(message);
    message = Wallet_Localize("PasswordNotification2");
    StrAllocCat(notification, message);
    PR_FREEIF(message);
    si_SetNotificationPref(PR_TRUE);
    if (!si_ConfirmYN(notification)) {
      PR_Free (notification);
      SI_SetBoolPref(pref_rememberSignons, PR_FALSE);
      return PR_FALSE;
    }
    SI_SetBoolPref(pref_rememberSignons, PR_TRUE); /* this is unnecessary */
    PR_Free (notification);
  }

  strippedURLName = si_StrippedURL(URLName);
  if (si_CheckForReject(strippedURLName, userName)) {
    PR_Free(strippedURLName);
    return PR_FALSE;
  }

  char * message = Wallet_Localize("WantToSavePassword?");
  if (!si_ConfirmYN(message)) {
    si_PutReject(strippedURLName, userName, PR_TRUE);
    PR_Free(strippedURLName);
    PR_FREEIF(message);
    return PR_FALSE;
  }
  PR_FREEIF(message);
  PR_Free(strippedURLName);
  return PR_TRUE;
}

/*
 * Check for a signon submission and remember the data if so
 */
PUBLIC void
SINGSIGN_RememberSignonData
       (char* URLName, char** name_array, char** value_array, char** type_array, PRInt32 value_cnt)
{
  int passwordCount = 0;
  int pswd[3];

  LO_FormSubmitData submit;
  submit.name_array = (PA_Block)name_array;
  submit.value_array = (PA_Block)value_array;
  submit.type_array = (PA_Block)type_array;
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
        ((char **)submit.value_array)[j] /* username */)) {
      SI_LoadSignonData(TRUE);
      si_PutData(URLName, &submit, PR_TRUE);
    }
  } else if (passwordCount == 2) {
    /* two-password form is a registration */

  } else if (passwordCount == 3) {
    /* three-password form is a change-of-password request */

    si_SignonDataStruct* data;
    si_SignonUserStruct* user;

    /* make sure all passwords are non-null and 2nd and 3rd are identical */
    if (!submit.value_array[pswd[0]] || ! submit.value_array[pswd[1]] ||
        !submit.value_array[pswd[2]] ||
        PL_strcmp(((char **)submit.value_array)[pswd[1]],
          ((char **)submit.value_array)[pswd[2]])) {
      return;
    }

    /* ask user if this is a password change */
    si_lock_signon_list();
    user = si_GetURLAndUserForChangeForm(((char **)submit.value_array)[pswd[0]]);

    /* return if user said no */
    if (!user) {
      si_unlock_signon_list();
      return;
    }

    /* get to password being saved */
    SI_LoadSignonData(TRUE); /* this destroys "user" so we need to recalculate it */
    user = si_GetURLAndUserForChangeForm(((char **)submit.value_array)[pswd[0]]);
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
    si_Randomize(((char **)submit.value_array)[pswd[1]]);
    PL_strcpy(((char **)submit.value_array)[pswd[2]],((char **)submit.value_array)[pswd[1]]);
    StrAllocCopy(data->value, ((char **)submit.value_array)[pswd[1]]);
    si_signon_list_changed = PR_TRUE;
    si_SaveSignonDataLocked(PR_TRUE);
    si_unlock_signon_list();
  }
}

PUBLIC void
SINGSIGN_RestoreSignonData (char* URLName, char* name, char** value) {
  si_SignonUserStruct* user;
  si_SignonDataStruct* data;

  /* do nothing if signon preference is not enabled */
  if (!si_GetSignonRememberingPref()){
    return;
  }

  si_lock_signon_list();

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

#ifndef NECKO
/* fix bug 9326 */
/*
 * This is a temporary hack until necko lands because it is believed necko will fix this
 * The problem is that if the database of saved passwords had not already been unlocked,
 * a dialog will come up at this time asking for password to unlock the database.  But
 * that dialog was coming up blank because we are on the netlib thread.  Hack is to not
 * attempt to prefill with saved passwords at this time if the database has not been
 * previously unlocked.  Such unlocking could be done by doing edit/wallet/wallet-contents
 * for example.
 */
extern PRBool keySet;
if (!keySet) user=0; else
#endif

  user = si_GetUser(URLName, PR_FALSE, name);
  if (user) {
    SI_LoadSignonData(TRUE); /* this destroys user so need to recaculate it */
    user = si_GetUser(URLName, PR_TRUE, name);
    if (user) { /* this should always be true but just in case */
      PRInt32 dataCount = LIST_COUNT(user->signonData_list);
      for (PRInt32 i=0; i<dataCount; i++) {
        data = NS_STATIC_CAST(si_SignonDataStruct*, user->signonData_list->ElementAt(i));
        if(name && PL_strcmp(data->name, name)==0) {
          *value = data->value;
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
si_RememberSignonDataFromBrowser(char* URLName, char* username, char* password) {
  LO_FormSubmitData submit;
  char* USERNAME = "username";
  char* PASSWORD = "password";
  char* name_array[2];
  char* value_array[2];
  uint8 type_array[2];

  /* do nothing if signon preference is not enabled */
  if (!si_GetSignonRememberingPref()){
    return;
  }

  /* initialize a temporary submit structure */
  submit.name_array = (PA_Block)name_array;
  submit.value_array = (PA_Block)value_array;
  submit.type_array = (PA_Block)type_array;
  submit.value_cnt = 2;

  name_array[0] = USERNAME;
  value_array[0] = NULL;
  StrAllocCopy(value_array[0], username);
  type_array[0] = FORM_TYPE_TEXT;

  name_array[1] = PASSWORD;
  value_array[1] = NULL;
  StrAllocCopy(value_array[1], password);
  type_array[1] = FORM_TYPE_PASSWORD;

  /* Save the signon data */
  SI_LoadSignonData(TRUE);
  si_PutData(URLName, &submit, PR_TRUE);

  /* Free up the data memory just allocated */
  PR_Free(value_array[0]);
  PR_Free(value_array[1]);
}

PUBLIC void
SI_RememberSignonDataFromBrowser (char* URLName, char* username, char* password) {
  if (si_OkToSave(URLName, username)) {
    si_RememberSignonDataFromBrowser (URLName, username, password);
  }
}

/*
 * Check for remembered data from a previous browser-generated password dialog
 * restore it if so
 */
PRIVATE void
si_RestoreOldSignonDataFromBrowser
    (char* URLName, PRBool pickFirstUser, char** username, char** password) {
  si_SignonUserStruct* user;
  si_SignonDataStruct* data;

  /* get the data from previous time this URL was visited */
  si_lock_signon_list();
  user = si_GetUser(URLName, pickFirstUser, "username");
  if (!user) {
    /* leave original username and password from caller unchanged */
    /* username = 0; */
    /* *password = 0; */
    si_unlock_signon_list();
    return;
  }
  SI_LoadSignonData(TRUE); /* this destroys "user" so need to recalculate it */
  user = si_GetUser(URLName, pickFirstUser, "username");

  /* restore the data from previous time this URL was visited */
  PRInt32 dataCount = LIST_COUNT(user->signonData_list);
  for (PRInt32 i=0; i<dataCount; i++) {
    data = NS_STATIC_CAST(si_SignonDataStruct*, user->signonData_list->ElementAt(i));
    if(PL_strcmp(data->name, "username")==0) {
      StrAllocCopy(*username, data->value);
    } else if(PL_strcmp(data->name, "password")==0) {
      StrAllocCopy(*password, data->value);
    }
  }
  si_unlock_signon_list();
}

/* Browser-generated prompt for user-name and password */
PUBLIC PRBool
SINGSIGN_PromptUsernameAndPassword
    (char *prompt, char **username, char **password, char *URLName) {
  PRBool status;
  char *copyOfPrompt=0;

  /* just for safety -- really is a problem in SINGSIGN_Prompt */
  StrAllocCopy(copyOfPrompt, prompt);

  /* do the FE thing if signon preference is not enabled */
  if (!si_GetSignonRememberingPref()){
    status = si_PromptUsernameAndPassword(copyOfPrompt, username, password);
    PR_FREEIF(copyOfPrompt);
    return status;
  }

  /* prefill with previous username/password if any */
  si_RestoreOldSignonDataFromBrowser(URLName, PR_FALSE, username, password);

  /* get new username/password from user */
  status = si_PromptUsernameAndPassword(copyOfPrompt, username, password);

  /* remember these values for next time */
  if (status && si_OkToSave(URLName, *username)) {
    si_RememberSignonDataFromBrowser (URLName, *username, *password);
  }

  /* cleanup and return */
  PR_FREEIF(copyOfPrompt);
  return status;
}

/* Browser-generated prompt for password */
PUBLIC char *
SINGSIGN_PromptPassword
    (char *prompt, char *URLName, PRBool pickFirstUser) {
  char *password=0, *username=0, *copyOfPrompt=0, *result;
  char *urlname = URLName;
  char *s;

  /* just for safety -- really is a problem in SINGSIGN_Prompt */
  StrAllocCopy(copyOfPrompt, prompt);

  /* do the FE thing if signon preference is not enabled */
  if (!si_GetSignonRememberingPref()){
    result = si_PromptPassword(copyOfPrompt);
    PR_FREEIF(copyOfPrompt);
    return result;
  }

  /* get host part of URL which is of form user@host */
  s = URLName;
  while (*s && (*s != '@')) {
    s++;
  }
  if (*s) {
    /* @ found, host is URL part following the @ */
    urlname = s+1; /* urlname is part of URL after the @ */
  }

  /* get previous password used with this username */
  si_RestoreOldSignonDataFromBrowser(urlname, pickFirstUser, &username, &password);

  /* return if a password was found */
  /*
   * Note that we reject a password of " ".  It is a dummy password
   * that was put in by a preceding call to SINGSIGN_Prompt.  This occurs
   * in mknews which calls on SINGSIGN_Prompt to get the username and then
   * SINGSIGN_PromptPassword to get the password (why they didn't simply
   * call on SINGSIGN_PromptUsernameAndPassword is beyond me).  So the call
   * to SINGSIGN_Prompt will save the username along with the dummy password.
   * In this call to SI_Password, the real password gets saved in place
   * of the dummy one.
   */
  if (password && PL_strlen(password) && PL_strcmp(password, " ")) {
    PR_FREEIF(copyOfPrompt);
    return password;
  }

  /* if no password found, get new password from user */
  password = si_PromptPassword(copyOfPrompt);

  /* if username wasn't even found, extract it from URLName */
  if (!username) {
    s = URLName;

    /* get to @ in URL if any */
    while (*s && (*s != '@')) {
      s++;
    }
    if (*s) {
      /* @ found, username is URL part preceding the @ */
      *s = '\0';
      StrAllocCopy(username, URLName);
      *s = '@';
    } else {
      /* no @ found, use entire URL as uername */
    StrAllocCopy(username, URLName);
    }
  }

  /* remember these values for next time */
  if (password && PL_strlen(password) && si_OkToSave(urlname, username)) {
    si_RememberSignonDataFromBrowser (urlname, username, password);
  }

  /* cleanup and return */
  PR_FREEIF(username);
  PR_FREEIF(copyOfPrompt);
  return password;
}

/* Browser-generated prompt for username */
PUBLIC char *
SINGSIGN_Prompt (char *prompt, char* defaultUsername, char *URLName)
{
  char *password=0, *username=0, *copyOfPrompt=0, *result;

  /*
   * make copy of prompt because it is currently in a temporary buffer in
   * the caller (from GetString) which will be destroyed if GetString() is
   * called again before prompt is used
   */
  StrAllocCopy(copyOfPrompt, prompt);

  /* do the FE thing if signon preference is not enabled */
  if (!si_GetSignonRememberingPref()){
    result = si_Prompt(copyOfPrompt, defaultUsername);
    PR_FREEIF(copyOfPrompt);
    return result;
  }

  /* get previous username used */
  if (!defaultUsername || !PL_strlen(defaultUsername)) {
    si_RestoreOldSignonDataFromBrowser(URLName, PR_FALSE, &username, &password);
  } else {
     StrAllocCopy(username, defaultUsername);
  }

  /* prompt for new username */
  result = si_Prompt(copyOfPrompt, username);

  /* remember this username for next time */
  if (result && PL_strlen(result)) {
    if (username && (PL_strcmp(username, result) == 0)) {
      /*
       * User is re-using the same user name as from previous time
       * so keep the previous password as well
       */
      si_RememberSignonDataFromBrowser (URLName, result, password);
    } else {
      /*
       * We put in a dummy password of " " which we will test
       * for in a following call to SINGSIGN_PromptPassword.  See comments
       * in that routine.
       */
      si_RememberSignonDataFromBrowser (URLName, result, " ");
    }
  }

  /* cleanup and return */
  PR_FREEIF(username);
  PR_FREEIF(password);
  PR_FREEIF(copyOfPrompt);
  return result;
}


/*****************
 * Signon Viewer *
 *****************/

/* return PR_TRUE if "number" is in sequence of comma-separated numbers */
PUBLIC
PRBool SI_InSequence(char* sequence, int number) {
  char* ptr;
  char* endptr;
  char* undo = NULL;
  PRBool retval = PR_FALSE;
  int i;

  /* not necessary -- routine will work even with null sequence */
  if (!*sequence) {
    return PR_FALSE;
  }

  for (ptr = sequence ; ptr ; ptr = endptr) {

    /* get to next comma */
    endptr = PL_strchr(ptr, ',');

    /* if comma found, process it */
    if (endptr) {

      /* restore last comma-to-null back to comma */
      if (undo) {
        *undo = ',';
      }

      /* set the comma to a null */
      undo = endptr;
      *endptr++ = '\0';

      /* compare the number before the comma with "number" */
      if (*ptr) {
        i = atoi(ptr);
        if (i == number) {

          /* "number" was in the sequence so return PR_TRUE */
          retval = PR_TRUE;
          break;
        }
      }
    }
  }

  /* restore last comma-to-null back to comma */
  if (undo) {
    *undo = ',';
  }
  return retval;
}

PUBLIC char*
SI_FindValueInArgs(nsAutoString results, char* name) {
  /* note: name must start and end with a vertical bar */
  nsAutoString value;
  PRInt32 start, length;
  start = results.Find(name);
  PR_ASSERT(start >= 0);
  if (start < 0) {
    return nsAutoString("").ToNewCString();
  }
  start += PL_strlen(name); /* get passed the |name| part */
  length = results.FindChar('|', PR_FALSE,start) - start;
  results.Mid(value, start, length);
  return value.ToNewCString();
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
  char * gone = SI_FindValueInArgs(results, "|goneS|");
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
  delete[] gone;

  /* step backwards through all rejects and delete those that are in the sequence */
  gone = SI_FindValueInArgs(results, "|goneR|");
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
  delete[] gone;

  /* now give wallet a chance to do its deletions */
  Wallet_SignonViewerReturn(results);
}

#define BUFLEN2 5000
#define BREAK '\001'

PUBLIC void
SINGSIGN_GetSignonListForViewer(nsString& aSignonList)
{
  char *buffer = (char*)PR_Malloc(BUFLEN2);
  int g = 0, signonNum;
  si_SignonURLStruct *url;
  si_SignonUserStruct * user;
  si_SignonDataStruct* data;

  /* force loading of the signons file */
  si_RegisterSignonPrefCallbacks();

  buffer[0] = '\0';
  signonNum = 0;
  PRInt32 urlCount = LIST_COUNT(si_signon_list);
  for (PRInt32 i=0; i<urlCount; i++) {
    url = NS_STATIC_CAST(si_SignonURLStruct*, si_signon_list->ElementAt(i));
    PRInt32 userCount = LIST_COUNT(url->signonUser_list);
    for (PRInt32 j=0; j<userCount; j++) {
      user = NS_STATIC_CAST(si_SignonUserStruct*, url->signonUser_list->ElementAt(j));

      /* first data item for user is the username */
      data = (si_SignonDataStruct *) (user->signonData_list->ElementAt(0));
      g += PR_snprintf(buffer+g, BUFLEN2-g,
"%c        <OPTION value=%d>%s:%s</OPTION>\n",
        BREAK,
        signonNum,
        url->URLName,
        data->value
      );
      signonNum++;
    }
  }
  aSignonList = buffer;
  PR_FREEIF(buffer);
}

PUBLIC void
SINGSIGN_GetRejectListForViewer(nsString& aRejectList)
{
  char *buffer = (char*)PR_Malloc(BUFLEN2);
  int g = 0, rejectNum;
  si_Reject *reject;

  /* force loading of the signons file */
  si_RegisterSignonPrefCallbacks();

  buffer[0] = '\0';
  rejectNum = 0;
  PRInt32 rejectCount = LIST_COUNT(si_reject_list);
  for (PRInt32 i=0; i<rejectCount; i++) {
    reject = NS_STATIC_CAST(si_Reject*, si_reject_list->ElementAt(i));
    g += PR_snprintf(buffer+g, BUFLEN2-g,
"%c        <OPTION value=%d>%s:%s</OPTION>\n",
      BREAK,
      rejectNum,
      reject->URLName,
      reject->userName
    );
    rejectNum++;
  }
  aRejectList = buffer;
  PR_FREEIF(buffer);
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
  LO_FormSubmitData submit;
  char* name_array[MAX_ARRAY_SIZE];
  char* value_array[MAX_ARRAY_SIZE];
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
  submit.name_array = (PA_Block)name_array;
  submit.value_array = (PA_Block)value_array;
  submit.type_array = (PA_Block)type_array;

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

      /* free up all the allocations done for processing this URL */
      for (i = 0; i < submit.value_cnt; i++) {
        PR_Free(name_array[i]);
        PR_Free(value_array[i]);
      }
    } else {
      /* reject */
      si_PutReject(URLName, buffer, PR_FALSE);
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
