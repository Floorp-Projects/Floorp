/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
#include "java_security_AccessController.h"
#include "prprf.h"

extern "C" {

/*
 * Class : java/security/AccessController
 * Method : beginPrivileged
 * Signature : ()V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_security_AccessController_beginPrivileged()
{
  PR_fprintf(PR_STDERR, "Netscape_Java_java_security_AccessController_beginPrivilege not implemented\n");
}


/*
 * Class : java/security/AccessController
 * Method : endPrivileged
 * Signature : ()V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_security_AccessController_endPrivileged()
{
  PR_fprintf(PR_STDERR, "Netscape_Java_java_security_AccessController_endPrivileged not implemented\n");
}



/*
 * Class : java/security/AccessController
 * Method : getInheritedAccessControlContext
 * Signature : ()Ljava/security/AccessControlContext;
 */
NS_EXPORT NS_NATIVECALL(Java_java_security_AccessControlContext *)
Netscape_Java_java_security_AccessController_getInheritedAccessControlContext()
{
  PR_fprintf(PR_STDERR, "Netscape_Java_java_security_AccessController_getInheritedAccessControlContext not implemented\n");
  return 0;
}



/*
 * Class : java/security/AccessController
 * Method : getStackAccessControlContext
 * Signature : ()Ljava/security/AccessControlContext;
 */
NS_EXPORT NS_NATIVECALL(Java_java_security_AccessControlContext *)
Netscape_Java_java_security_AccessController_getStackAccessControlContext()
{
  PR_fprintf(PR_STDERR, "Netscape_Java_java_security_AccessController_getStackAccessControlContext not implemented\n");
  return 0;

}

}





