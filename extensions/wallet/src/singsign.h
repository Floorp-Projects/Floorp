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
class nsIPrompt;
XP_BEGIN_PROTOS

extern void
SINGSIGN_GetSignonListForViewer (nsAutoString& aSignonList);

extern void
SINGSIGN_GetRejectListForViewer (nsAutoString& aRejectList);

extern void
SINGSIGN_SignonViewerReturn(nsAutoString results);

extern void
SINGSIGN_RestoreSignonData(char* URLName, PRUnichar* name, PRUnichar** value, PRUint32 elementNumber);

extern nsresult
SINGSIGN_PromptUsernameAndPassword
    (const PRUnichar *text, PRUnichar **user, PRUnichar **pwd,
     const char *urlname,nsIPrompt* dialog, PRBool *returnValue, PRBool strip);

extern nsresult
SINGSIGN_PromptPassword
    (const PRUnichar *text, PRUnichar **pwd, const char *urlname,
    nsIPrompt* dialog, PRBool *returnValue, PRBool strip);

extern nsresult
SINGSIGN_Prompt
    (const PRUnichar *text, const PRUnichar *defaultText, PRUnichar **resultText,
     const char *urlname,nsIPrompt* dialog, PRBool *returnValue, PRBool strip);

extern PRBool
SINGSIGN_RemoveUser
    (const char *URLName, const PRUnichar *userName, PRBool strip);

extern PRBool
SINGSIGN_StorePassword
    (const char *URLName, const PRUnichar *userName, const PRUnichar *password, PRBool strip);

extern nsresult
SINGSIGN_HaveData(const char *url, const PRUnichar *userName, PRBool strip, PRBool *retval);


XP_END_PROTOS

#endif /* !_SINGSIGN_H */
