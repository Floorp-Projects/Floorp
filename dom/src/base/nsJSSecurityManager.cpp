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
 * Copyright (C) 1998-1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsJSSecurityManager.h"
#include "nsCCapsManager.h"
#include "nsIPrincipalManager.h"
#include "nsIServiceManager.h"
#ifdef OJI
#include "jvmmgr.h"
#endif
#include "nsIScriptObjectOwner.h"
#include "nspr.h"
#include "plstr.h"
#include "nsPrivilegeManager.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectData.h"
#include "nsIPref.h"
#include "nsIURL.h"

static NS_DEFINE_IID(kIXPCSecurityManagerIID, NS_IXPCSECURITYMANAGER_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kICapsSecurityCallbacksIID, NS_ICAPSSECURITYCALLBACKS_IID);

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kURLCID, NS_STANDARDURL_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

static nsString gUnknownOriginStr("[unknown origin]");
static nsString gFileUrlPrefix("file:");

static nsString gFileDoubleSlashUrlPrefix("file://");
static char * targetStrings[] = {
  "UniversalBrowserRead",
  "UniversalBrowserWrite",
  "UniversalSendMail",
  "UniversalFileRead",
  "UniversalFileWrite",
  "UniversalPreferencesRead",
  "UniversalPreferencesWrite",
  "UniversalDialerAccess",
  "Max",
  "AccountSetup",
  /* See Target.java for more targets */
};
/*
extern "C" NS_DOM nsresult 
NS_NewScriptSecurityManager(nsIScriptSecurityManager ** aInstancePtrResult)
{
  nsIScriptSecurityManager* it = new nsJSSecurityManager();
  if (nsnull == it) return NS_ERROR_OUT_OF_MEMORY;
  nsresult ret = it->QueryInterface(kIScriptSecurityManagerIID, (void **) aInstancePtrResult);
  if (NS_FAILED(ret)) return ret;
  if (NS_FAILED(ret)) NS_RELEASE(*aInstancePtrResult);
  return ret;
}
*/
nsJSSecurityManager::nsJSSecurityManager()
{
  NS_INIT_REFCNT();
  nsServiceManager::GetService(kPrefServiceCID, NS_GET_IID(nsIPref), (nsISupports**)&mPrefs);
}

nsJSSecurityManager::~nsJSSecurityManager()
{
  nsServiceManager::ReleaseService(kPrefServiceCID, mPrefs);
//    NS_IF_RELEASE(mCapsManager);
}

NS_IMETHODIMP
nsJSSecurityManager::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) return NS_ERROR_NULL_POINTER;
  /*
  if (aIID.Equals(kICapsSecurityCallbacksIID)) {
      *aInstancePtr = (void*)(nsICapsSecurityCallbacks*)this;
      NS_ADDREF_THIS();
      return NS_OK;
  }
  */
  if (aIID.Equals(kIXPCSecurityManagerIID)) {
      *aInstancePtr = (void*)(nsIXPCSecurityManager*)this;
      NS_ADDREF_THIS();
      return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsJSSecurityManager)
NS_IMPL_RELEASE(nsJSSecurityManager)

PR_STATIC_CALLBACK(void *)
GetPrincipalArray(JSContext *aCx, struct JSPrincipals *aPrincipals);

PR_STATIC_CALLBACK(void)
DestroyJSPrincipals(JSContext *aCx, JSPrincipals *aPrincipals);

PR_STATIC_CALLBACK(PRBool)
GlobalPrivilegesEnabled(JSContext *aCx, JSPrincipals *aPrincipals);

static nsJSPrincipalsData unknownPrincipals = {
  {
    gUnknownOriginStr.ToNewCString(),
    GetPrincipalArray,
    GlobalPrivilegesEnabled,
    0,
    DestroyJSPrincipals
  },
  nsnull
};

//JSPrincipal callback
PR_STATIC_CALLBACK(void)
DestroyJSPrincipals(JSContext *aCx, JSPrincipals *aPrincipals)
{
  if (aPrincipals != nsnull && aPrincipals != (JSPrincipals*)&unknownPrincipals) {
    nsJSPrincipalsData* data = (nsJSPrincipalsData*)aPrincipals;
    if (aPrincipals->codebase) delete aPrincipals->codebase;
    if (data->principalsArrayRef != nsnull) {
      /* XXX: raman: Should we free up the principals that are in that array also? */
      ((nsIPrincipalArray *)data->principalsArrayRef)->FreePrincipalArray();
    }
    //XXX
    if (data->name) delete data->name;
    //data->untransformed
    //data->transformed
    if (data->codebaseBeforeSettingDomain) delete data->codebaseBeforeSettingDomain;
    if (data->zip)
      //ns_zip_close(data->zip);
    if (data->url) NS_RELEASE(data->url);
    PR_Free(data);
  }
}

//JSPrincipal callback
PR_STATIC_CALLBACK(void *)
GetPrincipalArray(JSContext *aCx, struct JSPrincipals *aPrincipals)
{
  nsJSPrincipalsData *data = (nsJSPrincipalsData *)aPrincipals;
/*
  //Get array of principals 
	if (data->principalsArrayRef == nsnull) {
		nsICapsManager * capsMan;
		nsresult res = nsServiceManager::GetService(kCCapsManagerCID, kICapsManagerIID, (nsISupports**)& capsMan);
		if ((NS_OK == res) && (nsnull != capsMan)) {
			nsIPrincipalManager * prinMan;
			capsMan->GetPrincipalManager(& prinMan);
//			prinMan->CreateMixedPrincipalArray(nsnull, nsnull, aPrincipals->codebase, (nsIPrincipalArray * *)&(data->principalsArrayRef));
			NS_RELEASE(capsMan);
		}
	}
*/
  return data->principalsArrayRef;
}


//JSPrincipal callback
PR_STATIC_CALLBACK(PRBool)
GlobalPrivilegesEnabled(JSContext *aCx, JSPrincipals *aPrincipals)
{
  nsJSPrincipalsData *data = (nsJSPrincipalsData *) aPrincipals;
  return (PRBool)(nsnull != data->principalsArrayRef || gUnknownOriginStr.Equals(aPrincipals->codebase));
}



NS_IMETHODIMP
nsJSSecurityManager::CanCreateWrapper(JSContext * aJSContext, const nsIID & aIID, nsISupports * aObj)
{
  return NS_OK;
}
 
NS_IMETHODIMP
nsJSSecurityManager::CanCreateInstance(JSContext * aJSContext, const nsCID & aCID)
{
  return NS_OK;
}
 
NS_IMETHODIMP
nsJSSecurityManager::CanGetService(JSContext * aJSContext, const nsCID & aCID)
{
  return NS_OK;
}
 
NS_IMETHODIMP
nsJSSecurityManager::CanCallMethod(JSContext * aJSContext, 
                                   const nsIID & aIID, 
                                   nsISupports *aObj, 
                                   nsIInterfaceInfo *aInterfaceInfo, 
                                   PRUint16 aMethodIndex, 
                                   const jsid aName)
{
    return NS_OK;
}
 
NS_IMETHODIMP
nsJSSecurityManager::CanGetProperty(JSContext * aJSContext, 
                                    const nsIID & aIID, 
                                    nsISupports *aObj, 
                                    nsIInterfaceInfo *aInterfaceInfo, 
                                    PRUint16 aMethodIndex, 
                                    const jsid aName)
{
     return NS_OK;
}
 
NS_IMETHODIMP
nsJSSecurityManager::CanSetProperty(JSContext * aJSContext, 
                                    const nsIID & aIID, 
                                    nsISupports *aObj, 
                                    nsIInterfaceInfo *aInterfaceInfo, 
                                    PRUint16 aMethodIndex, 
                                    const jsid aName)
{
    return NS_OK;
}




















#if 0
// Later: used for signed scripts

NS_IMETHODIMP
nsJSSecurityManager::FreeNSJSJavaFrameWrapper(struct nsFrameWrapper *aWrapper)
{
  PR_FREEIF(aWrapper);
  return NS_OK;
}

NS_IMETHODIMP
nsJSSecurityManager::GetStartFrame(struct nsFrameWrapper *aWrapper)
{
  return NS_OK;
}

NS_IMETHODIMP
nsJSSecurityManager::IsEndOfFrame(struct nsFrameWrapper *aWrapper, PRBool* aReturn)
{
  *aReturn = PR_FALSE;

  if ((aWrapper == nsnull) || (aWrapper->iterator == nsnull)) {
    *aReturn = PR_TRUE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsJSSecurityManager::IsValidFrame(struct nsFrameWrapper *aWrapper, PRBool* aReturn)
{
  *aReturn = (aWrapper->iterator != nsnull);
  return NS_OK;
}

NS_IMETHODIMP
nsJSSecurityManager::NewNSJSJavaFrameWrapper(void *aContext, struct nsFrameWrapper ** aWrapper)
{
  struct nsFrameWrapper *result;

  result = (struct nsFrameWrapper *)PR_MALLOC(sizeof(struct nsFrameWrapper));
  if (result == nsnull) {
      return NS_ERROR_FAILURE;
  }

  result->iterator = (void*)NewJSFrameIterator(aContext);
  *aWrapper = result;
  return NS_OK;
}

NS_IMETHODIMP
nsJSSecurityManager::GetNextFrame(struct nsFrameWrapper *aWrapper, int *aDepth, void** aReturn)
{
  nsJSFrameIterator *iterator;
  if (aWrapper->iterator == nsnull) {
    return NS_ERROR_FAILURE;
  }
  iterator = (nsJSFrameIterator*)(aWrapper->iterator);

  if (!NextJSFrame(&iterator)) {
    return NS_ERROR_FAILURE;
  }

  (*aDepth)++;
  *aReturn = aWrapper->iterator;
  return NS_OK;
}

PRBool
nsJSSecurityManager::NextJSFrame(struct nsJSFrameIterator **aIterator)
{
  nsJSFrameIterator *iterator = *aIterator;
  PRBool result = NextJSJavaFrame(iterator);
	if (!result) {
		if (iterator->intersect) 
			((nsIPrincipalArray *)(* aIterator)->intersect)->FreePrincipalArray();
		PR_Free(iterator);
		* aIterator = nsnull;
	}
	return result;
}

nsJSFrameIterator *
nsJSSecurityManager::NewJSFrameIterator(void *aContext)
{
    JSContext *aCx = (JSContext *)aContext;
    nsJSFrameIterator *result;
    void *array;

    result = (nsJSFrameIterator*)PR_MALLOC(sizeof(nsJSFrameIterator));
    if (result == nsnull) {
        return nsnull;
    }

    if (aCx == nsnull) {
        return nsnull;
    }

    result->fp = nsnull;
    result->cx = aCx;
    result->fp = JS_FrameIterator(aCx, &result->fp);
    array = result->fp
        ? JS_GetFramePrincipalArray(aCx, result->fp)
        : nsnull;
    result->intersect = array;
    result->sawEmptyPrincipals =
        (result->intersect == nsnull && result->fp &&
         JS_GetFrameScript(aCx, result->fp))
        ? PR_TRUE : PR_FALSE;

    return result;
}

NS_IMETHODIMP
nsJSSecurityManager::OJIGetPrincipalArray(struct nsFrameWrapper *aWrapper, void** aReturn)
{
  nsJSFrameIterator *iterator;
  if (aWrapper->iterator == nsnull) {
    return NS_ERROR_FAILURE;
  }
  iterator = (nsJSFrameIterator*)(aWrapper->iterator);
  *aReturn = ::JS_GetFramePrincipalArray(iterator->cx, iterator->fp);
  return NS_OK;
}

PRBool
nsJSSecurityManager::NextJSJavaFrame(struct nsJSFrameIterator *aIterator)
{
	nsIPrincipalArray * current;
	nsIPrincipalArray * previous;
    if (aIterator->fp == 0) return PR_FALSE;
    current = (nsIPrincipalArray *)JS_GetFramePrincipalArray(aIterator->cx, aIterator->fp);
    if (current == nsnull) {
        if (JS_GetFrameScript(aIterator->cx, aIterator->fp))
            aIterator->sawEmptyPrincipals = PR_TRUE;
    } else {
        nsIPrincipalArray * arrayIntersect;
        if (aIterator->intersect) {
            previous = (nsIPrincipalArray *)aIterator->intersect;
            current->IntersectPrincipalArray(previous,& arrayIntersect);
            /* XXX: raman: should we do a free the previous principal Array */
            ((nsIPrincipalArray *)aIterator->intersect)->FreePrincipalArray();
        }
        aIterator->intersect = current;
    }
    aIterator->fp = JS_FrameIterator(aIterator->cx, &aIterator->fp);
    return aIterator->fp != nsnull;
}

NS_IMETHODIMP
nsJSSecurityManager::OJIGetAnnotation(struct nsFrameWrapper *aWrapper, void** aReturn)
{
  nsJSFrameIterator *iterator;
  void *annotation;
  void *current;

  if (aWrapper->iterator == nsnull) {
    return NS_ERROR_FAILURE;
  }
  iterator = (nsJSFrameIterator*)(aWrapper->iterator);

  annotation = JS_GetFrameAnnotation(iterator->cx, iterator->fp);
  if (annotation == nsnull)
    return NS_ERROR_FAILURE;

  current = JS_GetFramePrincipalArray(iterator->cx, iterator->fp);

  if (iterator->sawEmptyPrincipals || current == nsnull ||
      (iterator->intersect /*&&
      !CanExtendTrust(iterator->cx, current, iterator->intersect)*/))
    return NS_ERROR_FAILURE;

  *aReturn = annotation;
  return NS_OK;
}

NS_IMETHODIMP
nsJSSecurityManager::OJISetAnnotation(struct nsFrameWrapper *aWrapper, void *aPrivTable,  void** aReturn)
{
  if (aWrapper->iterator) {
    nsJSFrameIterator *iterator = (nsJSFrameIterator*)(aWrapper->iterator);
    JS_SetFrameAnnotation(iterator->cx, iterator->fp, aPrivTable);
  }
  *aReturn = aPrivTable;
  return NS_OK;
}


/*
 * If given principals can access the given target, return true. Otherwise
 * return false.  The script must already have explicitly requested access
 * to the given target.
 */
PRBool
nsJSSecurityManager::PrincipalsCanAccessTarget(JSContext *aCx, PRInt16 aTarget)
{
  nsPrivilegeTable * annotation;
  JSStackFrame *fp;
  void *annotationRef;
  nsIPrincipalArray * principalArray = nsnull;
#ifdef OJI
  JSStackFrame *pFrameToStartLooking = *JVM_GetStartJSFrameFromParallelStack();
  JSStackFrame *pFrameToEndLooking   = JVM_GetEndJSFrameFromParallelStack(pFrameToStartLooking);
  PRBool  bCalledFromJava = (pFrameToEndLooking != nsnull);
  if (pFrameToStartLooking == nsnull) {
    pFrameToStartLooking = JS_FrameIterator(aCx, &pFrameToStartLooking);
    if (pFrameToStartLooking == nsnull) {
// There are no frames or scripts at this point.
      pFrameToEndLooking = nsnull;
    }
  }
#else
    JSStackFrame *pFrameToStartLooking = JS_FrameIterator(aCx, &fp);
    JSStackFrame *pFrameToEndLooking   = nsnull;
#endif

   /* Map eJSTarget to nsTarget */
  NS_ASSERTION(aTarget >= 0, "No target in PrincipalsCanAccessTarget");
  //NS_ASSERTION(aTarget < sizeof(targetStrings)/sizeof(targetStrings[0]), "");

  /* Find annotation */
	annotationRef = nsnull;
	principalArray = nsnull;
	fp = pFrameToStartLooking;
	while ((fp = JS_FrameIterator(aCx, &fp)) != pFrameToEndLooking) {
		nsIPrincipalArray * current;
		if (JS_GetFrameScript(aCx, fp) == nsnull) continue;
		current = (nsIPrincipalArray *)JS_GetFramePrincipalArray(aCx, fp);
		if (current == nsnull) return PR_FALSE;
		annotationRef = (void *) JS_GetFrameAnnotation(aCx, fp);
		if (annotationRef) {
			if (principalArray != nsnull) {
				PRBool canExtend;
        nsresult rv;
        NS_WITH_SERVICE(nsIPrincipalManager, prinMan,NS_PRINCIPALMANAGER_PROGID,&rv);
				prinMan->CanExtendTrust(current, principalArray, & canExtend);
				if (!canExtend) return PR_FALSE;
				break;
			}
		}
		if (principalArray != nsnull) current->IntersectPrincipalArray(principalArray,& principalArray);
		else principalArray =  current;
	}
	if (annotationRef) annotation = (nsPrivilegeTable *)annotationRef;
	else {
#ifdef OJI
  /*
   * Call from Java into JS. Just call the Java routine for checking
   * privileges.
   */
  if (nsnull == pFrameToEndLooking) {
    if (principalArray) {
      /*
       * Must check that the principals that signed the Java applet are
       * a subset of the principals that signed this script.
       */
      void *javaPrincipals = JVM_GetJavaPrincipalsFromStackAsNSVector(pFrameToStartLooking);

      //if (!CanExtendTrust(aCx, javaPrincipals, principalArray)) {
      //  return PR_FALSE;
      //}
    }
    /*
     * XXX sudu: TODO: Setup the parameters representing a target.
     */
    return JVM_NSISecurityContextImplies(pFrameToStartLooking, targetStrings[aTarget], NULL);
  }
#endif /* OJI */
    // No annotation in stack
    return PR_FALSE;
  }

  // Now find permission for (annotation, target) pair.
  PRBool allowed;
  //mCapsManager->IsAllowed(annotation, targetStrings[aTarget], &allowed);

  return allowed;
}



/* This array must be kept in sync with nsIScriptSecurityManager.idl */


#if JS_SECURITY_OBJ
/*

static char access_error_message[] =
    "access disallowed from scripts at %s to documents at another domain";
static char container_error_message[] =
    "script at '%s' is not signed by sufficient principals to access "
    "signed container";

static char enablePrivilegeStr[] =      "enablePrivilege";
static char isPrivilegeEnabledStr[] =   "isPrivilegeEnabled";
static char disablePrivilegeStr[] =     "disablePrivilege";
static char revertPrivilegeStr[] =      "revertPrivilege";

//XXX what about the PREXTERN?
typedef PRBool
(*nsCapsFn)(void* context, struct nsTarget *target, PRInt32 callerDepth);

static JSBool
callCapsCode(JSContext *aCx, JSObject *obj, uintN argc, jsval *argv,
             jsval *rval, nsCapsFn fn, char *name)
{
    JSString *str;
    char *cstr;
    struct nsTarget *target;

    if (argc == 0 || !JSVAL_IS_STRING(argv[0])) {
        JS_ReportError(aCx, "String argument expected for %s.", name);
        return JS_FALSE;
    }
    // We don't want to use JS_ValueToString because we want to be able
    // to have an object to represent a target in subsequent versions.
    // XXX but then use of an object will cause errors here....
    str = JSVAL_TO_STRING(argv[0]);
    if (!str)
        return JS_FALSE;

    cstr = JS_GetStringBytes(str);
    if (cstr == nsnull)
        return JS_FALSE;

    target = nsCapsFindTarget(cstr);
    if (target == nsnull)
        return JS_FALSE;
    // stack depth of 1: first frame is for the native function called
    if (!(*fn)(aCx, target, 1)) {
        //XXX report error, later, throw exception
        return JS_FALSE;
    }
    return JS_TRUE;
}


JSBool
lm_netscape_security_isPrivilegeEnabled(JSContext *aCx, JSObject *obj, uintN argc,
                                        jsval *argv, jsval *rval)
{
    return callCapsCode(aCx, obj, argc, argv, rval, nsCapsIsPrivilegeEnabled,
                        isPrivilegeEnabledStr);
}

JSBool
lm_netscape_security_enablePrivilege(JSContext *aCx, JSObject *obj, uintN argc,
                                     jsval *argv, jsval *rval)
{
    return callCapsCode(aCx, obj, argc, argv, rval, nsCapsEnablePrivilege,
                        enablePrivilegeStr);
}

JSBool
lm_netscape_security_disablePrivilege(JSContext *aCx, JSObject *obj, uintN argc,
                                      jsval *argv, jsval *rval)
{
    return callCapsCode(aCx, obj, argc, argv, rval, nsCapsDisablePrivilege,
                        disablePrivilegeStr);
}

JSBool
lm_netscape_security_revertPrivilege(JSContext *aCx, JSObject *obj, uintN argc,
                                     jsval *argv, jsval *rval)
{
    return callCapsCode(aCx, obj, argc, argv, rval, nsCapsRevertPrivilege,
                        revertPrivilegeStr);
}

static JSFunctionSpec PrivilegeManager_static_methods[] = {
    { isPrivilegeEnabledStr, lm_netscape_security_isPrivilegeEnabled,   1},
    { enablePrivilegeStr,    lm_netscape_security_enablePrivilege,      1},
    { disablePrivilegeStr,   lm_netscape_security_disablePrivilege,     1},
    { revertPrivilegeStr,    lm_netscape_security_revertPrivilege,      1},
    {0}
};

JSBool
lm_InitSecurity(MochaDecoder *decoder)
{
    JSContext  *aCx;
    JSObject   *obj;
    JSObject   *proto;
    JSClass    *objectClass;
    jsval      v;
    JSObject   *securityObj;

    //XXX * "Steal" calls to netscape.security.PrivilegeManager.enablePrivilege,
    //et. al. so that code that worked with 4.0 can still work.

    // Find Object.prototype's class by walking up the window object's
    // prototype chain.
    aCx = decoder->js_context;
    obj = decoder->window_object;
    while (proto = JS_GetPrototype(aCx, obj))
        obj = proto;
    objectClass = JS_GetClass(aCx, obj);

    if (!JS_GetProperty(aCx, decoder->window_object, "netscape", &v))
        return JS_FALSE;
    if (JSVAL_IS_OBJECT(v)) {
        // "netscape" property of window object exists; must be LiveConnect
        // package. Get the "security" property.
        obj = JSVAL_TO_OBJECT(v);
        if (!JS_GetProperty(aCx, obj, "security", &v) || !JSVAL_IS_OBJECT(v))
            return JS_FALSE;
        securityObj = JSVAL_TO_OBJECT(v);
    } else {
        //define netscape.security object
        obj = JS_DefineObject(aCx, decoder->window_object, "netscape",
                              objectClass, nsnull, 0);
        if (obj == nsnull)
            return JS_FALSE;
        securityObj = JS_DefineObject(aCx, obj, "security", objectClass,
                                      nsnull, 0);
        if (securityObj == nsnull)
            return JS_FALSE;
    }

    // Define PrivilegeManager object with the necessary "static" methods. 
    obj = JS_DefineObject(aCx, securityObj, "PrivilegeManager", objectClass,
                          nsnull, 0);
    if (obj == nsnull)
        return JS_FALSE;

    return JS_DefineFunctions(aCx, obj, PrivilegeManager_static_methods);
}
*/
#endif //JS_SECURITY_OBJ



//+++

NS_IMETHODIMP
nsJSSecurityManager::GetCompilationPrincipals(nsIScriptContext *aContext, 
                                              nsIScriptGlobalObject* aGlobal,
                                              JSPrincipals *aLayoutPrincipals, 
                                              JSPrincipals** aPrincipals)
{
  JSContext *cx;
  *aPrincipals = nsnull;

  cx = (JSContext*)(aContext->GetNativeContext());

  if (0) {//script from doc.write(decoder->writing_input && lm_writing_context != nsnull) {
    /*
     * Compiling a script added due to a document.write.
     * Get principals from stack frame. We can't just use these
     * principals since the document.write code will fail signature
     * verification. So just grab the codebase and form a new set
     * of principals.
     */
    GetPrincipalsFromStackFrame(cx, aPrincipals);
    if (*aPrincipals) {
        nsAutoString cb((*aPrincipals)->codebase);
        NewJSPrincipals(nsnull, nsnull, &cb, aPrincipals);
    } else {
        nsAutoString cb(gUnknownOriginStr);
        NewJSPrincipals(nsnull, nsnull, &cb, aPrincipals);
    }
    if (*aPrincipals == nsnull) {
      JS_ReportOutOfMemory(cx);
      return NS_ERROR_FAILURE;
    }
    InvalidateCertPrincipals(cx, *aPrincipals);
    return NS_OK;
  }

  if (aLayoutPrincipals) {
    *aPrincipals = aLayoutPrincipals;
    return NS_OK;
  }

  /*
   * Just get principals corresponding to the window or layer we're
   * currently parsing.
   */
  nsIScriptObjectOwner* globalOwner;
  JSObject* global = nsnull;

  if (NS_OK == aGlobal->QueryInterface(kIScriptObjectOwnerIID, (void**)&globalOwner)) {
    globalOwner->GetScriptObject(aContext, (void**)&global);
  }
  if (nsnull != global) {
    return GetContainerPrincipals(cx, global, aPrincipals);
  }

  return NS_ERROR_FAILURE;
}




NS_IMETHODIMP
nsJSSecurityManager::CheckContainerAccess(JSContext *aCx, JSObject *aObj,
                                PRInt16 aTarget, PRBool* aReturn)
{
  JSPrincipals *principals;
  nsJSPrincipalsData *data;
  JSStackFrame *fp;
  JSScript *script;
  JSPrincipals *subjPrincipals;
  nsString* fn = nsnull;

  /* The decoder's js_context isn't in a request, so we should put it
   *   in one during this call. */
  //XXXJoki, why the begin request?  Does it needs to be aObj's cx?
  JS_BeginRequest(aCx);
  GetContainerPrincipals(aCx, aObj, &principals);
  JS_EndRequest(aCx);

  if (principals == nsnull) {
#ifdef EARLY_ACCESS_STUFF
    /*
     * Attempt to access container before container has any scripts.
     * Most of these accesses come from natives when initializing a
     * window. Check for that by seeing if we have an executing script.
     * If we do, remember the principals of the script that performed
     * the access so we can report an error later if need be.
     */
    fp = nsnull;
    GetPrincipalsFromStackFrame(aCx, &subjPrincipals);
    if (subjPrincipals == nsnull) {
      *aReturn = PR_TRUE;
      return NS_OK;
    }

    /* See if subjPrincipals are already on list */
    list = (nsJSPrincipalsList *) decoder->early_access_list;
    while (list && list->principals != subjPrincipals) {
        list = list->next;
    }
    if (list == nsnull) {
      list = PR_MALLOC(sizeof(*list));
      if (list == nsnull) {
          JS_ReportOutOfMemory(aCx);
          *aReturn = PR_FALSE;
          return NS_ERROR_FAILURE;
      }
      list->principals = subjPrincipals;
      JSPRINCIPALS_HOLD(aCx, list->principals);
      list->next = (nsJSPrincipalsList *) decoder->early_access_list;
      decoder->early_access_list = list;
    }
    /*
     * XXX - Still possible to modify contents of another page
     * even if cross-origin access is disabled by setting to
     * about:blank, modifying, and then loading the attackee.
     * Similarly with window.open("").
     */
#endif
    *aReturn = PR_TRUE;
    return NS_OK;
  }
  /*
   * If object doesn't have signed scripts and cross-origin access
   * is enabled, return true.
   */
  data = (nsJSPrincipalsData *) principals;
  if (data->signedness != HAS_SIGNED_SCRIPTS) {//XXXGlobalAPI && GetCrossOriginEnabled()) {
    *aReturn = PR_TRUE;
    return NS_OK;
  }

  /* Check if user requested lower privileges */

  if (data->signedness == HAS_SIGNED_SCRIPTS) {
       //XXX Do we need CompromisePrincipals?XXX && !GetPrincipalsCompromise(aCx, obj)) {
      /*
       * We have signed scripts. Must check that the object principals are
       * a subset of the the subject principals.
       */
      fp = nsnull;
      fp = JS_FrameIterator(aCx, &fp);
      if (fp == nsnull || (script = JS_GetFrameScript(aCx, fp)) == nsnull) {
          /* haven't begun execution yet; allow the r to create functions */
        *aReturn = PR_TRUE;
        return NS_OK;
      }
      subjPrincipals = JS_GetScriptPrincipals(aCx, script);
      if (subjPrincipals &&
          CanExtendTrust(aCx,
              principals->getPrincipalArray(aCx, principals),
              subjPrincipals->getPrincipalArray(aCx, subjPrincipals)))
      {
        *aReturn = PR_TRUE;
        return NS_OK;
      }
      GetSubjectOriginURL(aCx, &fn);
      if (!fn) {
        *aReturn = PR_FALSE;
        return NS_OK;
      }
      if (subjPrincipals && principals) {
          PrintToConsole("Principals of script: ");
          PrintPrincipalsToConsole(aCx, subjPrincipals);
          PrintToConsole("Principals of signed container: ");
          PrintPrincipalsToConsole(aCx, principals);
      }
      char fnChar[128];

      JS_ReportError(aCx, "Container error message", fn->ToCString(fnChar, 128));
      *aReturn = PR_FALSE;
      delete fn;
      return NS_ERROR_FAILURE;
  }

  /* The signed script has called compromisePrincipals(), so
   * we do the weaker origin check.
   */
  return CheckPermissions(aCx, aObj, aTarget, aReturn);
}

NS_IMETHODIMP
nsJSSecurityManager::SetContainerPrincipals(JSContext *aCx, JSObject *aContainer, JSPrincipals *aPrincipals)
{
  //Start from topmost item.
  while (nsnull != (aContainer = JS_GetParent(aCx, aContainer)));

  nsISupports *tmp;
  nsIScriptGlobalObjectData *globalData;

  tmp = (nsISupports*)JS_GetPrivate(aCx, aContainer);
  if (nsnull != tmp &&
      NS_OK == tmp->QueryInterface(NS_GET_IID(nsIScriptGlobalObjectData), (void**)&globalData)) {
    globalData->SetPrincipals((void*)aPrincipals);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsJSSecurityManager::CanCaptureEvent(JSContext *aCx, JSFunction *aFun, JSObject *aEventTarget, PRBool* aReturn)
{
  JSScript *script;
  JSPrincipals *principals;
  nsString* origin = nsnull;

  script = JS_GetFunctionScript(aCx, aFun);
  if (script == nsnull) {
    *aReturn = PR_FALSE;
    return NS_OK;
  }
  principals = JS_GetScriptPrincipals(aCx, script);
  if (principals == nsnull) {
    *aReturn = PR_FALSE;
    return NS_OK;
  }
  GetObjectOriginURL(aCx, aEventTarget, &origin);
  char* originChar;
  if (origin) {
    originChar = origin->ToNewCString();
  }
  if (!originChar) {
    if (origin) {
      delete origin;
    }
    *aReturn = PR_FALSE;
    return NS_OK;
  }

  *aReturn = (PRBool)(this->SameOrigins(aCx, originChar, principals->codebase) ||
              IsExternalCaptureEnabled(aCx, principals));

  delete origin;
  delete originChar;
  return NS_OK;
}

PRBool
nsJSSecurityManager::IsExternalCaptureEnabled(JSContext *aCx, JSPrincipals *aPrincipals)
{
  nsJSPrincipalsData *data = (nsJSPrincipalsData*)aPrincipals;

  if (data->externalCapturePrincipalsCount == 0) {
    return PR_FALSE;
  }
  else {
    PRUint32 count = GetPrincipalsCount(aCx, aPrincipals);
    return (PRBool)(data->externalCapturePrincipalsCount == count);
  }
}

NS_IMETHODIMP
nsJSSecurityManager::SetExternalCapture(JSContext *aCx, JSPrincipals *aPrincipals, PRBool aBool)
{
  nsJSPrincipalsData *data = (nsJSPrincipalsData*)aPrincipals;

  if (aBool) {
    PRUint32 count = GetPrincipalsCount(aCx, aPrincipals);
    data->externalCapturePrincipalsCount = count;
  } else {
    data->externalCapturePrincipalsCount = 0;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsJSSecurityManager::CheckSetParentSlot(JSContext *aCx, JSObject *aObj, jsval *aVp, PRBool* aReturn)
{
  JSObject *newParent;
  *aReturn = PR_TRUE;

  if (!JSVAL_IS_OBJECT(*aVp)) {
    return NS_OK;
  }
  newParent = JSVAL_TO_OBJECT(*aVp);
  if (newParent) {
    nsString* oldOrigin = nsnull;
    nsString* newOrigin = nsnull;
    
    GetObjectOriginURL(aCx, aObj, &oldOrigin);
    if (!oldOrigin) {
      return NS_ERROR_FAILURE;
    }
    GetObjectOriginURL(aCx, newParent, &newOrigin);
    if (!newOrigin) {
      delete oldOrigin;
      return NS_ERROR_FAILURE;
    }
    if (!this->SameOrigins(aCx, oldOrigin, newOrigin)) {
      delete oldOrigin;
      delete newOrigin;
      return NS_OK;
    }
    delete oldOrigin;
    delete newOrigin;
  } 
  else {
    //Should only be called from window
    if (JS_GetParent(aCx, aObj)) {
      return NS_OK;
    }
    JSPrincipals *principals;
    GetContainerPrincipals(aCx, aObj, &principals);
    if (!principals) {
      *aReturn = PR_FALSE;
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsJSSecurityManager::SetDocumentDomain(JSContext *aCx, JSPrincipals *aPrincipals, 
                                       nsString* aNewDomain, PRBool* aReturn)
{
  nsJSPrincipalsData *data;
  nsresult result;

  if (aNewDomain->Equals(aPrincipals->codebase)) {
    *aReturn = PR_TRUE;
    return NS_OK;
  }
  data = (nsJSPrincipalsData *) aPrincipals;
  if (!data->codebaseBeforeSettingDomain) {
    data->codebaseBeforeSettingDomain = aPrincipals->codebase;
  }
  else {
    delete aPrincipals->codebase;
  }

  nsString* codebaseStr;
  if ((result = GetOriginFromSourceURL(aNewDomain, &codebaseStr)) != NS_OK)
      return result;

  if (!codebaseStr) {
    return NS_ERROR_FAILURE;
  }

  aPrincipals->codebase = codebaseStr->ToNewCString();
  delete codebaseStr;
  if (aPrincipals->codebase == nsnull) {
    JS_ReportOutOfMemory(aCx);
    *aReturn = PR_FALSE;
    return NS_ERROR_FAILURE;
  }
  if (data->principalsArrayRef != nsnull) {
	((nsIPrincipalArray *)data->principalsArrayRef)->FreePrincipalArray();
	data->principalsArrayRef = nsnull;
  }
  *aReturn = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsJSSecurityManager::DestroyPrincipalsList(JSContext *aCx, nsJSPrincipalsList *aList)
{
  //early access stuff
  while (aList) {
    nsJSPrincipalsList *next = aList->next;
    if (aList->principals)
      JSPRINCIPALS_DROP(aCx, aList->principals);
    PR_Free(aList);
    aList = next;
  }
  return NS_OK;
}

void
nsJSSecurityManager::PrintToConsole(const char *data)
{
  /* XXX: raman: We should write to JS console when it is ready */
  /* JS_PrintToConsole(data); */
  printf("%s", data);
}





void
nsJSSecurityManager::PrintPrincipalsToConsole(JSContext *aCx, JSPrincipals *aPrincipals)
{
	nsIPrincipalArray * principalsArray;
	nsIPrincipal *principal;
//cd ..  char *vendor;
  PRUint32 i, count;
  static char emptyStr[] = "<empty>\n";
  principalsArray = (nsIPrincipalArray *)aPrincipals->getPrincipalArray(aCx, aPrincipals);
  if (principalsArray == nsnull) {
    PrintToConsole(emptyStr);
    return;
  }
  PrintToConsole("[\n");
	principalsArray->GetPrincipalArraySize(& count);
	for (i = 0; i < count; i++) {
		principalsArray->GetPrincipalArrayElement(i, & principal);
//    mCapsManager->GetVendor(principal, &vendor);
//    if (vendor == nsnull) {
//      JS_ReportOutOfMemory(aCx);
//      return;
//    }
//    PrintToConsole(vendor);
//    PrintToConsole(",\n");
  }
  PrintToConsole("]\n");
}

void
nsJSSecurityManager::InvalidateCertPrincipals(JSContext *aCx, JSPrincipals *aPrincipals)
{
  nsJSPrincipalsData *data = (nsJSPrincipalsData*)aPrincipals;

  if (data->principalsArrayRef) {
    PrintToConsole("Invalidating certificate principals in ");
    PrintPrincipalsToConsole(aCx, aPrincipals);
		((nsIPrincipalArray *)data->principalsArrayRef)->FreePrincipalArray();
		data->principalsArrayRef = nsnull;
	}
  data->signedness = HAS_UNSIGNED_SCRIPTS;
}

PRBool
nsJSSecurityManager::CanExtendTrust(JSContext *aCx, void *aFrom, void *aTo)
{
// this code is already taken care of by nsPrivilegeManager, there is no need for the
//JSContext object, this needs to be reviewed and modified, i don't think this method
//should exist at all in this class
  if (aFrom == nsnull || aTo == nsnull) {
    return PR_FALSE;
  }
  if (aFrom == aTo) {
    return PR_TRUE;
  }

	PRBool canExtend;
	nsIPrincipalManager * prinMan;
	mCapsManager->GetPrincipalManager(& prinMan);
	prinMan->CanExtendTrust((nsIPrincipalArray *)aFrom,(nsIPrincipalArray *) aTo, &canExtend);
	return canExtend;
}

PRUint32
nsJSSecurityManager::GetPrincipalsCount(JSContext *aCx, JSPrincipals *aPrincipals)
{
// again, why???????
// these methods are handled by nsIPrincipalArray, get rid of duplicates
    nsIPrincipalArray * principalArray;
    PRUint32 count;

	principalArray = (nsIPrincipalArray *)aPrincipals->getPrincipalArray(aCx, aPrincipals);
	// this check is already done in nsPrincipalArray
//    if (nsnull == principalArray) return 0;
	principalArray->GetPrincipalArraySize(& count);
	return count;
}

char *
nsJSSecurityManager::GetJavaCodebaseFromOrigin(const char *origin)
{
    /* Remove filename part. */
    char *result = PL_strdup(origin);
    if (result) {
        char *slash = PL_strrchr(result, '/');
        if (slash && slash > result && slash[-1] != '/')
            slash[1] = '\0';
    }
    return result;
}



#ifdef DO_JAVA_STUFF
/*

PR_PUBLIC_API(char *)
LM_LoadFromZipFile(void *zip, char *fn)
{
    struct stat st;
    char* data;

    if (!ns_zip_stat((ns_zip_t *)zip, fn, &st)) {
        return nsnull;
    }
    if ((data = malloc((size_t)st.st_size + 1)) == 0) {
        return nsnull;
    }
    if (!ns_zip_get((ns_zip_t *)zip, fn, data, st.st_size)) {
        PR_Free(data);
        return nsnull;
    }
    data[st.st_size] = '\0';
    return data;
}

extern char *
LM_ExtractFromPrincipalsArchive(JSPrincipals *principals, char *name,
                                uint *length)
{
    nsJSPrincipalsData *data = (nsJSPrincipalsData *) principals;
    char *result = nsnull;

    result = LM_LoadFromZipFile(data->zip, name);
    *length = result ? PL_strlen(result) : 0;

    return result;
}

extern PRBool
LM_SetUntransformedSource(JSPrincipals *principals, char *original,
                          char *transformed)
{
    nsJSPrincipalsData *data = (nsJSPrincipalsData *) principals;

    NS_ASSERTION(data->untransformed == nsnull);
    data->untransformed = PL_strdup(original);
    if (data->untransformed == nsnull)
        return PR_FALSE;
    data->transformed = PL_strdup(transformed);
    if (data->transformed == nsnull)
        return PR_FALSE;
    return PR_TRUE;
}


JSPrincipals * PR_CALLBACK
LM_GetJSPrincipalsFromJavaCaller(JSContext *aCx, void *principalsArray, void *pNSISecurityContext)
{
    setupJSCapsCallbacks();
    if (principalsArray == nsnull)
        return nsnull;

    return newJSPrincipalsFromArray(aCx, principalsArray, pNSISecurityContext);
}

static JSPrincipals *
newJSPrincipalsFromArray(JSContext *aCx, void *principalsArray, void *pNSISecurityContext)
{
    JSPrincipals *result;
    nsIPrincipal *principal;
    const char *codebase;
    nsJSPrincipalsData *data;
    uint32 i, count;

    setupJSCapsCallbacks();

    count = nsCapsGetPrincipalArraySize(principalsArray);
    if (count == 0) {
        JS_ReportError(aCx, "No principals found for Java caller");
        return nsnull;
    }

    codebase = nsnull;
    for (i = count; i > 0; i--) {
        principal = nsCapsGetPrincipalArrayElement(principalsArray, i-1);
        if (nsCapsIsCodebaseExact(principal)) {
            codebase = nsCapsPrincipalToString(principal);
            break;
        }
    }

    result = NewJSPrincipals(nsnull, nsnull, (char *) codebase);
    if (result == nsnull) {
        JS_ReportOutOfMemory(aCx);
        return nsnull;
    }

    data = (nsJSPrincipalsData *) result;
    data->principalsArrayRef = principalsArray;
    data->pNSISecurityContext = pNSISecurityContext;
    data->signedness = count == 1 && codebase
                       ? HAS_UNSIGNED_SCRIPTS
                       : HAS_SIGNED_SCRIPTS;

    return result;
}
*/
#endif //DO_JAVA_STUFF

#ifdef NEED_CANACCESSTARGETSTR
/*
int
findTarget(const char *target)
{ 
   int i=0;
   for(i=0; i<eJSTarget_MAX; i++)
   {
      if (PL_strcmp(target, targetStrings[i]) == 0)
      {
         return i;
      }
   }
   return -1;
}


// Exported entry point to support nsISecurityContext::Implies method.

NS_IMETHODIMP
nsJSSecurityManager::CanAccessTargetStr(JSContext *aCx, const char *target, PRBool* aReturn)
{
    int      intTarget = findTarget(target);
    PRInt16 eJSTarget;
    if(intTarget < 0)
    {
      return PR_FALSE;
    }
    eJSTarget = (eJSTarget)intTarget;
    return CanAccessTarget(aCx, eJSTarget, aReturn);
}
*/
#endif //NEED_CANACCESSTARGETSTR

NS_IMETHODIMP
nsJSSecurityManager::RegisterPrincipals(nsIScriptContext *aContext, nsIScriptGlobalObject* aGlobal, 
                      JSPrincipals *aPrincipals, nsString* aName, nsString* aSrc, JSPrincipals** aRetPrincipals)
{
  JSContext *cx;
  PRBool verified = PR_FALSE;
  nsJSPrincipalsData *data;
  JSObject *inner;
  JSPrincipals *containerPrincipals;
  nsJSPrincipalsData *containerData;
  nsString *untransformed = nsnull, *implicitName = nsnull;
  nsIScriptObjectOwner *aGlobalObjOwner;

  data = (nsJSPrincipalsData*)aPrincipals;
  cx = (JSContext*)aContext->GetNativeContext();
  *aRetPrincipals = nsnull;

  if (NS_OK == aGlobal->QueryInterface(kIScriptObjectOwnerIID, (void**)&aGlobalObjOwner)) {
    aGlobalObjOwner->GetScriptObject(aContext, (void**)&inner);
  }
  if (inner == nsnull) return NS_ERROR_FAILURE;
  if (NS_OK != GetContainerPrincipals(cx, inner, &containerPrincipals)) return NS_ERROR_FAILURE;
  containerData = (nsJSPrincipalsData *)containerPrincipals;
  JSObject* container = inner;
  if (!aName && aPrincipals != containerPrincipals && aPrincipals) {
    // "name" argument omitted since it was specified when "principals"
    // was created. Get it from "principals".
    aName = new nsString(data->name);
  }
#if 0
  implicitName = nsnull;
  if (!aName && data && data->signedness == HAS_SIGNED_SCRIPTS) {
    /*
     * Name is unspecified. Use the implicit name formed from the
     * origin URL and the ordinal within the page. For example, the
     * third implicit name on http://www.co.com/dir/mypage.html
     * would be "_mypage2".
     */
    char * urlchars, * path, * tempchars;
    urlchars = FindOriginURL(cx, inner);
    if (!urlchars) return nsnull;
    path = ParseURL(urlchars, GET_PATH_PART)
    nsIURL * url;
    nsresult rv;
    NS_WITH_SERVICE(nsIComponentManager, compMan,kComponentManagerCID,&rv);
    if (!NS_SUCCEEDED(rv)) return nsnull;
    rv = compMan->CreateInstance(kURLCID,NULL,nsIURL::GetIID(),(void**)&url);
    if (!NS_SUCCEEDED(rv)) return nsnull;
    rv = url->SetSpec(urlchars);
    if (!NS_SUCCEEDED(rv)) return nsnull;
    url->GetPath(& path);
    if (path && *path) {
        char * s = PL_strrchr(path, '.');
        if (s) *s = '\0';
        s = PL_strrchr(path, '/');
        //XXXGlobalApi
        implicitName = PR_sprintf_append(nsnull, "_%s%d", s ? s+1 : path,
                                         aGlobal->signature_ordinal++);
        name = implicitName;
    }
    PR_FREEIF(path);
    delete url;
  }
#endif
  untransformed = nsnull;
  if (data && data->untransformed && data->transformed == aSrc) {
      /* Perform verification on original source. */
      aSrc = untransformed = data->untransformed;
      data->untransformed = nsnull;
      PR_Free(data->transformed);
      data->transformed = nsnull;
  }

  PR_FREEIF(untransformed);
  aSrc = nsnull;
  PR_FREEIF(implicitName);
  aName = nsnull;

  /*
   * Now that we've attempted verification, we need to set the appropriate
   * level of signedness based on whether verification succeeded.
   * We avoid setting signedness if principals is the same as container
   * principals (i.e., we "inherited" the principals from a script earlier
   * in the page) and we are not in a subcontainer of the container where
   * the principals were found. In that case we will create a new set of
   * principals for the inner container.
   */
  if (data && !(aPrincipals == containerPrincipals && container != inner)) {
      data->signedness = HAS_UNSIGNED_SCRIPTS;
  }

#ifdef EARLY_ACCESS_STUFF
  /*
  //XXXGlobalApi
  if (verified && aGlobal->early_access_list &&
      !CheckEarlyAccess(cx, aGlobal, principals))
  {
      return nsnull;
  }
  */
#endif

  if (!verified) {
    //Add pref check of "javascript.all.unsigngedExecution"
    if (0) {//!GetUnsignedExecutionEnabled()) {
      /* Execution of unsigned scripts disabled. Return now. */
      return NS_ERROR_FAILURE;
    }
    /* No cert principals; try codebase principal */
    if (!aPrincipals || aPrincipals == containerPrincipals) {
        if (container == inner ||
            containerData->signedness == HAS_UNSIGNED_SCRIPTS) {
          aPrincipals = containerPrincipals;
          data = (nsJSPrincipalsData *)aPrincipals;
        } 
        else {
          /* Just put restricted principals in inner */
          nsAutoString contCodebase(containerPrincipals->codebase);

          NewJSPrincipals(nsnull, nsnull,
                          &contCodebase, &aPrincipals);
          if (!aPrincipals) {
              JS_ReportOutOfMemory(cx);
              return NS_ERROR_FAILURE;
          }
          data = (nsJSPrincipalsData *)aPrincipals;
        }
    }
    InvalidateCertPrincipals(cx, aPrincipals);

#ifdef EARLY_ACCESS_STUFF
      /*
      //XXXGlobalApi
      if (aGlobal->early_access_list && !GetCrossOriginEnabled() &&
          !CheckEarlyAccess(cx, aGlobal, principals))
      {
          return nsnull;
      }
      */
#endif

      if (container == inner) {
        InvalidateCertPrincipals(cx, containerPrincipals);

        /* compare codebase principals */
        if (!this->SameOrigins(cx, containerPrincipals->codebase,
                         aPrincipals->codebase)) {
          /* Codebases don't match; evaluate under different
             principals than container */
          *aRetPrincipals = aPrincipals;
          return NS_OK;
        }
        /* Codebases match */
        *aRetPrincipals = containerPrincipals;
        return NS_OK;
      }

      /* Just put restricted principals in inner */
      SetContainerPrincipals(cx, inner, aPrincipals);
      *aRetPrincipals =  aPrincipals;
      return NS_OK;
  }

  if (!PrincipalsEqual(cx, aPrincipals, containerPrincipals)) {
    /* We have two unequal sets of principals. */
    if (containerData->signedness == HAS_NO_SCRIPTS &&
      this->SameOrigins(cx, aPrincipals->codebase,
                  containerPrincipals->codebase)) {
      /*
       * Principals are unequal because we have container principals
       * carrying only a codebase, and the principals of this script
       * that carry cert principals as well.
       */
      SetContainerPrincipals(cx, container, aPrincipals);
      *aRetPrincipals = aPrincipals;
      return NS_OK;
    }
    if (inner == container) {
      if (containerData->signedness == HAS_NO_SCRIPTS) {
        SetContainerPrincipals(cx, container, aPrincipals);
        *aRetPrincipals =  aPrincipals;
        return NS_OK;
      }
      /*
       * Intersect principals and container principals,
       * modifying the container principals.
       */
      PrintToConsole("Intersecting principals ");
      PrintPrincipalsToConsole(cx, containerPrincipals);
      PrintToConsole("with ");
      PrintPrincipalsToConsole(cx, aPrincipals);
      if (!IntersectPrincipals(cx, containerPrincipals,
                               aPrincipals)) {
        return NS_OK;
      }
      PrintToConsole("yielding ");
      PrintPrincipalsToConsole(cx, containerPrincipals);
    }
    else {
      /*
       * Store the disjoint set of principals in the
       * innermost container
       */
      SetContainerPrincipals(cx, inner, aPrincipals);
      *aRetPrincipals = aPrincipals;
      return NS_OK;
    }

  }
  *aRetPrincipals = containerPrincipals;
  return NS_OK;
}

#if 0
NS_IMETHODIMP
nsJSSecurityManager::CheckURI(nsString *uri, nsIURI *base, PRBool checkFile, 
                              PRBool *isOkay)
{
    *isOkay = PR_TRUE;
    return NS_OK;

    // TODO: Perform checks on uri
    nsresult result;
    nsIURI *url;
    char *str;

    if (!NS_SUCCEEDED(result = NS_NewURI(&url, *uri)))
        return result;
    if (!NS_SUCCEEDED(result = url->GetProtocol(&str)))
        return result;  // TODO: what happens if no protocol?
#endif

#if 0
    // Old 4.x code
    const char *
    lm_CheckURL(JSContext *cx, const char *url_string, JSBool checkFile)
    {
        char *protocol, *absolute;
        JSObject *obj;
        MochaDecoder *decoder;

        protocol = NET_ParseURL(url_string, GET_PROTOCOL_PART);
        if (!protocol || *protocol == '\0' || XP_STRCHR(protocol, '?')) {
            lo_TopState *top_state;

	    obj = JS_GetGlobalObject(cx);
	    decoder = JS_GetPrivate(cx, obj);

	    LO_LockLayout();
	    top_state = lo_GetMochaTopState(decoder->window_context);
            if (top_state && top_state->base_url) {
	        absolute = NET_MakeAbsoluteURL(top_state->base_url,
				               (char *)url_string);	/*XXX*/
                /* 
	         * Temporarily unlock layout so that we don't hold the lock
	         * across a call (lm_CheckPermissions) that may result in 
	         * synchronous event handling.
	         */
	        LO_UnlockLayout();
                if (!lm_CheckPermissions(cx, obj, JSTARGET_UNIVERSAL_BROWSER_READ))
                {
                    /* Don't leak information about the url of this page. */
                    XP_FREEIF(absolute);
                    return NULL;
                }
	        LO_LockLayout();
	    } else {
	        absolute = NULL;
	    }
	    if (absolute) {
	        if (protocol) XP_FREE(protocol);
	        protocol = NET_ParseURL(absolute, GET_PROTOCOL_PART);
	    }
	    LO_UnlockLayout();
        } else {
	    absolute = JS_strdup(cx, url_string);
	    if (!absolute) {
	        XP_FREE(protocol);
	        return NULL;
	    }
	    decoder = NULL;
        }

        if (absolute) {

	    /* Make sure it's a safe URL type. */
	    switch (NET_URL_Type(protocol)) {
	      case FILE_TYPE_URL:
                if (checkFile) {
                    const char *subjectOrigin = lm_GetSubjectOriginURL(cx);
                    if (subjectOrigin == NULL) {
	                XP_FREE(protocol);
	                return NULL;
                    }
                    if (NET_URL_Type(subjectOrigin) != FILE_TYPE_URL &&
                        !lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_FILE_READ)) 
                    {
                        XP_FREE(absolute);
                        absolute = NULL;
                    }
                }
                break;
	      case FTP_TYPE_URL:
	      case GOPHER_TYPE_URL:
	      case HTTP_TYPE_URL:
	      case MAILTO_TYPE_URL:
	      case NEWS_TYPE_URL:
	      case RLOGIN_TYPE_URL:
	      case TELNET_TYPE_URL:
	      case TN3270_TYPE_URL:
	      case WAIS_TYPE_URL:
	      case SECURE_HTTP_TYPE_URL:
	      case URN_TYPE_URL:
	      case NFS_TYPE_URL:
	      case MOCHA_TYPE_URL:
	      case VIEW_SOURCE_TYPE_URL:
	      case NETHELP_TYPE_URL:
	      case WYSIWYG_TYPE_URL:
	      case LDAP_TYPE_URL:
    #ifdef JAVA
    /* DHIREN */
	      case MARIMBA_TYPE_URL:
    /* ~DHIREN */
    #endif
	        /* These are "safe". */
	        break;
	      case ABOUT_TYPE_URL:
	        if (XP_STRCASECMP(absolute, "about:blank") == 0)
		    break;
	        if (XP_STRNCASECMP(absolute, "about:pics", 10) == 0)
		    break;
	        /* these are OK if we are signed */
	        if (lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_READ))
		    break;
	        /* FALL THROUGH */
	      default:
	        /* All others are naughty. */
	        /* XXX signing - should we allow these for signed scripts? */
	        XP_FREE(absolute);
	        absolute = NULL;
	        break;
	    }
        }

        if (!absolute) {
	    JS_ReportError(cx, "illegal URL method '%s'",
		           protocol && *protocol ? protocol : url_string);
        }
        if (protocol)
	    XP_FREE(protocol);
        return absolute;
    }

}
#endif

#ifdef EARLY_ACCESS_STUFF
/*
PRBool
nsJSSecurityManager::CheckEarlyAccess(JSContext* aCx, JSPrincipals *aPrincipals)
{
  nsJSPrincipalsData *data;
  JSPrincipalsList *p;
  PRBool ok;

  data = (nsJSPrincipalsData*)aPrincipals;
  ok = PR_TRUE;

  for (p = (JSPrincipalsList *) decoder->early_access_list; p; p = p->next) {
    if (data->signedness == HAS_SIGNED_SCRIPTS) {
      if (!CanExtendTrust(aCx,
          aPrincipals->getPrincipalArray(aCx, aPrincipals),
          p->principals->getPrincipalArray(aCx, p->principals))) {
        JS_ReportError(aCx, container_error_message,
                       p->principals->codebase);
        ok = PR_FALSE;
        break;
      }
    } 
    else {
      if (!SameOrigins(aCx, p->principals->codebase,
                       aPrincipals->codebase)) {
        // Check to see if early access violated the cross-origin
        // container check.
        JS_ReportError(aCx, access_error_message,
                       p->principals->codebase);
        ok = PR_FALSE;
        break;
      }
    }
  }
  DestroyPrincipalsList(aCx, decoder->early_access_list);
  decoder->early_access_list = nsnull;
  return ok;
}
*/
#endif

/*
 * Compute the intersection of "principals" and "other", saving in
 * "principals". Return true iff the intersection is nonnsnull.
 */
PRBool
nsJSSecurityManager::IntersectPrincipals(JSContext *aCx, JSPrincipals *aPrincipals, JSPrincipals *aNewPrincipals)
{
  nsJSPrincipalsData* data = (nsJSPrincipalsData*)aPrincipals;
  nsJSPrincipalsData* newData = (nsJSPrincipalsData*)aNewPrincipals;

  NS_ASSERTION(data->signedness != HAS_NO_SCRIPTS, "Signed page with no scripts");
  NS_ASSERTION(newData->signedness != HAS_NO_SCRIPTS, "Signed page with no scripts");

  if (!this->SameOrigins(aCx, aPrincipals->codebase, aNewPrincipals->codebase)) {
    delete aPrincipals->codebase;
    aPrincipals->codebase = gUnknownOriginStr.ToNewCString();
    if (aPrincipals->codebase == nsnull) {
      return PR_FALSE;
    }
  }

  if (data->signedness == HAS_UNSIGNED_SCRIPTS ||
      newData->signedness == HAS_UNSIGNED_SCRIPTS) {
    // No cert principals. Nonempty only if there is a codebase
    // principal.
    InvalidateCertPrincipals(aCx, aPrincipals);
    return PR_TRUE;
  }
  // Compute the intersection.
	nsIPrincipalArray * principalArray = (nsIPrincipalArray *)aPrincipals->getPrincipalArray(aCx, aPrincipals);
	nsIPrincipalArray * newPrincipalArray = (nsIPrincipalArray *)aNewPrincipals->getPrincipalArray(aCx, aNewPrincipals);
  if (principalArray == nsnull || newPrincipalArray == nsnull) {
    InvalidateCertPrincipals(aCx, aPrincipals);
    return PR_TRUE;
  }

	nsIPrincipalArray * intersectArray;
	principalArray->IntersectPrincipalArray(newPrincipalArray, & intersectArray);
	if (nsnull == intersectArray) {
		InvalidateCertPrincipals(aCx, aPrincipals);
		return PR_TRUE;
	}

  data->principalsArrayRef = intersectArray;
  return PR_TRUE;
}

PRBool
nsJSSecurityManager::PrincipalsEqual(JSContext *aCx, JSPrincipals *aPrinA, JSPrincipals *aPrinB)
{
    if (aPrinA == aPrinB)
        return PR_TRUE;

    nsJSPrincipalsData *dataA, *dataB;
    dataA = (nsJSPrincipalsData*)aPrinA;
    dataB = (nsJSPrincipalsData*)aPrinB;

    if (dataA->signedness != dataB->signedness)
        return PR_FALSE;

	nsIPrincipalArray * arrayA = (nsIPrincipalArray *)aPrinA->getPrincipalArray(aCx, aPrinA);
	nsIPrincipalArray * arrayB = (nsIPrincipalArray *)aPrinB->getPrincipalArray(aCx, aPrinB);

	PRInt16 comparisonType;
	arrayA->ComparePrincipalArray(arrayB, & comparisonType);
	return (PRBool)(nsIPrincipalArray::SetComparisonType_Equal == comparisonType);
}

/*******************************************************************************
 * Glue code for JS stack crawling callbacks
 ******************************************************************************/





/**
  * nsICapsSecurityCallbacks interface
  */



/* Get the site-specific policy associated with object origin org. */
char *
nsJSSecurityManager::GetSitePolicy(const char *org)
{
  char *sitepol;
  char *sp;
  char *nextsp;
  char *orghost = 0;
  char *retval = 0;
  char *prot;
  int splen;
  char *bar;
  char *end;
  char *match = 0;
  int matlen;

  if (NS_OK != mPrefs->CopyCharPref("js_security.site_policy", &sitepol)) {
    return 0;
  }

  /* Site policy comprises text of the form
   *	site1-policy,site2-policy,...,siteNpolicy
   * where each site-policy is
   *	site|policy
   * and policy is presumed to be one of strict/moderate/default
   * site may be either a URL or a hostname.  In the former case
   * we do a prefix match with the origin URL; in the latter case
   * we just compare hosts.
   */

  /* Process entry by entry.  Take longest match, to account for
   * cases like:
   *	http://host/|moderate,http://host/dir/|strict
   */
  for (sp = sitepol; sp != 0; sp = nextsp) {
    if ((nextsp = strchr(sp, ',')) != 0) {
	    *nextsp++ = '\0';
    }

    if ((bar = strchr(sp, '|')) == 0) {
	    continue;			/* no | for this entry */
    }
	  *bar = '\0';

	  /* Isolate host, then policy. */
	  sp += strspn(sp, " ");	/* skip leading spaces */
	  end = sp + strcspn(sp, " |"); /* skip up to space or | */
	  *end = '\0';
    if ((splen = end-sp) == 0) {
	    continue;			/* no URL or hostname */
    }

	  /* Check whether this is long enough. */
    if (match != 0 && matlen >= splen) {
	    continue;			/* Nope.  New shorter than old. */
    }

	  /* Check which case, URL or hostname, we're dealing with. */
    if ((prot = ParseURL(sp, GET_PROTOCOL_PART)) != 0 && *prot != '\0') {
	    /* URL case.  Do prefix match, make sure we're at proper boundaries. */
	    if (PL_strncmp(org, sp, splen) != 0 || (org[splen] != '\0'	/* exact match */
		      && sp[splen-1] != '/'	/* site policy ends with / */
	        && org[splen] != '/'	/* site policy doesn't, but org does */
		      )) {
		    PR_Free(prot);
		    continue;			/* no match */
	    }
  	}
	  else {
	    /* Host-only case. */
	    PR_FREEIF(prot);

      if (orghost == 0 && (orghost = ParseURL(org, GET_HOST_PART)) == 0) {
		    return 0;			/* out of mem */
      }
      if (PL_strcasecmp(orghost, sp) != 0) {
		    continue;			/* no match */
      }
	  }
	  /* Had a match.  Remember policy and length of host/URL match. */
	  match = bar;
	  matlen = splen;
  }

  if (match != 0) {
	  /* Longest hostname or URL match.  Get policy.
	  ** match points to |.
	  ** Skip spaces after | and after policy name.
	  */
	  ++match;
	  sp = match + strspn(match, " ");
	  end = sp + strcspn(sp, " ");
	  *end = '\0';
	  if (sp != end) {
	    retval = PL_strdup(sp);
	  }
  }

  PR_FREEIF(orghost);
  PR_FREEIF(sitepol);
  return retval;
}

static const char* continue_on_violation = "continue_on_access_violation";

JSBool
nsJSSecurityManager::ContinueOnViolation(JSContext *cx, int pref_code)
{
  PRBool cont;

  char *pref_str;

  pref_str = (char*)continue_on_violation;
  pref_str = AddSecPolicyPrefix(cx, pref_str);
  if (pref_str == NULL) {
    return JS_TRUE;
  }

  mPrefs->GetBoolPref(pref_str, &cont);

  if(cont) {
	  return JS_TRUE;
  }

  return JS_FALSE;
}

/* Check named property for access; if fail, check for
 * permission to continue from violation.
 * Arguments:
 *	priv_code	privilege:  LM_PRIV_READONLY or LM_PRIV_READWRITE
 *	pref_code	prefix for continuation
 *			(arg. to lm_ContinueOnViolation)
 * Returns:
 *	LM_PRIV_OK	if access okay
 *	JS_TRUE		if access denied, but continuation (interpretation) okay
 *	JS_FALSE	if access denied, continuation denied
 */
JSBool
nsJSSecurityManager::CheckForPrivilegeContinue(JSContext *cx, char *prop_name, int priv_code, int pref_code)
{
  if (CheckForPrivilege(cx, prop_name, priv_code) == JS_TRUE) {
	  return JS_TRUE;
  }

  //JS_ReportError(cx, "Access denied: Cannot %s %s",
	  //             priv_code == LM_PRIV_READONLY ? "read" : "write", prop_name);

  return ContinueOnViolation(cx, pref_code);
}

//def'ing ACL stuff for the moment.
#if 0
/* 
static JSObject *
getObjectDocument(JSContext *cx, JSObject *container)
{
    while(container) {

	if(JS_InstanceOf(cx, container, &lm_layer_class, 0)) {
            return lm_GetLayerDocument(cx, container);
	} else if (JS_InstanceOf(cx, container, &lm_window_class, 0)) {
	    MochaDecoder *decoder = JS_GetInstancePrivate(cx, container,
						       &lm_window_class, NULL);

	    return decoder? decoder->document: NULL;
	}

	container = JS_GetParent(cx, container);
    }
    return NULL;
}

// Get ACL for obj.  If there's an explicit ACL, return it.
// Otherwise, return the implicit ACL.
char *
lm_GetObjectACL(JSContext *cx, JSObject *obj)
{
    JSDocument *doc;
    char *acl;
    char *doc_str;
    char *slash;

    fprintf(stderr, "in lm_GetObjectACL\n");

    // Get/check explicit value.
    doc = JS_GetPrivate(cx, getObjectDocument(cx, obj));
    doc_str = lm_GetDocACLName(doc);
    fprintf(stderr, "*#*#*#*#* doc = %s #*#*#*#*\n", doc_str);
    if (PREF_CopyCharPref(doc_str, &acl) != PREF_OK)
	acl = 0;
    PR_FREEIF(doc_str);

    if (acl == 0) {
	// Get the default value
	acl = lm_GetObjectOriginURL(cx, obj);
	if (acl == 0 || PL_strcmp(acl, lm_unknown_origin_str) == 0
#if 0	// ???
	|| (PL_strlen(origin) <= 1)
#else	// there really seem to be such!
	|| acl[0] == '\0'
#endif
	)
            return NULL;

	fprintf(stderr, "--> OOORRRIIIGGGIIINNN %s\n", acl);
	fflush(stderr);

	// Default ACL for object is protocol://host[:port].
	acl = ParseURL(acl, GET_PROTOCOL_PART | GET_HOST_PART);
    }

    fprintf(stderr, "ACL -> %s\n", acl);
    fflush(stderr);

    return acl;
}

static JSBool
CheckTrustedHosts(JSContext *cx, char *subjectOriginURL, char *objectOriginURL)
{
    int i, j, k;
    int acl_len;
    const char *subjectOriginDomain, *objectOriginDomain;
    char *hostlist;
    const char objectHostList[1024];
    char substr[1024];
    JSBool no_match = JS_FALSE;
    
    // Get the domain of the subject origin

    subjectOriginDomain = getCanonicalizedOrigin(cx, subjectOriginURL);

    fprintf(stderr, "Subject Origin Domain - %s, ", subjectOriginDomain);

    objectOriginDomain = getCanonicalizedOrigin(cx, objectOriginURL);

    fprintf(stderr, "Object Origin Domain - %s\n", objectOriginDomain);
    
    hostlist = AddSecPolicyPrefix(cx, "trusted_domain_pair");

    if(hostlist[0] == '\0') {
        return JS_TRUE;
    }
  
    acl_len = sizeof(objectHostList);
    PR_BZERO(objectHostList, acl_len);
    PREF_GetCharPref(hostlist, objectHostList, &acl_len);

    acl_len = PL_strlen(objectHostList);
    j = k = 0;
    while(j < acl_len) {
        k++;
        PR_BZERO(substr, 1024);

        for(i = 0; (j < acl_len) && (objectHostList[j] != ','); j++) {
            if(!isspace(objectHostList[j])) {
                substr[i++] = objectHostList[j];
            }
        }

        if(k % 2) {
            if(PL_strcmp(substr, subjectOriginDomain)) {
                no_match = JS_FALSE;
                continue;
            }
        } else if(no_match) {
           continue;
        }
           
        // Remove trailing '/'
        if(substr[i-1] == '/')
            i--;

        substr[i] = '\0';

        if(PL_strcmp(objectOriginDomain, substr) == 0)
            return JS_TRUE;
        

        while(j < acl_len && (objectHostList[j] == ',' || isspace(objectHostList[j])))
            j++;
        
    }

    return JS_FALSE;
}

JSBool
lm_CheckACL(JSContext *cx, JSObject *obj, JSTarget target)
{
    const char *sOrigin;
    const char *objectACL;
    const char *oOrigin;
    char *acl;
    char *subj;
    char *subjhost = 0;
    char *slash;
    char *aclmem, *nextmem;
    JSBool allow = JS_FALSE;
    
    fprintf(stderr, "in lm_CheckACL\n");

    // Get the ACL associated with the object(document).
    if ((objectACL = lm_GetObjectACL(cx, obj)) == 0)
	return JS_TRUE;			// unrestricted access (??)

    // Get the subject origin.
    sOrigin = lm_GetSubjectOriginURL(cx);

    // Object is always on its own ACL. 
    oOrigin = lm_GetObjectOriginURL(cx, obj);
    if (PL_strcmp(sOrigin, oOrigin) == 0) {
	allow = JS_TRUE;
	goto done;
    }

    fprintf(stderr, "-- subj: %s\n-- obj ACL: %s\n", sOrigin, objectACL);

    // Now, check whether sOrigin is part of objectACL.
    // Walk through space-separated elements of ACL, doing prefix match
    // of ACL member against sOrigin.

    // First strip last part of sOrigin path.
    subj = ParseURL(sOrigin, GET_PROTOCOL_PART | GET_HOST_PART | GET_PATH_PART);
    if ((slash = PL_strrchr(subj, '/')) != 0)
	*slash = '\0';
    acl = PL_strdup(objectACL);

    // Walk through the ACL to see whether the subject host/URL is in
    //  the ACL.  There are two styles of check:
    //	- if the ACL member is a pure host; check for match against
    //	  host in subject URL.
    //	- otherwise, do pure prefix string match.
    // Determine whether the member is a pure host by whether it does or
    // does not have a protocol specified.
    //
    for (aclmem = acl; aclmem != 0; aclmem = nextmem) {
	int memlen;
	char *prot;

	// Chop up list, trim leading spaces.
	if ((nextmem = PL_strchr(aclmem, ' ')) != 0)
	    *nextmem++ = '\0';
	aclmem += strspn(aclmem, " ");
	memlen = PL_strlen(aclmem);

	// Decide which case we've got.
        if ((prot = ParseURL(aclmem, GET_PROTOCOL_PART)) != 0 && *prot != '\0') {
	    // URL case.  Do prefix match, make sure we're at proper boundaries. 
	    if (   PL_strncmp(subj, aclmem, memlen) == 0
	        && (   subj[memlen] == '\0'	// exact match
		    || aclmem[memlen-1] == '/'	// ACL ends with /
	            || subj[memlen] == '/'	// ACL doesn't, but subj starts
		   )
		) {
		allow = JS_TRUE;
		PR_Free(prot);
		break;
	    }
	}
	else {
	    // Host-only case.
	    PR_FREEIF(prot);
	    if (subjhost == 0)
		subjhost = ParseURL(sOrigin, GET_HOST_PART);
	    if ((allow = (PL_strcasecmp(subjhost, aclmem) == 0)) != 0)
		break;
	}
    }

    PR_FREEIF(subjhost);

    if(allow == JS_FALSE) {
	char *err_mesg =
	    PR_smprintf("Access disallowed from scripts at %s to documents at %s",
	    	sOrigin, lm_GetObjectOriginURL(cx,obj));
	JS_ReportError(cx, err_mesg);
	PR_FREEIF(err_mesg);
    }

    PR_FREEIF(subj);
    PR_FREEIF(acl);

done:;
    fprintf(stderr, "-- returns %s\n", allow == JS_TRUE ? "true" : "false");
    return allow;
}

char *
lm_NotifyUserAboutACLExpansion(JSContext *cx)
{
    int i, j;
    int uhl_len;
    const char *subjectOriginDomain;
    char substr[1024];
    char *pref;
    char untrustedHostsBuf[1024];
    
    pref = AddSecPolicyPrefix(cx, "untrusted_host");

    if(pref == NULL) 
        return NULL;

    uhl_len = sizeof(untrustedHostsBuf);
    PR_BZERO(untrustedHostsBuf, uhl_len);
    PREF_GetCharPref(pref, untrustedHostsBuf, &uhl_len);
    PR_FREEIF(pref);

    if(!untrustedHostsBuf[0])
        return NULL;

    // Get the domain of the subject origin

    subjectOriginDomain = getCanonicalizedOrigin(cx, lm_GetSubjectOriginURL(cx));

    fprintf(stderr, "subjectDomain - %s, \n", subjectOriginDomain);

    // Now, check whether subjectOriginDomain
    // is part of untrustedHostsBuf
    uhl_len = PL_strlen(untrustedHostsBuf);
    j = 0;
    while(j < uhl_len) {
        PR_BZERO(substr, 1024);

        for(i = 0; (j < uhl_len) && (untrustedHostsBuf[j] != ','); j++) {
            if(!isspace(untrustedHostsBuf[j])) {
                substr[i++] = untrustedHostsBuf[j];
            }
        }

        // Remove trailing '/'
        if(substr[i-1] == '/')
            i--;

        substr[i] = '\0';

        if(PL_strcmp(substr, subjectOriginDomain) == 0) {
            char *message_str = PR_smprintf("Script from untrusted domain: %s trying to expand ACL\n Allow?", subjectOriginDomain);
            return message_str;
        }

        fprintf(stderr, "lm_NotifyUserAboutACLExpansion: substr - %s, \n", substr);

        while((j < uhl_len) && 
              (untrustedHostsBuf[j] == ',' || isspace(untrustedHostsBuf[j]))) {
            j++;
        }
        
    }

    return NULL;
}

JSBool
lm_CheckPrivateTag(JSContext *cx, JSObject *obj, jsval id)
{
    char *name;
    const char *subjectOrigin, *objectOrigin;
    JSObject *parent;

    if(!JSVAL_IS_INT(id))
        return JS_FALSE;

    fprintf(stderr, "Inside lm_CheckPrivateTag, id %d\n", JSVAL_TO_INT(id));

    // May be in a layer loaded from a different origin.
    subjectOrigin = lm_GetSubjectOriginURL(cx);
    objectOrigin = lm_GetObjectOriginURL(cx, obj);

    // If the subjectURL and objectURL don't match, stop here
    if (   subjectOrigin != 0
        && objectOrigin != 0
	&& PL_strcmp(subjectOrigin, objectOrigin) == 0
	&& PL_strcmp(subjectOrigin, lm_unknown_origin_str) != 0
    )
        return JS_FALSE;

    // Check for private tag by examining object, then its ancestors.
    name = lm_GetPrivateName_slot(JSVAL_TO_INT(id));

    for (parent = obj; parent != 0; parent = JS_GetParent(cx, obj)) {
	 jsval val;

	fprintf(stderr, "(((( %s ))))\n", name);

	if (   JS_GetProperty(cx, obj, name, &val)
	    && val != JSVAL_VOID
	    && JSVAL_TO_BOOLEAN(val) == JS_TRUE
	) {
	    JS_ReportError(cx, "Cannot access private property");
	    return JS_TRUE;
	}

        // Now check for private tag on composite objects
	obj = parent;
	PR_Free(name);
	name = lm_GetPrivateName_obj(obj);
    }
    PR_Free(name);
    return JS_FALSE;
}
*/
#endif //def'ing out ACL code.




#endif