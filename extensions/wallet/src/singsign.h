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


/*
   singsign.h --- prototypes for wallet functions.
*/


#ifndef _SINGSIGN_H
#define _SINGSIGN_H

#include "ntypes.h"
#include "nsString.h"

XP_BEGIN_PROTOS

extern void
SINGSIGN_GetSignonListForViewer (nsString& aSignonList);

extern void
SINGSIGN_GetRejectListForViewer (nsString& aRejectList);

extern void
SINGSIGN_SignonViewerReturn(nsAutoString results);

extern void
SINGSIGN_RestoreSignonData(char* URLName, char* name, char** value);

#ifdef xxx
extern PRBool
SINGSIGN_PromptUsernameAndPassword2 
    (char *prompt, char **username, char **password, char *URLName);

extern char *
SINGSIGN_PromptPassword2 (char *prompt, char *URLName, PRBool pickFirstUser);

extern char *
SINGSIGN_Prompt2 (char *prompt, char* defaultUsername, char *URLName);
#endif

extern nsresult
SINGSIGN_PromptUsernameAndPassword
    (const PRUnichar *text, PRUnichar **user, PRUnichar **pwd,
     PRBool *returnValue, char* urlname);

extern nsresult
SINGSIGN_PromptPassword
    (const PRUnichar *text, PRUnichar **pwd, PRBool *returnValue, char* urlname);

extern nsresult
SINGSIGN_Prompt
    (const PRUnichar *text, const PRUnichar *defaultText, PRUnichar **resultText,
     PRBool *returnValue, char* urlname);

extern PRBool
SINGSIGN_RemoveUser(char *URLName, char *userName);

XP_END_PROTOS

#endif /* !_SINGSIGN_H */
