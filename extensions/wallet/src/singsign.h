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

XP_BEGIN_PROTOS

extern void
SINGSIGN_DisplaySignonInfoAsHTML();

extern void
SINGSIGN_RememberSignonData(char* URLName, char** name_array, char** value_array, char** type_array, PRInt32 value_cnt);

extern void
SINGSIGN_RestoreSignonData(char* URLName, char* name, char** value);

extern PRBool
SINGSIGN_PromptUsernameAndPassword 
    (char *prompt, char **username, char **password, char *URLName);

extern char *
SINGSIGN_PromptPassword (char *prompt, char *URLName, PRBool pickFirstUser);

extern char *
SINGSIGN_Prompt (char *prompt, char* defaultUsername, char *URLName);

XP_END_PROTOS

#endif /* !_SINGSIGN_H */
