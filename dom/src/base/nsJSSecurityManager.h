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
* Communications Corporation.	Portions created by Netscape are
* Copyright (C) 1998 Netscape Communications Corporation.	All Rights
* Reserved.
*/
#ifndef nsJSSecurityManager_h___
#define nsJSSecurityManager_h___

#include "nsIScriptContext.h"
#include "jsapi.h"
#include "jsdbgapi.h"
#include "nsIXPCSecurityManager.h"
#include "nsIScriptSecurityManager.h"
#include "nsICapsSecurityCallbacks.h"
#include "nsICapsManager.h"
class nsIPref;

typedef struct nsJSFrameIterator {
	JSStackFrame *fp;
	JSContext *cx;
	void *intersect;
	PRBool sawEmptyPrincipals;
} nsJSFrameIterator;

typedef struct nsFrameWrapper {
    void *iterator;
} nsFrameWrapper;

typedef struct nsJSPrincipalsList {
    JSPrincipals *principals;
    struct nsJSPrincipalsList *next;
} nsJSPrincipalsList;

enum Signedness {
	HAS_NO_SCRIPTS,
		HAS_UNSIGNED_SCRIPTS,
		HAS_SIGNED_SCRIPTS
};

typedef struct nsJSPrincipalsData {
	JSPrincipals principals;
	void* principalsArrayRef;
	nsIURI *url;
	char* name; 
	void* zip; 
	uint32 externalCapturePrincipalsCount;
	nsString* untransformed;
	nsString* transformed;
	PRBool needUnlock;
	char* codebaseBeforeSettingDomain;
	enum Signedness signedness;
	void* pNSISecurityContext;
} nsJSPrincipalsData;

class nsJSSecurityManager : //public nsICapsSecurityCallbacks,
                            public nsIXPCSecurityManager {
public:
	nsJSSecurityManager();
	virtual ~nsJSSecurityManager();

	NS_DECL_ISUPPORTS

#if 0
        //nsICapsSecurityCallbacks interface
	NS_IMETHOD NewNSJSJavaFrameWrapper(void *aContext, struct nsFrameWrapper ** aWrapper);
	NS_IMETHOD FreeNSJSJavaFrameWrapper(struct nsFrameWrapper *aWrapper);
	NS_IMETHOD GetStartFrame(struct nsFrameWrapper *aWrapper);
	NS_IMETHOD IsEndOfFrame(struct nsFrameWrapper *aWrapper, PRBool* aReturn);
	NS_IMETHOD IsValidFrame(struct nsFrameWrapper *aWrapper, PRBool* aReturn);
	NS_IMETHOD GetNextFrame(struct nsFrameWrapper *aWrapper, int *aDepth, void** aReturn);
        NS_IMETHOD OJIGetPrincipalArray(struct nsFrameWrapper *aWrapper, void** aReturn);
	NS_IMETHOD OJIGetAnnotation(struct nsFrameWrapper *aWrapper, void** aReturn);
	NS_IMETHOD OJISetAnnotation(struct nsFrameWrapper *aWrapper, void *aPrivTable,  void** aReturn);
#endif

        //nsIXPCSecurityManager interface
	NS_IMETHOD CanCreateWrapper(JSContext * aJSContext, const nsIID & aIID, nsISupports *aObj);
	NS_IMETHOD CanCreateInstance(JSContext * aJSContext, const nsCID & aCID);
	NS_IMETHOD CanGetService(JSContext * aJSContext, const nsCID & aCID);
	NS_IMETHOD CanCallMethod(JSContext * aJSContext, const nsIID & aIID, nsISupports *aObj, nsIInterfaceInfo *aInterfaceInfo,
								PRUint16 aMethodIndex, const jsid aName);
	NS_IMETHOD CanGetProperty(JSContext * aJSContext, const nsIID & aIID, nsISupports *aObj, nsIInterfaceInfo *aInterfaceInfo,
								PRUint16 aMethodIndex, const jsid aName);
	NS_IMETHOD CanSetProperty(JSContext * aJSContext, const nsIID & aIID, nsISupports *aObj, nsIInterfaceInfo *aInterfaceInfo,
								PRUint16 aMethodIndex, const jsid aName);

#if 0
  NS_IMETHOD GetCompilationPrincipals(nsIScriptContext *aContext, nsIScriptGlobalObject* aGlobal, 
		JSPrincipals *aLayoutPrincipals, JSPrincipals** aPrincipals);
	NS_IMETHOD CheckContainerAccess(JSContext *aCx, JSObject *aObj, PRInt16 aTarget, PRBool* aReturn);
	NS_IMETHOD SetContainerPrincipals(JSContext *aCx, JSObject *aContainer, JSPrincipals* aPrincipals);
	NS_IMETHOD CanCaptureEvent(JSContext *aCx, JSFunction *aFun, JSObject *aEventTarget, PRBool* aReturn);
	NS_IMETHOD SetExternalCapture(JSContext *aCx, JSPrincipals* aPrincipals, PRBool aBool);
	NS_IMETHOD CheckSetParentSlot(JSContext *aCx, JSObject *aObj, jsval *vp, PRBool* aReturn);
	NS_IMETHOD SetDocumentDomain(JSContext *aCx, JSPrincipals *principals, 
		nsString* newDomain, PRBool* aReturn);
	NS_IMETHOD DestroyPrincipalsList(JSContext *aCx, nsJSPrincipalsList *list);
	//XXX From include/libmocha.h
	NS_IMETHOD RegisterPrincipals(nsIScriptContext *aContext, nsIScriptGlobalObject* aGlobal, JSPrincipals *aPrincipals, 
		nsString* aName, nsString* aSrc, JSPrincipals** aRetPrincipals);

#ifdef DO_JAVA_STUFF
	NS_IMETHOD ExtractFromPrincipalsArchive(JSPrincipals *aPrincipals, char *aName, uint *aLength, char** aReturn);
	NS_IMETHOD SetUntransformedSource(JSPrincipals *principals, char *original, char *transformed, PRBool* aReturn);
	NS_IMETHOD GetJSPrincipalsFromJavaCaller(JSContext *aCx, void *principalsArray, void *pNSISecurityContext, JSPrincipals** aPrincipals);
#endif
#if 0
	NS_IMETHOD CanAccessTargetStr(JSContext *aCx, const char *target, PRBool* aReturn);
#endif
#endif
private:
  nsIPref* mPrefs;
  PRBool PrincipalsCanAccessTarget(JSContext *cx, short target);
  nsJSFrameIterator* NewJSFrameIterator(void *aContext);
  PRBool NextJSFrame(struct nsJSFrameIterator **aIterator);
  PRBool NextJSJavaFrame(struct nsJSFrameIterator *aIterator);
#if 0
	void PrintToConsole(const char *data);
	void PrintPrincipalsToConsole(JSContext *cx, JSPrincipals *principals);
	PRUint32 GetPrincipalsCount(JSContext *aCx, JSPrincipals *aPrincipals);
	void InvalidateCertPrincipals(JSContext *cx, JSPrincipals *principals);
#ifdef EARLY_ACCESS_STUFF
	PRBool CheckEarlyAccess(MochaDecoder *decoder, JSPrincipals *principals);
#endif
	PRBool IntersectPrincipals(JSContext *aCx, JSPrincipals *principals,
		JSPrincipals *newPrincipals);
	PRBool PrincipalsEqual(JSContext *aCx, JSPrincipals *aA, JSPrincipals *aB);
	
	PRBool IsExternalCaptureEnabled(JSContext *cx, JSPrincipals *principals);
	PRBool CanExtendTrust(JSContext *cx, void *from, void *to);
	char* GetJavaCodebaseFromOrigin(const char *origin);
	JSBool ContinueOnViolation(JSContext *cx, int pref_code);
	JSBool CheckForPrivilegeContinue(JSContext *cx, char *prop_name, int priv_code, int pref_code);
    nsICapsManager * mCapsManager;
#endif
};

#endif /* nsJSSecurityManager_h___ */
