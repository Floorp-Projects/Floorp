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

class nsJSSecurityManager : public nsIScriptSecurityManager,
							public nsICapsSecurityCallbacks,
							public nsIXPCSecurityManager {
public:
	nsJSSecurityManager();
	virtual ~nsJSSecurityManager();
	
	NS_DECL_ISUPPORTS
		
	//nsIScriptSecurityManager interface
	NS_IMETHOD Init();
	
	NS_IMETHOD CheckScriptAccess(nsIScriptContext* aContext, 
		void* aObj, 
		const char* aProp, 
		PRBool* aResult);
	
	//XXX From lib/libmocha/lm.h
	NS_IMETHOD GetSubjectOriginURL(JSContext *aCx, nsString** aOrigin);
	NS_IMETHOD GetObjectOriginURL(JSContext *aCx, JSObject *object, nsString** aOrigin);
	NS_IMETHOD GetPrincipalsFromStackFrame(JSContext *aCx, JSPrincipals** aPrincipals);
	NS_IMETHOD GetCompilationPrincipals(nsIScriptContext *aContext, nsIScriptGlobalObject* aGlobal, 
		JSPrincipals *aLayoutPrincipals, JSPrincipals** aPrincipals);
	NS_IMETHOD CanAccessTarget(JSContext *aCx, PRInt16 target, PRBool* aReturn);
	NS_IMETHOD CheckPermissions(JSContext *aCx, JSObject *aObj, short target, PRBool* aReturn);
	NS_IMETHOD CheckContainerAccess(JSContext *aCx, JSObject *aObj, PRInt16 aTarget, PRBool* aReturn);
	NS_IMETHOD GetContainerPrincipals(JSContext *aCx, JSObject *aContainer, JSPrincipals** aPrincipals);
	NS_IMETHOD SetContainerPrincipals(JSContext *aCx, JSObject *aContainer, JSPrincipals* aPrincipals);
	NS_IMETHOD CanCaptureEvent(JSContext *aCx, JSFunction *aFun, JSObject *aEventTarget, PRBool* aReturn);
	NS_IMETHOD SetExternalCapture(JSContext *aCx, JSPrincipals* aPrincipals, PRBool aBool);
	NS_IMETHOD CheckSetParentSlot(JSContext *aCx, JSObject *aObj, jsval *vp, PRBool* aReturn);
	NS_IMETHOD SetDocumentDomain(JSContext *aCx, JSPrincipals *principals, 
		nsString* newDomain, PRBool* aReturn);
	NS_IMETHOD DestroyPrincipalsList(JSContext *aCx, nsJSPrincipalsList *list);
	//XXX From include/libmocha.h
	NS_IMETHOD NewJSPrincipals(nsIURI *aURL, nsString* aName, nsString* aCodebase, JSPrincipals** aPrincipals);
#ifdef DO_JAVA_STUFF
	NS_IMETHOD ExtractFromPrincipalsArchive(JSPrincipals *aPrincipals, char *aName, 
								uint *aLength, char** aReturn);
	NS_IMETHOD SetUntransformedSource(JSPrincipals *principals, char *original, 
						  char *transformed, PRBool* aReturn);
	NS_IMETHOD GetJSPrincipalsFromJavaCaller(JSContext *aCx, void *principalsArray, void *pNSISecurityContext, JSPrincipals** aPrincipals);
#endif
#if 0
	NS_IMETHOD CanAccessTargetStr(JSContext *aCx, const char *target, PRBool* aReturn);
#endif
	NS_IMETHOD RegisterPrincipals(nsIScriptContext *aContext, nsIScriptGlobalObject* aGlobal, JSPrincipals *aPrincipals, 
		nsString* aName, nsString* aSrc, JSPrincipals** aRetPrincipals);
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
private:
	void PrintToConsole(const char *data);
	void PrintPrincipalsToConsole(JSContext *cx, JSPrincipals *principals);
	
	PRUint32 GetPrincipalsCount(JSContext *aCx, JSPrincipals *aPrincipals);
	PRBool PrincipalsCanAccessTarget(JSContext *cx, short target);
	void InvalidateCertPrincipals(JSContext *cx, JSPrincipals *principals);
	
	//Helper funcs for RegisterPrincipals
#ifdef EARLY_ACCESS_STUFF
	PRBool CheckEarlyAccess(MochaDecoder *decoder, JSPrincipals *principals);
#endif
	PRBool IntersectPrincipals(JSContext *aCx, JSPrincipals *principals,
		JSPrincipals *newPrincipals);
	PRBool PrincipalsEqual(JSContext *aCx, JSPrincipals *aA, JSPrincipals *aB);
	
	PRBool IsExternalCaptureEnabled(JSContext *cx, JSPrincipals *principals);
	PRBool CanExtendTrust(JSContext *cx, void *from, void *to);
	char* GetJavaCodebaseFromOrigin(const char *origin);
	
	NS_IMETHOD GetOriginFromSourceURL(nsString* sourceURL, nsString* *result);
	char* FindOriginURL(JSContext *aCx, JSObject *aGlobal);
	
	PRBool SameOrigins(JSContext *aCx, const char* aOrigin1, const char* aOrigin2);
	PRBool SameOriginsStr(JSContext *aCx, nsString* aOrigin1, nsString* aOrigin2);
	nsString* GetCanonicalizedOrigin(JSContext *cx, nsString* aUrlString);
	
	// Glue code for JS stack crawling callbacks
	nsJSFrameIterator* NewJSFrameIterator(void *aContext);
	PRBool NextJSJavaFrame(struct nsJSFrameIterator *aIterator);
	PRBool NextJSFrame(struct nsJSFrameIterator **aIterator);
	
	void InitCaps(void);
	
	//Helper funcs
	char* AddSecPolicyPrefix(JSContext *cx, char *pref_str);
	char* GetSitePolicy(const char *org);
	PRInt32 CheckForPrivilege(JSContext *cx, char *prop_name, int priv_code);
	JSBool ContinueOnViolation(JSContext *cx, int pref_code);
	JSBool CheckForPrivilegeContinue(JSContext *cx, char *prop_name, int priv_code, int pref_code);
	
	//XXX temporarily
	char * ParseURL (const char *url, int parts_requested);
	char * SACopy (char *destination, const char *source);
	char * SACat (char *destination, const char *source);
	
	//Local vars
	nsIPref* mPrefs;
	nsICapsManager * mCapsManager;
};

//XXX temporarily bit flags for determining what we want to parse from the URL
#define GET_ALL_PARTS				127
#define GET_PASSWORD_PART			64
#define GET_USERNAME_PART			32
#define GET_PROTOCOL_PART			16
#define GET_HOST_PART				8
#define GET_PATH_PART				4
#define GET_HASH_PART				2
#define GET_SEARCH_PART 			1

#endif /* nsJSSecurityManager_h___ */
