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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/*native java calls from oji into security system*/
#ifndef _ADMIN_H_
#define _ADMIN_H_

PR_BEGIN_EXTERN_C

PR_PUBLIC_API(const char *)
java_netscape_security_getPrincipals(const char *charSetName);

PR_PUBLIC_API(PRBool)
java_netscape_security_removePrincipal(const char *charSetName, char *prinName);


PR_PUBLIC_API(void)
java_netscape_security_getPrivilegeDescs(const char *charSetName, char *prinName,
                                         char** forever, char** session, 
                                         char **denied);

PR_PUBLIC_API(PRBool)
java_netscape_security_removePrivilege(const char *charSetName, char *prinName, 
                                       char *targetName);

PR_END_EXTERN_C

#endif /* _ADMIN_H_ */
