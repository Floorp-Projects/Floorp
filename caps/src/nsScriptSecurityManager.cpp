/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Norris Boyd
 * Mitch Stoltz
 * Steve Morse
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsScriptSecurityManager.h"
#include "nsIServiceManager.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptContext.h"
#include "nsIURL.h"
#include "nsIJARURI.h"
#include "nspr.h"
#include "nsJSPrincipals.h"
#include "nsSystemPrincipal.h"
#include "nsCodebasePrincipal.h"
#include "nsCertificatePrincipal.h"
#include "nsAggregatePrincipal.h"
#include "nsXPIDLString.h"
#include "nsCRT.h"
#include "nsIJSContextStack.h"
#include "nsDOMError.h"
#include "nsDOMCID.h"
#include "jsdbgapi.h"
#include "nsIXPConnect.h"
#include "nsIXPCSecurityManager.h"
#include "nsTextFormatter.h"
#include "nsIStringBundle.h"
#include "nsNetUtil.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsIZipReader.h"
#include "nsIJAR.h"
#include "nsIPluginInstance.h"
#include "nsIXPConnect.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIPrompt.h"
#include "nsIWindowWatcher.h"
#include "nsIConsoleService.h"
#include "nsISecurityCheckedComponent.h"
#include "nsIPrefBranchInternal.h"
#include "nsIJSRuntimeService.h"
#include "nsIObserverService.h"
#include "nsIContent.h"

static NS_DEFINE_IID(kIStringBundleServiceIID, NS_ISTRINGBUNDLESERVICE_IID);
static NS_DEFINE_IID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_CID(kCScriptNameSetRegistryCID,
                     NS_SCRIPT_NAMESET_REGISTRY_CID);
static NS_DEFINE_CID(kZipReaderCID, NS_ZIPREADER_CID);
static NS_DEFINE_IID(kObserverServiceIID, NS_IOBSERVERSERVICE_IID);

///////////////////////////
// Convenience Functions //
///////////////////////////
// Result of this function should not be freed.
static inline const PRUnichar *
JSValIDToString(JSContext *cx, const jsval idval) {
    JSString *str = JS_ValueToString(cx, idval);
    if(!str)
        return nsnull;
    return NS_REINTERPRET_CAST(PRUnichar*, JS_GetStringChars(str));
}

// Helper class to get stuff from the ClassInfo and not waste extra time with
// virtual method calls for things it has already gotten
class ClassInfoData
{
public:
    ClassInfoData(nsIClassInfo *aClassInfo)
        : mClassInfo(aClassInfo), mDidGetFlags(PR_FALSE)
    {
    }

    PRUint32 GetFlags()
    {
        if (!mDidGetFlags) {
            if (mClassInfo) {
                mDidGetFlags = PR_TRUE;
                nsresult rv = mClassInfo->GetFlags(&mFlags);
                if (NS_FAILED(rv)) {
                    mFlags = 0;
                }
            } else {
                mFlags = 0;
            }
        }

        return mFlags;
    }

    PRBool IsDOMClass()
    {
        return GetFlags() & nsIClassInfo::DOM_OBJECT;
    }
    PRBool IsContentNode()
    {
        return GetFlags() & nsIClassInfo::CONTENT_NODE;
    }

private:
    nsIClassInfo *mClassInfo; // WEAK
    PRBool mDidGetFlags;
    PRUint32 mFlags;
};
 
JSContext *
nsScriptSecurityManager::GetCurrentJSContext()
{
    // Get JSContext from stack.
    if (!mJSContextStack)
    {
        mJSContextStack = do_GetService("@mozilla.org/js/xpc/ContextStack;1");
        if (!mJSContextStack)
            return nsnull;
    }
    JSContext *cx;
    if (NS_FAILED(mJSContextStack->Peek(&cx)))
        return nsnull;
    return cx;
}

JSContext *
nsScriptSecurityManager::GetSafeJSContext()
{
    // Get JSContext from stack.
    if (!mJSContextStack) {
        mJSContextStack = do_GetService("@mozilla.org/js/xpc/ContextStack;1");
        if (!mJSContextStack)
            return nsnull;
    }

    JSContext *cx;
    if (NS_FAILED(mJSContextStack->GetSafeJSContext(&cx)))
        return nsnull;
    return cx;
}


class ClassNameHolder
{
public:
    ClassNameHolder(const char* aClassName, nsIClassInfo* aClassInfo) : 
      mClassName((char*)aClassName), mClassInfo(aClassInfo), mMustFree(PR_FALSE)
    {
    }

    ~ClassNameHolder()
    {
        if (mMustFree)
            nsMemory::Free(mClassName);
    }

    char* get()
    {
        if (mClassName)
            return mClassName;

        if (mClassInfo)
            mClassInfo->GetClassDescription(&mClassName);
        if (mClassName)
            mMustFree = PR_TRUE;
        else
            mClassName = "UnnamedClass";

        return mClassName;
    }

private:
    char* mClassName;
    nsCOMPtr<nsIClassInfo> mClassInfo;
    PRBool mMustFree;
};

/* Static function for comparing two URIs - for security purposes,
 * two URIs are equivalent if their scheme, host, and port are equal.
 */
/*static*/ nsresult
nsScriptSecurityManager::SecurityCompareURIs(nsIURI* aSourceURI,
                                             nsIURI* aTargetURI,
                                             PRBool* result)
{
    nsresult rv;
    *result = PR_FALSE;

    if (aSourceURI == aTargetURI)
    {
        *result = PR_TRUE;
        return NS_OK;
    }
    if (aTargetURI == nsnull) 
    {
        // return false
        return NS_OK;
    }

    // If either uri is a jar URI, get the base URI
    nsCOMPtr<nsIJARURI> jarURI;
    nsCOMPtr<nsIURI> sourceBaseURI(aSourceURI);
    while((jarURI = do_QueryInterface(sourceBaseURI)))
    {
        jarURI->GetJARFile(getter_AddRefs(sourceBaseURI));
    }
    nsCOMPtr<nsIURI> targetBaseURI(aTargetURI);
    while((jarURI = do_QueryInterface(targetBaseURI)))
    {
        jarURI->GetJARFile(getter_AddRefs(targetBaseURI));
    }

    if (!sourceBaseURI || !targetBaseURI)
        return NS_ERROR_FAILURE;

    // Compare schemes
    nsCAutoString targetScheme;
    rv = targetBaseURI->GetScheme(targetScheme);
    nsCAutoString sourceScheme;
    if (NS_SUCCEEDED(rv))
        rv = sourceBaseURI->GetScheme(sourceScheme);
    if (NS_SUCCEEDED(rv) && 
        targetScheme.Equals(sourceScheme, nsCaseInsensitiveCStringComparator())) 
    {
        if (targetScheme.Equals(NS_LITERAL_CSTRING("file"),
            nsCaseInsensitiveCStringComparator()))
        {
            // All file: urls are considered to have the same origin.
            *result = PR_TRUE;
        }
        else if (targetScheme.Equals(NS_LITERAL_CSTRING("imap"),
                                     nsCaseInsensitiveCStringComparator()) ||
                 targetScheme.Equals(NS_LITERAL_CSTRING("mailbox"),
                                     nsCaseInsensitiveCStringComparator()) ||
                 targetScheme.Equals(NS_LITERAL_CSTRING("news"),
                                     nsCaseInsensitiveCStringComparator()))
        {
            // Each message is a distinct trust domain; use the 
            // whole spec for comparison
            nsCAutoString targetSpec;
            if (NS_FAILED(targetBaseURI->GetSpec(targetSpec)))
                return NS_ERROR_FAILURE;
            nsCAutoString sourceSpec;
            if (NS_FAILED(sourceBaseURI->GetSpec(sourceSpec)))
                return NS_ERROR_FAILURE;
            *result = targetSpec.Equals(sourceSpec);
        }
        else
        {
            // Compare hosts
            nsCAutoString targetHost;
            rv = targetBaseURI->GetHost(targetHost);
            nsCAutoString sourceHost;
            if (NS_SUCCEEDED(rv))
                rv = sourceBaseURI->GetHost(sourceHost);
            *result = NS_SUCCEEDED(rv) &&
                      targetHost.Equals(sourceHost,
                                        nsCaseInsensitiveCStringComparator());
            if (*result) 
            {
                // Compare ports
                PRInt32 targetPort;
                rv = targetBaseURI->GetPort(&targetPort);
                PRInt32 sourcePort;
                if (NS_SUCCEEDED(rv))
                    rv = sourceBaseURI->GetPort(&sourcePort);
                *result = NS_SUCCEEDED(rv) && targetPort == sourcePort;
                // If the port comparison failed, see if either URL has a
                // port of -1. If so, replace -1 with the default port
                // for that scheme.
                if (!*result && (sourcePort == -1 || targetPort == -1))
                {
                    PRInt32 defaultPort;
                    //XXX had to hard-code the defualt port for http(s) here.
                    //    remove this after darin fixes bug 113206
                    if (sourceScheme.Equals(NS_LITERAL_CSTRING("http"),
                                            nsCaseInsensitiveCStringComparator()))
                        defaultPort = 80;
                    else if (sourceScheme.Equals(NS_LITERAL_CSTRING("https"),
                                                 nsCaseInsensitiveCStringComparator()))
                        defaultPort = 443;
                    else
                    {
                        nsCOMPtr<nsIIOService> ioService(
                            do_GetService(NS_IOSERVICE_CONTRACTID));
                        if (!ioService)
                            return NS_ERROR_FAILURE;
                        nsCOMPtr<nsIProtocolHandler> protocolHandler;
                        rv = ioService->GetProtocolHandler(sourceScheme.get(),
                                                           getter_AddRefs(protocolHandler));
                        if (NS_FAILED(rv))
                        {
                            *result = PR_FALSE;
                            return NS_OK;
                        }
                    
                        rv = protocolHandler->GetDefaultPort(&defaultPort);
                        if (NS_FAILED(rv) || defaultPort == -1)
                            return NS_OK; // No default port for this scheme
                    }
                    if (sourcePort == -1)
                        sourcePort = defaultPort;
                    else if (targetPort == -1)
                        targetPort = defaultPort;
                    *result = targetPort == sourcePort;
                }
            }
        }
    }
    return NS_OK;
}

////////////////////
// Policy Storage //
////////////////////

// Table of security levels
PR_STATIC_CALLBACK(PRBool)
DeleteCapability(nsHashKey *aKey, void *aData, void* closure)
{
    nsMemory::Free(aData);
    return PR_TRUE;
}

//-- Per-Domain Policy - applies to one or more protocols or hosts
struct DomainEntry
{
    DomainEntry(const char* aOrigin,
                DomainPolicy* aDomainPolicy) : mOrigin(aOrigin),
                                               mDomainPolicy(aDomainPolicy),
                                               mNext(nsnull)
    {
        mDomainPolicy->Hold();
    }

    ~DomainEntry()
    {
        mDomainPolicy->Drop();
    }

    PRBool Matches(const char *anOrigin)
    {
        int len = strlen(anOrigin);
        int thisLen = mOrigin.Length();
        if (len < thisLen)
            return PR_FALSE;
        if (mOrigin.RFindChar(':', thisLen-1, 1) != -1)
        //-- Policy applies to all URLs of this scheme, compare scheme only
            return mOrigin.EqualsWithConversion(anOrigin, PR_TRUE, thisLen);

        //-- Policy applies to a particular host; compare domains
        if (!mOrigin.Equals(anOrigin + (len - thisLen)))
            return PR_FALSE;
        if (len == thisLen)
            return PR_TRUE;
        char charBefore = anOrigin[len-thisLen-1];
        return (charBefore == '.' || charBefore == ':' || charBefore == '/');
    }

    nsCString         mOrigin;
    DomainPolicy*     mDomainPolicy;
    DomainEntry*      mNext;
#ifdef DEBUG
    nsCString         mPolicyName_DEBUG;
#endif
};

PR_STATIC_CALLBACK(PRBool)
DeleteDomainEntry(nsHashKey *aKey, void *aData, void* closure)
{
    DomainEntry *entry = (DomainEntry*) aData;
    do
    {
        DomainEntry *next = entry->mNext;
        delete entry;
        entry = next;
    } while (entry);
    return PR_TRUE;
}

/////////////////////////////
// nsScriptSecurityManager //
/////////////////////////////

////////////////////////////////////
// Methods implementing ISupports //
////////////////////////////////////
NS_IMPL_ISUPPORTS3(nsScriptSecurityManager,
                   nsIScriptSecurityManager,
                   nsIXPCSecurityManager,
                   nsIObserver)

///////////////////////////////////////////////////
// Methods implementing nsIScriptSecurityManager //
///////////////////////////////////////////////////

///////////////// Security Checks /////////////////
JSBool JS_DLL_CALLBACK
nsScriptSecurityManager::CheckJSFunctionCallerAccess(JSContext *cx, JSObject *obj,
                                                     jsval id, JSAccessMode mode,
                                                     jsval *vp)
{
    // Currently, this function will be called only when function.caller
    // is accessed. If that changes, we will need to change this function.
    NS_ASSERTION(nsCRT::strcmp(NS_REINTERPRET_CAST(PRUnichar*,
                                                   JS_GetStringChars(JSVAL_TO_STRING(id))),
                               NS_LITERAL_STRING("caller").get()) == 0,
                 "CheckJSFunctionCallerAccess called for a property other than \'caller\'");
    // Get the security manager

    nsScriptSecurityManager *ssm =
        nsScriptSecurityManager::GetScriptSecurityManager();

    if (!ssm)
    {
        NS_ERROR("Failed to get security manager service");
        return JS_FALSE;
    }

    // Get the caller function object
    NS_ASSERTION(JSVAL_IS_OBJECT(*vp), "*vp is not an object");
    JSObject* target = JSVAL_TO_OBJECT(*vp);

    // Do the same-origin check - this sets a JS exception if the check fails
    nsresult rv =
        ssm->CheckPropertyAccess(cx, target, "Function", sCallerID,
                                 nsIXPCSecurityManager::ACCESS_GET_PROPERTY);

    if (NS_FAILED(rv))
        return JS_FALSE; // Security check failed

    return JS_TRUE;
}

NS_IMETHODIMP
nsScriptSecurityManager::CheckPropertyAccess(JSContext* cx,
                                             JSObject* aJSObject,
                                             const char* aClassName,
                                             jsval aProperty,
                                             PRUint32 aAction)
{
    return CheckPropertyAccessImpl(aAction, nsnull, cx, aJSObject,
                                   nsnull, nsnull, nsnull,
                                   aClassName, aProperty, nsnull);
}

NS_IMETHODIMP
nsScriptSecurityManager::CheckConnect(JSContext* cx,
                                      nsIURI* aTargetURI,
                                      const char* aClassName,
                                      const char* aPropertyName)
{
    // Get a context if necessary
    if (!cx)
    {
        cx = GetCurrentJSContext();
        if (!cx)
            return NS_OK; // No JS context, so allow the load
    }

    nsresult rv = CheckLoadURIFromScript(cx, aTargetURI);
    if (NS_FAILED(rv)) return rv;

    JSString* propertyName = ::JS_InternString(cx, aPropertyName);
    if (!propertyName)
        return NS_ERROR_OUT_OF_MEMORY;

    return CheckPropertyAccessImpl(nsIXPCSecurityManager::ACCESS_CALL_METHOD, nsnull,
                                   cx, nsnull, nsnull, aTargetURI,
                                   nsnull, aClassName, STRING_TO_JSVAL(propertyName), nsnull);
}

NS_IMETHODIMP
nsScriptSecurityManager::CheckSameOrigin(JSContext* cx,
                                         nsIURI* aTargetURI)
{
    nsresult rv;

    // Get a context if necessary
    if (!cx)
    {
        cx = GetCurrentJSContext();
        if (!cx)
            return NS_OK; // No JS context, so allow access
    }

    // Get a principal from the context
    nsCOMPtr<nsIPrincipal> sourcePrincipal;
    rv = GetSubjectPrincipal(cx, getter_AddRefs(sourcePrincipal));
    if (NS_FAILED(rv))
        return rv;

    if (!sourcePrincipal)
    {
        NS_WARNING("CheckSameOrigin called on script w/o principals; should this happen?");
        return NS_OK;
    }

    PRBool equals = PR_FALSE;
    rv = sourcePrincipal->Equals(mSystemPrincipal, &equals);
    if (NS_SUCCEEDED(rv) && equals)
    {
        // This is a system (chrome) script, so allow access
        return NS_OK;
    }

    // Get the original URI from the source principal.
    // This has the effect of ignoring any change to document.domain
    // which must be done to avoid DNS spoofing (bug 154930)
    nsCOMPtr<nsIAggregatePrincipal> sourceAgg(do_QueryInterface(sourcePrincipal, &rv));
    NS_ENSURE_SUCCESS(rv, rv); // If it's not a system principal, it must be an aggregate
    nsCOMPtr<nsIPrincipal> sourceOriginal;
    rv = sourceAgg->GetOriginalCodebase(getter_AddRefs(sourceOriginal));
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsICodebasePrincipal> sourceCodebase(do_QueryInterface(sourcePrincipal, &rv));
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIURI> sourceURI;
    rv = sourceCodebase->GetURI(getter_AddRefs(sourceURI));
    NS_ENSURE_TRUE(sourceURI, NS_ERROR_FAILURE);

    // Compare origins
    PRBool sameOrigin = PR_FALSE;
    rv = SecurityCompareURIs(sourceURI, aTargetURI, &sameOrigin);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!sameOrigin)
    {
         ReportError(cx, NS_LITERAL_STRING("CheckSameOriginError"), sourceURI, aTargetURI);
         return NS_ERROR_DOM_BAD_URI;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::CheckSameOriginURI(nsIURI* aSourceURI,
                                            nsIURI* aTargetURI)
{
    nsresult rv;
    PRBool sameOrigin = PR_FALSE;
    rv = SecurityCompareURIs(aSourceURI, aTargetURI, &sameOrigin);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!sameOrigin)
    {
         ReportError(nsnull, NS_LITERAL_STRING("CheckSameOriginError"), 
                     aSourceURI, aTargetURI);
         return NS_ERROR_DOM_BAD_URI;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::CheckSameOriginPrincipal(nsIPrincipal* aSourcePrincipal,
                                                  nsIPrincipal* aTargetPrincipal)
{
    return CheckSameOriginDOMProp(aSourcePrincipal, aTargetPrincipal,
                                  nsIXPCSecurityManager::ACCESS_SET_PROPERTY);
}


nsresult
nsScriptSecurityManager::CheckPropertyAccessImpl(PRUint32 aAction,
                                                 nsIXPCNativeCallContext* aCallContext,
                                                 JSContext* cx, JSObject* aJSObject,
                                                 nsISupports* aObj, nsIURI* aTargetURI,
                                                 nsIClassInfo* aClassInfo,
                                                 const char* aClassName, jsval aProperty,
                                                 void** aCachedClassPolicy)
{
    nsCOMPtr<nsIPrincipal> subjectPrincipal;
    if (NS_FAILED(GetSubjectPrincipal(cx, getter_AddRefs(subjectPrincipal))))
        return NS_ERROR_FAILURE;

    PRBool equals;
    if (!subjectPrincipal ||
        NS_SUCCEEDED(subjectPrincipal->Equals(mSystemPrincipal, &equals)) && equals)
        // We have native code or the system principal: just allow access
        return NS_OK;

    nsresult rv;
    ClassNameHolder className(aClassName, aClassInfo);
#ifdef DEBUG_mstoltz
    nsCAutoString propertyName;
    propertyName.AssignWithConversion((PRUnichar*)JSValIDToString(cx, aProperty));
    printf("### CanAccess(%s.%s, %i) ", className.get(), 
           propertyName.get(), aAction);
#endif

    //-- Initialize policies if necessary
    if (mPolicyPrefsChanged)
    {
        rv = InitPolicies();
        if (NS_FAILED(rv))
            return rv;
    }

    //-- Look up the policy for this class
    ClassPolicy* cpolicy = aCachedClassPolicy ? 
                           NS_REINTERPRET_CAST(ClassPolicy*, *aCachedClassPolicy) : nsnull;
    if (!cpolicy)
    {
        //-- No cached policy for this class, need to look it up
#ifdef DEBUG_mstoltz
        printf("Miss! ");
#endif
    	rv = GetClassPolicy(subjectPrincipal, className.get(), &cpolicy);
        if (NS_FAILED(rv))
            return rv;
        if (aCachedClassPolicy)
            *aCachedClassPolicy = cpolicy;
    }

    SecurityLevel securityLevel = GetPropertyPolicy(aProperty, cpolicy, aAction);

    // If the class policy we have is a wildcard policy, then we may
    // still need to try the default for this class
    if (cpolicy != NO_POLICY_FOR_CLASS &&
        cpolicy->key[0] == '*' && cpolicy->key[1] == '\0' &&
        securityLevel.level == SCRIPT_SECURITY_UNDEFINED_ACCESS)
    {
        cpolicy = 
          NS_REINTERPRET_CAST(ClassPolicy*,
                              PL_DHashTableOperate(mDefaultPolicy,
                                                   className.get(),
                                                   PL_DHASH_LOOKUP));
        if (PL_DHASH_ENTRY_IS_LIVE(cpolicy))
            securityLevel = GetPropertyPolicy(aProperty, cpolicy, aAction);
    }

    // Hold the class info data here so we don't have to go back to virtual
    // methods all the time
    ClassInfoData classInfoData(aClassInfo);

    if (securityLevel.level == SCRIPT_SECURITY_UNDEFINED_ACCESS)
    {   
        // No policy found for this property so use the default of last resort.
        // If we were called from somewhere other than XPConnect
        // (no XPC call context), assume this is a DOM class. Otherwise,
        // ask the ClassInfo.
        if (!aCallContext || classInfoData.IsDOMClass())
            securityLevel.level = SCRIPT_SECURITY_SAME_ORIGIN_ACCESS;
        else
            securityLevel.level = SCRIPT_SECURITY_NO_ACCESS;
    }

    if (SECURITY_ACCESS_LEVEL_FLAG(securityLevel))
    // This flag means securityLevel is allAccess, noAccess, or sameOrigin
    {
        switch (securityLevel.level)
        {
        case SCRIPT_SECURITY_NO_ACCESS:
#ifdef DEBUG_mstoltz
            printf("noAccess ");
#endif
            rv = NS_ERROR_DOM_PROP_ACCESS_DENIED;
            break;

        case SCRIPT_SECURITY_ALL_ACCESS:
#ifdef DEBUG_mstoltz
            printf("allAccess ");
#endif
            rv = NS_OK;
            break;

        case SCRIPT_SECURITY_SAME_ORIGIN_ACCESS:
            {
#ifdef DEBUG_mstoltz
                printf("sameOrigin ");
#endif
                nsCOMPtr<nsIPrincipal> objectPrincipal;
                if(aJSObject)
                {
                    rv = doGetObjectPrincipal(cx,
                                              NS_REINTERPRET_CAST(JSObject*,
                                                                  aJSObject),
                                              getter_AddRefs(objectPrincipal));
                    if (NS_FAILED(rv))
                        return NS_ERROR_FAILURE;
                }
                else if(aTargetURI)
                {
                    if (NS_FAILED(GetCodebasePrincipal(
                          aTargetURI, getter_AddRefs(objectPrincipal))))
                        return NS_ERROR_FAILURE;
                }
                else
                {
                    NS_ERROR("CheckPropertyAccessImpl called without a target object or URL");
                    return NS_ERROR_FAILURE;
                }
                rv = CheckSameOriginDOMProp(subjectPrincipal, objectPrincipal, aAction);
                break;
            }
        default:
#ifdef DEBUG_mstoltz
                printf("ERROR ");
#endif
            NS_ERROR("Bad Security Level Value");
            return NS_ERROR_FAILURE;
        }
    }
    else // if SECURITY_ACCESS_LEVEL_FLAG is false, securityLevel is a capability
    {
#ifdef DEBUG_mstoltz
        printf("Cap:%s ", securityLevel.capability);
#endif
        PRBool capabilityEnabled = PR_FALSE;
        rv = IsCapabilityEnabled(securityLevel.capability, &capabilityEnabled);
        if (NS_FAILED(rv) || !capabilityEnabled)
            rv = NS_ERROR_DOM_SECURITY_ERR;
        else
            rv = NS_OK;
    }

    if (NS_SUCCEEDED(rv) && classInfoData.IsContentNode())
    {
        // No access to anonymous content from the web!  (bug 164086)
        nsCOMPtr<nsIContent> content(do_QueryInterface(aObj));
        NS_ASSERTION(content, "classinfo had CONTENT_NODE set but node did not"
                              "implement nsIContent!  Fasten your seat belt.");
        if (content->IsNativeAnonymous()) {
            rv = NS_ERROR_DOM_SECURITY_ERR;
        }
    }

    if (NS_SUCCEEDED(rv))
    {
#ifdef DEBUG_mstoltz
    printf(" GRANTED.\n");
#endif
        return rv;
    }

    //--See if the object advertises a non-default level of access
    //  using nsISecurityCheckedComponent
    nsCOMPtr<nsISecurityCheckedComponent> checkedComponent =
        do_QueryInterface(aObj);

    nsXPIDLCString objectSecurityLevel;
    if (checkedComponent)
    {
        nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
        nsCOMPtr<nsIInterfaceInfo> interfaceInfo;
        const nsIID* objIID;
        rv = aCallContext->GetCalleeWrapper(getter_AddRefs(wrapper));
        if (NS_SUCCEEDED(rv))
            rv = wrapper->FindInterfaceWithMember(aProperty, getter_AddRefs(interfaceInfo));
        if (NS_SUCCEEDED(rv))
            rv = interfaceInfo->GetIIDShared(&objIID);
        if (NS_SUCCEEDED(rv))
        {
            switch (aAction)
            {
            case nsIXPCSecurityManager::ACCESS_GET_PROPERTY:
                checkedComponent->CanGetProperty(objIID,
                                                 JSValIDToString(cx, aProperty),
                                                 getter_Copies(objectSecurityLevel));
                break;
            case nsIXPCSecurityManager::ACCESS_SET_PROPERTY:
                checkedComponent->CanSetProperty(objIID,
                                                 JSValIDToString(cx, aProperty),
                                                 getter_Copies(objectSecurityLevel));
                break;
            case nsIXPCSecurityManager::ACCESS_CALL_METHOD:
                checkedComponent->CanCallMethod(objIID,
                                                JSValIDToString(cx, aProperty),
                                                getter_Copies(objectSecurityLevel));
            }
        }
    }
    rv = CheckXPCPermissions(aObj, objectSecurityLevel);
#ifdef DEBUG_mstoltz
    if(NS_SUCCEEDED(rv))
        printf("CheckXPCPerms GRANTED.\n");
    else
        printf("CheckXPCPerms DENIED.\n");
#endif

    if (NS_FAILED(rv)) //-- Security tests failed, access is denied, report error
    {
        //XXX Clean up string usage here too
        nsCAutoString errorMsg("Permission denied to ");
        switch(aAction)
        {
        case nsIXPCSecurityManager::ACCESS_GET_PROPERTY:
            errorMsg += "get property ";
            break;
        case nsIXPCSecurityManager::ACCESS_SET_PROPERTY:
            errorMsg += "set property ";
            break;
        case nsIXPCSecurityManager::ACCESS_CALL_METHOD:
            errorMsg += "call method ";
        }
        errorMsg += className.get();
        errorMsg += '.';
        errorMsg.AppendWithConversion((PRUnichar*)JSValIDToString(cx, aProperty));

        JS_SetPendingException(cx,
                               STRING_TO_JSVAL(JS_NewStringCopyZ(cx, errorMsg.get())));
        nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID());
        if (xpc)
        {
            nsCOMPtr<nsIXPCNativeCallContext> xpcCallContext;
            xpc->GetCurrentNativeCallContext(getter_AddRefs(xpcCallContext));
            if (xpcCallContext)
                xpcCallContext->SetExceptionWasThrown(PR_TRUE);
        }
    }

    return rv;
}

nsresult
nsScriptSecurityManager::CheckSameOriginDOMProp(nsIPrincipal* aSubject,
                                                nsIPrincipal* aObject,
                                                PRUint32 aAction)
{
    nsresult rv;
    /*
    ** Get origin of subject and object and compare.
    */
    if (aSubject == aObject)
        return NS_OK;

    PRBool isSameOrigin = PR_FALSE;
    rv = aSubject->Equals(aObject, &isSameOrigin);
    NS_ENSURE_SUCCESS(rv, rv);

    if (isSameOrigin)
    {   // If either the subject or the object has changed its principal by
        // explicitly setting document.domain then the other must also have
        // done so in order to be considered the same origin. This prevents
        // DNS spoofing based on document.domain (154930)
        nsCOMPtr<nsIAggregatePrincipal> subjectAgg(do_QueryInterface(aSubject, &rv));
        NS_ENSURE_SUCCESS(rv, rv);
        PRBool subjectSetDomain = PR_FALSE;
        subjectAgg->WasCodebaseChanged(&subjectSetDomain);

        nsCOMPtr<nsIAggregatePrincipal> objectAgg(do_QueryInterface(aObject, &rv));
        NS_ENSURE_SUCCESS(rv, rv);
        PRBool objectSetDomain = PR_FALSE;
        objectAgg->WasCodebaseChanged(&objectSetDomain);

        // If both or neither explicitly set their domain, allow the access
        if (!(subjectSetDomain || objectSetDomain) ||
            (subjectSetDomain && objectSetDomain))
            return NS_OK;
    }


    // Allow access to about:blank
    nsCOMPtr<nsICodebasePrincipal> objectCodebase(do_QueryInterface(aObject));
    if (objectCodebase)
    {
        nsXPIDLCString origin;
        rv = objectCodebase->GetOrigin(getter_Copies(origin));
        NS_ENSURE_SUCCESS(rv, rv);
        if (nsCRT::strcasecmp(origin, "about:blank") == 0)
            return NS_OK;
    }

    /*
    * If we failed the origin tests it still might be the case that we
    * are a signed script and have permissions to do this operation.
    * Check for that here.
    */
    PRBool capabilityEnabled = PR_FALSE;
    const char* cap = aAction == nsIXPCSecurityManager::ACCESS_SET_PROPERTY ?
                      "UniversalBrowserWrite" : "UniversalBrowserRead";
    rv = IsCapabilityEnabled(cap, &capabilityEnabled);
    NS_ENSURE_SUCCESS(rv, rv);
    if (capabilityEnabled)
        return NS_OK;

    /*
    ** Access tests failed, so now report error.
    */
    return NS_ERROR_DOM_PROP_ACCESS_DENIED;
}

nsresult
nsScriptSecurityManager::GetClassPolicy(nsIPrincipal* principal,
                                        const char* aClassName,
                                        ClassPolicy** result)
{
    nsresult rv;
    *result = nsnull;
    DomainPolicy* dpolicy = nsnull;
    if (mOriginToPolicyMap)
    {   //-- Look up the relevant domain policy, if any
        nsCOMPtr<nsICodebasePrincipal> codebase(do_QueryInterface(principal));
        if (!codebase)
            return NS_ERROR_FAILURE;

        nsXPIDLCString origin;
        if (NS_FAILED(rv = codebase->GetOrigin(getter_Copies(origin))))
            return rv;
 
        const char *start = origin;
        const char *nextToLastDot = nsnull;
        const char *lastDot = nsnull;
        const char *colon = nsnull;
        const char *p = start;
        while (*p)
        {
            if (*p == '.')
            {
                nextToLastDot = lastDot;
                lastDot = p;
            }
            if (!colon && *p == ':')
                colon = p;
            p++;
        }

        nsCStringKey key(nextToLastDot ? nextToLastDot+1 : start);
        DomainEntry *de = (DomainEntry*) mOriginToPolicyMap->Get(&key);
        if (!de)
        {
            nsCAutoString scheme(start, colon-start+1);
            nsCStringKey schemeKey(scheme);
            de = (DomainEntry*) mOriginToPolicyMap->Get(&schemeKey);
        }

        while (de)
        {
            if (de->Matches(start))
            {
                dpolicy = de->mDomainPolicy;
                break;
            }
            de = de->mNext;
        }
    }

    ClassPolicy* wildcardPolicy = nsnull;
    if (dpolicy)
    {
        //-- Now get the class policy
        *result = 
          NS_REINTERPRET_CAST(ClassPolicy*,
            PL_DHashTableOperate(dpolicy,
                                 aClassName,
                                 PL_DHASH_LOOKUP));

        //-- and the wildcard policy (class "*" for this domain)
        wildcardPolicy = 
          NS_REINTERPRET_CAST(ClassPolicy*,
            PL_DHashTableOperate(dpolicy,
                                 "*",
                                 PL_DHASH_LOOKUP));
    }

    //-- and the default policy for this class
    ClassPolicy* defaultClassPolicy = 
          NS_REINTERPRET_CAST(ClassPolicy*,
            PL_DHashTableOperate(mDefaultPolicy,
                                 aClassName,
                                 PL_DHASH_LOOKUP));

    if (*result && PL_DHASH_ENTRY_IS_LIVE(*result))
    {
        if (PL_DHASH_ENTRY_IS_LIVE(wildcardPolicy))
            (*result)->mWildcard = wildcardPolicy;
        if (PL_DHASH_ENTRY_IS_LIVE(defaultClassPolicy))
            (*result)->mDefault  = defaultClassPolicy;
    }
    else
    {
        if (wildcardPolicy && PL_DHASH_ENTRY_IS_LIVE(wildcardPolicy))
            *result = wildcardPolicy;
        else if (PL_DHASH_ENTRY_IS_LIVE(defaultClassPolicy))
            *result = defaultClassPolicy;
        else
            *result = NO_POLICY_FOR_CLASS;
    }

    return NS_OK;
}

SecurityLevel
nsScriptSecurityManager::GetPropertyPolicy(jsval aProperty, ClassPolicy* aClassPolicy,
                                           PRUint32 aAction)
{
    //-- Look up the policy for this property/method
    PropertyPolicy* ppolicy = nsnull;
    if (aClassPolicy && aClassPolicy != NO_POLICY_FOR_CLASS)
    {
        ppolicy = 
          (PropertyPolicy*) PL_DHashTableOperate(aClassPolicy->mPolicy,
                                                 NS_REINTERPRET_CAST(void*, aProperty),
                                                 PL_DHASH_LOOKUP);
        if (!PL_DHASH_ENTRY_IS_LIVE(ppolicy))
        {   // No domain policy for this property, look for a wildcard policy
            if (aClassPolicy->mWildcard)
            {
                ppolicy = NS_REINTERPRET_CAST(PropertyPolicy*,
                  PL_DHashTableOperate(aClassPolicy->mWildcard->mPolicy,
                                       NS_REINTERPRET_CAST(void*, aProperty),
                                       PL_DHASH_LOOKUP));
            }
            if (!PL_DHASH_ENTRY_IS_LIVE(ppolicy) && aClassPolicy->mDefault)
            {   // Now look for a default policy
                ppolicy = NS_REINTERPRET_CAST(PropertyPolicy*,
                  PL_DHashTableOperate(aClassPolicy->mDefault->mPolicy,
                                       NS_REINTERPRET_CAST(void*, aProperty),
                                       PL_DHASH_LOOKUP));
            }
        }
        if (PL_DHASH_ENTRY_IS_LIVE(ppolicy))
        {
            // Get the correct security level from the property policy
            if (aAction == nsIXPCSecurityManager::ACCESS_SET_PROPERTY)
                return ppolicy->mSet;
            return ppolicy->mGet;
        }
    }

    SecurityLevel nopolicy;
    nopolicy.level = SCRIPT_SECURITY_UNDEFINED_ACCESS;
    return nopolicy;
}


NS_IMETHODIMP
nsScriptSecurityManager::CheckLoadURIFromScript(JSContext *cx, nsIURI *aURI)
{
    // Get principal of currently executing script.
    nsCOMPtr<nsIPrincipal> principal;
    if (NS_FAILED(GetSubjectPrincipal(cx, getter_AddRefs(principal))))
        return NS_ERROR_FAILURE;

    // Native code can load all URIs.
    if (!principal)
        return NS_OK;

    // The system principal can load all URIs.
    PRBool equals = PR_FALSE;
    if (NS_FAILED(principal->Equals(mSystemPrincipal, &equals)))
        return NS_ERROR_FAILURE;
    if (equals)
        return NS_OK;

    // Otherwise, principal should have a codebase that we can use to
    // do the remaining tests.
    nsCOMPtr<nsICodebasePrincipal> codebase(do_QueryInterface(principal));
    if (!codebase)
        return NS_ERROR_FAILURE;
    nsCOMPtr<nsIURI> uri;
    if (NS_FAILED(codebase->GetURI(getter_AddRefs(uri))))
        return NS_ERROR_FAILURE;
    if (NS_SUCCEEDED(CheckLoadURI(uri, aURI, nsIScriptSecurityManager::STANDARD )))
        return NS_OK;

    // See if we're attempting to load a file: URI. If so, let a
    // UniversalFileRead capability trump the above check.
    PRBool isFile = PR_FALSE;
    PRBool isRes = PR_FALSE;
    if (NS_FAILED(aURI->SchemeIs("file", &isFile)) ||
        NS_FAILED(aURI->SchemeIs("resource", &isRes)))
        return NS_ERROR_FAILURE;
    if (isFile || isRes)
    {
        PRBool enabled;
        if (NS_FAILED(IsCapabilityEnabled("UniversalFileRead", &enabled)))
            return NS_ERROR_FAILURE;
        if (enabled)
            return NS_OK;
    }

    // Report error.
    nsCAutoString spec;
    if (NS_FAILED(aURI->GetAsciiSpec(spec)))
        return NS_ERROR_FAILURE;
    JS_ReportError(cx, "Access to '%s' from script denied", spec.get());
    return NS_ERROR_DOM_BAD_URI;
}

nsresult
nsScriptSecurityManager::GetBaseURIScheme(nsIURI* aURI, char** aScheme)
{
    if (!aURI)
       return NS_ERROR_FAILURE;

    nsresult rv;
    nsCOMPtr<nsIURI> uri(aURI);

    //-- get the source scheme
    nsCAutoString scheme;
    rv = uri->GetScheme(scheme);
    if (NS_FAILED(rv)) return rv;

    //-- If uri is a view-source URI, drill down to the base URI
    nsCAutoString path;
    while(PL_strcmp(scheme.get(), "view-source") == 0)
    {
        rv = uri->GetPath(path);
        if (NS_FAILED(rv)) return rv;
        rv = NS_NewURI(getter_AddRefs(uri), path, nsnull);
        if (NS_FAILED(rv)) return rv;
        rv = uri->GetScheme(scheme);
        if (NS_FAILED(rv)) return rv;
    }

    //-- If uri is a jar URI, drill down again
    nsCOMPtr<nsIJARURI> jarURI;
    PRBool isJAR = PR_FALSE;
    while((jarURI = do_QueryInterface(uri)))
    {
        jarURI->GetJARFile(getter_AddRefs(uri));
        isJAR = PR_TRUE;
    }
    if (!uri) return NS_ERROR_FAILURE;
    if (isJAR)
    {
        rv = uri->GetScheme(scheme);
        if (NS_FAILED(rv)) return rv;
    }

    //-- if uri is an about uri, distinguish 'safe' and 'unsafe' about URIs
    static const char aboutScheme[] = "about";
    if(nsCRT::strcasecmp(scheme.get(), aboutScheme) == 0)
    {
        nsCAutoString spec;
        if(NS_FAILED(uri->GetAsciiSpec(spec)))
            return NS_ERROR_FAILURE;
        const char* page = spec.get() + sizeof(aboutScheme);
        if ((strcmp(page, "blank") == 0)   ||
            (strcmp(page, "") == 0)        ||
            (strcmp(page, "mozilla") == 0) ||
            (strcmp(page, "credits") == 0))
        {
            *aScheme = nsCRT::strdup("about safe");
            return *aScheme ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
        }
    }

    *aScheme = nsCRT::strdup(scheme.get());
    return *aScheme ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsScriptSecurityManager::CheckLoadURI(nsIURI *aSourceURI, nsIURI *aTargetURI,
                                      PRUint32 aFlags)
{
    nsresult rv;
    //-- get the source scheme
    nsXPIDLCString sourceScheme;
    rv = GetBaseURIScheme(aSourceURI, getter_Copies(sourceScheme));
    if (NS_FAILED(rv)) return rv;

    // Some loads are not allowed from mail/news messages
    if ((aFlags & nsIScriptSecurityManager::DISALLOW_FROM_MAIL) &&
        (nsCRT::strcasecmp(sourceScheme, "mailbox")  == 0 ||
         nsCRT::strcasecmp(sourceScheme, "imap")     == 0 ||
         nsCRT::strcasecmp(sourceScheme, "news")     == 0))
    {
        return NS_ERROR_DOM_BAD_URI;
    }

    //-- get the target scheme
    nsXPIDLCString targetScheme;
    rv = GetBaseURIScheme(aTargetURI, getter_Copies(targetScheme));
    if (NS_FAILED(rv)) return rv;

    if (nsCRT::strcasecmp(targetScheme, sourceScheme) == 0)
    {
        // every scheme can access another URI from the same scheme
        return NS_OK;
    }

    //-- If the schemes don't match, the policy is specified in this table.
    enum Action { AllowProtocol, DenyProtocol, PrefControlled, ChromeProtocol };
    static const struct
    {
        const char *name;
        Action action;
    } protocolList[] =
    {
        //-- Keep the most commonly used protocols at the top of the list
        //   to increase performance
        { "http",            AllowProtocol  },
        { "chrome",          ChromeProtocol },
        { "file",            PrefControlled },
        { "https",           AllowProtocol  },
        { "mailbox",         DenyProtocol   },
        { "pop",             AllowProtocol  },
        { "imap",            DenyProtocol   },
        { "pop3",            DenyProtocol   },
        { "news",            AllowProtocol  },
        { "javascript",      AllowProtocol  },
        { "ftp",             AllowProtocol  },
        { "about safe",      AllowProtocol  },
        { "about",           DenyProtocol   },
        { "mailto",          AllowProtocol  },
        { "aim",             AllowProtocol  },
        { "data",            AllowProtocol  },
        { "keyword",         DenyProtocol   },
        { "resource",        ChromeProtocol },
        { "gopher",          AllowProtocol  },
        { "datetime",        DenyProtocol   },
        { "finger",          AllowProtocol  },
        { "res",             DenyProtocol   }
    };

    NS_NAMED_LITERAL_STRING(errorTag, "CheckLoadURIError");
    for (unsigned i=0; i < sizeof(protocolList)/sizeof(protocolList[0]); i++)
    {
        if (nsCRT::strcasecmp(targetScheme, protocolList[i].name) == 0)
        {
            PRBool doCheck = PR_FALSE;
            switch (protocolList[i].action)
            {
            case AllowProtocol:
                // everyone can access these schemes.
                return NS_OK;
            case PrefControlled:
                // Allow access if pref is false
                {
                    mSecurityPref->SecurityGetBoolPref("security.checkloaduri", &doCheck);
                    if (doCheck)
                    {
                        // resource: and chrome: are equivalent, securitywise
                        if ((PL_strcmp(sourceScheme, "chrome") == 0) ||
                            (PL_strcmp(sourceScheme, "resource") == 0))
                            return NS_OK;

                        ReportError(nsnull, errorTag, aSourceURI, aTargetURI);
                        return NS_ERROR_DOM_BAD_URI;
                    }
                    return NS_OK;
                }
            case ChromeProtocol:
                if (aFlags & nsIScriptSecurityManager::ALLOW_CHROME)
                    return NS_OK;
                // resource: and chrome: are equivalent, securitywise
                if ((PL_strcmp(sourceScheme, "chrome") == 0) ||
                    (PL_strcmp(sourceScheme, "resource") == 0))
                    return NS_OK;
                ReportError(nsnull, errorTag, aSourceURI, aTargetURI);
                return NS_ERROR_DOM_BAD_URI;
            case DenyProtocol:
                // Deny access
                ReportError(nsnull, errorTag, aSourceURI, aTargetURI);
                return NS_ERROR_DOM_BAD_URI;
            }
        }
    }

    // If we reach here, we have an unknown protocol. Warn, but allow.
    // This is risky from a security standpoint, but allows flexibility
    // in installing new protocol handlers after initial ship.
    NS_WARNING("unknown protocol in nsScriptSecurityManager::CheckLoadURI");

    return NS_OK;
}

#define PROPERTIES_URL "chrome://communicator/locale/security/caps.properties"

nsresult
nsScriptSecurityManager::ReportError(JSContext* cx, const nsAString& messageTag,
                                     nsIURI* aSource, nsIURI* aTarget)
{
    nsresult rv;
    NS_ENSURE_TRUE(aSource && aTarget, NS_ERROR_NULL_POINTER);

    // First, create the error message text
    // create a bundle for the localization
    nsCOMPtr<nsIStringBundleService> bundleService(do_GetService(kStringBundleServiceCID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIStringBundle> bundle;
    rv = bundleService->CreateBundle(PROPERTIES_URL, getter_AddRefs(bundle));
    NS_ENSURE_SUCCESS(rv, rv);

    // Get the source URL spec
    nsCAutoString sourceSpec;
    rv = aSource->GetAsciiSpec(sourceSpec);
    NS_ENSURE_SUCCESS(rv, rv);

    // Get the target URL spec
    nsCAutoString targetSpec;
    rv = aTarget->GetAsciiSpec(targetSpec);
    NS_ENSURE_SUCCESS(rv, rv);

    // Localize the error message
    nsXPIDLString message;
    NS_ConvertASCIItoUCS2 ucsSourceSpec(sourceSpec);
    NS_ConvertASCIItoUCS2 ucsTargetSpec(targetSpec);
    const PRUnichar *formatStrings[] = { ucsSourceSpec.get(), ucsTargetSpec.get() };
    rv = bundle->FormatStringFromName(PromiseFlatString(messageTag).get(),
                                      formatStrings,
                                      2,
                                      getter_Copies(message));
    NS_ENSURE_SUCCESS(rv, rv);

    // If a JS context was passed in, set a JS exception.
    // Otherwise, print the error message directly to the JS console
    // and to standard output
    if (cx)
    {
        JS_SetPendingException(cx,
            STRING_TO_JSVAL(JS_NewUCStringCopyZ(cx,
                NS_REINTERPRET_CAST(const jschar*, message.get()))));
        // Tell XPConnect that an exception was thrown, if appropriate
        nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID());
        if (xpc)
        {
            nsCOMPtr<nsIXPCNativeCallContext> xpcCallContext;
            xpc->GetCurrentNativeCallContext(getter_AddRefs(xpcCallContext));
             if (xpcCallContext)
                xpcCallContext->SetExceptionWasThrown(PR_TRUE);
        }
    }
    else // Print directly to the console
    {
        nsCOMPtr<nsIConsoleService> console(
            do_GetService("@mozilla.org/consoleservice;1"));
        NS_ENSURE_TRUE(console, NS_ERROR_FAILURE);

        console->LogStringMessage(message.get());
#ifdef DEBUG
        fprintf(stderr, "%s\n", NS_LossyConvertUCS2toASCII(message).get());
#endif
    }
    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::CheckLoadURIStr(const char* aSourceURIStr, const char* aTargetURIStr,
                                         PRUint32 aFlags)
{
    nsCOMPtr<nsIURI> source;
    nsresult rv = NS_NewURI(getter_AddRefs(source), nsDependentCString(aSourceURIStr), nsnull);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIURI> target;
    rv = NS_NewURI(getter_AddRefs(target), nsDependentCString(aTargetURIStr), nsnull);
    NS_ENSURE_SUCCESS(rv, rv);
    return CheckLoadURI(source, target, aFlags);
}

NS_IMETHODIMP
nsScriptSecurityManager::CheckFunctionAccess(JSContext *aCx, void *aFunObj,
                                             void *aTargetObj)
{
    //-- This check is called for event handlers
    nsCOMPtr<nsIPrincipal> subject;
    nsresult rv = GetFunctionObjectPrincipal(aCx, (JSObject *)aFunObj,
                                             getter_AddRefs(subject));
    //-- If subject is null, get a principal from the function object's scope.
    if (NS_SUCCEEDED(rv) && !subject)
        rv = doGetObjectPrincipal(aCx, (JSObject*)aFunObj,
                                  getter_AddRefs(subject));

    if (NS_FAILED(rv)) return rv;
    if (!subject) return NS_ERROR_FAILURE;


    PRBool isSystem;
    if (NS_SUCCEEDED(subject->Equals(mSystemPrincipal, &isSystem)) && isSystem)
        // This is the system principal: just allow access
        return NS_OK;

    // Check if the principal the function was compiled under is
    // allowed to execute scripts.

    PRBool result;
    rv = CanExecuteScripts(aCx, subject, &result);
    if (NS_FAILED(rv))
      return rv;

    if (!result)
      return NS_ERROR_DOM_SECURITY_ERR;

    /*
    ** Get origin of subject and object and compare.
    */
    JSObject* obj = (JSObject*)aTargetObj;
    nsCOMPtr<nsIPrincipal> object;
    if (NS_FAILED(doGetObjectPrincipal(aCx, obj, getter_AddRefs(object))))
        return NS_ERROR_FAILURE;
    if (subject == object)
        return NS_OK;

    PRBool isSameOrigin = PR_FALSE;
    if (NS_FAILED(subject->Equals(object, &isSameOrigin)))
        return NS_ERROR_FAILURE;

    if (isSameOrigin)
        return NS_OK;

    // Allow access to about:blank
    nsCOMPtr<nsICodebasePrincipal> objectCodebase(do_QueryInterface(object));
    if (objectCodebase)
    {
        nsXPIDLCString origin;
        if (NS_FAILED(objectCodebase->GetOrigin(getter_Copies(origin))))
            return NS_ERROR_FAILURE;
        if (nsCRT::strcasecmp(origin, "about:blank") == 0)
            return NS_OK;
    }

    /*
    ** Access tests failed.  Fail silently without a JS exception.
    */
    return NS_ERROR_DOM_SECURITY_ERR;
}

nsresult
nsScriptSecurityManager::GetRootDocShell(JSContext *cx, nsIDocShell **result)
{
    nsresult rv;
    *result = nsnull;
    nsCOMPtr<nsIDocShell> docshell;
    nsCOMPtr<nsIScriptContext> scriptContext = (nsIScriptContext*)JS_GetContextPrivate(cx);
    if (!scriptContext) return NS_ERROR_FAILURE;
    nsCOMPtr<nsIScriptGlobalObject> globalObject;
    scriptContext->GetGlobalObject(getter_AddRefs(globalObject));
    if (!globalObject)  return NS_ERROR_FAILURE;
    rv = globalObject->GetDocShell(getter_AddRefs(docshell));
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIDocShellTreeItem> docshellTreeItem(do_QueryInterface(docshell, &rv));
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIDocShellTreeItem> rootItem;
    rv = docshellTreeItem->GetRootTreeItem(getter_AddRefs(rootItem));
    if (NS_FAILED(rv)) return rv;

    return rootItem->QueryInterface(NS_GET_IID(nsIDocShell), (void**)result);
}

NS_IMETHODIMP
nsScriptSecurityManager::CanExecuteScripts(JSContext* cx,
                                           nsIPrincipal *aPrincipal,
                                           PRBool *result)
{
    *result = PR_FALSE; 

    if (aPrincipal == mSystemPrincipal)
    {
        // Even if JavaScript is disabled, we must still execute system scripts
        *result = PR_TRUE;
        return NS_OK;
    }

    //-- Always allow chrome pages to run scripts
    //   This is for about: URLs, which are chrome but don't
    //   have the system principal
    nsresult rv;
    if (!mIsJavaScriptEnabled)
    {
        nsCOMPtr<nsICodebasePrincipal> codebase(do_QueryInterface(aPrincipal));
        if (codebase)
        {
            nsXPIDLCString origin;
            rv = codebase->GetOrigin(getter_Copies(origin));
            static const char chromePrefix[] = "chrome:";
            if (NS_SUCCEEDED(rv) &&
                (PL_strncmp(origin, chromePrefix, sizeof(chromePrefix)-1) == 0))
            {
                *result = PR_TRUE;
                return NS_OK;
            }
        }
    }

    //-- See if the current window allows JS execution
    nsCOMPtr<nsIScriptContext> scriptContext = (nsIScriptContext*)JS_GetContextPrivate(cx);
    if (!scriptContext) return NS_ERROR_FAILURE;
    nsCOMPtr<nsIScriptGlobalObject> globalObject;
    scriptContext->GetGlobalObject(getter_AddRefs(globalObject));
    if (!globalObject) return NS_ERROR_FAILURE;
    
    nsCOMPtr<nsIDocShell> docshell;
    globalObject->GetDocShell(getter_AddRefs(docshell));
    nsCOMPtr<nsIDocShellTreeItem> treeItem;
    if (docshell) 
    {
        treeItem = do_QueryInterface(docshell);
        nsCOMPtr<nsIDocShellTreeItem> parentItem;
        // Walk up the docshell tree to see if any containing docshell disallows scripts
        do 
        {
            rv = docshell->GetAllowJavascript(result);
            if (NS_FAILED(rv)) return rv;
            if (!*result)
                return NS_OK; // Do not run scripts
            if (treeItem) 
            {
                treeItem->GetParent(getter_AddRefs(parentItem));
                if (parentItem)
                {
                    treeItem = parentItem;
                    docshell = do_QueryInterface(treeItem, &rv);
                    NS_ASSERTION(docshell, "cannot get a docshell from a treeItem!");
                    if (NS_FAILED(rv)) break;
                }
            }
        } while (parentItem);
    }

    //-- See if JS is disabled globally (via prefs)
    *result = mIsJavaScriptEnabled;
    if (mIsJavaScriptEnabled != mIsMailJavaScriptEnabled) 
    {
        // Get docshell from the global window again.
        globalObject->GetDocShell(getter_AddRefs(docshell));
        treeItem = do_QueryInterface(docshell);
        if (treeItem) 
        {
            nsCOMPtr<nsIDocShellTreeItem> rootItem;
            treeItem->GetRootTreeItem(getter_AddRefs(rootItem));
            docshell = do_QueryInterface(rootItem);
            if (docshell) 
            {
                // Is this script running from mail?
                PRUint32 appType;
                rv = docshell->GetAppType(&appType);
                if (NS_FAILED(rv)) return rv;
                if (appType == nsIDocShell::APP_TYPE_MAIL) 
                {
                    *result = mIsMailJavaScriptEnabled;
                }
            }
        }
    }
    
    if (!*result)
        return NS_OK; // Do not run scripts

    //-- Check for a per-site policy
    static const char jsPrefGroupName[] = "javascript";

    //-- Initialize policies if necessary
    if (mPolicyPrefsChanged)
    {
        rv = InitPolicies();
        if (NS_FAILED(rv))
            return rv;
    }

    ClassPolicy* cpolicy;
    SecurityLevel secLevel;
    rv = GetClassPolicy(aPrincipal, jsPrefGroupName, &cpolicy);

    if (NS_SUCCEEDED(rv))
    {
        secLevel = GetPropertyPolicy(sEnabledID, cpolicy,
                                     nsIXPCSecurityManager::ACCESS_GET_PROPERTY);
    }
    if (NS_FAILED(rv) || secLevel.level == SCRIPT_SECURITY_NO_ACCESS)
    {
        *result = PR_FALSE;
        return rv;
    }

    //-- Nobody vetoed, so allow the JS to run.
    *result = PR_TRUE;
    return NS_OK;
}

///////////////// Principals ///////////////////////
NS_IMETHODIMP
nsScriptSecurityManager::GetSubjectPrincipal(nsIPrincipal **result)
{
    JSContext *cx = GetCurrentJSContext();
    if (!cx)
    {
        *result = nsnull;
        return NS_OK;
    }
    return GetSubjectPrincipal(cx, result);
}

NS_IMETHODIMP
nsScriptSecurityManager::GetSystemPrincipal(nsIPrincipal **result)
{
    if (!mSystemPrincipal)
    {
        mSystemPrincipal = new nsSystemPrincipal();
        if (!mSystemPrincipal)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(mSystemPrincipal);
    }
    *result = mSystemPrincipal;
    NS_ADDREF(*result);
    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::SubjectPrincipalIsSystem(PRBool* aIsSystem)
{
    NS_ENSURE_ARG_POINTER(aIsSystem);
    *aIsSystem = PR_FALSE;

    if (!mSystemPrincipal)
        return NS_OK;

    nsCOMPtr<nsIPrincipal> subject;
    nsresult rv = GetSubjectPrincipal(getter_AddRefs(subject));
    if (NS_FAILED(rv))
        return rv;

    if(!subject)
    {
        // No subject principal means no JS is running;
        // this is the equivalent of system principal code
        *aIsSystem = PR_TRUE;
        return NS_OK;
    }

    return mSystemPrincipal->Equals(subject, aIsSystem);
}

NS_IMETHODIMP
nsScriptSecurityManager::GetCertificatePrincipal(const char* aCertID,
                                                 nsIPrincipal **result)
{
    nsresult rv;
    //-- Create a certificate principal
    nsCertificatePrincipal *certificate = new nsCertificatePrincipal();
    if (!certificate)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(certificate);
    if (NS_FAILED(certificate->Init(aCertID)))
    {
        NS_RELEASE(certificate);
        return NS_ERROR_FAILURE;
    }
    nsCOMPtr<nsIPrincipal> principal(do_QueryInterface((nsBasePrincipal*)certificate, &rv));
    NS_RELEASE(certificate);
    if (NS_FAILED(rv)) return rv;

    if (mPrincipals)
    {
        // Check to see if we already have this principal.
        nsIPrincipalKey key(principal);
        nsCOMPtr<nsIPrincipal> fromTable = (nsIPrincipal *) mPrincipals->Get(&key);
        if (fromTable)
            principal = fromTable;
    }

    //-- Bundle this certificate principal into an aggregate principal
    nsAggregatePrincipal* agg = new nsAggregatePrincipal();
    if (!agg) return NS_ERROR_OUT_OF_MEMORY;
    rv = agg->SetCertificate(principal);
    if (NS_FAILED(rv)) return rv;
    principal = do_QueryInterface((nsBasePrincipal*)agg, &rv);
    if (NS_FAILED(rv)) return rv;

    *result = principal;
    NS_ADDREF(*result);
    return NS_OK;
}

nsresult
nsScriptSecurityManager::CreateCodebasePrincipal(nsIURI* aURI, nsIPrincipal **result)
{
    nsresult rv = NS_OK;
    nsCodebasePrincipal *codebase = new nsCodebasePrincipal();
    if (!codebase)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(codebase);
    if (NS_FAILED(codebase->Init(aURI)))
    {
        NS_RELEASE(codebase);
        return NS_ERROR_FAILURE;
    }
    rv = CallQueryInterface((nsBasePrincipal*)codebase, result);
    NS_RELEASE(codebase);
    return rv;
}

NS_IMETHODIMP
nsScriptSecurityManager::GetCodebasePrincipal(nsIURI *aURI,
                                              nsIPrincipal **result)
{
    nsresult rv;
    nsCOMPtr<nsIPrincipal> principal;
    rv = CreateCodebasePrincipal(aURI, getter_AddRefs(principal));
    if (NS_FAILED(rv)) return rv;

    if (mPrincipals)
    {
        //-- Check to see if we already have this principal.
        nsIPrincipalKey key(principal);
        nsCOMPtr<nsIPrincipal> fromTable = (nsIPrincipal *) mPrincipals->Get(&key);
        if (fromTable)
            principal = fromTable;
        else //-- Check to see if we have a more general principal
        {
            nsCOMPtr<nsICodebasePrincipal> codebasePrin(do_QueryInterface(principal));
            nsXPIDLCString originUrl;
            rv = codebasePrin->GetOrigin(getter_Copies(originUrl));
            if (NS_FAILED(rv)) return rv;
            nsCOMPtr<nsIURI> newURI;
            rv = NS_NewURI(getter_AddRefs(newURI), originUrl, nsnull);
            if (NS_FAILED(rv)) return rv;
            nsCOMPtr<nsIPrincipal> principal2;
            rv = CreateCodebasePrincipal(newURI, getter_AddRefs(principal2));
            if (NS_FAILED(rv)) return rv;
             nsIPrincipalKey key2(principal2);
                fromTable = (nsIPrincipal *) mPrincipals->Get(&key2);
            if (fromTable)
                principal = fromTable;
        }		
    }

    //-- Bundle this codebase principal into an aggregate principal
    nsAggregatePrincipal* agg = new nsAggregatePrincipal();
    if (!agg) return NS_ERROR_OUT_OF_MEMORY;
    rv = agg->SetCodebase(principal);
    if (NS_FAILED(rv)) return rv;
    principal = do_QueryInterface((nsBasePrincipal*)agg, &rv);
    if (NS_FAILED(rv)) return rv;

    *result = principal;
    NS_ADDREF(*result);
    return NS_OK;
}

nsresult
nsScriptSecurityManager::GetPrincipalFromContext(JSContext *cx,
                                                 nsIPrincipal **result)
{
    *result = nsnull;
    NS_ENSURE_TRUE(::JS_GetOptions(cx) & JSOPTION_PRIVATE_IS_NSISUPPORTS,
                   NS_ERROR_FAILURE);
    nsISupports* scriptContextSupports =
        NS_REINTERPRET_CAST(nsISupports*, JS_GetContextPrivate(cx));
    nsCOMPtr<nsIScriptContext> scriptContext(do_QueryInterface(scriptContextSupports));

    if (scriptContext)
    {
        nsCOMPtr<nsIScriptGlobalObject> global;
        scriptContext->GetGlobalObject(getter_AddRefs(global));
        nsCOMPtr<nsIScriptObjectPrincipal> globalData(do_QueryInterface(global));
        if (globalData)
            globalData->GetPrincipal(result);
    }
    return NS_OK;
}

nsresult
nsScriptSecurityManager::GetScriptPrincipal(JSContext *cx,
                                            JSScript *script,
                                            nsIPrincipal **result)
{
    if (!script)
    {
        *result = nsnull;
        return NS_OK;
    }
    JSPrincipals *jsp = JS_GetScriptPrincipals(cx, script);
    if (!jsp) {
        // Script didn't have principals -- shouldn't happen.
        return NS_ERROR_FAILURE;
    }
    nsJSPrincipals *nsJSPrin = NS_STATIC_CAST(nsJSPrincipals *, jsp);
    *result = nsJSPrin->nsIPrincipalPtr;
    if (!*result)
        return NS_ERROR_FAILURE;
    NS_ADDREF(*result);
    return NS_OK;

}

nsresult
nsScriptSecurityManager::GetFunctionObjectPrincipal(JSContext *cx,
                                                    JSObject *obj,
                                                    nsIPrincipal **result)
{
    JSFunction *fun = (JSFunction *) JS_GetPrivate(cx, obj);
    JSScript *script = JS_GetFunctionScript(cx, fun);

    nsCOMPtr<nsIPrincipal> scriptPrincipal;
    if (script)
        if (NS_FAILED(GetScriptPrincipal(cx, script, getter_AddRefs(scriptPrincipal))))
            return NS_ERROR_FAILURE;

    if (script && (JS_GetFunctionObject(fun) != obj) &&
        (scriptPrincipal.get() == mSystemPrincipal))
    {
        // Function is brutally-shared chrome. For this case only,
        // get a principal from the object's scope instead of the
        // principal compiled into the function.
        return doGetObjectPrincipal(cx, obj, result);
    }

    *result = scriptPrincipal.get();
    NS_IF_ADDREF(*result);
    return NS_OK;
}

nsresult
nsScriptSecurityManager::GetFramePrincipal(JSContext *cx,
                                           JSStackFrame *fp,
                                           nsIPrincipal **result)
{
    JSObject *obj = JS_GetFrameFunctionObject(cx, fp);
    if (!obj)
    {
        // Must be in a top-level script. Get principal from the script.
        JSScript *script = JS_GetFrameScript(cx, fp);
        return GetScriptPrincipal(cx, script, result);
    }
    return GetFunctionObjectPrincipal(cx, obj, result);
}

nsresult
nsScriptSecurityManager::GetPrincipalAndFrame(JSContext *cx,
                                              nsIPrincipal **result,
                                              JSStackFrame **frameResult)
{
    // Get principals from innermost frame of JavaScript or Java.
    JSStackFrame *fp = nsnull; // tell JS_FrameIterator to start at innermost
    for (fp = JS_FrameIterator(cx, &fp); fp; fp = JS_FrameIterator(cx, &fp))
    {
        if (NS_FAILED(GetFramePrincipal(cx, fp, result)))
            return NS_ERROR_FAILURE;
        if (*result)
        {
            *frameResult = fp;
            return NS_OK;
        }
    }

    //-- If there's no principal on the stack, look at the global object
    //   and return the innermost frame for annotations.
    if (cx)
    {
        nsCOMPtr<nsIScriptContext> scriptContext =
            NS_REINTERPRET_CAST(nsIScriptContext*,JS_GetContextPrivate(cx));
        if (scriptContext)
        {
            nsCOMPtr<nsIScriptGlobalObject> global;
            scriptContext->GetGlobalObject(getter_AddRefs(global));
            NS_ENSURE_TRUE(global, NS_ERROR_FAILURE);
            nsCOMPtr<nsIScriptObjectPrincipal> globalData(do_QueryInterface(global));
            NS_ENSURE_TRUE(globalData, NS_ERROR_FAILURE);
            globalData->GetPrincipal(result);
            if (*result)
            {
                JSStackFrame *inner = nsnull;
                *frameResult = JS_FrameIterator(cx, &inner);
                return NS_OK;
            }
        }
    }

    *result = nsnull;
    return NS_OK;
}

nsresult
nsScriptSecurityManager::GetSubjectPrincipal(JSContext *cx,
                                             nsIPrincipal **result)
{
    JSStackFrame *fp;
    return GetPrincipalAndFrame(cx, result, &fp);
}

NS_IMETHODIMP
nsScriptSecurityManager::GetObjectPrincipal(JSContext *aCx, JSObject *aObj,
                                            nsIPrincipal **result)
{
    return doGetObjectPrincipal(aCx, aObj, result);
}

// static
nsresult
nsScriptSecurityManager::doGetObjectPrincipal(JSContext *aCx, JSObject *aObj,
                                              nsIPrincipal **result)
{
    NS_ASSERTION(aCx && aObj && result, "Bad call to doGetObjectPrincipal()!");

    do
    {
        const JSClass *jsClass = JS_GetClass(aCx, aObj);

        if (jsClass && !(~jsClass->flags & (JSCLASS_HAS_PRIVATE |
                                            JSCLASS_PRIVATE_IS_NSISUPPORTS)))
        {
            // No need to refcount |priv| here.
            nsISupports *priv = (nsISupports *)JS_GetPrivate(aCx, aObj);
            nsCOMPtr<nsIScriptObjectPrincipal> objPrin;

            /*
             * If it's a wrapped native (as most
             * JSCLASS_PRIVATE_IS_NSISUPPORTS objects are in mozilla),
             * check the underlying native instead.
             */
            nsCOMPtr<nsIXPConnectWrappedNative> xpcWrapper =
                do_QueryInterface(priv);

            if (xpcWrapper)
            {
                nsCOMPtr<nsISupports> supports;
                xpcWrapper->GetNative(getter_AddRefs(supports));

                objPrin = do_QueryInterface(supports);
            }
            else
            {
                objPrin = do_QueryInterface(priv);
            }

            if (objPrin && NS_SUCCEEDED(objPrin->GetPrincipal(result)))
            {
                return NS_OK;
            }
        }

        aObj = JS_GetParent(aCx, aObj);
    } while (aObj);

    // Couldn't find a principal for this object.
    return NS_ERROR_FAILURE;
}

nsresult
nsScriptSecurityManager::SavePrincipal(nsIPrincipal* aToSave)
{
    nsresult rv;
    nsCOMPtr<nsIPrincipal> persistent = aToSave;
    nsCOMPtr<nsIAggregatePrincipal> aggregate(do_QueryInterface(aToSave, &rv));
    if (NS_SUCCEEDED(rv))
        if (NS_FAILED(aggregate->GetPrimaryChild(getter_AddRefs(persistent))))
            return NS_ERROR_FAILURE;

    //-- Save to mPrincipals
    if (!mPrincipals)
    {
        mPrincipals = new nsSupportsHashtable(31);
        if (!mPrincipals)
            return NS_ERROR_OUT_OF_MEMORY;
    }
    nsIPrincipalKey key(persistent);
    mPrincipals->Put(&key, persistent);

    //-- Save to prefs
    nsXPIDLCString idPrefName;
    nsXPIDLCString id;
    nsXPIDLCString grantedList;
    nsXPIDLCString deniedList;
    rv = persistent->GetPreferences(getter_Copies(idPrefName),
                                    getter_Copies(id),
                                    getter_Copies(grantedList),
                                    getter_Copies(deniedList));
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    nsXPIDLCString grantedPrefName;
    nsXPIDLCString deniedPrefName;
    rv = PrincipalPrefNames( idPrefName,
                             getter_Copies(grantedPrefName),
                             getter_Copies(deniedPrefName)  );
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    mIsWritingPrefs = PR_TRUE;
    if (grantedList)
        mSecurityPref->SecuritySetCharPref(grantedPrefName, grantedList);
    else
        mSecurityPref->SecurityClearUserPref(grantedPrefName);

    if (deniedList)
        mSecurityPref->SecuritySetCharPref(deniedPrefName, deniedList);
    else
        mSecurityPref->SecurityClearUserPref(deniedPrefName);

    if (grantedList || deniedList)
        mSecurityPref->SecuritySetCharPref(idPrefName, id);
    else
        mSecurityPref->SecurityClearUserPref(idPrefName);

    mIsWritingPrefs = PR_FALSE;

    nsCOMPtr<nsIPrefService> prefService(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);
    return prefService->SavePrefFile(nsnull);
}

///////////////// Capabilities API /////////////////////
NS_IMETHODIMP
nsScriptSecurityManager::IsCapabilityEnabled(const char *capability,
                                                 PRBool *result)
{
    nsresult rv;
    JSStackFrame *fp = nsnull;
    JSContext *cx = GetCurrentJSContext();
    fp = cx ? JS_FrameIterator(cx, &fp) : nsnull;
    if (!fp)
    {
        // No script code on stack. Allow execution.
        *result = PR_TRUE;
        return NS_OK;
    }
    *result = PR_FALSE;
    nsCOMPtr<nsIPrincipal> previousPrincipal;
    do
    {
        nsCOMPtr<nsIPrincipal> principal;
        if (NS_FAILED(GetFramePrincipal(cx, fp, getter_AddRefs(principal))))
            return NS_ERROR_FAILURE;
        if (!principal)
            continue;
        // If caller has a different principal, stop looking up the stack.
        if(previousPrincipal)
        {
            PRBool isEqual = PR_FALSE;
            if(NS_FAILED(previousPrincipal->Equals(principal, &isEqual)) || !isEqual)
                break;
        }
        else
            previousPrincipal = principal;

        // First check if the principal is even able to enable the
        // given capability. If not, don't look any further.
        PRInt16 canEnable;
        rv = principal->CanEnableCapability(capability, &canEnable);
        if (NS_FAILED(rv)) return rv;
        if (canEnable != nsIPrincipal::ENABLE_GRANTED &&
            canEnable != nsIPrincipal::ENABLE_WITH_USER_PERMISSION)
            return NS_OK;

        // Now see if the capability is enabled.
        void *annotation = JS_GetFrameAnnotation(cx, fp);
        rv = principal->IsCapabilityEnabled(capability, annotation, result);
        if (NS_FAILED(rv)) return rv;
        if (*result)
            return NS_OK;
    } while ((fp = JS_FrameIterator(cx, &fp)) != nsnull);

    if (!previousPrincipal)
    {
        // No principals on the stack, all native code.  Allow execution.
        *result = PR_TRUE;
    }
    return NS_OK;
}

PRBool
nsScriptSecurityManager::CheckConfirmDialog(JSContext* cx, nsIPrincipal* aPrincipal,
                                            PRBool *checkValue)
{
    nsresult rv;
    *checkValue = PR_FALSE;

    //-- Get a prompter for the current window.
    nsCOMPtr<nsIPrompt> prompter;
    if (cx)
    {
        nsCOMPtr<nsIScriptContext> scriptContext = (nsIScriptContext*)JS_GetContextPrivate(cx);
        if (scriptContext)
        {
            nsCOMPtr<nsIScriptGlobalObject> globalObject;
            scriptContext->GetGlobalObject(getter_AddRefs(globalObject));
            NS_ASSERTION(globalObject, "script context has no global object");
            nsCOMPtr<nsIDOMWindowInternal> domWin(do_QueryInterface(globalObject));
            if (domWin)
                domWin->GetPrompter(getter_AddRefs(prompter));
        }
    }

    if (!prompter)
    {
        //-- Couldn't get prompter from the current window, so get the prompt service.
        nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
        if (wwatch)
          wwatch->GetNewPrompter(0, getter_AddRefs(prompter));
        if (!prompter)
            return PR_FALSE;
    }

    // create a bundle for the localization
    nsCOMPtr<nsIStringBundleService> bundleService(do_GetService(kStringBundleServiceCID, &rv));
    if (NS_FAILED(rv))
        return PR_FALSE;

    nsCOMPtr<nsIStringBundle> bundle;
    rv = bundleService->CreateBundle(PROPERTIES_URL, getter_AddRefs(bundle));
    if (NS_FAILED(rv))
        return PR_FALSE;

    //-- Localize the dialog text
    nsXPIDLString query, check, title;
    rv = bundle->GetStringFromName(NS_LITERAL_STRING("EnableCapabilityQuery").get(),
                                   getter_Copies(query));
    if (NS_FAILED(rv))
        return PR_FALSE;
    rv = bundle->GetStringFromName(NS_LITERAL_STRING("CheckMessage").get(),
                                   getter_Copies(check));
    if (NS_FAILED(rv))
        return PR_FALSE;
    rv = bundle->GetStringFromName(NS_LITERAL_STRING("Titleline").get(),
                                   getter_Copies(title));
    if (NS_FAILED(rv))
        return PR_FALSE;

    nsXPIDLCString source;
    rv = aPrincipal->ToUserVisibleString(getter_Copies(source));
    if (NS_FAILED(rv))
        return PR_FALSE;
    PRUnichar* message = nsTextFormatter::smprintf(query.get(), source.get());
    NS_ENSURE_TRUE(message, PR_FALSE);

    PRInt32 buttonPressed = 1; // If the user exits by clicking the close box, assume No (button 1)
    rv = prompter->ConfirmEx(title.get(), message,
                             (nsIPrompt::BUTTON_TITLE_YES * nsIPrompt::BUTTON_POS_0) +
                             (nsIPrompt::BUTTON_TITLE_NO * nsIPrompt::BUTTON_POS_1),
                             nsnull, nsnull, nsnull, check.get(), checkValue, &buttonPressed);
    nsTextFormatter::smprintf_free(message);

    if (NS_FAILED(rv))
        *checkValue = PR_FALSE;
    return (buttonPressed == 0);
}

NS_IMETHODIMP
nsScriptSecurityManager::RequestCapability(nsIPrincipal* aPrincipal,
                                           const char *capability, PRInt16* canEnable)
{
    if (NS_FAILED(aPrincipal->CanEnableCapability(capability, canEnable)))
        return NS_ERROR_FAILURE;
    if (*canEnable == nsIPrincipal::ENABLE_WITH_USER_PERMISSION)
    {
        // Prompt user for permission to enable capability.
        JSContext* cx = GetCurrentJSContext();
        PRBool remember;
        if (CheckConfirmDialog(cx, aPrincipal, &remember))
            *canEnable = nsIPrincipal::ENABLE_GRANTED;
        else
            *canEnable = nsIPrincipal::ENABLE_DENIED;
        if (remember)
        {
            //-- Save principal to prefs and to mPrincipals
            if (NS_FAILED(aPrincipal->SetCanEnableCapability(capability, *canEnable)))
                return NS_ERROR_FAILURE;
            if (NS_FAILED(SavePrincipal(aPrincipal)))
                return NS_ERROR_FAILURE;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::EnableCapability(const char *capability)
{
    JSContext *cx = GetCurrentJSContext();
    JSStackFrame *fp;

    //-- Error checks for capability string length (200)
    if(PL_strlen(capability)>200)
    {
        static const char msg[] = "Capability name too long";
        JS_SetPendingException(cx, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, msg)));
        return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIPrincipal> principal;
    if (NS_FAILED(GetPrincipalAndFrame(cx, getter_AddRefs(principal), &fp)))
        return NS_ERROR_FAILURE;
    void *annotation = JS_GetFrameAnnotation(cx, fp);
    PRBool enabled;
    if (NS_FAILED(principal->IsCapabilityEnabled(capability, annotation,
                                                 &enabled)))
        return NS_ERROR_FAILURE;
    if (enabled)
        return NS_OK;

    PRInt16 canEnable;
    if (NS_FAILED(RequestCapability(principal, capability, &canEnable)))
        return NS_ERROR_FAILURE;

    if (canEnable != nsIPrincipal::ENABLE_GRANTED)
    {
        static const char msg[] = "enablePrivilege not granted";
        JS_SetPendingException(cx, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, msg)));
        return NS_ERROR_FAILURE; // XXX better error code?
    }
    if (NS_FAILED(principal->EnableCapability(capability, &annotation)))
        return NS_ERROR_FAILURE;
    JS_SetFrameAnnotation(cx, fp, annotation);
    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::RevertCapability(const char *capability)
{
    JSContext *cx = GetCurrentJSContext();
    JSStackFrame *fp;
    nsCOMPtr<nsIPrincipal> principal;
    if (NS_FAILED(GetPrincipalAndFrame(cx, getter_AddRefs(principal), &fp)))
        return NS_ERROR_FAILURE;
    void *annotation = JS_GetFrameAnnotation(cx, fp);
    principal->RevertCapability(capability, &annotation);
    JS_SetFrameAnnotation(cx, fp, annotation);
    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::DisableCapability(const char *capability)
{
    JSContext *cx = GetCurrentJSContext();
    JSStackFrame *fp;
    nsCOMPtr<nsIPrincipal> principal;
    if (NS_FAILED(GetPrincipalAndFrame(cx, getter_AddRefs(principal), &fp)))
        return NS_ERROR_FAILURE;
    void *annotation = JS_GetFrameAnnotation(cx, fp);
    principal->DisableCapability(capability, &annotation);
    JS_SetFrameAnnotation(cx, fp, annotation);
    return NS_OK;
}

//////////////// Master Certificate Functions ///////////////////////////////////////
NS_IMETHODIMP
nsScriptSecurityManager::SetCanEnableCapability(const char* certificateID,
                                                const char* capability,
                                                PRInt16 canEnable)
{
    nsresult rv;
    nsCOMPtr<nsIPrincipal> subjectPrincipal;
    rv = GetSubjectPrincipal(getter_AddRefs(subjectPrincipal));
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    //-- Get the system certificate
    if (!mSystemCertificate)
    {
        nsCOMPtr<nsIFile> systemCertFile;
        nsCOMPtr<nsIProperties> directoryService =
                 do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
        if (!directoryService) return NS_ERROR_FAILURE;
        rv = directoryService->Get(NS_XPCOM_CURRENT_PROCESS_DIR, NS_GET_IID(nsIFile),
                              getter_AddRefs(systemCertFile));
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
#ifdef XP_MAC
        // On Mac, this file will be located in the Essential Files folder
        systemCertFile->AppendNative(NS_LITERAL_CSTRING("Essential Files"));
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
#endif
        systemCertFile->AppendNative(NS_LITERAL_CSTRING("systemSignature.jar"));
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
        nsCOMPtr<nsIZipReader> systemCertZip;
        rv = nsComponentManager::CreateInstance(kZipReaderCID, nsnull,
                                                NS_GET_IID(nsIZipReader),
                                                getter_AddRefs(systemCertZip));
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
        systemCertZip->Init(systemCertFile);
        rv = systemCertZip->Open();
        if (NS_SUCCEEDED(rv))
        {
            nsCOMPtr<nsIJAR> systemCertJar(do_QueryInterface(systemCertZip, &rv));
            if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
            rv = systemCertJar->GetCertificatePrincipal(nsnull,
                                                        getter_AddRefs(mSystemCertificate));
            if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
        }
    }

    //-- Make sure the caller's principal is the system certificate
    PRBool isEqual = PR_FALSE;
    if (mSystemCertificate)
    {
        rv = mSystemCertificate->Equals(subjectPrincipal, &isEqual);
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    }
    if (!isEqual)
    {
        JSContext* cx = GetCurrentJSContext();
        if (!cx) return NS_ERROR_FAILURE;
        static const char msg1[] = "Only code signed by the system certificate may call SetCanEnableCapability or Invalidate";
        static const char msg2[] = "Attempt to call SetCanEnableCapability or Invalidate when no system certificate has been established";
            JS_SetPendingException(cx, STRING_TO_JSVAL(JS_NewStringCopyZ(cx,
                                   mSystemCertificate ? msg1 : msg2)));
        return NS_ERROR_FAILURE;
    }

    //-- Get the target principal
    nsCOMPtr<nsIPrincipal> objectPrincipal;
    rv =  GetCertificatePrincipal(certificateID, getter_AddRefs(objectPrincipal));
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    rv = objectPrincipal->SetCanEnableCapability(capability, canEnable);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    return SavePrincipal(objectPrincipal);
}

////////////////////////////////////////////////
// Methods implementing nsIXPCSecurityManager //
////////////////////////////////////////////////

NS_IMETHODIMP
nsScriptSecurityManager::CanCreateWrapper(JSContext *cx,
                                          const nsIID &aIID,
                                          nsISupports *aObj,
                                          nsIClassInfo *aClassInfo,
                                          void **aPolicy)
{
#if 0
    char* iidStr = aIID.ToString();
    printf("### CanCreateWrapper(%s) ", iidStr);
    PR_FREEIF(iidStr);
#endif
// XXX Special case for nsIXPCException ?
    if (ClassInfoData(aClassInfo).IsDOMClass())
    {
#if 0
        printf("DOM class - GRANTED.\n");
#endif
        return NS_OK;
    }

    //--See if the object advertises a non-default level of access
    //  using nsISecurityCheckedComponent
    nsCOMPtr<nsISecurityCheckedComponent> checkedComponent =
        do_QueryInterface(aObj);

    nsXPIDLCString objectSecurityLevel;
    if (checkedComponent)
        checkedComponent->CanCreateWrapper((nsIID *)&aIID, getter_Copies(objectSecurityLevel));

    nsresult rv = CheckXPCPermissions(aObj, objectSecurityLevel);
    if (NS_FAILED(rv))
    {
        //-- Access denied, report an error
        nsCAutoString errorMsg("Permission denied to create wrapper for object ");
        nsXPIDLCString className;
        if (aClassInfo)
        {
            aClassInfo->GetClassDescription(getter_Copies(className));
            if (className)
            {
                errorMsg += "of class ";
                errorMsg += className;
            }
        }
        JS_SetPendingException(cx,
                               STRING_TO_JSVAL(JS_NewStringCopyZ(cx, errorMsg.get())));
    }
    return rv;
}

NS_IMETHODIMP
nsScriptSecurityManager::CanCreateInstance(JSContext *cx,
                                           const nsCID &aCID)
{
    nsresult rv = CheckXPCPermissions(nsnull, nsnull);
    if (NS_FAILED(rv))
    {
        //-- Access denied, report an error
        nsCAutoString errorMsg("Permission denied to create instance of class. CID=");
        nsXPIDLCString cidStr;
        cidStr += aCID.ToString();
        errorMsg.Append(cidStr);
        JS_SetPendingException(cx,
                               STRING_TO_JSVAL(JS_NewStringCopyZ(cx, errorMsg.get())));
    }
    return rv;
}

NS_IMETHODIMP
nsScriptSecurityManager::CanGetService(JSContext *cx,
                                       const nsCID &aCID)
{
#if 0
    char* cidStr = aCID.ToString();
    printf("### CanGetService(%s) ", cidStr);
    PR_FREEIF(cidStr);
#endif

    nsresult rv = CheckXPCPermissions(nsnull, nsnull);
    if (NS_FAILED(rv))
    {
        //-- Access denied, report an error
        nsCAutoString errorMsg("Permission denied to get service. CID=");
        nsXPIDLCString cidStr;
        cidStr += aCID.ToString();
        errorMsg.Append(cidStr);
        JS_SetPendingException(cx,
                               STRING_TO_JSVAL(JS_NewStringCopyZ(cx, errorMsg.get())));
    }
    return rv;
}


NS_IMETHODIMP
nsScriptSecurityManager::CanAccess(PRUint32 aAction,
                                   nsIXPCNativeCallContext* aCallContext,
                                   JSContext* cx,
                                   JSObject* aJSObject,
                                   nsISupports* aObj,
                                   nsIClassInfo* aClassInfo,
                                   jsval aPropertyName,
                                   void** aPolicy)
{
    return CheckPropertyAccessImpl(aAction, aCallContext, cx,
                                   aJSObject, aObj, nsnull, aClassInfo,
                                   nsnull, aPropertyName, aPolicy);
}

nsresult
nsScriptSecurityManager::CheckXPCPermissions(nsISupports* aObj,
                                             const char* aObjectSecurityLevel)
{
    //-- Check for the all-powerful UniversalXPConnect privilege
    PRBool ok = PR_FALSE;
    if (NS_SUCCEEDED(IsCapabilityEnabled("UniversalXPConnect", &ok)) && ok)
        return NS_OK;

    //-- If the object implements nsISecurityCheckedComponent, it has a non-default policy.
    if (aObjectSecurityLevel)
    {
        if (PL_strcasecmp(aObjectSecurityLevel, "AllAccess") == 0)
            return NS_OK;
        else if (PL_strcasecmp(aObjectSecurityLevel, "NoAccess") != 0)
        {
            PRBool canAccess = PR_FALSE;
            if (NS_SUCCEEDED(IsCapabilityEnabled(aObjectSecurityLevel, &canAccess)) &&
                canAccess)
                return NS_OK;
        }
    }

    //-- If user allows scripting of plugins by untrusted scripts,
    //   and the target object is a plugin, allow the access.
    if(aObj)
    {
        nsresult rv;
        nsCOMPtr<nsIPluginInstance> plugin(do_QueryInterface(aObj, &rv));
        if (NS_SUCCEEDED(rv))
        {
            static PRBool prefSet = PR_FALSE;
            static PRBool allowPluginAccess = PR_FALSE;
            if (!prefSet)
            {
                rv = mSecurityPref->SecurityGetBoolPref("security.xpconnect.plugin.unrestricted",
                                                       &allowPluginAccess);
                prefSet = PR_TRUE;
            }
            if (allowPluginAccess)
                return NS_OK;
        }
    }

    //-- Access tests failed
    return NS_ERROR_DOM_XPCONNECT_ACCESS_DENIED;
}

/////////////////////////////////////
// Method implementing nsIObserver //
/////////////////////////////////////
static const char sPrincipalPrefix[] = "capability.principal";
static NS_NAMED_LITERAL_CSTRING(sPolicyPrefix, "capability.policy.");

NS_IMETHODIMP
nsScriptSecurityManager::Observe(nsISupports* aObject, const char* aTopic,
                                 const PRUnichar* aMessage)
{
    nsresult rv = NS_OK;
    NS_ConvertUCS2toUTF8 messageStr(aMessage);
    const char *message = messageStr.get();

    static const char jsPrefix[] = "javascript.";
    if(PL_strncmp(message, jsPrefix, sizeof(jsPrefix)-1) == 0)
        JSEnabledPrefChanged(mSecurityPref);
    if(PL_strncmp(message, sPolicyPrefix.get(), sPolicyPrefix.Length()) == 0)
        mPolicyPrefsChanged = PR_TRUE; // This will force re-initialization of the pref table
    else if((PL_strncmp(message, sPrincipalPrefix, sizeof(sPrincipalPrefix)-1) == 0) &&
            !mIsWritingPrefs)
    {
        static const char id[] = "id";
        char* lastDot = PL_strrchr(message, '.');
        //-- This check makes sure the string copy below doesn't overwrite its bounds
        if(PL_strlen(lastDot) >= sizeof(id))
        {
            PL_strcpy(lastDot + 1, id);
            const char** idPrefArray = (const char**)&message;
            rv = InitPrincipals(1, idPrefArray, mSecurityPref);
        }
    }
    return rv;
}

/////////////////////////////////////////////
// Constructor, Destructor, Initialization //
/////////////////////////////////////////////
nsScriptSecurityManager::nsScriptSecurityManager(void)
    : mOriginToPolicyMap(nsnull),
      mDefaultPolicy(nsnull),
      mCapabilities(nsnull),
      mSystemPrincipal(nsnull), mPrincipals(nsnull),
      mIsJavaScriptEnabled(PR_FALSE),
      mIsMailJavaScriptEnabled(PR_FALSE),
      mIsWritingPrefs(PR_FALSE),
      mNameSetRegistered(PR_FALSE),
      mPolicyPrefsChanged(PR_TRUE)

{
    NS_ASSERTION(sizeof(long) == sizeof(void*), "long and void* have different lengths on this platform. This may cause a security failure.");

    JSContext* cx = GetSafeJSContext();
    if (sCallerID == JSVAL_VOID)
        sCallerID = STRING_TO_JSVAL(::JS_InternString(cx, "caller"));
    if (sEnabledID == JSVAL_VOID)
        sEnabledID = STRING_TO_JSVAL(::JS_InternString(cx, "enabled"));

    InitPrefs();

    //-- Register security check callback in the JS engine
    //   Currently this is used to control access to function.caller
    nsresult rv;
    nsCOMPtr<nsIJSRuntimeService> runtimeService =
        do_GetService("@mozilla.org/js/xpc/RuntimeService;1", &rv);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to get runtime service");

    JSRuntime *rt;
    rv = runtimeService->GetRuntime(&rt);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to get current JS runtime");

#ifdef DEBUG
    JSCheckAccessOp oldCallback =
#endif
        JS_SetCheckObjectAccessCallback(rt, CheckJSFunctionCallerAccess);

    // For now, assert that no callback was set previously
    NS_ASSERTION(!oldCallback, "Someone already set a JS CheckObjectAccess callback");
}

jsval nsScriptSecurityManager::sCallerID   = JSVAL_VOID;
jsval nsScriptSecurityManager::sEnabledID   = JSVAL_VOID;

nsScriptSecurityManager::~nsScriptSecurityManager(void)
{
    delete mOriginToPolicyMap;
    delete mDefaultPolicy;
    NS_IF_RELEASE(mSystemPrincipal);
    delete mPrincipals;
    delete mCapabilities;
}

void
nsScriptSecurityManager::Shutdown()
{
    sCallerID = JSVAL_VOID;
    sEnabledID = JSVAL_VOID;
}

static nsScriptSecurityManager *ssecMan = NULL;

nsScriptSecurityManager *
nsScriptSecurityManager::GetScriptSecurityManager()
{
    if (!ssecMan)
    {
        ssecMan = new nsScriptSecurityManager();
        if (!ssecMan)
            return NULL;
        nsresult rv;

        rv = nsJSPrincipals::Startup();

        NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                         "can't initialize JS engine security protocol glue!");

        nsCOMPtr<nsIXPConnect> xpc =
            do_GetService(nsIXPConnect::GetCID(), &rv);
        if (NS_SUCCEEDED(rv) && xpc)
        {
            rv = xpc->SetDefaultSecurityManager(
                            NS_STATIC_CAST(nsIXPCSecurityManager*, ssecMan),
                            nsIXPCSecurityManager::HOOK_ALL);

            NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                             "failed to install xpconnect security manager!");
        }
        else
            NS_WARNING("can't get xpconnect to install security manager!");
    }
    return ssecMan;
}

// Currently this nsGenericFactory constructor is used only from FastLoad
// (XPCOM object deserialization) code, when "creating" the system principal
// singleton.
nsSystemPrincipal *
nsScriptSecurityManager::SystemPrincipalSingletonConstructor()
{
    nsIPrincipal *sysprin = nsnull;
    if (ssecMan)
        ssecMan->GetSystemPrincipal(&sysprin);
    return NS_STATIC_CAST(nsSystemPrincipal*, sysprin);
}

nsresult
nsScriptSecurityManager::InitPolicies()
{
    nsresult rv;

    // Reset the "dirty" flag
    mPolicyPrefsChanged = PR_FALSE;

    // Clear any policies cached on XPConnect wrappers
    nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
    if (NS_FAILED(rv)) return rv;
    rv = xpc->ClearAllWrappedNativeSecurityPolicies();
    if (NS_FAILED(rv)) return rv;

    //-- Reset mOriginToPolicyMap
    delete mOriginToPolicyMap;
    mOriginToPolicyMap =
      new nsObjectHashtable(nsnull, nsnull, DeleteDomainEntry, nsnull);

    //-- Reset and initialize the default policy
    delete mDefaultPolicy;
    mDefaultPolicy =
      new DomainPolicy();
    if (!mOriginToPolicyMap || !mDefaultPolicy)
        return NS_ERROR_OUT_OF_MEMORY;

    //-- Initialize the table of security levels
    if (!mCapabilities)
    {
        mCapabilities = 
          new nsObjectHashtable(nsnull, nsnull, DeleteCapability, nsnull);
        if (!mCapabilities)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    // Get a JS context - we need it to create internalized strings later.
    JSContext* cx = GetSafeJSContext();
    NS_ASSERTION(cx, "failed to get JS context");
    rv = InitDomainPolicy(cx, "default", mDefaultPolicy);
    NS_ENSURE_SUCCESS(rv, rv);

    nsXPIDLCString policyNames;
    rv = mSecurityPref->SecurityGetCharPref("capability.policy.policynames",
                                            getter_Copies(policyNames));

    nsXPIDLCString defaultPolicyNames;
    rv = mSecurityPref->SecurityGetCharPref("capability.policy.default_policynames",
                                            getter_Copies(defaultPolicyNames));
    policyNames += NS_LITERAL_CSTRING(" ") + defaultPolicyNames;

    //-- Initialize domain policies
    char* policyCurrent = (char*)policyNames.get();
    PRBool morePolicies = PR_TRUE;
    while (morePolicies)
    {
        while(*policyCurrent == ' ' || *policyCurrent == ',')
            policyCurrent++;
        if (*policyCurrent == '\0')
            break;
        char* nameBegin = policyCurrent;

        while(*policyCurrent != '\0' && *policyCurrent != ' ' && *policyCurrent != ',')
            policyCurrent++;

        morePolicies = (*policyCurrent != '\0');
        *policyCurrent = '\0';
        policyCurrent++;

        nsCAutoString sitesPrefName(sPolicyPrefix +
				    nsDependentCString(nameBegin) +
				    NS_LITERAL_CSTRING(".sites"));
        nsXPIDLCString domainList;
        rv = mSecurityPref->SecurityGetCharPref(sitesPrefName.get(),
                                                getter_Copies(domainList));
        if (NS_FAILED(rv))
            continue;

        DomainPolicy* domainPolicy = new DomainPolicy();
        if (!domainPolicy)
            return NS_ERROR_OUT_OF_MEMORY;

        //-- Parse list of sites and create an entry in mOriginToPolicyMap for each
        char* domainStart = (char*)domainList.get();
        char* domainCurrent = domainStart;
        char* lastDot = nsnull;
        char* nextToLastDot = nsnull;
        PRBool moreDomains = PR_TRUE;
        while (moreDomains)
        {
            if (*domainCurrent == ' ' || *domainCurrent == '\0')
            {
                moreDomains = (*domainCurrent != '\0');
                *domainCurrent = '\0';
                nsCStringKey key(nextToLastDot ? nextToLastDot+1 : domainStart);
                DomainEntry *newEntry = new DomainEntry(domainStart, domainPolicy);
                if (!newEntry)
                    return NS_ERROR_OUT_OF_MEMORY;
#ifdef DEBUG
                newEntry->mPolicyName_DEBUG = nameBegin;
#endif
                DomainEntry *existingEntry = (DomainEntry *)
                    mOriginToPolicyMap->Get(&key);
                if (!existingEntry)
                    mOriginToPolicyMap->Put(&key, newEntry);
                else
                {
                    if (existingEntry->Matches(domainStart))
                    {
                        newEntry->mNext = existingEntry;
                        mOriginToPolicyMap->Put(&key, newEntry);
                    }
                    else
                    {
                        while (existingEntry->mNext)
                        {
                            if (existingEntry->mNext->Matches(domainStart))
                            {
                                newEntry->mNext = existingEntry->mNext;
                                existingEntry->mNext = newEntry;
                                break;
                            }
                            existingEntry = existingEntry->mNext;
                        }
                        if (!existingEntry->mNext)
                            existingEntry->mNext = newEntry;
                    }
                }
                domainStart = domainCurrent + 1;
                lastDot = nextToLastDot = nsnull;
            }
            else if (*domainCurrent == '.')
            {
                nextToLastDot = lastDot;
                lastDot = domainCurrent;
            }
            domainCurrent++;
        }

        rv = InitDomainPolicy(cx, nameBegin, domainPolicy);
        NS_ENSURE_SUCCESS(rv, rv);
    }

#ifdef DEBUG_mstoltz
    PrintPolicyDB();
#endif
    return NS_OK;
}


nsresult
nsScriptSecurityManager::InitDomainPolicy(JSContext* cx,
                                          const char* aPolicyName,
                                          DomainPolicy* aDomainPolicy)
{
    nsresult rv;
    nsCAutoString policyPrefix(sPolicyPrefix +
			       nsDependentCString(aPolicyName) +
			       NS_LITERAL_CSTRING("."));
    PRUint32 prefixLength = policyPrefix.Length() - 1; // subtract the '.'

    // XXX fix string use here.
    PRUint32 prefCount;
    char** prefNames;
    rv = mPrefBranch->GetChildList(policyPrefix.get(),
				   &prefCount, &prefNames);
    if (NS_FAILED(rv)) return rv;
    if (prefCount == 0)
        return NS_OK;

    //-- Populate the policy
    PRUint32 currentPref = 0;
    for (; currentPref < prefCount; currentPref++)
    {
        // Get the class name
        const char* start = prefNames[currentPref] + prefixLength +1;
        char* end = PL_strchr(start, '.');
        if (!end) // malformed pref, bail on this one
            continue;
        static const char sitesStr[] = "sites";

	// We dealt with "sites" in InitPolicies(), so no need to do
	// that again...
        if (PL_strncmp(start, sitesStr, sizeof(sitesStr)-1) == 0)
            continue;

        // Get the pref value
        nsXPIDLCString prefValue;
        rv = mSecurityPref->SecurityGetCharPref(prefNames[currentPref],
                                                getter_Copies(prefValue));
        if (NS_FAILED(rv) || !prefValue)
            continue;

        SecurityLevel secLevel;
        if (PL_strcasecmp(prefValue, "noAccess") == 0)
            secLevel.level = SCRIPT_SECURITY_NO_ACCESS;
        else if (PL_strcasecmp(prefValue, "allAccess") == 0)
            secLevel.level = SCRIPT_SECURITY_ALL_ACCESS;
        else if (PL_strcasecmp(prefValue, "sameOrigin") == 0)
            secLevel.level = SCRIPT_SECURITY_SAME_ORIGIN_ACCESS;
        else 
        {  //-- pref value is the name of a capability
            nsCStringKey secLevelKey(prefValue);
            secLevel.capability =
                NS_REINTERPRET_CAST(char*, mCapabilities->Get(&secLevelKey));
            if (!secLevel.capability)
            {
                secLevel.capability = nsCRT::strdup(prefValue);
                if (!secLevel.capability)
                    break;
                mCapabilities->Put(&secLevelKey, 
                                   secLevel.capability);
            }
        }

        *end = '\0';
        // Find or store this class in the classes table
        ClassPolicy* cpolicy = 
          NS_REINTERPRET_CAST(ClassPolicy*,
                              PL_DHashTableOperate(aDomainPolicy,
                                                   start,
                                                   PL_DHASH_ADD));

        if (!cpolicy)
            break;

        // Get the property name
        start = end + 1;
        end = PL_strchr(start, '.');
        if (end)
            *end = '\0';

        JSString* propertyKey = ::JS_InternString(cx, start);
        if (!propertyKey)
            return NS_ERROR_OUT_OF_MEMORY;

        // Store this property in the class policy
        PropertyPolicy* ppolicy = 
          (PropertyPolicy*) PL_DHashTableOperate(cpolicy->mPolicy,
                                                 (void*)STRING_TO_JSVAL(propertyKey),
                                                 PL_DHASH_ADD);
        if (!ppolicy)
            break;

        if (end) // The pref specifies an access mode
        {
            start = end + 1;
            if (PL_strcasecmp(start, "set") == 0)
                ppolicy->mSet = secLevel;
            else
                ppolicy->mGet = secLevel;
        }
        else
        {
            if (ppolicy->mGet.level == SCRIPT_SECURITY_UNDEFINED_ACCESS)
                ppolicy->mGet = secLevel;
            if (ppolicy->mSet.level == SCRIPT_SECURITY_UNDEFINED_ACCESS)
                ppolicy->mSet = secLevel;
        }
    }

    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(prefCount, prefNames);
    if (currentPref < prefCount) // Loop exited early because of out-of-memory error
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}


nsresult
nsScriptSecurityManager::PrincipalPrefNames(const char* pref,
                                            char** grantedPref, char** deniedPref)
{
    char* lastDot = PL_strrchr(pref, '.');
    if (!lastDot) return NS_ERROR_FAILURE;
    PRInt32 prefLen = lastDot - pref + 1;

    *grantedPref = nsnull;
    *deniedPref = nsnull;

    static const char granted[] = "granted";
    *grantedPref = (char*)PR_MALLOC(prefLen + sizeof(granted));
    if (!grantedPref) return NS_ERROR_OUT_OF_MEMORY;
    PL_strncpy(*grantedPref, pref, prefLen);
    PL_strcpy(*grantedPref + prefLen, granted);

    static const char denied[] = "denied";
    *deniedPref = (char*)PR_MALLOC(prefLen + sizeof(denied));
    if (!deniedPref)
    {
        PR_FREEIF(*grantedPref);
        return NS_ERROR_OUT_OF_MEMORY;
    }
    PL_strncpy(*deniedPref, pref, prefLen);
    PL_strcpy(*deniedPref + prefLen, denied);

    return NS_OK;
}

nsresult
nsScriptSecurityManager::InitPrincipals(PRUint32 aPrefCount, const char** aPrefNames,
                                        nsISecurityPref* aSecurityPref)
{
    /* This is the principal preference syntax:
     * capability.principal.[codebase|codebaseTrusted|certificate].<name>.[id|granted|denied]
     * For example:
     * user_pref("capability.principal.certificate.p1.id","12:34:AB:CD");
     * user_pref("capability.principal.certificate.p1.granted","Capability1 Capability2");
     * user_pref("capability.principal.certificate.p1.denied","Capability3");
     */

    /* codebaseTrusted means a codebase principal that can enable capabilities even if
     * codebase principals are disabled. Don't use trustedCodebase except with unspoofable
     * URLs such as HTTPS URLs.
     */

    static const char idSuffix[] = ".id";
    for (PRUint32 c = 0; c < aPrefCount; c++)
    {
        PRInt32 prefNameLen = PL_strlen(aPrefNames[c]) - (sizeof(idSuffix)-1);
        if (PL_strcasecmp(aPrefNames[c] + prefNameLen, idSuffix) != 0)
            continue;

        nsXPIDLCString id;
        if (NS_FAILED(mSecurityPref->SecurityGetCharPref(aPrefNames[c], getter_Copies(id))))
            return NS_ERROR_FAILURE;

        nsXPIDLCString grantedPrefName;
        nsXPIDLCString deniedPrefName;
        nsresult rv = PrincipalPrefNames(aPrefNames[c],
                                         getter_Copies(grantedPrefName),
                                         getter_Copies(deniedPrefName));
        if (rv == NS_ERROR_OUT_OF_MEMORY)
            return rv;
        else if (NS_FAILED(rv))
            continue;

        char* grantedList = nsnull;
        mSecurityPref->SecurityGetCharPref(grantedPrefName, &grantedList);
        char* deniedList = nsnull;
        mSecurityPref->SecurityGetCharPref(deniedPrefName, &deniedList);

        //-- Delete prefs if their value is the empty string
        if ((!id || id[0] == '\0') ||
            ((!grantedList || grantedList[0] == '\0') && (!deniedList || deniedList[0] == '\0')))
        {
            mSecurityPref->SecurityClearUserPref(aPrefNames[c]);
            mSecurityPref->SecurityClearUserPref(grantedPrefName);
            mSecurityPref->SecurityClearUserPref(deniedPrefName);
            PR_FREEIF(grantedList);
            PR_FREEIF(deniedList);
            continue;
        }

        //-- Create a principal based on the prefs
        static const char certificateName[] = "capability.principal.certificate";
        static const char codebaseName[] = "capability.principal.codebase";
        static const char codebaseTrustedName[] = "capability.principal.codebaseTrusted";
        nsCOMPtr<nsIPrincipal> principal;
        if (PL_strncmp(aPrefNames[c], certificateName,
                       sizeof(certificateName)-1) == 0)
        {
            nsCertificatePrincipal *certificate = new nsCertificatePrincipal();
            if (certificate) {
                NS_ADDREF(certificate);
                if (NS_SUCCEEDED(certificate->InitFromPersistent(aPrefNames[c], id,
                                                                 grantedList, deniedList)))
                    principal = do_QueryInterface((nsBasePrincipal*)certificate);
                NS_RELEASE(certificate);
            }
        } else if(PL_strncmp(aPrefNames[c], codebaseName,
                             sizeof(codebaseName)-1) == 0)
        {
            nsCodebasePrincipal *codebase = new nsCodebasePrincipal();
            if (codebase) {
                NS_ADDREF(codebase);
                PRBool trusted = (PL_strncmp(aPrefNames[c], codebaseTrustedName,
                                             sizeof(codebaseTrustedName)-1) == 0);
                if (NS_SUCCEEDED(codebase->InitFromPersistent(aPrefNames[c], id,
                                                              grantedList, deniedList,
                                                              trusted)))
                    principal = do_QueryInterface((nsBasePrincipal*)codebase);
                NS_RELEASE(codebase);
            }
        }
        PR_FREEIF(grantedList);
        PR_FREEIF(deniedList);

        if (principal)
        {
            if (!mPrincipals)
            {
                mPrincipals = new nsSupportsHashtable(31);
                if (!mPrincipals)
                    return NS_ERROR_OUT_OF_MEMORY;
            }
            nsIPrincipalKey key(principal);
            mPrincipals->Put(&key, principal);
        }
    }
    return NS_OK;
}

const char* nsScriptSecurityManager::sJSEnabledPrefName = "javascript.enabled";
const char* nsScriptSecurityManager::sJSMailEnabledPrefName = "javascript.allow.mailnews";

inline void
nsScriptSecurityManager::JSEnabledPrefChanged(nsISecurityPref* aSecurityPref)
{
    if (NS_FAILED(mSecurityPref->SecurityGetBoolPref(sJSEnabledPrefName,
                                                     &mIsJavaScriptEnabled)))
        // Default to enabled.
        mIsJavaScriptEnabled = PR_TRUE;

    if (NS_FAILED(mSecurityPref->SecurityGetBoolPref(sJSMailEnabledPrefName,
                                                     &mIsMailJavaScriptEnabled)))
        // Default to enabled.
        mIsMailJavaScriptEnabled = PR_TRUE;
}

nsresult
nsScriptSecurityManager::InitPrefs()
{
    nsresult rv;
    nsCOMPtr<nsIPrefService> prefService(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = prefService->GetBranch(nsnull, getter_AddRefs(mPrefBranch));
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIPrefBranchInternal> prefBranchInternal(do_QueryInterface(mPrefBranch, &rv));
    NS_ENSURE_SUCCESS(rv, rv);
    mSecurityPref = do_QueryInterface(mPrefBranch, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // Set the initial value of the "javascript.enabled" prefs
    JSEnabledPrefChanged(mSecurityPref);
    // set observer callbacks in case the value of the prefs change
    prefBranchInternal->AddObserver(sJSEnabledPrefName, this, PR_FALSE);
    prefBranchInternal->AddObserver(sJSMailEnabledPrefName, this, PR_FALSE);
    PRUint32 prefCount;
    char** prefNames;

    // Set a callback for policy pref changes
    prefBranchInternal->AddObserver(sPolicyPrefix.get(), this, PR_FALSE);

    //-- Initialize the principals database from prefs
    rv = mPrefBranch->GetChildList(sPrincipalPrefix, &prefCount, &prefNames);
    if (NS_SUCCEEDED(rv) && prefCount > 0)
    {
        rv = InitPrincipals(prefCount, (const char**)prefNames, mSecurityPref);
        NS_ENSURE_SUCCESS(rv, rv);
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(prefCount, prefNames);
    }
    //-- Set a callback for principal changes
    prefBranchInternal->AddObserver(sPrincipalPrefix, this, PR_FALSE);

    return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// The following code prints the contents of the policy DB to the console.
#ifdef DEBUG_mstoltz

//typedef PLDHashOperator
//(* PR_CALLBACK PLDHashEnumerator)(PLDHashTable *table, PLDHashEntryHdr *hdr,
//                                      PRUint32 number, void *arg);
PR_STATIC_CALLBACK(PLDHashOperator)
PrintPropertyPolicy(PLDHashTable *table, PLDHashEntryHdr *entry,
                    PRUint32 number, void *arg)
{
    PropertyPolicy* pp = (PropertyPolicy*)entry;
    nsCAutoString prop("        ");
    JSContext* cx = (JSContext*)arg;
    prop.AppendInt((PRUint32)pp->key);
    prop += ' ';
    prop.AppendWithConversion((PRUnichar*)JSValIDToString(cx, pp->key));
    prop += ": Get=";
    if (SECURITY_ACCESS_LEVEL_FLAG(pp->mGet))
        prop.AppendInt(pp->mGet.level);
    else
        prop += pp->mGet.capability;

    prop += " Set=";
    if (SECURITY_ACCESS_LEVEL_FLAG(pp->mSet))
        prop.AppendInt(pp->mSet.level);
    else
        prop += pp->mSet.capability;
        
    printf("%s.\n", prop.get());
    return PL_DHASH_NEXT;
}

PR_STATIC_CALLBACK(PLDHashOperator)
PrintClassPolicy(PLDHashTable *table, PLDHashEntryHdr *entry,
                 PRUint32 number, void *arg)
{
    ClassPolicy* cp = (ClassPolicy*)entry;
    printf("    %s\n", cp->key);

    PL_DHashTableEnumerate(cp->mPolicy, PrintPropertyPolicy, arg);
    return PL_DHASH_NEXT;
}

// typedef PRBool
// (*PR_CALLBACK nsHashtableEnumFunc)(nsHashKey *aKey, void *aData, void* aClosure);
PR_STATIC_CALLBACK(PRBool)
PrintDomainPolicy(nsHashKey *aKey, void *aData, void* aClosure)
{
    DomainEntry* de = (DomainEntry*)aData;
    printf("----------------------------\n");
    printf("Domain: %s Policy Name: %s.\n", de->mOrigin.get(),
           de->mPolicyName_DEBUG.get());
    PL_DHashTableEnumerate(de->mDomainPolicy, PrintClassPolicy, aClosure);
    return PR_TRUE;
}

PR_STATIC_CALLBACK(PRBool)
PrintCapability(nsHashKey *aKey, void *aData, void* aClosure)
{
    char* cap = (char*)aData;
    printf("    %s.\n", cap);
    return PR_TRUE;
}

void
nsScriptSecurityManager::PrintPolicyDB()
{
    printf("############## Security Policies ###############\n");
    if(mOriginToPolicyMap)
    {
        JSContext* cx = GetCurrentJSContext();
        printf("----------------------------\n");
        printf("Domain: Default.\n");
        PL_DHashTableEnumerate(mDefaultPolicy, PrintClassPolicy, (void*)cx);
        mOriginToPolicyMap->Enumerate(PrintDomainPolicy, (void*)cx);
    }
    printf("############ End Security Policies #############\n\n");
    printf("############## Capabilities ###############\n");
    mCapabilities->Enumerate(PrintCapability);
    printf("############## End Capabilities ###############\n");
}
#endif

