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
 */


/*
   singsign.h --- prototypes for wallet functions.
*/


#ifndef _SINGSIGN_H
#define _SINGSIGN_H

#include "ntypes.h"
#include "nsString.h"
#include "nsVoidArray.h"

class nsIPrompt;
XP_BEGIN_PROTOS

extern void
SINGSIGN_GetSignonListForViewer (nsAutoString& aSignonList);

extern void
SINGSIGN_GetRejectListForViewer (nsAutoString& aRejectList);

extern void
SINGSIGN_SignonViewerReturn(nsAutoString results);

extern void
SINGSIGN_RestoreSignonData(char* passwordRealm, PRUnichar* name, PRUnichar** value, PRUint32 elementNumber);

extern nsresult
SINGSIGN_PromptUsernameAndPassword
    (const PRUnichar *dialogTitle, const PRUnichar *text, PRUnichar **user, PRUnichar **pwd,
     const char* passwordRealm, nsIPrompt* dialog, PRBool *returnValue, PRBool persistPassword = PR_TRUE);

extern nsresult
SINGSIGN_PromptPassword
    (const PRUnichar *dialogTitle, const PRUnichar *text, PRUnichar **pwd, const char* passwordRealm,
     nsIPrompt* dialog, PRBool *returnValue, PRBool persistPassword = PR_TRUE);

extern nsresult
SINGSIGN_Prompt
    (const PRUnichar *dialogTitle, const PRUnichar *text, const PRUnichar *defaultText, PRUnichar **resultText,
     const char* passwordRealm, nsIPrompt* dialog, PRBool *returnValue);

extern PRBool
SINGSIGN_RemoveUser
    (const char* passwordRealm, const PRUnichar *userName);

extern PRBool
SINGSIGN_StorePassword
    (const char* passwordRealm, const PRUnichar *userName, const PRUnichar *password);

extern nsresult
SINGSIGN_HaveData(const char* passwordRealm, const PRUnichar *userName, PRBool *retval);

typedef int (*PR_CALLBACK PrefChangedFunc) (const char *, void *);

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

extern void SI_InitSignonFileName();

extern PRBool
SI_InSequence(nsAutoString sequence, int number);

extern PRUnichar*
SI_FindValueInArgs(nsAutoString results, nsAutoString name);

extern PRBool
SINGSIGN_ReencryptAll();

extern void
SINGSIGN_RememberSignonData (char* URLName, nsVoidArray * signonData);

XP_END_PROTOS

#endif /* !_SINGSIGN_H */
