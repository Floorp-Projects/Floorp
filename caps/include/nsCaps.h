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

#ifndef _NS_CAPS_H_
#define _NS_CAPS_H_

#include "prtypes.h"
PR_BEGIN_EXTERN_C

class nsTarget;
class nsIPrincipal;
class nsIPrivilege;
class nsPrivilegeTable;
class nsPrivilegeManager;
struct NSJSJavaFrameWrapper;

/* wrappers for nsPrivilegeManager object */
PR_IMPLEMENT(PRBool) 
nsCapsInitialize();

PR_EXTERN(PRBool) 
nsCapsRegisterPrincipal(class nsIPrincipal * principal); 

PR_EXTERN(PRBool) 
nsCapsEnablePrivilege(void* context, class nsITarget * target, PRInt32 callerDepth);

PR_EXTERN(PRBool) 
nsCapsIsPrivilegeEnabled(void* context, class nsITarget * target, PRInt32 callerDepth);

PR_EXTERN(PRBool) 
nsCapsRevertPrivilege(void* context, class nsITarget * target, PRInt32 callerDepth);

PR_EXTERN(PRBool) 
nsCapsDisablePrivilege(void* context, class nsITarget * target, PRInt32 callerDepth);

PR_EXTERN(void*) 
nsCapsGetClassPrincipalsFromStack(void* context, PRInt32 callerDepth);

PR_EXTERN(PRBool) 
nsCapsCanExtendTrust(void* from, void* to);



/* wrappers for nsPrincipal object */
PR_EXTERN(class nsIPrincipal *) 
nsCapsNewPrincipal(PRInt16 * principalType, void * key, 
                   PRUint32 key_len, void *zig);

PR_EXTERN(const char *) 
nsCapsPrincipalToString(nsIPrincipal * principal);

PR_EXTERN(PRBool) 
nsCapsIsCodebaseExact(nsIPrincipal * principal);

PR_EXTERN(const char *) 
nsCapsPrincipalGetVendor(nsIPrincipal * principal);

PR_EXTERN(void *) 
nsCapsNewPrincipalArray(PRUint32 count);

/* wrappers for nsITarget object */
PR_EXTERN(class nsITarget *) 
nsCapsFindTarget(char * name);


/* wrappers for nsPrivilege object */
PR_EXTERN(PRInt16) 
nsCapsGetPermission(class nsIPrivilege * privilege);


/* wrappers for nsPrivilegeTable object */
PR_EXTERN(nsIPrivilege *)
nsCapsGetPrivilege(nsPrivilegeTable * annotation, class nsITarget * target);

/* Methods for stack walking */

extern struct NSJSJavaFrameWrapper * (*nsCapsNewNSJSJavaFrameWrapperCallback)(void *);
PR_EXTERN(void)
setNewNSJSJavaFrameWrapperCallback(struct NSJSJavaFrameWrapper * (*fp)(void *));

extern void (*nsCapsFreeNSJSJavaFrameWrapperCallback)(struct NSJSJavaFrameWrapper *);
PR_EXTERN(void)
setFreeNSJSJavaFrameWrapperCallback(void (*fp)(struct NSJSJavaFrameWrapper *));

extern void (*nsCapsGetStartFrameCallback)(struct NSJSJavaFrameWrapper *);
PR_EXTERN(void)
setGetStartFrameCallback(void (*fp)(struct NSJSJavaFrameWrapper *));

extern PRBool (*nsCapsIsEndOfFrameCallback)(struct NSJSJavaFrameWrapper *);
PR_EXTERN(void)
setIsEndOfFrameCallback(PRBool (*fp)(struct NSJSJavaFrameWrapper *));

extern PRBool (*nsCapsIsValidFrameCallback)(struct NSJSJavaFrameWrapper *);
PR_EXTERN(void)
setIsValidFrameCallback(PRBool (*fp)(struct NSJSJavaFrameWrapper *));

extern void * (*nsCapsGetNextFrameCallback)(struct NSJSJavaFrameWrapper *, int *);
PR_EXTERN(void)
setGetNextFrameCallback(void * (*fp)(struct NSJSJavaFrameWrapper *, int *));

extern void * (*nsCapsGetPrincipalArrayCallback)(struct NSJSJavaFrameWrapper *);
PR_EXTERN(void)
setOJIGetPrincipalArrayCallback(void * (*fp)(struct NSJSJavaFrameWrapper *));

extern void * (*nsCapsGetAnnotationCallback)(struct NSJSJavaFrameWrapper *);
PR_EXTERN(void)
setOJIGetAnnotationCallback(void * (*fp)(struct NSJSJavaFrameWrapper *));

extern void * (*nsCapsSetAnnotationCallback)(struct NSJSJavaFrameWrapper *, void *);
PR_EXTERN(void)
setOJISetAnnotationCallback(void * (*fp)(struct NSJSJavaFrameWrapper *, void *));

/* 
 * Registration flag is set when the communicator
 * starts with argument '-reg_mode'. The following
 * functions provide API to enable and disable the flag.
 * Current state of the flag can be obtained using
 * nsGetRegistrationModeFlag().
 */
void 
nsCapsEnableRegistrationModeFlag(void);

void 
nsCapsDisableRegistrationModeFlag(void);

PRBool 
nsCapsGetRegistrationModeFlag(void);

PR_END_EXTERN_C

#endif /* _NS_CAPS_H_ */
