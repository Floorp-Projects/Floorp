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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef PERMISSIONS_H
#define PERMISSIONS_H

#include "nsString.h"

#define COOKIEPERMISSION 0
#define IMAGEPERMISSION 1
#define NUMBER_OF_PERMISSIONS 2

typedef enum {
  PERMISSION_Accept,
  PERMISSION_DontAcceptForeign,
  PERMISSION_DontUse
} PERMISSION_BehaviorEnum;

class nsIPrompt;

extern nsresult PERMISSION_Read();
extern void PERMISSION_Add(const char * objectURL, PRBool permission, PRInt32 type);
extern void PERMISSION_RemoveAll();
extern void PERMISSION_DeletePersistentUserData(void);

extern PRInt32 PERMISSION_HostCount();
extern PRInt32 PERMISSION_TypeCount(PRInt32 host);
extern nsresult PERMISSION_Enumerate
  (PRInt32 hostNumber, PRInt32 typeNumber, char **host, PRInt32 *type, PRBool *capability);
extern void PERMISSION_Remove(const char* host, PRInt32 type);

extern PRBool Permission_Check
  (nsIPrompt *aPrompter, const char * hostname, PRInt32 type,
   PRBool warningPref, PRUnichar * message);
extern nsresult Permission_AddHost
  (char * host, PRBool permission, PRInt32 type, PRBool save);
//extern void Permission_Free(PRInt32 hostNumber, PRInt32 type, PRBool save);
extern void Permission_Save();

#endif /* PERMISSIONS_H */
