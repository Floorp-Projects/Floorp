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
#include "nsScriptSecurityManager.h"
#include "nsIServiceManager.h"
#include "nsIPrincipalManager.h"
#include "nsIScriptGlobalObjectData.h"
#include "nsIPref.h"
#include "nsIURL.h"
#ifdef OJI
#include "jvmmgr.h"
#endif
#include "nspr.h"
#include "plstr.h"
#include "nsCOMPtr.h"
#include "nsJSPrincipals.h"
#include "nsCRT.h"
#include "nsXPIDLString.h"

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kURLCID, NS_STANDARDURL_CID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_IID(kIScriptSecurityManagerIID, NS_ISCRIPTSECURITYMANAGER_IID);

NS_IMPL_ISUPPORTS(nsScriptSecurityManager, kIScriptSecurityManagerIID);

static char gUnknownOrigin[] = "[unknown origin]";
static char gFileScheme[] = "file";
static nsString gUnknownOriginStr(gUnknownOrigin);
static nsString gFileUrlPrefix("file:");

static char accessErrorMessage[] = 
    "access disallowed from scripts at %s to documents at another domain";

nsScriptSecurityManager::nsScriptSecurityManager(void)
{
    NS_INIT_REFCNT();
}

nsScriptSecurityManager::~nsScriptSecurityManager(void)
{
//  nsServiceManager::ReleaseService(kPrefServiceCID, mPrefs);  
} 

nsScriptSecurityManager *
nsScriptSecurityManager::GetScriptSecurityManager()
{
    static nsScriptSecurityManager *ssecMan = NULL;
    if (!ssecMan) 
        ssecMan = new nsScriptSecurityManager();
    return ssecMan;
}


NS_IMETHODIMP
nsScriptSecurityManager::CheckScriptAccess(nsIScriptContext *aContext, 
                                           void *aObj, const char *aProp, 
                                           PRBool isWrite, PRBool *aResult)
{
    *aResult = PR_FALSE;
    JSContext *cx = (JSContext *)aContext->GetNativeContext();
    PRInt32 secLevel = GetSecurityLevel(cx, (char *) aProp, nsnull);
    switch (secLevel) {
    case SCRIPT_SECURITY_ALL_ACCESS:
        *aResult = PR_TRUE;
        return NS_OK;
    case SCRIPT_SECURITY_SAME_DOMAIN_ACCESS:
        return CheckPermissions(cx, (JSObject *) aObj, eJSTarget_Max, aResult);
    default:
        // Default is no access
        *aResult = PR_FALSE;
        return NS_OK;
    }
}

NS_IMETHODIMP
nsScriptSecurityManager::CanAccessTarget(JSContext *aCx, PRInt16 aTarget, 
                                         PRBool *aReturn)
{
#if 0
    JSPrincipals *principals;
    *aReturn = PR_TRUE;
    GetSubjectPrincipal(aCx, &principals);
    if ((nsCapsGetRegistrationModeFlag()) && principals && 
        (NET_URL_Type(principals->codebase) == FILE_TYPE_URL)) 
    {
        return NS_OK;
    } else if (principals && !principals->globalPrivilegesEnabled(aCx, principals)) {
        *aReturn = PR_FALSE;
    }
    // only if signed scripts
    else if (!this->PrincipalsCanAccessTarget(aCx, aTarget)) {
        *aReturn = PR_FALSE;
    }
#else
    *aReturn = PR_FALSE;
#endif
    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::GetSubjectPrincipal(JSContext *aCx, 
                                             nsIPrincipal **result)
{
    // Get principals from innermost frame of JavaScript or Java.
    JSPrincipals *principals;
    JSStackFrame *fp;
    JSScript *script;
#ifdef OJI
    JSStackFrame *pFrameToStartLooking = 
        *JVM_GetStartJSFrameFromParallelStack();
    JSStackFrame *pFrameToEndLooking   = 
        JVM_GetEndJSFrameFromParallelStack(pFrameToStartLooking);
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
    fp = pFrameToStartLooking;
    while (fp != pFrameToEndLooking) {
        script = JS_GetFrameScript(aCx, fp);
        if (script) {
            principals = JS_GetScriptPrincipals(aCx, script);
            if (principals) {
                nsJSPrincipals *nsJSPrin = (nsJSPrincipals *) principals;
                *result = nsJSPrin->nsIPrincipalPtr;
                NS_ADDREF(*result);
                return NS_OK;
            } else {
                return NS_ERROR_FAILURE;
            }
        }
        fp = JS_FrameIterator(aCx, &fp);
    }
#ifdef OJI
    principals = JVM_GetJavaPrincipalsFromStack(pFrameToStartLooking);
    if (principals && principals->codebase) {
        // create new principals
        nsresult rv;
        NS_WITH_SERVICE(nsIPrincipalManager, prinMan, 
                        NS_PRINCIPALMANAGER_PROGID, &rv);
        if (NS_SUCCEEDED(rv)) 
            rv = prinMan->CreateCodebasePrincipal(principals->codebase, 
                                                  nsnull, result);
        if (NS_SUCCEEDED(rv))
            return NS_OK;
    }
#endif
    // Couldn't find principals.
    return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
nsScriptSecurityManager::GetObjectPrincipal(JSContext *aCx, JSObject *aObj, 
                                            nsIPrincipal **result)
{
    JSObject *parent;
    while (parent = JS_GetParent(aCx, aObj)) 
        aObj = parent;
    
    nsISupports *supports = (nsISupports *) JS_GetPrivate(aCx, aObj);
    nsCOMPtr<nsIScriptGlobalObjectData> globalData;
    if (!supports || NS_FAILED(supports->QueryInterface(
                                     NS_GET_IID(nsIScriptGlobalObjectData), 
                                     (void **) getter_AddRefs(globalData))))
    {
        return NS_ERROR_FAILURE;
    }
    if (NS_FAILED(globalData->GetPrincipal(result))) {
        return NS_ERROR_FAILURE;
    }
    NS_ADDREF(*result);
    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::CheckPermissions(JSContext *aCx, JSObject *aObj, 
                                          PRInt16 aTarget, PRBool* aReturn)
{
    nsCOMPtr<nsIPrincipal> subject;
    if (NS_FAILED(GetSubjectPrincipal(aCx, getter_AddRefs(subject))))
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIPrincipal> object;
    if (NS_FAILED(GetObjectPrincipal(aCx, aObj, getter_AddRefs(object))))
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsICodebasePrincipal> subjectCodebase;
    if (NS_FAILED(subject->QueryInterface(
                        NS_GET_IID(nsICodebasePrincipal),
	                (void **) getter_AddRefs(subjectCodebase))))
    {
        return NS_ERROR_FAILURE;
    }
    if (NS_FAILED(subjectCodebase->SameOrigin(object, aReturn)))
        return NS_ERROR_FAILURE;

    if (*aReturn)
        return NS_OK;

    /*
    ** If we failed the origin tests it still might be the case that we
    ** are a signed script and have permissions to do this operation.
    ** Check for that here
    */
    if (aTarget != eJSTarget_Max) {
        PRBool canAccess;
        CanAccessTarget(aCx, aTarget, &canAccess);
        if (canAccess) {
            *aReturn = PR_TRUE;
            return NS_OK;
        }
    }
    
    nsCOMPtr<nsIURI> uri;
    if (NS_FAILED(subjectCodebase->GetURI(getter_AddRefs(uri)))) 
        return NS_ERROR_FAILURE;

    char *spec;
    if (NS_FAILED(uri->GetSpec(&spec)))
        return NS_ERROR_FAILURE;

    JS_ReportError(aCx, accessErrorMessage, spec);
    nsCRT::free(spec);
    *aReturn = PR_FALSE;
    return NS_OK;
}


PRInt32 
nsScriptSecurityManager::GetSecurityLevel(JSContext *cx, char *prop_name, 
                                          int priv_code)
{
    if (prop_name == nsnull) 
        return SCRIPT_SECURITY_NO_ACCESS;
    char *tmp_prop_name = AddSecPolicyPrefix(cx, prop_name);
    if (tmp_prop_name == nsnull) 
        return SCRIPT_SECURITY_NO_ACCESS;
    PRInt32 secLevel;
    char *secLevelString;
    nsIPref *mPrefs;
    nsServiceManager::GetService(kPrefServiceCID, NS_GET_IID(nsIPref), 
                                 (nsISupports**) &mPrefs);
    if (NS_SUCCEEDED(mPrefs->CopyCharPref(tmp_prop_name, &secLevelString)) &&
        secLevelString) 
    {
        PR_FREEIF(tmp_prop_name);
        if (PL_strcmp(secLevelString, "sameOrigin") == 0)
            secLevel = SCRIPT_SECURITY_SAME_DOMAIN_ACCESS;
        else if (PL_strcmp(secLevelString, "all") == 0)
            secLevel = SCRIPT_SECURITY_ALL_ACCESS;
        else if (PL_strcmp(secLevelString, "none") == 0)
            secLevel = SCRIPT_SECURITY_NO_ACCESS;
        else
            secLevel = SCRIPT_SECURITY_NO_ACCESS;
        // NB TODO: what about signed scripts?
        PR_Free(secLevelString);
        return secLevel;
    }

    // If no preference is defined for this property, allow access. 
    // This violates the rule of a safe default, but means we don't have
    // to specify the large majority of unchecked properties, only the
    // minority of checked ones.
    PR_FREEIF(tmp_prop_name);
    return SCRIPT_SECURITY_ALL_ACCESS;
}


char *
nsScriptSecurityManager::AddSecPolicyPrefix(JSContext *cx, char *pref_str)
{
    const char *subjectOrigin = "";//GetSubjectOriginURL(cx);
    char *policy_str, *retval = 0;
    if ((policy_str = GetSitePolicy(subjectOrigin)) == 0) {
        /* No site-specific policy.  Get global policy name. */
        nsIPref * mPrefs;
        nsServiceManager::GetService(kPrefServiceCID,NS_GET_IID(nsIPref), (nsISupports**)&mPrefs);
        if (NS_OK != mPrefs->CopyCharPref("javascript.security_policy", &policy_str))
            policy_str = PL_strdup("default");
    }
    if (policy_str) { //why can't this be default? && PL_strcasecmp(policy_str, "default") != 0) {
        retval = PR_sprintf_append(NULL, "security.policy.%s.%s", policy_str, pref_str);
        PR_Free(policy_str);
    }
    
    return retval;
}


char *
nsScriptSecurityManager::GetSitePolicy(const char *org)
{
    char *sitepol, *sp, *nextsp, *orghost = 0, *retval = 0, *prot, *bar, *end, *match = 0;
    int splen, matlen;
    nsIURL *url;
    nsresult rv;
    nsIPref *mPrefs;
    NS_WITH_SERVICE(nsIComponentManager, compMan, kComponentManagerCID, &rv);
    if (NS_FAILED(rv)) 
        return nsnull;
    rv = compMan->CreateInstance(kURLCID,NULL, NS_GET_IID(nsIURL), (void**) &url);
    if (NS_FAILED(rv)) 
        return nsnull;
    nsServiceManager::GetService(kPrefServiceCID, NS_GET_IID(nsIPref), 
        (nsISupports **) &mPrefs);
    if (NS_OK != mPrefs->CopyCharPref("security.policy.site_policy", &sitepol)) 
        return nsnull;
    /* Site policy comprises text of the form site1-policy,site2-policy,siteNpolicy
     * where each site-policy is site|policy and policy is presumed to be one of strict/moderate/default
     * site may be either a URL or a hostname.  In the former case we do a prefix match with the origin URL; in the latter case
     * we just compare hosts. Process entry by entry.  Take longest match, to account for
     * cases like: *	http://host/|moderate,http://host/dir/|strict
     */
    for (sp = sitepol; sp != 0; sp = nextsp) {
        if ((nextsp = strchr(sp, ',')) != 0) *nextsp++ = '\0';
        if ((bar = strchr(sp, '|')) == 0) 
            continue;			/* no | for this entry */
        *bar = '\0';
        /* Isolate host, then policy. */
        sp += strspn(sp, " ");	/* skip leading spaces */
        end = sp + strcspn(sp, " |"); /* skip up to space or | */
        *end = '\0';
        if ((splen = end-sp) == 0) 
            continue;			/* no URL or hostname */
        /* Check whether this is long enough. */
        if (match != 0 && matlen >= splen) 
            continue;			/* Nope.  New shorter than old. */
        /* Check which case, URL or hostname, we're dealing with. */
        rv = url->SetSpec(sp);
        if (NS_FAILED(rv)) 
            return nsnull;
        url->GetScheme(& prot);
        if (prot != 0 && *prot != '\0') {
            /* URL case.  Do prefix match, make sure we're at proper boundaries. */
            if (PL_strncmp(org, sp, splen) != 0 || (org[splen] != '\0'	/* exact match */
                && sp[splen-1] != '/'	/* site policy ends with / */
                && org[splen] != '/'	/* site policy doesn't, but org does */
                )) 
            {
                nsCRT::free(prot);
                continue;			/* no match */
            }
        } else {
            /* Host-only case. */
            PR_FREEIF(prot);
            rv = url->SetSpec((char *)org);
            if (NS_FAILED(rv)) 
                return nsnull;
            url->GetHost(& orghost);
            if (orghost == 0) 
                return nsnull;			/* out of mem */
            if (PL_strcasecmp(orghost, sp) != 0) 
                continue;			/* no match */
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
        if (sp != end) 
            retval = PL_strdup(sp);
    }
    
    nsCRT::free(orghost);
    PR_FREEIF(sitepol);
    return retval;
}
