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
#include "nsCRT.h"
#include "nsXPIDLString.h"

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kURLCID, NS_STANDARDURL_CID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_IID(kIScriptSecurityManagerIID, NS_ISCRIPTSECURITYMANAGER_IID);

NS_IMPL_ISUPPORTS(nsScriptSecurityManager, kIScriptSecurityManagerIID);

static nsString gUnknownOriginStr("[unknown origin]");
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
    static nsScriptSecurityManager * ssecMan = NULL;
    if (!ssecMan) 
        ssecMan = new nsScriptSecurityManager();
    return ssecMan;
}

NS_IMETHODIMP 
nsScriptSecurityManager::NewJSPrincipals(nsIURI *aURL, nsString *aName, 
                                         nsIPrincipal **result)
{
//  nsJSPrincipalsData * pdata;
  PRBool needUnlock = PR_FALSE;
#ifdef CERT_PRINS
  void *zip = nsnull; //ns_zip_t
  if (aURL) {
    char *fn = nsnull;
    if (NET_IsLocalFileURL(archive->address)) {
      char* pathPart = ParseURL(archive->address, GET_PATH_PART);
      fn = WH_FileName(pathPart, xpURL);
      PR_Free(pathPart);
    } 
    else if (archive->cache_file && NET_ChangeCacheFileLock(archive, TRUE)) {
      fn = WH_FileName(archive->cache_file, xpCache);
      needUnlock = PR_TRUE;
    }
    if (fn) {
#ifdef XP_MAC
      /*
       * Unfortunately, ns_zip_open wants a Unix-style name. Convert
       * Mac path to a Unix-style path. This code is copied from
       * appletStubs.c.
       */
      OSErr ConvertMacPathToUnixPath(const char *macPath, char **unixPath);
      char *unixPath = nsnull;
      if (ConvertMacPathToUnixPath(fn, &unixPath) == 0) 
        zip = ns_zip_open(unixPath);
      PR_FREEIF(unixPath);
#else
      zip = ns_zip_open(fn);
#endif
      pdata->zip = zip;
      PR_Free(fn);
    }
  }
#endif 
    nsresult rv;
    nsXPIDLCString codebaseStr;
    if (!NS_SUCCEEDED(rv = GetOriginFromSourceURL(aURL, getter_Copies(codebaseStr)))) 
        return rv;
    if (!codebaseStr) {
        return NS_ERROR_FAILURE;
    }
    NS_WITH_SERVICE(nsIPrincipalManager, prinMan, NS_PRINCIPALMANAGER_PROGID, &rv);
    if (NS_SUCCEEDED(rv)) 
        rv = prinMan->CreateCodebasePrincipal(codebaseStr, aURL, result);
    return rv;
}

NS_IMETHODIMP
nsScriptSecurityManager::CheckScriptAccess(nsIScriptContext *aContext, 
                                           void *aObj, const char *aProp, 
                                           PRBool *aResult)
{
  *aResult = PR_FALSE;
  JSContext* cx = (JSContext*)aContext->GetNativeContext();
  PRInt32 secLevel = CheckForPrivilege(cx, (char *) aProp, nsnull);
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
nsScriptSecurityManager::GetSubjectOriginURL(JSContext *aCx, char * * aOrigin)
{
// Get origin from script of innermost interpreted frame.
  JSPrincipals * principals;
  JSStackFrame * fp;
  JSScript * script;
#ifdef OJI
  JSStackFrame * pFrameToStartLooking = *JVM_GetStartJSFrameFromParallelStack();
  JSStackFrame * pFrameToEndLooking   = JVM_GetEndJSFrameFromParallelStack(pFrameToStartLooking);
  if (pFrameToStartLooking == nsnull) {
    pFrameToStartLooking = JS_FrameIterator(aCx,& pFrameToStartLooking);
    if (pFrameToStartLooking == nsnull) {
      // There are no frames or scripts at this point.
      pFrameToEndLooking = nsnull;
    }
  }
#else
  JSStackFrame * pFrameToStartLooking = JS_FrameIterator(aCx, &fp);
  JSStackFrame * pFrameToEndLooking   = nsnull;
#endif
  fp = pFrameToStartLooking;
  while (fp  != pFrameToEndLooking) {
    script = JS_GetFrameScript(aCx, fp);
    if (script) {
      principals = JS_GetScriptPrincipals(aCx, script);
      * aOrigin = principals ? (char *)principals->codebase : (char *)JS_GetScriptFilename(aCx, script);
      return (* aOrigin) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    }
    fp = JS_FrameIterator(aCx, &fp);
  }
#ifdef OJI
  principals = JVM_GetJavaPrincipalsFromStack(pFrameToStartLooking);
  if (principals) {
    *aOrigin = principals->codebase;
    return (* aOrigin) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
  }
#endif
  /*
   * Not called from either JS or Java. We must be called
   * from the interpreter. Get the origin from the decoder.
   */
  // NB TODO: Does this ever happen? 
  return this->GetObjectOriginURL(aCx, ::JS_GetGlobalObject(aCx), aOrigin);
}

NS_IMETHODIMP
nsScriptSecurityManager::GetObjectOriginURL(JSContext *aCx, JSObject *aObj, 
                                            char **aOrigin)
{
  nsresult rv;
  JSObject *parent;
  while (parent = ::JS_GetParent(aCx, aObj)) 
      aObj = parent;
  nsIPrincipal *prin;
  if (!NS_SUCCEEDED(rv = GetContainerPrincipals(aCx, aObj, & prin)))
      return rv;
  nsICodebasePrincipal *cbprin;
  rv = prin->QueryInterface(NS_GET_IID(nsICodebasePrincipal), (void **) &cbprin);
  if (!NS_SUCCEEDED(rv))
      return rv;
  if (!NS_SUCCEEDED(rv = cbprin->GetURLString(aOrigin)))
      return rv;
  return (*aOrigin) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsScriptSecurityManager::GetOriginFromSourceURL(nsIURI *url, char **result)
{
    nsXPIDLCString tempChars;
    nsresult rv;
    if (!NS_SUCCEEDED(rv = url->GetScheme(getter_Copies(tempChars))))
        return rv;
    nsAutoString buffer(tempChars);
    // NB TODO: what about file: urls and about:blank?
    buffer.Append("://");
    if (!NS_SUCCEEDED(rv = url->GetHost(getter_Copies(tempChars))))
        return rv;
    buffer.Append(tempChars);
    if (!NS_SUCCEEDED(rv = url->GetPath(getter_Copies(tempChars))))
        return rv;
    buffer.Append(tempChars);
    if (buffer.Length() == 0 || buffer.EqualsIgnoreCase(gUnknownOriginStr))
        return NS_ERROR_FAILURE;
    *result = buffer.ToNewCString();
    return *result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

PRInt32 
nsScriptSecurityManager::CheckForPrivilege(JSContext *cx, char *prop_name, 
                                           int priv_code)
{
    if (prop_name == nsnull) 
        return SCRIPT_SECURITY_NO_ACCESS;
    char *tmp_prop_name = AddSecPolicyPrefix(cx, prop_name);
    if (tmp_prop_name == nsnull) 
        return SCRIPT_SECURITY_NO_ACCESS;
    PRInt32 secLevel = SCRIPT_SECURITY_NO_ACCESS;
    nsIPref *mPrefs;
    nsServiceManager::GetService(kPrefServiceCID, NS_GET_IID(nsIPref), 
        (nsISupports**) &mPrefs);
    if (NS_OK == mPrefs->GetIntPref(tmp_prop_name, &secLevel)) {
        PR_FREEIF(tmp_prop_name);
        return secLevel;
    }
    // If no preference is defined for this property, allow access. 
    // This violates the rule of a safe default, but means we don't have
    // to specify the large majority of unchecked properties, only the
    // minority of checked ones.
    PR_FREEIF(tmp_prop_name);
    return SCRIPT_SECURITY_ALL_ACCESS;
}

NS_IMETHODIMP
nsScriptSecurityManager::CheckPermissions(JSContext *aCx, JSObject *aObj, 
                                          PRInt16 aTarget, PRBool* aReturn)
{
    nsXPIDLCString subjectOrigin;
    nsXPIDLCString objectOrigin;
    nsresult rv = GetSubjectOriginURL(aCx, getter_Copies(subjectOrigin));
    if (!NS_SUCCEEDED(rv))
        return rv;
    /*
    ** Hold onto reference to the running decoder's principals
    ** in case a call to GetObjectOriginURL ends up
    ** dropping a reference due to an origin changing
    ** underneath us.
    */
    rv = GetObjectOriginURL(aCx, aObj, getter_Copies(objectOrigin));
    if (rv != NS_OK || !subjectOrigin || !objectOrigin) {
        *aReturn = PR_FALSE;
        return NS_OK;
    }
    /* Now see whether the origin methods and servers match. */
    if (this->SameOrigins(aCx, subjectOrigin, objectOrigin)) {
        * aReturn = PR_TRUE;
        return NS_OK;
    }
    /*
    ** If we failed the origin tests it still might be the case that we
    **   are a signed script and have permissions to do this operation.
    ** Check for that here
    */
    if (aTarget != eJSTarget_Max) {
        PRBool canAccess;
        this->CanAccessTarget(aCx, aTarget, &canAccess);
        if (canAccess) {
            *aReturn = PR_TRUE;
            return NS_OK;
        }
    }
    
    JS_ReportError(aCx, accessErrorMessage, (const char*)subjectOrigin);
    *aReturn = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::GetContainerPrincipals(JSContext *aCx, 
                                                JSObject *container, 
                                                nsIPrincipal **result)
{
  nsresult rv;
  *result = nsnull;

  // Need to check that the origin hasn't changed underneath us
  char *originUrl = FindOriginURL(aCx, container);
  if (!originUrl) 
      return NS_ERROR_FAILURE;
  nsISupports * tmp;
  nsIScriptGlobalObjectData * globalData;
  tmp = (nsISupports *)JS_GetPrivate(aCx, container);
  if (tmp == nsnull || (rv = tmp->QueryInterface(NS_GET_IID(nsIScriptGlobalObjectData), (void * *)& globalData)) != NS_OK) 
  {
      delete originUrl;
      return rv;
  }
  globalData->GetPrincipal(result);
  if (* result) {
    nsICodebasePrincipal * cbprin;
    nsXPIDLCString cbStr;
    (* result)->QueryInterface(NS_GET_IID(nsICodebasePrincipal),(void * *)& cbprin);
    cbprin->GetURLString(getter_Copies(cbStr));
    if (this->SameOrigins(aCx, originUrl, cbStr)) {
      delete originUrl;
      return NS_OK;
    }
#ifdef THREADING_ISSUES
//    nsJSPrincipalsData * data;
//    data = (nsJSPrincipalsData*)*aPrincipals;
//    if (data->codebaseBeforeSettingDomain &&
//        this->SameOrigins(aCx, originUrl, data->codebaseBeforeSettingDomain)) {
      /* document.domain was set, so principals are okay */
//      delete originUrl;
//      return NS_OK;
//    }
    /* Principals have changed underneath us. Remove them. */
//    globalData->SetPrincipals(nsnull);
#endif    
  }
  /* Create new principals and return them. */
  //why should we create a new principal, removing this
//  nsAutoString originUrlStr(originUrl);
//  if (!NS_SUCCEEDED(this->NewJSPrincipals(nsnull, nsnull, &originUrlStr, aPrincipals))) {
//    delete originUrl;
//    return NS_ERROR_FAILURE;
//  }
//  globalData->SetPrincipals((void*)*aPrincipals);
  delete originUrl;
  return NS_OK;
}

PRBool
nsScriptSecurityManager::SameOrigins(JSContext * aCx, const char * aOrigin1, const char * aOrigin2)
{
  if ((aOrigin1 == nsnull) || (aOrigin2 == nsnull) || (PL_strlen(aOrigin1) == 0) || (PL_strlen(aOrigin2) == 0))
      return PR_FALSE;
  // Shouldn't return true if both origin1 and origin2 are unknownOriginStr. 
  nsString * tmp = new nsString(aOrigin1);
  if (gUnknownOriginStr.EqualsIgnoreCase(*tmp)) 
  {
    delete tmp;
    return PR_FALSE;
  }
  delete tmp;
  if (PL_strcmp(aOrigin1, aOrigin2) == 0) return PR_TRUE;
  nsString * cmp1 = new nsString(this->GetCanonicalizedOrigin(aCx, aOrigin1));
  nsString * cmp2 = new nsString(this->GetCanonicalizedOrigin(aCx, aOrigin2));
  
  PRBool result = PR_FALSE;
  // Either the strings are equal or they are both file: uris.
  if (cmp1 && cmp2 && 
      (*cmp1 == *cmp2 ||
       (cmp1->Find(gFileUrlPrefix) == 0 && cmp2->Find(gFileUrlPrefix) == 0)))
  {
    result = PR_TRUE;
  }
  delete cmp1;
  delete cmp2;
  return result;
}

char * 
nsScriptSecurityManager::GetCanonicalizedOrigin(JSContext* aCx, const char * aUrlString)
{
  nsString * buffer;
  nsIURL * url;
  nsresult rv;
  nsXPIDLCString tmp;
  char * origin;
  NS_WITH_SERVICE(nsIComponentManager, compMan,kComponentManagerCID,&rv);
  if (!NS_SUCCEEDED(rv)) return nsnull;
  rv = compMan->CreateInstance(kURLCID,NULL,NS_GET_IID(nsIURL),(void * *)& url);
  if (!NS_SUCCEEDED(rv)) return nsnull;
  rv = url->SetSpec((char*) aUrlString);
  if (!NS_SUCCEEDED(rv)) return nsnull;
  url->GetScheme(getter_Copies(tmp));
  buffer = new nsString(tmp);
  url->GetHost(getter_Copies(tmp)); 
  // I dont understand this part enuf but shouldn't there be a separator here? 
  buffer->Append(tmp);
  if (!buffer) {
    JS_ReportOutOfMemory(aCx);
    return nsnull;
  }
  origin = buffer->ToNewCString();
  delete buffer;
  return origin;
}

char*
nsScriptSecurityManager::FindOriginURL(JSContext * aCx, JSObject * aGlobal)
{
  nsISupports * tmp1, * tmp2;
  nsIScriptGlobalObjectData* globalData = nsnull;
  nsIURI *origin = nsnull;
  tmp1 = (nsISupports *)JS_GetPrivate(aCx, aGlobal);
  if (nsnull != tmp1 && 
      NS_OK == tmp1->QueryInterface(NS_GET_IID(nsIScriptGlobalObjectData), (void**)&globalData)) {
    globalData->GetOrigin(&origin);
  }
  if (origin == nsnull) {
      // does this ever happen?
    /* Must be a new, empty window?  Use running origin. */
    tmp2 = (nsISupports*)JS_GetPrivate(aCx, JS_GetGlobalObject(aCx));
    /* Compare running and current to avoid infinite recursion. */
    if (tmp1 == tmp2) {
        nsAutoString urlString = "[unknown origin]";
        NS_IF_RELEASE(globalData);
        return urlString.ToNewCString();
    } else if (nsnull != tmp2 && NS_OK == tmp2->QueryInterface(NS_GET_IID(nsIScriptGlobalObjectData), (void**)&globalData)) {
      globalData->GetOrigin(&origin);
    }
  }
  if (origin != nsnull) {
    nsXPIDLCString spec;
    origin->GetSpec(getter_Copies(spec));
    nsAutoString urlString(spec);
    NS_IF_RELEASE(globalData);
    return urlString.ToNewCString();
  }
  NS_IF_RELEASE(globalData);

  // return an empty string
  nsAutoString urlString("");
  return urlString.ToNewCString();
}

char *
nsScriptSecurityManager::AddSecPolicyPrefix(JSContext *cx, char *pref_str)
{
  const char *subjectOrigin = "";//GetSubjectOriginURL(cx);
  char *policy_str, *retval = 0;
  if ((policy_str = this->GetSitePolicy(subjectOrigin)) == 0) {
    /* No site-specific policy.  Get global policy name. */
    nsIPref * mPrefs;
    nsServiceManager::GetService(kPrefServiceCID,NS_GET_IID(nsIPref), (nsISupports**)&mPrefs);
    if (NS_OK != mPrefs->CopyCharPref("javascript.security_policy", &policy_str))
      policy_str = PL_strdup("default");
  }
  if (policy_str) { //why can't this be default? && PL_strcasecmp(policy_str, "default") != 0) {
    retval = PR_sprintf_append(NULL, "js_security.%s.%s", policy_str, pref_str);
    PR_Free(policy_str);
  }

  return retval;
}

NS_IMETHODIMP
nsScriptSecurityManager::CanAccessTarget(JSContext *aCx, PRInt16 aTarget, PRBool* aReturn)
{
  JSPrincipals *principals;
  * aReturn = PR_TRUE;
  this->GetPrincipalsFromStackFrame(aCx, &principals);
#if 0
  if ((nsCapsGetRegistrationModeFlag()) && principals && (NET_URL_Type(principals->codebase) == FILE_TYPE_URL)) {
    return NS_OK;
  }
  else 
#endif
  if (principals && !principals->globalPrivilegesEnabled(aCx, principals)) {
    *aReturn = PR_FALSE;
  }
#if 0
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
nsScriptSecurityManager::GetPrincipalsFromStackFrame(JSContext *aCx, JSPrincipals** aPrincipals)
{
//* Get principals from script of innermost interpreted frame.
  JSStackFrame * fp;
  JSScript * script;
#ifdef OJI
  JSStackFrame * pFrameToStartLooking = *JVM_GetStartJSFrameFromParallelStack();
  JSStackFrame * pFrameToEndLooking   = JVM_GetEndJSFrameFromParallelStack(pFrameToStartLooking);
  if (pFrameToStartLooking == nsnull) {
    pFrameToStartLooking = JS_FrameIterator(aCx, &pFrameToStartLooking);
    if (pFrameToStartLooking == nsnull) {
      /*
      ** There are no frames or scripts at this point.
      */
      pFrameToEndLooking = nsnull;
    }
  }
#else
  JSStackFrame * pFrameToStartLooking = JS_FrameIterator(aCx, &fp);
  JSStackFrame * pFrameToEndLooking   = nsnull;
#endif

  fp = pFrameToStartLooking;
  while ((fp = JS_FrameIterator(aCx, &fp)) != pFrameToEndLooking) {
    script = JS_GetFrameScript(aCx, fp);
    if (script) {
      * aPrincipals = JS_GetScriptPrincipals(aCx, script);
      return NS_OK;
    }
  }
#ifdef OJI
  * aPrincipals = JVM_GetJavaPrincipalsFromStack(pFrameToStartLooking);
  return NS_OK;
#endif
  * aPrincipals = nsnull;
  return NS_OK;
}

char *
nsScriptSecurityManager::GetSitePolicy(const char *org)
{
  char *sitepol, *sp, *nextsp, *orghost = 0, *retval = 0, *prot, *bar, *end, *match = 0;
  int splen, matlen;
  nsIURL * url;
  nsresult rv;
  nsIPref * mPrefs;
  NS_WITH_SERVICE(nsIComponentManager, compMan,kComponentManagerCID,&rv);
  if (!NS_SUCCEEDED(rv)) return nsnull;
  rv = compMan->CreateInstance(kURLCID,NULL,NS_GET_IID(nsIURL),(void**)&url);
  if (!NS_SUCCEEDED(rv)) return nsnull;
  nsServiceManager::GetService(kPrefServiceCID,NS_GET_IID(nsIPref), (nsISupports * *)& mPrefs);
  if (NS_OK != mPrefs->CopyCharPref("js_security.site_policy", &sitepol)) return 0;
  /* Site policy comprises text of the form site1-policy,site2-policy,siteNpolicy
   * where each site-policy is site|policy and policy is presumed to be one of strict/moderate/default
   * site may be either a URL or a hostname.  In the former case we do a prefix match with the origin URL; in the latter case
   * we just compare hosts. Process entry by entry.  Take longest match, to account for
   * cases like: *	http://host/|moderate,http://host/dir/|strict
   */
  for (sp = sitepol; sp != 0; sp = nextsp) {
    if ((nextsp = strchr(sp, ',')) != 0) *nextsp++ = '\0';
    if ((bar = strchr(sp, '|')) == 0) continue;			/* no | for this entry */
    *bar = '\0';
    /* Isolate host, then policy. */
    sp += strspn(sp, " ");	/* skip leading spaces */
    end = sp + strcspn(sp, " |"); /* skip up to space or | */
    *end = '\0';
    if ((splen = end-sp) == 0) continue;			/* no URL or hostname */
    /* Check whether this is long enough. */
    if (match != 0 && matlen >= splen) continue;			/* Nope.  New shorter than old. */
    /* Check which case, URL or hostname, we're dealing with. */
    rv = url->SetSpec(sp);
    if (!NS_SUCCEEDED(rv)) return nsnull;
    url->GetScheme(& prot);
    if (prot != 0 && *prot != '\0') {
      /* URL case.  Do prefix match, make sure we're at proper boundaries. */
      if (PL_strncmp(org, sp, splen) != 0 || (org[splen] != '\0'	/* exact match */
        && sp[splen-1] != '/'	/* site policy ends with / */
        && org[splen] != '/'	/* site policy doesn't, but org does */
        )) {
        nsCRT::free(prot);
        continue;			/* no match */
      }
    }
    else {
      /* Host-only case. */
      PR_FREEIF(prot);
      rv = url->SetSpec((char *)org);
      if (!NS_SUCCEEDED(rv)) return nsnull;
      url->GetHost(& orghost);
      if (orghost == 0) return 0;			/* out of mem */
      if (PL_strcasecmp(orghost, sp) != 0) continue;			/* no match */
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
    if (sp != end) retval = PL_strdup(sp);
  }
  
  nsCRT::free(orghost);
  PR_FREEIF(sitepol);
  return retval;
}
