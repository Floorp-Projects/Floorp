/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
   singsign.h --- prototypes for wallet functions.
*/


#ifndef _SINGSIGN_H
#define _SINGSIGN_H

#include "xp_core.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsIPref.h"
#include "nsIDOMWindowInternal.h"

/* Duplicates defines as in nsIPrompt.idl -- keep in sync! */
#define SINGSIGN_SAVE_PASSWORD_NEVER 0
#define SINGSIGN_SAVE_PASSWORD_FOR_SESSION    1
#define SINGSIGN_SAVE_PASSWORD_PERMANENTLY    2

class nsIPrompt;
XP_BEGIN_PROTOS

extern PRInt32
SINGSIGN_HostCount();

extern PRInt32
SINGSIGN_UserCount(PRInt32 host);

extern PRInt32
SINGSIGN_RejectCount();

extern nsresult
SINGSIGN_Enumerate
  (PRInt32 hostNumber, PRInt32 userNumber, char **host, PRUnichar **user, PRUnichar **pswd);

extern nsresult
SINGSIGN_RejectEnumerate(PRInt32 rejectNumber, char **host);

extern void
SINGSIGN_SignonViewerReturn(const nsString& results);

extern void
SINGSIGN_RestoreSignonData(nsIPrompt* dialog, const char* passwordRealm, const PRUnichar* name, PRUnichar** value, PRUint32 elementNumber);

extern nsresult
SINGSIGN_PromptUsernameAndPassword
    (const PRUnichar *dialogTitle, const PRUnichar *text, PRUnichar **user, PRUnichar **pwd,
     const char* passwordRealm, nsIPrompt* dialog, PRBool *returnValue,
     PRUint32 savePassword = SINGSIGN_SAVE_PASSWORD_PERMANENTLY);

extern nsresult
SINGSIGN_PromptPassword
    (const PRUnichar *dialogTitle, const PRUnichar *text, PRUnichar **pwd,
     const char* passwordRealm, nsIPrompt* dialog, PRBool *returnValue,
     PRUint32 savePassword = SINGSIGN_SAVE_PASSWORD_PERMANENTLY);

extern nsresult
SINGSIGN_Prompt
    (const PRUnichar *dialogTitle, const PRUnichar *text, const PRUnichar *defaultText, PRUnichar **resultText,
     const char* passwordRealm, nsIPrompt* dialog, PRBool *returnValue,
     PRUint32 savePassword = SINGSIGN_SAVE_PASSWORD_PERMANENTLY);

extern nsresult
SINGSIGN_RemoveUser
    (const char* passwordRealm, const PRUnichar *userName);

extern nsresult
SINGSIGN_RemoveReject
    (const char* host);

extern PRBool
SINGSIGN_StorePassword
    (const char* passwordRealm, const PRUnichar *userName, const PRUnichar *password);

extern nsresult
SINGSIGN_HaveData(nsIPrompt* dialog, const char* passwordRealm, const PRUnichar *userName, PRBool *retval);

extern void
SI_RegisterCallback(const char* domain, PrefChangedFunc callback, void* instance_data);

extern void
SI_UnregisterCallback(const char* domain, PrefChangedFunc callback, void* instance_data);

extern PRBool
SI_GetBoolPref(const char * prefname, PRBool defaultvalue);

extern void
SI_SetBoolPref(const char * prefname, PRBool prefvalue);

extern void
SI_SetCharPref(const char * prefname, const char * prefvalue);

extern void
SI_GetCharPref(const char * prefname, char** aPrefvalue);

extern void
SI_GetLocalizedUnicharPref(const char * prefname, PRUnichar** aPrefvalue);

extern void SI_InitSignonFileName();

extern PRBool
SI_InSequence(const nsString& sequence, int number);

extern void
SI_FindValueInArgs(const nsAReadableString& results, const nsAReadableString& name, nsAWritableString& value);

extern void
SI_DeleteAll();

extern void
SI_ClearUserData();

extern void
SI_DeletePersistentUserData();

extern PRBool
SINGSIGN_ReencryptAll();

extern void
SINGSIGN_RememberSignonData
  (nsIPrompt* dialog, const char* URLName, nsVoidArray * signonData, nsIDOMWindowInternal* window);

XP_END_PROTOS

#endif /* !_SINGSIGN_H */
