/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/*native java calls from oji into security system dealing with user targets*/
#ifndef _JPERMISSION_H_
#define _JPERMISSION_H_

typedef enum nsPermState {
  nsPermState_NotSet,
  nsPermState_AllowedForever,
  nsPermState_AllowedSession,
  nsPermState_ForbiddenForever,
  nsPermState_BlankSession
} nsPermState;

PR_PUBLIC_API(void)
java_netscape_security_savePrivilege(nsPermState permState);

PR_PUBLIC_API(nsPermState)
nsJSJavaDisplayDialog(char *prinStr, char *targetStr, char *rsikStr, PRBool isCert, void*cert);

PR_PUBLIC_API(void)
java_netscape_security_getTargetDetails(const char *charSetName,
                                        char* targetName, 
                                        char** details, 
                                        char **risk);

#endif /* _JPERMISSION_H_ */
