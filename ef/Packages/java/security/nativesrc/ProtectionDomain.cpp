/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "java_security_ProtectionDomain.h"
#include "prprf.h"

extern "C" {

/*
 * Class : java/security/ProtectionDomain
 * Method : getProtectionDomain
 * Signature : (Ljava/lang/Class;)Ljava/security/ProtectionDomain;
 */
NS_EXPORT NS_NATIVECALL(Java_java_security_ProtectionDomain *)
Netscape_Java_java_security_ProtectionDomain_getProtectionDomain(Java_java_lang_Class *)
{
  PR_fprintf(PR_STDERR, "Netscape_Java_java_security_ProtectionDomain_getProtectionDomain not implemented\n");
  return 0;
}


/*
 * Class : java/security/ProtectionDomain
 * Method : setProtectionDomain
 * Signature : (Ljava/lang/Class;Ljava/security/ProtectionDomain;)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_security_ProtectionDomain_setProtectionDomain(Java_java_lang_Class *, Java_java_security_ProtectionDomain *)
{
  PR_fprintf(PR_STDERR, "Netscape_Java_java_security_ProtectionDomain_setProtectionDomain not implemented\n");

}

}

