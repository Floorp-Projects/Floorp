/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsIScriptSecurityManager_h__
#define nsIScriptSecurityManager_h__

#include "nscore.h"
#include "jsapi.h"
#include "nsString.h"
#include "nsISupports.h"

class nsIScriptGlobalObject;
class nsIScriptContext;
class nsIDOMEvent;
class nsIURI;

/*
 * Event listener interface.
 */

#define NS_ISCRIPTSECURITYMANAGER_IID \
{ /* 58df5780-8006-11d2-bd91-00805f8ae3f4 */ \
0x58df5780, 0x8006, 0x11d2, \
{0xbd, 0x91, 0x00, 0x80, 0x5f, 0x8a, 0xe3, 0xf4} }

typedef enum eJSTarget {
    eJSTarget_UniversalBrowserRead,
    eJSTarget_UniversalBrowserWrite,
    eJSTarget_UniversalSendMail,
    eJSTarget_UniversalFileRead,
    eJSTarget_UniversalFileWrite,
    eJSTarget_UniversalPreferencesRead,
    eJSTarget_UniversalPreferencesWrite,
    eJSTarget_UniversalDialerAccess,
    eJSTarget_Max
} eJSTarget;

typedef struct nsJSPrincipalsList {
    JSPrincipals *principals;
    struct nsJSPrincipalsList *next;
} nsJSPrincipalsList;

class nsIScriptSecurityManager : public nsISupports {

public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISCRIPTSECURITYMANAGER_IID);

 /**
  * Inits the security manager
  */
  NS_IMETHOD Init() = 0;

  NS_IMETHOD CheckScriptAccess(nsIScriptContext* aContext, void* aObj, const char* aProp, PRBool* aResult) = 0;
/**
  * Existing API from lib/libmocha/lm_taint.c.  I'm maintaining the api largely as is, just xpcom'ifying
  * it.  After I get security working I'll reevaluate the need for each of these individually  -joki
  */
  NS_IMETHOD GetSubjectOriginURL(JSContext *aCx, nsString* aOrigin) = 0;
  NS_IMETHOD GetObjectOriginURL(JSContext *aCx, JSObject *object, nsString* aOrigin) = 0;
  NS_IMETHOD GetPrincipalsFromStackFrame(JSContext *aCx, JSPrincipals** aPrincipals) = 0;
  NS_IMETHOD GetCompilationPrincipals(nsIScriptContext *aContext, nsIScriptGlobalObject* aGlobal, 
                                      JSPrincipals *aLayoutPrincipals, JSPrincipals** aPrincipals) = 0;
  NS_IMETHOD CanAccessTarget(JSContext *cx, eJSTarget target, PRBool* aReturn) = 0;
  NS_IMETHOD CheckPermissions(JSContext *cx, JSObject *obj, eJSTarget target, PRBool* aReturn) = 0;
  NS_IMETHOD CheckContainerAccess(JSContext *aCx, JSObject *aObj, eJSTarget aTarget, PRBool* aReturn) = 0;
  NS_IMETHOD GetContainerPrincipals(JSContext *aCx, JSObject *aContainer, JSPrincipals** aPrincipals) = 0;
  NS_IMETHOD SetContainerPrincipals(JSContext *aCx, JSObject *aContainer, JSPrincipals* aPrincipals) = 0;
  NS_IMETHOD CanCaptureEvent(JSContext *aCx, JSFunction *aFun, JSObject *aEventTarget, PRBool* aReturn) = 0;
  NS_IMETHOD SetExternalCapture(JSContext *aCx, JSPrincipals *aPrincipals, PRBool aBool) = 0;
  NS_IMETHOD CheckSetParentSlot(JSContext *aCx, JSObject *aObj, jsval aId, jsval *aVp, PRBool* aReturn) = 0;
  NS_IMETHOD SetDocumentDomain(JSContext *aCx, JSPrincipals *aPrincipals, 
                               nsString* aNewDomain, PRBool* aReturn) = 0;
  NS_IMETHOD DestroyPrincipalsList(JSContext *aCx, nsJSPrincipalsList *aList) = 0;
  NS_IMETHOD NewJSPrincipals(nsIURI *aURL, nsString* aName, nsString* aCodebase, JSPrincipals** aPrincipals) = 0;
#ifdef DO_JAVA_STUFF
  NS_IMETHOD ExtractFromPrincipalsArchive(JSPrincipals *aPrincipals, char *aName, 
                                uint *aLength, char** aReturn) = 0;
  NS_IMETHOD SetUntransformedSource(JSPrincipals *principals, char *original, 
                          char *transformed, PRBool* aReturn) = 0;
  NS_IMETHOD GetJSPrincipalsFromJavaCaller(JSContext *cx, void *principalsArray, void *pNSISecurityContext, JSPrincipals** aPrincipals) = 0;
#endif
#if 0
  NS_IMETHOD CanAccessTargetStr(JSContext *cx, const char *target, PRBool* aReturn)= 0;
#endif
  NS_IMETHOD RegisterPrincipals(nsIScriptContext *aContext, nsIScriptGlobalObject* aGlobal, JSPrincipals *principals, 
                      nsString* aName, nsString* aSrc, JSPrincipals** aPrincipals) = 0;

};

//Security flags
#define SCRIPT_SECURITY_ALL_ACCESS          0x0000
#define SCRIPT_SECURITY_NO_ACCESS           0x0001
#define SCRIPT_SECURITY_SAME_DOMAIN_ACCESS  0x0002
//XXX expand this flag out once we know the privileges we'll support
#define SCRIPT_SECURITY_SIGNED_ACCESS       0x0004

extern "C" NS_DOM nsresult NS_NewScriptSecurityManager(nsIScriptSecurityManager ** aInstancePtrResult);

#endif // nsIScriptSecurityManager_h__
