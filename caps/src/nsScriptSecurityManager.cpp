/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Norris Boyd
 *   Mitch Stoltz
 *   Steve Morse
 *   Christopher A. Aillon
 *   Giorgio Maone
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsScriptSecurityManager.h"
#include "nsIServiceManager.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptContext.h"
#include "nsIURL.h"
#include "nsINestedURI.h"
#include "nspr.h"
#include "nsJSPrincipals.h"
#include "nsSystemPrincipal.h"
#include "nsPrincipal.h"
#include "nsNullPrincipal.h"
#include "nsXPIDLString.h"
#include "nsCRT.h"
#include "nsCRTGlue.h"
#include "nsIJSContextStack.h"
#include "nsDOMError.h"
#include "nsDOMCID.h"
#include "jsdbgapi.h"
#include "nsIXPConnect.h"
#include "nsIXPCSecurityManager.h"
#include "nsTextFormatter.h"
#include "nsIStringBundle.h"
#include "nsNetUtil.h"
#include "nsIProperties.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsIZipReader.h"
#include "nsIJAR.h"
#include "nsIPluginInstance.h"
#include "nsIXPConnect.h"
#include "nsIScriptGlobalObject.h"
#include "nsPIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIPrompt.h"
#include "nsIWindowWatcher.h"
#include "nsIConsoleService.h"
#include "nsISecurityCheckedComponent.h"
#include "nsIPrefBranch2.h"
#include "nsIJSRuntimeService.h"
#include "nsIObserverService.h"
#include "nsIContent.h"
#include "nsAutoPtr.h"
#include "nsDOMJSUtils.h"
#include "nsAboutProtocolUtils.h"
#include "nsIClassInfo.h"
#include "nsIURIFixup.h"
#include "nsCDefaultURIFixup.h"

static NS_DEFINE_CID(kZipReaderCID, NS_ZIPREADER_CID);

nsIIOService    *nsScriptSecurityManager::sIOService = nsnull;
nsIXPConnect    *nsScriptSecurityManager::sXPConnect = nsnull;
nsIStringBundle *nsScriptSecurityManager::sStrBundle = nsnull;
JSRuntime       *nsScriptSecurityManager::sRuntime   = 0;

///////////////////////////
// Convenience Functions //
///////////////////////////
// Result of this function should not be freed.
static inline const PRUnichar *
JSValIDToString(JSContext *cx, const jsval idval)
{
    JSAutoRequest ar(cx);
    JSString *str = JS_ValueToString(cx, idval);
    if(!str)
        return nsnull;
    return NS_REINTERPRET_CAST(PRUnichar*, JS_GetStringChars(str));
}

static nsIScriptContext *
GetScriptContext(JSContext *cx)
{
    return GetScriptContextFromJSContext(cx);
}

inline void SetPendingException(JSContext *cx, const char *aMsg)
{
    JSAutoRequest ar(cx);
    JSString *str = JS_NewStringCopyZ(cx, aMsg);
    if (str)
        JS_SetPendingException(cx, STRING_TO_JSVAL(str));
}

inline void SetPendingException(JSContext *cx, const PRUnichar *aMsg)
{
    JSAutoRequest ar(cx);
    JSString *str = JS_NewUCStringCopyZ(cx,
                        NS_REINTERPRET_CAST(const jschar*, aMsg));
    if (str)
        JS_SetPendingException(cx, STRING_TO_JSVAL(str));
}

// DomainPolicy members
#ifdef DEBUG_CAPS_DomainPolicyLifeCycle
PRUint32 DomainPolicy::sObjects=0;
void DomainPolicy::_printPopulationInfo()
{
    printf("CAPS.DomainPolicy: Gen. %d, %d DomainPolicy objects.\n",
        sGeneration, sObjects);
}
#endif
PRUint32 DomainPolicy::sGeneration = 0;

// Helper class to get stuff from the ClassInfo and not waste extra time with
// virtual method calls for things it has already gotten
class ClassInfoData
{
public:
    ClassInfoData(nsIClassInfo *aClassInfo, const char *aName)
        : mClassInfo(aClassInfo),
          mName(NS_CONST_CAST(char *, aName)),
          mDidGetFlags(PR_FALSE),
          mMustFreeName(PR_FALSE)
    {
    }

    ~ClassInfoData()
    {
        if (mMustFreeName)
            nsMemory::Free(mName);
    }

    PRUint32 GetFlags()
    {
        if (!mDidGetFlags) {
            if (mClassInfo) {
                nsresult rv = mClassInfo->GetFlags(&mFlags);
                if (NS_FAILED(rv)) {
                    mFlags = 0;
                }
            } else {
                mFlags = 0;
            }

            mDidGetFlags = PR_TRUE;
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

    const char* GetName()
    {
        if (!mName) {
            if (mClassInfo) {
                mClassInfo->GetClassDescription(&mName);
            }

            if (mName) {
                mMustFreeName = PR_TRUE;
            } else {
                mName = NS_CONST_CAST(char *, "UnnamedClass");
            }
        }

        return mName;
    }

private:
    nsIClassInfo *mClassInfo; // WEAK
    PRUint32 mFlags;
    char *mName;
    PRPackedBool mDidGetFlags;
    PRPackedBool mMustFreeName;
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

PRBool
nsScriptSecurityManager::SecurityCompareURIs(nsIURI* aSourceURI,
                                             nsIURI* aTargetURI)
{
    // Note that this is not an Equals() test on purpose -- for URIs that don't
    // support host/port, we want equality to basically be object identity, for
    // security purposes.  Otherwise, for example, two javascript: URIs that
    // are otherwise unrelated could end up "same origin", which would be
    // unfortunate.
    if (aSourceURI == aTargetURI)
    {
        return PR_TRUE;
    }

    if (!aTargetURI || !aSourceURI) 
    {
        return PR_FALSE;
    }

    // If either URI is a nested URI, get the base URI
    nsCOMPtr<nsIURI> sourceBaseURI = NS_GetInnermostURI(aSourceURI);
    
    nsCOMPtr<nsIURI> targetBaseURI = NS_GetInnermostURI(aTargetURI);

    if (!sourceBaseURI || !targetBaseURI)
        return PR_FALSE;

    // Compare schemes
    nsCAutoString targetScheme;
    nsresult rv = targetBaseURI->GetScheme(targetScheme);
    nsCAutoString sourceScheme;
    if (NS_SUCCEEDED(rv))
        rv = sourceBaseURI->GetScheme(sourceScheme);
    if (NS_FAILED(rv) || !targetScheme.Equals(sourceScheme)) {
        return PR_FALSE;
    }
    
    if (targetScheme.EqualsLiteral("file"))
    {
        // All file: urls are considered to have the same origin.
        return  PR_TRUE;
    }

    if (targetScheme.EqualsLiteral("imap") ||
        targetScheme.EqualsLiteral("mailbox") ||
        targetScheme.EqualsLiteral("news"))
    {
        // Each message is a distinct trust domain; use the 
        // whole spec for comparison
        nsCAutoString targetSpec;
        if (NS_FAILED(targetBaseURI->GetSpec(targetSpec)))
            return PR_FALSE;
        nsCAutoString sourceSpec;
        if (NS_FAILED(sourceBaseURI->GetSpec(sourceSpec)))
            return PR_FALSE;
        return targetSpec.Equals(sourceSpec);
    }

    // Compare hosts
    nsCAutoString targetHost;
    rv = targetBaseURI->GetHost(targetHost);
    nsCAutoString sourceHost;
    if (NS_SUCCEEDED(rv))
        rv = sourceBaseURI->GetHost(sourceHost);
    if (NS_FAILED(rv) ||
        !targetHost.Equals(sourceHost, nsCaseInsensitiveCStringComparator())) {
        return PR_FALSE;
    }
    
    // Compare ports
    PRInt32 targetPort;
    rv = targetBaseURI->GetPort(&targetPort);
    PRInt32 sourcePort;
    if (NS_SUCCEEDED(rv))
        rv = sourceBaseURI->GetPort(&sourcePort);
    PRBool result = NS_SUCCEEDED(rv) && targetPort == sourcePort;
    // If the port comparison failed, see if either URL has a
    // port of -1. If so, replace -1 with the default port
    // for that scheme.
    if (NS_SUCCEEDED(rv) && !result &&
        (sourcePort == -1 || targetPort == -1))
    {
        NS_ENSURE_STATE(sIOService);

        NS_ASSERTION(targetScheme.Equals(sourceScheme),
                     "Schemes should be equal here");
                    
        PRInt32 defaultPort;
        nsCOMPtr<nsIProtocolHandler> protocolHandler;
        rv = sIOService->GetProtocolHandler(sourceScheme.get(),
                                            getter_AddRefs(protocolHandler));
        if (NS_FAILED(rv))
        {
            return PR_FALSE;
        }
                    
        rv = protocolHandler->GetDefaultPort(&defaultPort);
        if (NS_FAILED(rv) || defaultPort == -1)
            return PR_FALSE; // No default port for this scheme

        if (sourcePort == -1)
            sourcePort = defaultPort;
        else if (targetPort == -1)
            targetPort = defaultPort;
        result = targetPort == sourcePort;
    }

    return result;
}

NS_IMETHODIMP
nsScriptSecurityManager::GetChannelPrincipal(nsIChannel* aChannel,
                                             nsIPrincipal** aPrincipal)
{
    NS_PRECONDITION(aChannel, "Must have channel!");
    nsCOMPtr<nsISupports> owner;
    aChannel->GetOwner(getter_AddRefs(owner));
    if (owner) {
        CallQueryInterface(owner, aPrincipal);
        if (*aPrincipal) {
            return NS_OK;
        }
    }

    // OK, get the principal from the URI.  Make sure this does the same thing
    // as nsDocument::Reset and nsXULDocument::StartDocumentLoad.
    nsCOMPtr<nsIURI> uri;
    nsLoadFlags loadFlags = 0;
    nsresult rv = aChannel->GetLoadFlags(&loadFlags);
    if (NS_SUCCEEDED(rv) && (loadFlags & nsIChannel::LOAD_REPLACE)) {
      aChannel->GetURI(getter_AddRefs(uri));
    } else {
      aChannel->GetOriginalURI(getter_AddRefs(uri));
    }

    return GetCodebasePrincipal(uri, aPrincipal);
}

////////////////////
// Policy Storage //
////////////////////

// Table of security levels
PR_STATIC_CALLBACK(PRBool)
DeleteCapability(nsHashKey *aKey, void *aData, void* closure)
{
    NS_Free(aData);
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
            return mOrigin.EqualsIgnoreCase(anOrigin, thisLen);

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
#if defined(DEBUG) || defined(DEBUG_CAPS_HACKER)
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
NS_IMPL_ISUPPORTS5(nsScriptSecurityManager,
                   nsIScriptSecurityManager,
                   nsIXPCSecurityManager,
                   nsIPrefSecurityCheck,
                   nsIChannelEventSink,
                   nsIObserver)

///////////////////////////////////////////////////
// Methods implementing nsIScriptSecurityManager //
///////////////////////////////////////////////////

///////////////// Security Checks /////////////////
JSBool JS_DLL_CALLBACK
nsScriptSecurityManager::CheckObjectAccess(JSContext *cx, JSObject *obj,
                                           jsval id, JSAccessMode mode,
                                           jsval *vp)
{
    // Get the security manager
    nsScriptSecurityManager *ssm =
        nsScriptSecurityManager::GetScriptSecurityManager();

    NS_ASSERTION(ssm, "Failed to get security manager service");
    if (!ssm)
        return JS_FALSE;

    // Get the object being accessed.  We protect these cases:
    // 1. The Function.prototype.caller property's value, which might lead
    //    an attacker up a call-stack to a function or another object from
    //    a different trust domain.
    // 2. A user-defined getter or setter function accessible on another
    //    trust domain's window or document object.
    // *vp can be a primitive, in that case, we use obj as the target
    // object.
    JSObject* target = JSVAL_IS_PRIMITIVE(*vp) ? obj : JSVAL_TO_OBJECT(*vp);

    // Do the same-origin check -- this sets a JS exception if the check fails.
    // Pass the parent object's class name, as we have no class-info for it.
    nsresult rv =
        ssm->CheckPropertyAccess(cx, target, JS_GetClass(cx, obj)->name, id,
                                 (mode & JSACC_WRITE) ?
                                 nsIXPCSecurityManager::ACCESS_SET_PROPERTY :
                                 nsIXPCSecurityManager::ACCESS_GET_PROPERTY);

    if (NS_FAILED(rv))
        return JS_FALSE; // Security check failed (XXX was an error reported?)

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

    JSAutoRequest ar(cx);

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
    nsIPrincipal* sourcePrincipal = GetSubjectPrincipal(cx, &rv);
    if (NS_FAILED(rv))
        return rv;

    if (!sourcePrincipal)
    {
        NS_WARNING("CheckSameOrigin called on script w/o principals; should this happen?");
        return NS_OK;
    }

    if (sourcePrincipal == mSystemPrincipal)
    {
        // This is a system (chrome) script, so allow access
        return NS_OK;
    }

    // Get the original URI from the source principal.
    // This has the effect of ignoring any change to document.domain
    // which must be done to avoid DNS spoofing (bug 154930)
    nsCOMPtr<nsIURI> sourceURI;
    sourcePrincipal->GetDomain(getter_AddRefs(sourceURI));
    if (!sourceURI) {
      sourcePrincipal->GetURI(getter_AddRefs(sourceURI));
      NS_ENSURE_TRUE(sourceURI, NS_ERROR_FAILURE);
    }

    // Compare origins
    if (!SecurityCompareURIs(sourceURI, aTargetURI))
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
    if (!SecurityCompareURIs(aSourceURI, aTargetURI))
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
    return CheckSameOriginPrincipalInternal(aSourcePrincipal,
                                            aTargetPrincipal,
                                            PR_FALSE);
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
    nsresult rv;
    nsIPrincipal* subjectPrincipal = GetSubjectPrincipal(cx, &rv);
    if (NS_FAILED(rv))
        return rv;

    if (!subjectPrincipal || subjectPrincipal == mSystemPrincipal)
        // We have native code or the system principal: just allow access
        return NS_OK;

    // Hold the class info data here so we don't have to go back to virtual
    // methods all the time
    ClassInfoData classInfoData(aClassInfo, aClassName);
#ifdef DEBUG_CAPS_CheckPropertyAccessImpl
    nsCAutoString propertyName;
    propertyName.AssignWithConversion((PRUnichar*)JSValIDToString(cx, aProperty));
    printf("### CanAccess(%s.%s, %i) ", classInfoData.GetName(), 
           propertyName.get(), aAction);
#endif

    //-- Look up the security policy for this class and subject domain
    SecurityLevel securityLevel;
    rv = LookupPolicy(subjectPrincipal, classInfoData, aProperty, aAction, 
                      (ClassPolicy**)aCachedClassPolicy, &securityLevel);
    if (NS_FAILED(rv))
        return rv;

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
#ifdef DEBUG_CAPS_CheckPropertyAccessImpl
            printf("noAccess ");
#endif
            rv = NS_ERROR_DOM_PROP_ACCESS_DENIED;
            break;

        case SCRIPT_SECURITY_ALL_ACCESS:
#ifdef DEBUG_CAPS_CheckPropertyAccessImpl
            printf("allAccess ");
#endif
            rv = NS_OK;
            break;

        case SCRIPT_SECURITY_SAME_ORIGIN_ACCESS:
            {
#ifdef DEBUG_CAPS_CheckPropertyAccessImpl
                printf("sameOrigin ");
#endif
                nsCOMPtr<nsIPrincipal> objectPrincipal;
                if(aJSObject)
                {
                    objectPrincipal = doGetObjectPrincipal(cx, aJSObject);
                    if (!objectPrincipal)
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
                rv = CheckSameOriginDOMProp(subjectPrincipal, objectPrincipal,
                                            aAction, aTargetURI != nsnull);
                break;
            }
        default:
#ifdef DEBUG_CAPS_CheckPropertyAccessImpl
                printf("ERROR ");
#endif
            NS_ERROR("Bad Security Level Value");
            return NS_ERROR_FAILURE;
        }
    }
    else // if SECURITY_ACCESS_LEVEL_FLAG is false, securityLevel is a capability
    {
#ifdef DEBUG_CAPS_CheckPropertyAccessImpl
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
#ifdef DEBUG_CAPS_CheckPropertyAccessImpl
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
#ifdef DEBUG_CAPS_CheckPropertyAccessImpl
    if(NS_SUCCEEDED(rv))
        printf("CheckXPCPerms GRANTED.\n");
    else
        printf("CheckXPCPerms DENIED.\n");
#endif

    if (NS_FAILED(rv)) //-- Security tests failed, access is denied, report error
    {
        nsAutoString stringName;
        switch(aAction)
        {
        case nsIXPCSecurityManager::ACCESS_GET_PROPERTY:
            stringName.AssignLiteral("GetPropertyDenied");
            break;
        case nsIXPCSecurityManager::ACCESS_SET_PROPERTY:
            stringName.AssignLiteral("SetPropertyDenied");
            break;
        case nsIXPCSecurityManager::ACCESS_CALL_METHOD:
            stringName.AssignLiteral("CallMethodDenied");
        }

        NS_ConvertUTF8toUTF16 className(classInfoData.GetName());
        const PRUnichar *formatStrings[] =
        {
            className.get(),
            JSValIDToString(cx, aProperty)
        };

        nsXPIDLString errorMsg;
        // We need to keep our existing failure rv and not override it
        // with a likely success code from the following string bundle
        // call in order to throw the correct security exception later.
        nsresult rv2 = sStrBundle->FormatStringFromName(stringName.get(),
                                                        formatStrings,
                                                        NS_ARRAY_LENGTH(formatStrings),
                                                        getter_Copies(errorMsg));
        NS_ENSURE_SUCCESS(rv2, rv2);

        SetPendingException(cx, errorMsg.get());

        if (sXPConnect)
        {
            nsCOMPtr<nsIXPCNativeCallContext> xpcCallContext;
            sXPConnect->GetCurrentNativeCallContext(getter_AddRefs(xpcCallContext));
            if (xpcCallContext)
                xpcCallContext->SetExceptionWasThrown(PR_TRUE);
        }
    }

    return rv;
}

nsresult
nsScriptSecurityManager::CheckSameOriginPrincipalInternal(nsIPrincipal* aSubject,
                                                          nsIPrincipal* aObject,
                                                          PRBool aIsCheckConnect)
{
    /*
    ** Get origin of subject and object and compare.
    */
    if (aSubject == aObject)
        return NS_OK;

    // These booleans are only used when !aIsCheckConnect.  Default
    // them to false, and change if that turns out wrong.
    PRBool subjectSetDomain = PR_FALSE;
    PRBool objectSetDomain = PR_FALSE;
    
    nsCOMPtr<nsIURI> subjectURI;
    nsCOMPtr<nsIURI> objectURI;

    if (aIsCheckConnect)
    {
        // Don't use domain for CheckConnect calls, since that's called for
        // data-only load checks like XMLHTTPRequest (bug 290100).
        aSubject->GetURI(getter_AddRefs(subjectURI));
        aObject->GetURI(getter_AddRefs(objectURI));
    }
    else
    {
        aSubject->GetDomain(getter_AddRefs(subjectURI));
        if (!subjectURI) {
            aSubject->GetURI(getter_AddRefs(subjectURI));
        } else {
            subjectSetDomain = PR_TRUE;
        }

        aObject->GetDomain(getter_AddRefs(objectURI));
        if (!objectURI) {
            aObject->GetURI(getter_AddRefs(objectURI));
        } else {
            objectSetDomain = PR_TRUE;
        }
    }

    if (SecurityCompareURIs(subjectURI, objectURI))
    {   // If either the subject or the object has changed its principal by
        // explicitly setting document.domain then the other must also have
        // done so in order to be considered the same origin. This prevents
        // DNS spoofing based on document.domain (154930)

        // But this restriction does not apply to CheckConnect calls, since
        // that's called for data-only load checks like XMLHTTPRequest where
        // we ignore domain (bug 290100).
        if (aIsCheckConnect)
            return NS_OK;

        // If both or neither explicitly set their domain, allow the access
        if (subjectSetDomain == objectSetDomain)
            return NS_OK;
    }

    /*
    ** Access tests failed, so now report error.
    */
    return NS_ERROR_DOM_PROP_ACCESS_DENIED;
}


nsresult
nsScriptSecurityManager::CheckSameOriginDOMProp(nsIPrincipal* aSubject,
                                                nsIPrincipal* aObject,
                                                PRUint32 aAction,
                                                PRBool aIsCheckConnect)
{
    nsresult rv = CheckSameOriginPrincipalInternal(aSubject, aObject,
                                                   aIsCheckConnect);
    
    if (NS_SUCCEEDED(rv))
        return NS_OK;

    /*
    * Content can't ever touch chrome (we check for UniversalXPConnect later)
    */
    if (aObject == mSystemPrincipal)
        return NS_ERROR_DOM_PROP_ACCESS_DENIED;

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
nsScriptSecurityManager::LookupPolicy(nsIPrincipal* aPrincipal,
                                      ClassInfoData& aClassData,
                                      jsval aProperty,
                                      PRUint32 aAction,
                                      ClassPolicy** aCachedClassPolicy,
                                      SecurityLevel* result)
{
    nsresult rv;
    result->level = SCRIPT_SECURITY_UNDEFINED_ACCESS;

    DomainPolicy* dpolicy = nsnull;
    //-- Initialize policies if necessary
    if (mPolicyPrefsChanged)
    {
        rv = InitPolicies();
        if (NS_FAILED(rv))
            return rv;
    }
    else
    {
        aPrincipal->GetSecurityPolicy((void**)&dpolicy);
    }

    if (!dpolicy && mOriginToPolicyMap)
    {
        //-- Look up the relevant domain policy, if any
#ifdef DEBUG_CAPS_LookupPolicy
        printf("DomainLookup ");
#endif

        nsXPIDLCString origin;
        if (NS_FAILED(rv = aPrincipal->GetOrigin(getter_Copies(origin))))
            return rv;
 
        char *start = origin.BeginWriting();
        const char *nextToLastDot = nsnull;
        const char *lastDot = nsnull;
        const char *colon = nsnull;
        char *p = start;

        //-- search domain (stop at the end of the string or at the 3rd slash)
        for (PRUint32 slashes=0; *p; p++)
        {
            if (*p == '/' && ++slashes == 3) 
            {
                *p = '\0'; // truncate at 3rd slash
                break;
            }
            if (*p == '.')
            {
                nextToLastDot = lastDot;
                lastDot = p;
            } 
            else if (!colon && *p == ':')
                colon = p;
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

        if (!dpolicy)
            dpolicy = mDefaultPolicy;

        aPrincipal->SetSecurityPolicy((void*)dpolicy);
    }

    ClassPolicy* cpolicy = nsnull;

    if ((dpolicy == mDefaultPolicy) && aCachedClassPolicy)
    {
        // No per-domain policy for this principal (the more common case)
        // so look for a cached class policy from the object wrapper
        cpolicy = *aCachedClassPolicy;
    }

    if (!cpolicy)
    { //-- No cached policy for this class, need to look it up
#ifdef DEBUG_CAPS_LookupPolicy
        printf("ClassLookup ");
#endif

        cpolicy = NS_STATIC_CAST(ClassPolicy*,
                                 PL_DHashTableOperate(dpolicy,
                                                      aClassData.GetName(),
                                                      PL_DHASH_LOOKUP));

        if (PL_DHASH_ENTRY_IS_FREE(cpolicy))
            cpolicy = NO_POLICY_FOR_CLASS;

        if ((dpolicy == mDefaultPolicy) && aCachedClassPolicy)
            *aCachedClassPolicy = cpolicy;
    }

    // We look for a PropertyPolicy in the following places:
    // 1)  The ClassPolicy for our class we got from our DomainPolicy
    // 2)  The mWildcardPolicy of our DomainPolicy
    // 3)  The ClassPolicy for our class we got from mDefaultPolicy
    // 4)  The mWildcardPolicy of our mDefaultPolicy
    PropertyPolicy* ppolicy = nsnull;
    if (cpolicy != NO_POLICY_FOR_CLASS)
    {
        ppolicy = NS_STATIC_CAST(PropertyPolicy*,
                                 PL_DHashTableOperate(cpolicy->mPolicy,
                                                      (void*)aProperty,
                                                      PL_DHASH_LOOKUP));
    }

    // If there is no class policy for this property, and we have a wildcard
    // policy, try that.
    if (dpolicy->mWildcardPolicy &&
        (!ppolicy || PL_DHASH_ENTRY_IS_FREE(ppolicy)))
    {
        ppolicy =
            NS_STATIC_CAST(PropertyPolicy*,
                           PL_DHashTableOperate(dpolicy->mWildcardPolicy->mPolicy,
                                                (void*)aProperty,
                                                PL_DHASH_LOOKUP));
    }

    // If dpolicy is not the defauly policy and there's no class or wildcard
    // policy for this property, check the default policy for this class and
    // the default wildcard policy
    if (dpolicy != mDefaultPolicy &&
        (!ppolicy || PL_DHASH_ENTRY_IS_FREE(ppolicy)))
    {
        cpolicy = NS_STATIC_CAST(ClassPolicy*,
                                 PL_DHashTableOperate(mDefaultPolicy,
                                                      aClassData.GetName(),
                                                      PL_DHASH_LOOKUP));

        if (PL_DHASH_ENTRY_IS_BUSY(cpolicy))
        {
            ppolicy =
                NS_STATIC_CAST(PropertyPolicy*,
                               PL_DHashTableOperate(cpolicy->mPolicy,
                                                    (void*)aProperty,
                                                    PL_DHASH_LOOKUP));
        }

        if ((!ppolicy || PL_DHASH_ENTRY_IS_FREE(ppolicy)) &&
            mDefaultPolicy->mWildcardPolicy)
        {
            ppolicy =
              NS_STATIC_CAST(PropertyPolicy*,
                             PL_DHashTableOperate(mDefaultPolicy->mWildcardPolicy->mPolicy,
                                                  (void*)aProperty,
                                                  PL_DHASH_LOOKUP));
        }
    }

    if (!ppolicy || PL_DHASH_ENTRY_IS_FREE(ppolicy))
        return NS_OK;

    // Get the correct security level from the property policy
    if (aAction == nsIXPCSecurityManager::ACCESS_SET_PROPERTY)
        *result = ppolicy->mSet;
    else
        *result = ppolicy->mGet;

    return NS_OK;
}


NS_IMETHODIMP
nsScriptSecurityManager::CheckLoadURIFromScript(JSContext *cx, nsIURI *aURI)
{
    // Get principal of currently executing script.
    nsresult rv;
    nsIPrincipal* principal = GetSubjectPrincipal(cx, &rv);
    if (NS_FAILED(rv))
        return rv;

    // Native code can load all URIs.
    if (!principal)
        return NS_OK;

    rv = CheckLoadURIWithPrincipal(principal, aURI,
                                   nsIScriptSecurityManager::STANDARD);
    if (NS_SUCCEEDED(rv)) {
        // OK to load
        return NS_OK;
    }

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

NS_IMETHODIMP
nsScriptSecurityManager::CheckLoadURI(nsIURI *aSourceURI, nsIURI *aTargetURI,
                                      PRUint32 aFlags)
{
    // FIXME: bug 327244 -- this function should really die...  Really truly.
    NS_PRECONDITION(aSourceURI, "CheckLoadURI called with null source URI");
    NS_ENSURE_ARG_POINTER(aSourceURI);

    // Note: this is not _quite_ right if aSourceURI has
    // NS_NULLPRINCIPAL_SCHEME, but we'll just extract the scheme in
    // CheckLoadURIWithPrincipal anyway, so this is good enough.  This method
    // really needs to go away....
    nsCOMPtr<nsIPrincipal> sourcePrincipal;
    nsresult rv = CreateCodebasePrincipal(aSourceURI,
                                          getter_AddRefs(sourcePrincipal));
    NS_ENSURE_SUCCESS(rv, rv);
    return CheckLoadURIWithPrincipal(sourcePrincipal, aTargetURI, aFlags);
}

/**
 * Helper method to handle cases where a flag passed to
 * CheckLoadURIWithPrincipal means denying loading if the given URI has certain
 * nsIProtocolHandler flags set.
 * @return if success, access is allowed. Otherwise, deny access
 */
static nsresult
DenyAccessIfURIHasFlags(nsIURI* aURI, PRUint32 aURIFlags)
{
    NS_PRECONDITION(aURI, "Must have URI!");
    
    PRBool uriHasFlags;
    nsresult rv =
        NS_URIChainHasFlags(aURI, aURIFlags, &uriHasFlags);
    NS_ENSURE_SUCCESS(rv, rv);

    if (uriHasFlags) {
        return NS_ERROR_DOM_BAD_URI;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::CheckLoadURIWithPrincipal(nsIPrincipal* aPrincipal,
                                                   nsIURI *aTargetURI,
                                                   PRUint32 aFlags)
{
    NS_PRECONDITION(aPrincipal, "CheckLoadURIWithPrincipal must have a principal");
    // If someone passes a flag that we don't understand, we should
    // fail, because they may need a security check that we don't
    // provide.
    NS_ENSURE_FALSE(aFlags & ~(nsIScriptSecurityManager::LOAD_IS_AUTOMATIC_DOCUMENT_REPLACEMENT |
                               nsIScriptSecurityManager::ALLOW_CHROME |
                               nsIScriptSecurityManager::DISALLOW_SCRIPT |
                               nsIScriptSecurityManager::DISALLOW_INHERIT_PRINCIPAL),
                    NS_ERROR_UNEXPECTED);
    NS_ENSURE_ARG_POINTER(aPrincipal);

    if (aPrincipal == mSystemPrincipal) {
        // Allow access
        return NS_OK;
    }
    
    nsCOMPtr<nsIURI> sourceURI;
    aPrincipal->GetURI(getter_AddRefs(sourceURI));

    NS_ASSERTION(sourceURI, "Non-system principals passed to CheckLoadURIWithPrincipal must have a URI!");
    
    // Automatic loads are not allowed from certain protocols.
    if (aFlags & nsIScriptSecurityManager::LOAD_IS_AUTOMATIC_DOCUMENT_REPLACEMENT) {
        nsresult rv =
            DenyAccessIfURIHasFlags(sourceURI,
                                    nsIProtocolHandler::URI_FORBIDS_AUTOMATIC_DOCUMENT_REPLACEMENT);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    // If DISALLOW_INHERIT_PRINCIPAL is set, we prevent loading of URIs which
    // would do such inheriting.  That would be URIs that do not have their own
    // security context.
    if (aFlags & nsIScriptSecurityManager::DISALLOW_INHERIT_PRINCIPAL) {
        nsresult rv =
            DenyAccessIfURIHasFlags(aTargetURI,
                                    nsIProtocolHandler::URI_INHERITS_SECURITY_CONTEXT);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    // If either URI is a nested URI, get the base URI
    nsCOMPtr<nsIURI> sourceBaseURI = NS_GetInnermostURI(sourceURI);
    nsCOMPtr<nsIURI> targetBaseURI = NS_GetInnermostURI(aTargetURI);

    //-- get the target scheme
    nsCAutoString targetScheme;
    nsresult rv = targetBaseURI->GetScheme(targetScheme);
    if (NS_FAILED(rv)) return rv;

    //-- Some callers do not allow loading javascript:
    if ((aFlags & nsIScriptSecurityManager::DISALLOW_SCRIPT) &&
         targetScheme.EqualsLiteral("javascript"))
    {
       return NS_ERROR_DOM_BAD_URI;
    }

    //-- get the source scheme
    nsCAutoString sourceScheme;
    rv = sourceBaseURI->GetScheme(sourceScheme);
    if (NS_FAILED(rv)) return rv;

    if (targetScheme.Equals(sourceScheme,
                            nsCaseInsensitiveCStringComparator()) &&
        !sourceScheme.LowerCaseEqualsLiteral(NS_NULLPRINCIPAL_SCHEME))
    {
        // every scheme can access another URI from the same scheme,
        // as long as they don't represent null principals.
        return NS_OK;
    }

    NS_NAMED_LITERAL_STRING(errorTag, "CheckLoadURIError");
    
    // If the schemes don't match, the policy is specified by the protocol
    // flags on the target URI.  Note that the order of policy checks here is
    // very important!  We start from most restrictive and work our way down.
    // Note that since we're working with the innermost URI, we can just use
    // the methods that work on chains of nested URIs and they will only look
    // at the flags for our one URI.

    // Check for system target URI
    rv = DenyAccessIfURIHasFlags(targetBaseURI,
                                 nsIProtocolHandler::URI_DANGEROUS_TO_LOAD);
    if (NS_FAILED(rv)) {
        // Deny access, since the origin principal is not system
        ReportError(nsnull, errorTag, sourceURI, aTargetURI);
        return rv;
    }

    // Check for chrome target URI
    PRBool hasFlags;
    rv = NS_URIChainHasFlags(targetBaseURI,
                             nsIProtocolHandler::URI_IS_UI_RESOURCE,
                             &hasFlags);
    NS_ENSURE_SUCCESS(rv, rv);
    if (hasFlags) {
        if (aFlags & nsIScriptSecurityManager::ALLOW_CHROME) {
            return NS_OK;
        }

        // resource: and chrome: are equivalent, securitywise
        // That's bogus!!  Fix this.  But watch out for
        // the view-source stylesheet?
        PRBool sourceIsChrome;
        rv = NS_URIChainHasFlags(sourceBaseURI,
                                 nsIProtocolHandler::URI_IS_UI_RESOURCE,
                                 &sourceIsChrome);
        NS_ENSURE_SUCCESS(rv, rv);
        if (sourceIsChrome) {
            return NS_OK;
        }
        ReportError(nsnull, errorTag, sourceURI, aTargetURI);
        return NS_ERROR_DOM_BAD_URI;
    }

    // Check for target URI pointing to a file
    rv = NS_URIChainHasFlags(targetBaseURI,
                             nsIProtocolHandler::URI_IS_LOCAL_FILE,
                             &hasFlags);
    NS_ENSURE_SUCCESS(rv, rv);
    if (hasFlags) {
        // resource: and chrome: are equivalent, securitywise
        // That's bogus!!  Fix this.  But watch out for
        // the view-source stylesheet?
        PRBool sourceIsChrome;
        rv = NS_URIChainHasFlags(sourceURI,
                                 nsIProtocolHandler::URI_IS_UI_RESOURCE,
                                 &sourceIsChrome);
        NS_ENSURE_SUCCESS(rv, rv);
        if (sourceIsChrome) {
            return NS_OK;
        }

        // Now check capability policies
        static const char loadURIPrefGroup[] = "checkloaduri";
        ClassInfoData nameData(nsnull, loadURIPrefGroup);

        SecurityLevel secLevel;
        rv = LookupPolicy(aPrincipal, nameData, sEnabledID,
                          nsIXPCSecurityManager::ACCESS_GET_PROPERTY, 
                          nsnull, &secLevel);
        if (NS_SUCCEEDED(rv) && secLevel.level == SCRIPT_SECURITY_ALL_ACCESS)
        {
            // OK for this site!
            return NS_OK;
        }

        ReportError(nsnull, errorTag, sourceURI, aTargetURI);
        return NS_ERROR_DOM_BAD_URI;
    }

    // OK, everyone is allowed to load this, since unflagged handlers are
    // deprecated but treated as URI_LOADABLE_BY_ANYONE.  But check whether we
    // need to warn.  At some point we'll want to make this warning into an
    // error and treat unflagged handlers as URI_DANGEROUS_TO_LOAD.
    rv = NS_URIChainHasFlags(targetBaseURI,
                             nsIProtocolHandler::URI_LOADABLE_BY_ANYONE,
                             &hasFlags);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!hasFlags) {
        nsXPIDLString message;
        NS_ConvertASCIItoUTF16 ucsTargetScheme(targetScheme);
        const PRUnichar* formatStrings[] = { ucsTargetScheme.get() };
        rv = sStrBundle->
            FormatStringFromName(NS_LITERAL_STRING("ProtocolFlagError").get(),
                                 formatStrings,
                                 NS_ARRAY_LENGTH(formatStrings),
                                 getter_Copies(message));
        if (NS_SUCCEEDED(rv)) {
            nsCOMPtr<nsIConsoleService> console(
              do_GetService("@mozilla.org/consoleservice;1"));
            NS_ENSURE_TRUE(console, NS_ERROR_FAILURE);

            console->LogStringMessage(message.get());
#ifdef DEBUG
            fprintf(stderr, "%s\n", NS_ConvertUTF16toUTF8(message).get());
#endif
        }
    }
    
    return NS_OK;
}

nsresult
nsScriptSecurityManager::ReportError(JSContext* cx, const nsAString& messageTag,
                                     nsIURI* aSource, nsIURI* aTarget)
{
    nsresult rv;
    NS_ENSURE_TRUE(aSource && aTarget, NS_ERROR_NULL_POINTER);

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
    NS_ConvertASCIItoUTF16 ucsSourceSpec(sourceSpec);
    NS_ConvertASCIItoUTF16 ucsTargetSpec(targetSpec);
    const PRUnichar *formatStrings[] = { ucsSourceSpec.get(), ucsTargetSpec.get() };
    rv = sStrBundle->FormatStringFromName(PromiseFlatString(messageTag).get(),
                                          formatStrings,
                                          NS_ARRAY_LENGTH(formatStrings),
                                          getter_Copies(message));
    NS_ENSURE_SUCCESS(rv, rv);

    // If a JS context was passed in, set a JS exception.
    // Otherwise, print the error message directly to the JS console
    // and to standard output
    if (cx)
    {
        SetPendingException(cx, message.get());
        // Tell XPConnect that an exception was thrown, if appropriate
        if (sXPConnect)
        {
            nsCOMPtr<nsIXPCNativeCallContext> xpcCallContext;
            sXPConnect->GetCurrentNativeCallContext(getter_AddRefs(xpcCallContext));
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
        fprintf(stderr, "%s\n", NS_LossyConvertUTF16toASCII(message).get());
#endif
    }
    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::CheckLoadURIStr(const nsACString& aSourceURIStr,
                                         const nsACString& aTargetURIStr,
                                         PRUint32 aFlags)
{
    // FIXME: bug 327244 -- this function should really die...  Really truly.
    nsCOMPtr<nsIURI> source;
    nsresult rv = NS_NewURI(getter_AddRefs(source), aSourceURIStr,
                            nsnull, nsnull, sIOService);
    NS_ENSURE_SUCCESS(rv, rv);

    // Note: this is not _quite_ right if aSourceURI has
    // NS_NULLPRINCIPAL_SCHEME, but we'll just extract the scheme in
    // CheckLoadURIWithPrincipal anyway, so this is good enough.  This method
    // really needs to go away....
    nsCOMPtr<nsIPrincipal> sourcePrincipal;
    rv = CreateCodebasePrincipal(source,
                                 getter_AddRefs(sourcePrincipal));
    NS_ENSURE_SUCCESS(rv, rv);

    return CheckLoadURIStrWithPrincipal(sourcePrincipal, aTargetURIStr,
                                        aFlags);
}

NS_IMETHODIMP
nsScriptSecurityManager::CheckLoadURIStrWithPrincipal(nsIPrincipal* aPrincipal,
                                                      const nsACString& aTargetURIStr,
                                                      PRUint32 aFlags)
{
    nsresult rv;
    nsCOMPtr<nsIURI> target;
    rv = NS_NewURI(getter_AddRefs(target), aTargetURIStr,
                   nsnull, nsnull, sIOService);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = CheckLoadURIWithPrincipal(aPrincipal, target, aFlags);
    NS_ENSURE_SUCCESS(rv, rv);

    // Now start testing fixup -- since aTargetURIStr is a string, not
    // an nsIURI, we may well end up fixing it up before loading.
    // Note: This needs to stay in sync with the nsIURIFixup api.
    nsCOMPtr<nsIURIFixup> fixup = do_GetService(NS_URIFIXUP_CONTRACTID);
    if (!fixup) {
        return rv;
    }

    PRUint32 flags[] = {
        nsIURIFixup::FIXUP_FLAG_NONE,
        nsIURIFixup::FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP,
        nsIURIFixup::FIXUP_FLAGS_MAKE_ALTERNATE_URI,
        nsIURIFixup::FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP |
        nsIURIFixup::FIXUP_FLAGS_MAKE_ALTERNATE_URI
    };

    for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(flags); ++i) {
        rv = fixup->CreateFixupURI(aTargetURIStr, flags[i],
                                   getter_AddRefs(target));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = CheckLoadURIWithPrincipal(aPrincipal, target, aFlags);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    return rv;
}

NS_IMETHODIMP
nsScriptSecurityManager::CheckFunctionAccess(JSContext *aCx, void *aFunObj,
                                             void *aTargetObj)
{
    // This check is called for event handlers
    nsresult rv;
    nsIPrincipal* subject =
        GetFunctionObjectPrincipal(aCx, (JSObject *)aFunObj, nsnull, &rv);

    // If subject is null, get a principal from the function object's scope.
    if (NS_SUCCEEDED(rv) && !subject)
    {
#ifdef DEBUG
        {
            JSFunction *fun =
                (JSFunction *)JS_GetPrivate(aCx, (JSObject *)aFunObj);
            JSScript *script = JS_GetFunctionScript(aCx, fun);

            NS_ASSERTION(!script, "Null principal for non-native function!");
        }
#endif

        subject = doGetObjectPrincipal(aCx, (JSObject*)aFunObj);
    }

    if (!subject)
        return NS_ERROR_FAILURE;

    if (subject == mSystemPrincipal)
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
    nsIPrincipal* object = doGetObjectPrincipal(aCx, obj);

    if (!object)
        return NS_ERROR_FAILURE;        

    // Note that CheckSameOriginPrincipalInternal already does an equality
    // comparison on subject and object, so no need for us to do it.
    return CheckSameOriginPrincipalInternal(subject, object, PR_TRUE);
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

    //-- See if the current window allows JS execution
    nsIScriptContext *scriptContext = GetScriptContext(cx);
    if (!scriptContext) return NS_ERROR_FAILURE;

    if (!scriptContext->GetScriptsEnabled()) {
        // No scripting on this context, folks
        *result = PR_FALSE;
        return NS_OK;
    }
    
    nsIScriptGlobalObject *sgo = scriptContext->GetGlobalObject();

    if (!sgo) {
        return NS_ERROR_FAILURE;
    }

    // window can be null here if we're running with a non-DOM window
    // as the script global (i.e. a XUL prototype document).
    nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(sgo);
    nsCOMPtr<nsIDocShell> docshell;
    nsresult rv;

    if (window) {
        docshell = window->GetDocShell();
    }

    nsCOMPtr<nsIDocShellTreeItem> globalObjTreeItem =
        do_QueryInterface(docshell);

    if (globalObjTreeItem) 
    {
        nsCOMPtr<nsIDocShellTreeItem> treeItem(globalObjTreeItem);
        nsCOMPtr<nsIDocShellTreeItem> parentItem;

        // Walk up the docshell tree to see if any containing docshell disallows scripts
        do
        {
            rv = docshell->GetAllowJavascript(result);
            if (NS_FAILED(rv)) return rv;
            if (!*result)
                return NS_OK; // Do not run scripts
            treeItem->GetParent(getter_AddRefs(parentItem));
            treeItem.swap(parentItem);
            docshell = do_QueryInterface(treeItem);
#ifdef DEBUG
            if (treeItem && !docshell) {
              NS_ERROR("cannot get a docshell from a treeItem!");
            }
#endif // DEBUG
        } while (treeItem && docshell);
    }

    // OK, the docshell doesn't have script execution explicitly disabled.
    // Check whether our URI is an "about:" URI that allows scripts.  If it is,
    // we need to allow JS to run.  In this case, don't apply the JS enabled
    // pref or policies.  On failures, just press on and don't do this special
    // case.
    nsCOMPtr<nsIURI> principalURI;
    aPrincipal->GetURI(getter_AddRefs(principalURI));
    if (principalURI)
    {
        PRBool isAbout;
        rv = principalURI->SchemeIs("about", &isAbout);
        if (NS_SUCCEEDED(rv) && isAbout) {
            nsCOMPtr<nsIAboutModule> module;
            rv = NS_GetAboutModule(principalURI, getter_AddRefs(module));
            if (NS_SUCCEEDED(rv)) {
                PRUint32 flags;
                rv = module->GetURIFlags(principalURI, &flags);
                if (NS_SUCCEEDED(rv) &&
                    (flags & nsIAboutModule::ALLOW_SCRIPT)) {
                    *result = PR_TRUE;
                    return NS_OK;              
                }
            }
        }
    }

    //-- See if JS is disabled globally (via prefs)
    *result = mIsJavaScriptEnabled;
    if (mIsJavaScriptEnabled != mIsMailJavaScriptEnabled && globalObjTreeItem) 
    {
        nsCOMPtr<nsIDocShellTreeItem> rootItem;
        globalObjTreeItem->GetRootTreeItem(getter_AddRefs(rootItem));
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

    if (!*result)
        return NS_OK; // Do not run scripts

    //-- Check for a per-site policy
    static const char jsPrefGroupName[] = "javascript";
    ClassInfoData nameData(nsnull, jsPrefGroupName);

    SecurityLevel secLevel;
    rv = LookupPolicy(aPrincipal, nameData, sEnabledID,
                      nsIXPCSecurityManager::ACCESS_GET_PROPERTY, 
                      nsnull, &secLevel);
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
nsScriptSecurityManager::GetSubjectPrincipal(nsIPrincipal **aSubjectPrincipal)
{
    nsresult rv;
    *aSubjectPrincipal = doGetSubjectPrincipal(&rv);
    if (NS_SUCCEEDED(rv))
        NS_IF_ADDREF(*aSubjectPrincipal);
    return rv;
}

nsIPrincipal*
nsScriptSecurityManager::doGetSubjectPrincipal(nsresult* rv)
{
    NS_PRECONDITION(rv, "Null out param");
    JSContext *cx = GetCurrentJSContext();
    if (!cx)
    {
        *rv = NS_OK;
        return nsnull;
    }
    return GetSubjectPrincipal(cx, rv);
}

NS_IMETHODIMP
nsScriptSecurityManager::GetSystemPrincipal(nsIPrincipal **result)
{
    NS_ADDREF(*result = mSystemPrincipal);

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
nsScriptSecurityManager::GetCertificatePrincipal(const nsACString& aCertFingerprint,
                                                 const nsACString& aSubjectName,
                                                 const nsACString& aPrettyName,
                                                 nsISupports* aCertificate,
                                                 nsIURI* aURI,
                                                 nsIPrincipal **result)
{
    *result = nsnull;
    
    NS_ENSURE_ARG(!aCertFingerprint.IsEmpty() &&
                  !aSubjectName.IsEmpty() &&
                  aCertificate);

    return DoGetCertificatePrincipal(aCertFingerprint, aSubjectName,
                                     aPrettyName, aCertificate, aURI, PR_TRUE,
                                     result);
}
    
nsresult
nsScriptSecurityManager::DoGetCertificatePrincipal(const nsACString& aCertFingerprint,
                                                   const nsACString& aSubjectName,
                                                   const nsACString& aPrettyName,
                                                   nsISupports* aCertificate,
                                                   nsIURI* aURI,
                                                   PRBool aModifyTable,
                                                   nsIPrincipal **result)
{
    NS_ENSURE_ARG(!aCertFingerprint.IsEmpty());
    
    // Create a certificate principal out of the certificate ID
    // and URI given to us.  We will use this principal to test
    // equality when doing our hashtable lookups below.
    nsRefPtr<nsPrincipal> certificate = new nsPrincipal();
    if (!certificate)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = certificate->Init(aCertFingerprint, aSubjectName,
                                    aPrettyName, aCertificate, aURI);
    NS_ENSURE_SUCCESS(rv, rv);

    // Check to see if we already have this principal.
    nsCOMPtr<nsIPrincipal> fromTable;
    mPrincipals.Get(certificate, getter_AddRefs(fromTable));
    if (fromTable) {
        // Bingo.  We found the certificate in the table, which means
        // that it has escalated privileges.

        if (aModifyTable) {
            // Make sure this principal has names, so if we ever go to save it
            // we'll save them.  If we get a name mismatch here we'll throw,
            // but that's desirable.
            rv = NS_STATIC_CAST(nsPrincipal*,
                                NS_STATIC_CAST(nsIPrincipal*, fromTable))
                ->EnsureCertData(aSubjectName, aPrettyName, aCertificate);
            if (NS_FAILED(rv)) {
                // We have a subject name mismatch for the same cert id.
                // Hand back the |certificate| object we created and don't give
                // it any rights from the table.
                NS_ADDREF(*result = certificate);
                return NS_OK;
            }                
        }
        
        if (!aURI) {
            // We were asked to just get the base certificate, so output
            // what we have in the table.
            certificate = NS_STATIC_CAST(nsPrincipal*,
                                         NS_STATIC_CAST(nsIPrincipal*,
                                                        fromTable));
        } else {
            // We found a certificate and now need to install a codebase
            // on it.  We don't want to modify the principal in the hash
            // table, so create a new principal and clone the pertinent
            // things.
            nsXPIDLCString prefName;
            nsXPIDLCString id;
            nsXPIDLCString subjectName;
            nsXPIDLCString granted;
            nsXPIDLCString denied;
            rv = fromTable->GetPreferences(getter_Copies(prefName),
                                           getter_Copies(id),
                                           getter_Copies(subjectName),
                                           getter_Copies(granted),
                                           getter_Copies(denied));
            // XXXbz assert something about subjectName and aSubjectName here?
            if (NS_SUCCEEDED(rv)) {
                certificate = new nsPrincipal();
                if (!certificate)
                    return NS_ERROR_OUT_OF_MEMORY;

                rv = certificate->InitFromPersistent(prefName, id,
                                                     subjectName, aPrettyName,
                                                     granted, denied,
                                                     aCertificate,
                                                     PR_TRUE, PR_FALSE);
                if (NS_SUCCEEDED(rv))
                    certificate->SetURI(aURI);
            }
        }
    }

    NS_ADDREF(*result = certificate);

    return rv;
}

nsresult
nsScriptSecurityManager::CreateCodebasePrincipal(nsIURI* aURI, nsIPrincipal **result)
{
    // I _think_ it's safe to not create null principals here based on aURI.
    // At least all the callers would do the right thing in those cases, as far
    // as I can tell.  --bz
    nsRefPtr<nsPrincipal> codebase = new nsPrincipal();
    if (!codebase)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = codebase->Init(EmptyCString(), EmptyCString(),
                                 EmptyCString(), nsnull, aURI);
    if (NS_FAILED(rv))
        return rv;

    NS_ADDREF(*result = codebase);

    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::GetCodebasePrincipal(nsIURI *aURI,
                                              nsIPrincipal **result)
{
    PRBool inheritsPrincipal;
    nsresult rv =
        NS_URIChainHasFlags(aURI,
                            nsIProtocolHandler::URI_INHERITS_SECURITY_CONTEXT,
                            &inheritsPrincipal);
    if (NS_FAILED(rv) || inheritsPrincipal) {
        return CallCreateInstance(NS_NULLPRINCIPAL_CONTRACTID, result);
    }
    
    nsCOMPtr<nsIPrincipal> principal;
    rv = CreateCodebasePrincipal(aURI, getter_AddRefs(principal));
    if (NS_FAILED(rv)) return rv;

    if (mPrincipals.Count() > 0)
    {
        //-- Check to see if we already have this principal.
        nsCOMPtr<nsIPrincipal> fromTable;
        mPrincipals.Get(principal, getter_AddRefs(fromTable));
        if (fromTable)
            principal = fromTable;
        else //-- Check to see if we have a more general principal
        {
            nsXPIDLCString originUrl;
            rv = principal->GetOrigin(getter_Copies(originUrl));
            if (NS_FAILED(rv)) return rv;
            nsCOMPtr<nsIURI> newURI;
            rv = NS_NewURI(getter_AddRefs(newURI), originUrl, nsnull, sIOService);
            if (NS_FAILED(rv)) return rv;
            nsCOMPtr<nsIPrincipal> principal2;
            rv = CreateCodebasePrincipal(newURI, getter_AddRefs(principal2));
            if (NS_FAILED(rv)) return rv;
            mPrincipals.Get(principal2, getter_AddRefs(fromTable));
            if (fromTable)
                principal = fromTable;
        }
    }

    NS_IF_ADDREF(*result = principal);

    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::GetPrincipalFromContext(JSContext *cx,
                                                 nsIPrincipal **result)
{
    *result = nsnull;

    nsIScriptContext *scriptContext = GetScriptContext(cx);

    if (!scriptContext)
    {
        return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIScriptObjectPrincipal> globalData =
        do_QueryInterface(scriptContext->GetGlobalObject());
    if (globalData)
        NS_IF_ADDREF(*result = globalData->GetPrincipal());

    return NS_OK;
}

// static
nsIPrincipal*
nsScriptSecurityManager::GetScriptPrincipal(JSContext *cx,
                                            JSScript *script,
                                            nsresult* rv)
{
    NS_PRECONDITION(rv, "Null out param");
    *rv = NS_OK;
    if (!script)
    {
        return nsnull;
    }
    JSPrincipals *jsp = JS_GetScriptPrincipals(cx, script);
    if (!jsp) {
        *rv = NS_ERROR_FAILURE;
        // Script didn't have principals -- shouldn't happen.
        return nsnull;
    }
    nsJSPrincipals *nsJSPrin = NS_STATIC_CAST(nsJSPrincipals *, jsp);
    nsIPrincipal* result = nsJSPrin->nsIPrincipalPtr;
    if (!result)
        *rv = NS_ERROR_FAILURE;
    return result;
}

// static
nsIPrincipal*
nsScriptSecurityManager::GetFunctionObjectPrincipal(JSContext *cx,
                                                    JSObject *obj,
                                                    JSStackFrame *fp,
                                                    nsresult *rv)
{
    NS_PRECONDITION(rv, "Null out param");
    JSFunction *fun = (JSFunction *) JS_GetPrivate(cx, obj);
    JSScript *script = JS_GetFunctionScript(cx, fun);

    *rv = NS_OK;

    if (!script)
    {
        // A native function: skip it in order to find its scripted caller.
        return nsnull;
    }

    JSScript *frameScript = fp ? JS_GetFrameScript(cx, fp) : nsnull;

    if (frameScript && frameScript != script)
    {
        // There is a frame script, and it's different from the
        // function script. In this case we're dealing with either
        // an eval or a Script object, and in these cases the
        // principal we want is in the frame's script, not in the
        // function's script. The function's script is where the
        // eval-calling code came from, not where the eval or new
        // Script object came from, and we want the principal of
        // the eval function object or new Script object.

        script = frameScript;
    }
    else if (JS_GetFunctionObject(fun) != obj)
    {
        // Here, obj is a cloned function object.  In this case, the
        // clone's prototype may have been precompiled from brutally
        // shared chrome, or else it is a lambda or nested function.
        // The general case here is a function compiled against a
        // different scope than the one it is parented by at runtime,
        // hence the creation of a clone to carry the correct scope
        // chain linkage.
        //
        // Since principals follow scope, we must get the object
        // principal from the clone's scope chain. There are no
        // reliable principals compiled into the function itself.

        nsIPrincipal *result = doGetObjectPrincipal(cx, obj);
        if (!result)
            *rv = NS_ERROR_FAILURE;
        return result;
    }

    return GetScriptPrincipal(cx, script, rv);
}

// static
nsIPrincipal*
nsScriptSecurityManager::GetFramePrincipal(JSContext *cx,
                                           JSStackFrame *fp,
                                           nsresult *rv)
{
    NS_PRECONDITION(rv, "Null out param");
    JSObject *obj = JS_GetFrameFunctionObject(cx, fp);
    if (!obj)
    {
        // Must be in a top-level script. Get principal from the script.
        JSScript *script = JS_GetFrameScript(cx, fp);
        return GetScriptPrincipal(cx, script, rv);
    }

    nsIPrincipal* result = GetFunctionObjectPrincipal(cx, obj, fp, rv);

#ifdef DEBUG
    if (NS_SUCCEEDED(*rv) && !result)
    {
        JSFunction *fun = (JSFunction *)JS_GetPrivate(cx, obj);
        JSScript *script = JS_GetFunctionScript(cx, fun);

        NS_ASSERTION(!script, "Null principal for non-native function!");
    }
#endif

    return result;
}

// static
nsIPrincipal*
nsScriptSecurityManager::GetPrincipalAndFrame(JSContext *cx,
                                              JSStackFrame **frameResult,
                                              nsresult* rv)
{
    NS_PRECONDITION(rv, "Null out param");    
    //-- If there's no principal on the stack, look at the global object
    //   and return the innermost frame for annotations.
    *rv = NS_OK;
    if (cx)
    {
        // Get principals from innermost frame of JavaScript or Java.
        JSStackFrame *fp = nsnull; // tell JS_FrameIterator to start at innermost
        for (fp = JS_FrameIterator(cx, &fp); fp; fp = JS_FrameIterator(cx, &fp))
        {
            nsIPrincipal* result = GetFramePrincipal(cx, fp, rv);
            if (result)
            {
                NS_ASSERTION(NS_SUCCEEDED(*rv), "Weird return");
                *frameResult = fp;
                return result;
            }
        }

        nsIScriptContext *scriptContext = GetScriptContext(cx);
        if (scriptContext)
        {
            nsCOMPtr<nsIScriptObjectPrincipal> globalData =
                do_QueryInterface(scriptContext->GetGlobalObject());
            if (!globalData)
            {
                *rv = NS_ERROR_FAILURE;
                return nsnull;
            }

            // Note that we're not in a loop or anything, and nothing comes
            // after this point in the function, so we can just return here.
            nsIPrincipal* result = globalData->GetPrincipal();
            if (result)
            {
                JSStackFrame *inner = nsnull;
                *frameResult = JS_FrameIterator(cx, &inner);
                return result;
            }
        }
    }

    return nsnull;
}

// static
nsIPrincipal*
nsScriptSecurityManager::GetSubjectPrincipal(JSContext *cx,
                                             nsresult* rv)
{
    NS_PRECONDITION(rv, "Null out param");
    JSStackFrame *fp;
    return GetPrincipalAndFrame(cx, &fp, rv);
}

NS_IMETHODIMP
nsScriptSecurityManager::GetObjectPrincipal(JSContext *aCx, JSObject *aObj,
                                            nsIPrincipal **result)
{
    *result = doGetObjectPrincipal(aCx, aObj);
    if (!*result)
        return NS_ERROR_FAILURE;
    NS_ADDREF(*result);
    return NS_OK;
}

// static
nsIPrincipal*
nsScriptSecurityManager::doGetObjectPrincipal(JSContext *aCx, JSObject *aObj,
                                              PRBool aAllowShortCircuit)
{
    NS_ASSERTION(aCx && aObj, "Bad call to doGetObjectPrincipal()!");
    nsIPrincipal* result = nsnull;

#ifdef DEBUG
    JSObject* origObj = aObj;
#endif
    
    do
    {
        const JSClass *jsClass = JS_GetClass(aCx, aObj);

        if (jsClass && !(~jsClass->flags & (JSCLASS_HAS_PRIVATE |
                                            JSCLASS_PRIVATE_IS_NSISUPPORTS)))
        {
            // No need to refcount |priv| here.
            nsISupports *priv = (nsISupports *)JS_GetPrivate(aCx, aObj);

            /*
             * If it's a wrapped native (as most
             * JSCLASS_PRIVATE_IS_NSISUPPORTS objects are in mozilla),
             * check the underlying native instead.
             */
            nsCOMPtr<nsIXPConnectWrappedNative> xpcWrapper =
                do_QueryInterface(priv);

            if (NS_LIKELY(xpcWrapper != nsnull))
            {
                if (NS_UNLIKELY(aAllowShortCircuit))
                {
                    result = xpcWrapper->GetObjectPrincipal();
                }
                else
                {
                    nsCOMPtr<nsIScriptObjectPrincipal> objPrin;
                    objPrin = do_QueryWrappedNative(xpcWrapper);
                    if (objPrin)
                    {
                        result = objPrin->GetPrincipal();
                    }                    
                }
            }
            else
            {
                nsCOMPtr<nsIScriptObjectPrincipal> objPrin;
                objPrin = do_QueryInterface(priv);
                if (objPrin)
                {
                    result = objPrin->GetPrincipal();
                }
            }

            if (result)
            {
                break;
            }
        }

        aObj = JS_GetParent(aCx, aObj);
    } while (aObj);

    NS_ASSERTION(!aAllowShortCircuit ||
                 result == doGetObjectPrincipal(aCx, origObj, PR_FALSE),
                 "Principal mismatch.  Not good");
    
    return result;
}

nsresult
nsScriptSecurityManager::SavePrincipal(nsIPrincipal* aToSave)
{
    //-- Save to mPrincipals
    mPrincipals.Put(aToSave, aToSave);

    //-- Save to prefs
    nsXPIDLCString idPrefName;
    nsXPIDLCString id;
    nsXPIDLCString subjectName;
    nsXPIDLCString grantedList;
    nsXPIDLCString deniedList;
    nsresult rv = aToSave->GetPreferences(getter_Copies(idPrefName),
                                          getter_Copies(id),
                                          getter_Copies(subjectName),
                                          getter_Copies(grantedList),
                                          getter_Copies(deniedList));
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    nsCAutoString grantedPrefName;
    nsCAutoString deniedPrefName;
    nsCAutoString subjectNamePrefName;
    rv = GetPrincipalPrefNames( idPrefName,
                                grantedPrefName,
                                deniedPrefName,
                                subjectNamePrefName );
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    mIsWritingPrefs = PR_TRUE;
    if (grantedList)
        mSecurityPref->SecuritySetCharPref(grantedPrefName.get(), grantedList);
    else
        mSecurityPref->SecurityClearUserPref(grantedPrefName.get());

    if (deniedList)
        mSecurityPref->SecuritySetCharPref(deniedPrefName.get(), deniedList);
    else
        mSecurityPref->SecurityClearUserPref(deniedPrefName.get());

    if (grantedList || deniedList) {
        mSecurityPref->SecuritySetCharPref(idPrefName, id);
        mSecurityPref->SecuritySetCharPref(subjectNamePrefName.get(),
                                           subjectName);
    }
    else {
        mSecurityPref->SecurityClearUserPref(idPrefName);
        mSecurityPref->SecurityClearUserPref(subjectNamePrefName.get());
    }

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
    nsIPrincipal* previousPrincipal = nsnull;
    do
    {
        nsIPrincipal* principal = GetFramePrincipal(cx, fp, &rv);
        if (NS_FAILED(rv))
            return rv;
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
        // No principals on the stack, all native code.  Allow
        // execution if the subject principal is the system principal.

        return SubjectPrincipalIsSystem(result);
    }

    return NS_OK;
}

void
nsScriptSecurityManager::FormatCapabilityString(nsAString& aCapability)
{
    nsAutoString newcaps;
    nsAutoString rawcap;
    NS_NAMED_LITERAL_STRING(capdesc, "capdesc.");
    PRInt32 pos;
    PRInt32 index = kNotFound;
    nsresult rv;

    NS_ASSERTION(kNotFound == -1, "Basic constant changed, algorithm broken!");

    do {
        pos = index+1;
        index = aCapability.FindChar(' ', pos);
        rawcap = Substring(aCapability, pos,
                           (index == kNotFound) ? index : index - pos);

        nsXPIDLString capstr;
        rv = sStrBundle->GetStringFromName(
                            nsPromiseFlatString(capdesc+rawcap).get(),
                            getter_Copies(capstr));
        if (NS_SUCCEEDED(rv))
            newcaps += capstr;
        else
        {
            nsXPIDLString extensionCap;
            const PRUnichar* formatArgs[] = { rawcap.get() };
            rv = sStrBundle->FormatStringFromName(
                                NS_LITERAL_STRING("ExtensionCapability").get(),
                                formatArgs,
                                NS_ARRAY_LENGTH(formatArgs),
                                getter_Copies(extensionCap));
            if (NS_SUCCEEDED(rv))
                newcaps += extensionCap;
            else
                newcaps += rawcap;
        }

        newcaps += NS_LITERAL_STRING("\n");
    } while (index != kNotFound);

    aCapability = newcaps;
}

PRBool
nsScriptSecurityManager::CheckConfirmDialog(JSContext* cx, nsIPrincipal* aPrincipal,
                                            const char* aCapability, PRBool *checkValue)
{
    nsresult rv;
    *checkValue = PR_FALSE;

    //-- Get a prompter for the current window.
    nsCOMPtr<nsIPrompt> prompter;
    if (cx)
    {
        nsIScriptContext *scriptContext = GetScriptContext(cx);
        if (scriptContext)
        {
            nsCOMPtr<nsIDOMWindowInternal> domWin =
                do_QueryInterface(scriptContext->GetGlobalObject());
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

    //-- Localize the dialog text
    nsXPIDLString check;
    rv = sStrBundle->GetStringFromName(NS_LITERAL_STRING("CheckMessage").get(),
                                       getter_Copies(check));
    if (NS_FAILED(rv))
        return PR_FALSE;

    nsXPIDLString title;
    rv = sStrBundle->GetStringFromName(NS_LITERAL_STRING("Titleline").get(),
                                       getter_Copies(title));
    if (NS_FAILED(rv))
        return PR_FALSE;

    nsXPIDLString yesStr;
    rv = sStrBundle->GetStringFromName(NS_LITERAL_STRING("Yes").get(),
                                       getter_Copies(yesStr));
    if (NS_FAILED(rv))
        return PR_FALSE;

    nsXPIDLString noStr;
    rv = sStrBundle->GetStringFromName(NS_LITERAL_STRING("No").get(),
                                       getter_Copies(noStr));
    if (NS_FAILED(rv))
        return PR_FALSE;

    nsXPIDLCString val;
    PRBool hasCert;
    aPrincipal->GetHasCertificate(&hasCert);
    if (hasCert)
        rv = aPrincipal->GetPrettyName(val);
    else
        rv = aPrincipal->GetOrigin(getter_Copies(val));

    if (NS_FAILED(rv))
        return PR_FALSE;

    NS_ConvertUTF8toUTF16 location(val);
    NS_ConvertASCIItoUTF16 capability(aCapability);
    FormatCapabilityString(capability);
    const PRUnichar *formatStrings[] = { location.get(), capability.get() };

    nsXPIDLString message;
    rv = sStrBundle->FormatStringFromName(NS_LITERAL_STRING("EnableCapabilityQuery").get(),
                                          formatStrings,
                                          NS_ARRAY_LENGTH(formatStrings),
                                          getter_Copies(message));
    if (NS_FAILED(rv))
        return PR_FALSE;

    PRInt32 buttonPressed = 1; // If the user exits by clicking the close box, assume No (button 1)
    rv = prompter->ConfirmEx(title.get(), message.get(),
                             (nsIPrompt::BUTTON_DELAY_ENABLE) +
                             (nsIPrompt::BUTTON_POS_1_DEFAULT) +
                             (nsIPrompt::BUTTON_TITLE_IS_STRING * nsIPrompt::BUTTON_POS_0) +
                             (nsIPrompt::BUTTON_TITLE_IS_STRING * nsIPrompt::BUTTON_POS_1),
                             yesStr.get(), noStr.get(), nsnull, check.get(), checkValue, &buttonPressed);

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
        if (CheckConfirmDialog(cx, aPrincipal, capability, &remember))
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
        SetPendingException(cx, msg);
        return NS_ERROR_FAILURE;
    }

    //-- Check capability string for valid characters
    //
    //   Logically we might have wanted this in nsPrincipal, but performance
    //   worries dictate it can't go in IsCapabilityEnabled() and we may have
    //   to show the capability on a dialog before we call the principal's
    //   EnableCapability().
    //
    //   We don't need to validate the capability string on the other APIs
    //   available to web content. Without the ability to enable junk then
    //   isPrivilegeEnabled, disablePrivilege, and revertPrivilege all do
    //   the right thing (effectively nothing) when passed unallowed chars.
    for (const char *ch = capability; *ch; ++ch)
    {
        if (!NS_IS_ALPHA(*ch) && *ch != ' ' && !NS_IS_DIGIT(*ch)
            && *ch != '_' && *ch != '-' && *ch != '.')
        {
            static const char msg[] = "Invalid character in capability name";
            SetPendingException(cx, msg);
            return NS_ERROR_FAILURE;
        }
    }

    nsresult rv;
    nsIPrincipal* principal = GetPrincipalAndFrame(cx, &fp, &rv);
    if (NS_FAILED(rv))
        return rv;
    if (!principal)
        return NS_ERROR_NOT_AVAILABLE;

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
        nsXPIDLCString val;
        PRBool hasCert;
        nsresult rv;
        principal->GetHasCertificate(&hasCert);
        if (hasCert)
            rv = principal->GetPrettyName(val);
        else
            rv = principal->GetOrigin(getter_Copies(val));

        if (NS_FAILED(rv))
            return rv;

        NS_ConvertUTF8toUTF16 location(val);
        NS_ConvertUTF8toUTF16 cap(capability);
        const PRUnichar *formatStrings[] = { location.get(), cap.get() };

        nsXPIDLString message;
        rv = sStrBundle->FormatStringFromName(NS_LITERAL_STRING("EnableCapabilityDenied").get(),
                                              formatStrings,
                                              NS_ARRAY_LENGTH(formatStrings),
                                              getter_Copies(message));
        if (NS_FAILED(rv))
            return rv;

        SetPendingException(cx, message.get());

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
    nsresult rv;
    nsIPrincipal* principal = GetPrincipalAndFrame(cx, &fp, &rv);
    if (NS_FAILED(rv))
        return rv;
    if (!principal)
        return NS_ERROR_NOT_AVAILABLE;
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
    nsresult rv;
    nsIPrincipal* principal = GetPrincipalAndFrame(cx, &fp, &rv);
    if (NS_FAILED(rv))
        return rv;
    if (!principal)
        return NS_ERROR_NOT_AVAILABLE;
    void *annotation = JS_GetFrameAnnotation(cx, fp);
    principal->DisableCapability(capability, &annotation);
    JS_SetFrameAnnotation(cx, fp, annotation);
    return NS_OK;
}

//////////////// Master Certificate Functions ///////////////////////////////////////
NS_IMETHODIMP
nsScriptSecurityManager::SetCanEnableCapability(const nsACString& certFingerprint,
                                                const char* capability,
                                                PRInt16 canEnable)
{
    NS_ENSURE_ARG(!certFingerprint.IsEmpty());
    
    nsresult rv;
    nsIPrincipal* subjectPrincipal = doGetSubjectPrincipal(&rv);
    if (NS_FAILED(rv))
        return rv;

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
        nsCOMPtr<nsIZipReader> systemCertZip = do_CreateInstance(kZipReaderCID, &rv);
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
        rv = systemCertZip->Open(systemCertFile);
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
        SetPendingException(cx, mSystemCertificate ? msg1 : msg2);
        return NS_ERROR_FAILURE;
    }

    //-- Get the target principal
    nsCOMPtr<nsIPrincipal> objectPrincipal;
    rv = DoGetCertificatePrincipal(certFingerprint, EmptyCString(),
                                   EmptyCString(), nsnull,
                                   nsnull, PR_FALSE,
                                   getter_AddRefs(objectPrincipal));
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
#ifdef DEBUG_CAPS_CanCreateWrapper
    char* iidStr = aIID.ToString();
    printf("### CanCreateWrapper(%s) ", iidStr);
    nsCRT::free(iidStr);
#endif
// XXX Special case for nsIXPCException ?
    ClassInfoData objClassInfo = ClassInfoData(aClassInfo, nsnull);
    if (objClassInfo.IsDOMClass())
    {
#ifdef DEBUG_CAPS_CanCreateWrapper
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

        NS_NAMED_LITERAL_STRING(strName, "CreateWrapperDenied");
        NS_ConvertUTF8toUTF16 className(objClassInfo.GetName());
        const PRUnichar* formatStrings[] = { className.get() };
        nsXPIDLString errorMsg;
        // We need to keep our existing failure rv and not override it
        // with a likely success code from the following string bundle
        // call in order to throw the correct security exception later.
        nsresult rv2 =
            sStrBundle->FormatStringFromName(strName.get(),
                                             formatStrings,
                                             NS_ARRAY_LENGTH(formatStrings),
                                             getter_Copies(errorMsg));
        NS_ENSURE_SUCCESS(rv2, rv2);

        SetPendingException(cx, errorMsg.get());

#ifdef DEBUG_CAPS_CanCreateWrapper
        printf("DENIED.\n");
    }
    else
    {
        printf("GRANTED.\n");
#endif
    }

    return rv;
}

#ifdef XPC_IDISPATCH_SUPPORT
nsresult
nsScriptSecurityManager::CheckComponentPermissions(JSContext *cx,
                                                   const nsCID &aCID)
{
    nsresult rv;
    nsIPrincipal* subjectPrincipal = GetSubjectPrincipal(cx, &rv);
    if (NS_FAILED(rv))
        return rv;

    // Reformat the CID string so it's suitable for prefs
    nsXPIDLCString cidTemp;
    cidTemp.Adopt(aCID.ToString());
    nsCAutoString cid(NS_LITERAL_CSTRING("CID") +
                      Substring(cidTemp, 1, cidTemp.Length() - 2));
    ToUpperCase(cid);

#ifdef DEBUG_CAPS_CheckComponentPermissions
    printf("### CheckComponentPermissions(ClassID.%s) ",cid.get());
#endif

    // Look up the policy for this class.
    // while this isn't a property we'll treat it as such, using ACCESS_CALL_METHOD
    JSAutoRequest ar(cx);
    jsval cidVal = STRING_TO_JSVAL(::JS_InternString(cx, cid.get()));

    ClassInfoData nameData(nsnull, "ClassID");
    SecurityLevel securityLevel;
    rv = LookupPolicy(subjectPrincipal, nameData, cidVal,
                      nsIXPCSecurityManager::ACCESS_CALL_METHOD, 
                      nsnull, &securityLevel);
    if (NS_FAILED(rv))
        return rv;

    // If there's no policy stored, use the "security.classID.allowByDefault" pref 
    if (securityLevel.level == SCRIPT_SECURITY_UNDEFINED_ACCESS)
        securityLevel.level = mXPCDefaultGrantAll ? SCRIPT_SECURITY_ALL_ACCESS :
                                                    SCRIPT_SECURITY_NO_ACCESS;

    if (securityLevel.level == SCRIPT_SECURITY_ALL_ACCESS)
    {
#ifdef DEBUG_CAPS_CheckComponentPermissions
        printf(" GRANTED.\n");
#endif
        return NS_OK;
    }

#ifdef DEBUG_CAPS_CheckComponentPermissions
    printf(" DENIED.\n");
#endif
    return NS_ERROR_DOM_PROP_ACCESS_DENIED;
}
#endif

NS_IMETHODIMP
nsScriptSecurityManager::CanCreateInstance(JSContext *cx,
                                           const nsCID &aCID)
{
#ifdef DEBUG_CAPS_CanCreateInstance
    char* cidStr = aCID.ToString();
    printf("### CanCreateInstance(%s) ", cidStr);
    nsCRT::free(cidStr);
#endif

    nsresult rv = CheckXPCPermissions(nsnull, nsnull);
    if (NS_FAILED(rv))
#ifdef XPC_IDISPATCH_SUPPORT
    {
        rv = CheckComponentPermissions(cx, aCID);
    }
    if (NS_FAILED(rv))
#endif
    {
        //-- Access denied, report an error
        nsCAutoString errorMsg("Permission denied to create instance of class. CID=");
        nsXPIDLCString cidStr;
        cidStr += aCID.ToString();
        errorMsg.Append(cidStr);
        SetPendingException(cx, errorMsg.get());

#ifdef DEBUG_CAPS_CanCreateInstance
        printf("DENIED\n");
    }
    else
    {
        printf("GRANTED\n");
#endif
    }
    return rv;
}

NS_IMETHODIMP
nsScriptSecurityManager::CanGetService(JSContext *cx,
                                       const nsCID &aCID)
{
#ifdef DEBUG_CAPS_CanGetService
    char* cidStr = aCID.ToString();
    printf("### CanGetService(%s) ", cidStr);
    nsCRT::free(cidStr);
#endif

    nsresult rv = CheckXPCPermissions(nsnull, nsnull);
    if (NS_FAILED(rv))
    {
        //-- Access denied, report an error
        nsCAutoString errorMsg("Permission denied to get service. CID=");
        nsXPIDLCString cidStr;
        cidStr += aCID.ToString();
        errorMsg.Append(cidStr);
        SetPendingException(cx, errorMsg.get());

#ifdef DEBUG_CAPS_CanGetService
        printf("DENIED\n");
    }
    else
    {
        printf("GRANTED\n");
#endif
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
        if (PL_strcasecmp(aObjectSecurityLevel, "allAccess") == 0)
            return NS_OK;
        else if (PL_strcasecmp(aObjectSecurityLevel, "noAccess") != 0)
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

//////////////////////////////////////////////
// Method implementing nsIPrefSecurityCheck //
//////////////////////////////////////////////

NS_IMETHODIMP
nsScriptSecurityManager::CanAccessSecurityPreferences(PRBool* _retval)
{
    return IsCapabilityEnabled("CapabilityPreferencesAccess", _retval);  
}

/////////////////////////////////////////////
// Method implementing nsIChannelEventSink //
/////////////////////////////////////////////
NS_IMETHODIMP
nsScriptSecurityManager::OnChannelRedirect(nsIChannel* oldChannel, 
                                           nsIChannel* newChannel,
                                           PRUint32 redirFlags)
{
    nsCOMPtr<nsIPrincipal> oldPrincipal;
    GetChannelPrincipal(oldChannel, getter_AddRefs(oldPrincipal));

    nsCOMPtr<nsIURI> newURI;
    newChannel->GetURI(getter_AddRefs(newURI));

    NS_ENSURE_STATE(oldPrincipal && newURI);

    const PRUint32 flags =
        nsIScriptSecurityManager::LOAD_IS_AUTOMATIC_DOCUMENT_REPLACEMENT |
        nsIScriptSecurityManager::DISALLOW_SCRIPT;
    return CheckLoadURIWithPrincipal(oldPrincipal, newURI, flags);
}


/////////////////////////////////////
// Method implementing nsIObserver //
/////////////////////////////////////
static const char sPrincipalPrefix[] = "capability.principal";
static const char sPolicyPrefix[] = "capability.policy.";

NS_IMETHODIMP
nsScriptSecurityManager::Observe(nsISupports* aObject, const char* aTopic,
                                 const PRUnichar* aMessage)
{
    nsresult rv = NS_OK;
    NS_ConvertUTF16toUTF8 messageStr(aMessage);
    const char *message = messageStr.get();

    static const char jsPrefix[] = "javascript.";
    if((PL_strncmp(message, jsPrefix, sizeof(jsPrefix)-1) == 0)
#ifdef XPC_IDISPATCH_SUPPORT
        || (PL_strcmp(message, sXPCDefaultGrantAllName) == 0)
#endif
        )
        JSEnabledPrefChanged(mSecurityPref);
    if(PL_strncmp(message, sPolicyPrefix, sizeof(sPolicyPrefix)-1) == 0)
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
      mIsJavaScriptEnabled(PR_FALSE),
      mIsMailJavaScriptEnabled(PR_FALSE),
      mIsWritingPrefs(PR_FALSE),
      mPolicyPrefsChanged(PR_TRUE)
#ifdef XPC_IDISPATCH_SUPPORT
      ,mXPCDefaultGrantAll(PR_FALSE)
#endif
{
    NS_ASSERTION(sizeof(long) == sizeof(void*), "long and void* have different lengths on this platform. This may cause a security failure.");
    mPrincipals.Init(31);
}


nsresult nsScriptSecurityManager::Init()
{
    JSContext* cx = GetSafeJSContext();
    if (!cx) return NS_ERROR_FAILURE;   // this can happen of xpt loading fails
    
    ::JS_BeginRequest(cx);
    if (sEnabledID == JSVAL_VOID)
        sEnabledID = STRING_TO_JSVAL(::JS_InternString(cx, "enabled"));
    ::JS_EndRequest(cx);

    nsresult rv = InitPrefs();
    NS_ENSURE_SUCCESS(rv, rv);

    rv = CallGetService(NS_IOSERVICE_CONTRACTID, &sIOService);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = CallGetService(nsIXPConnect::GetCID(), &sXPConnect);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = bundleService->CreateBundle("chrome://global/locale/security/caps.properties", &sStrBundle);
    NS_ENSURE_SUCCESS(rv, rv);

    // Create our system principal singleton
    nsRefPtr<nsSystemPrincipal> system = new nsSystemPrincipal();
    NS_ENSURE_TRUE(system, NS_ERROR_OUT_OF_MEMORY);

    rv = system->Init();
    NS_ENSURE_SUCCESS(rv, rv);

    mSystemPrincipal = system;

    //-- Register security check callback in the JS engine
    //   Currently this is used to control access to function.caller
    nsCOMPtr<nsIJSRuntimeService> runtimeService =
        do_GetService("@mozilla.org/js/xpc/RuntimeService;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = runtimeService->GetRuntime(&sRuntime);
    NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG
    JSCheckAccessOp oldCallback =
#endif
        JS_SetCheckObjectAccessCallback(sRuntime, CheckObjectAccess);

    // For now, assert that no callback was set previously
    NS_ASSERTION(!oldCallback, "Someone already set a JS CheckObjectAccess callback");
    return NS_OK;
}

static nsScriptSecurityManager *gScriptSecMan = nsnull;

jsval nsScriptSecurityManager::sEnabledID   = JSVAL_VOID;

nsScriptSecurityManager::~nsScriptSecurityManager(void)
{
    delete mOriginToPolicyMap;
    if(mDefaultPolicy)
        mDefaultPolicy->Drop();
    delete mCapabilities;
    gScriptSecMan = nsnull;
}

void
nsScriptSecurityManager::Shutdown()
{
    if (sRuntime) {
#ifdef DEBUG
        JSCheckAccessOp oldCallback =
#endif
            JS_SetCheckObjectAccessCallback(sRuntime, nsnull);
        NS_ASSERTION(oldCallback == CheckObjectAccess, "Oops, we just clobbered someone else, oh well.");
        sRuntime = nsnull;
    }
    sEnabledID = JSVAL_VOID;

    NS_IF_RELEASE(sIOService);
    NS_IF_RELEASE(sXPConnect);
    NS_IF_RELEASE(sStrBundle);
}

nsScriptSecurityManager *
nsScriptSecurityManager::GetScriptSecurityManager()
{
    if (!gScriptSecMan)
    {
        nsScriptSecurityManager* ssManager = new nsScriptSecurityManager();
        if (!ssManager)
            return nsnull;
        nsresult rv;
        rv = ssManager->Init();
        NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to initialize nsScriptSecurityManager");
        if (NS_FAILED(rv)) {
            delete ssManager;
            return nsnull;
        }
 
        rv = nsJSPrincipals::Startup();
        if (NS_FAILED(rv)) {
            NS_WARNING("can't initialize JS engine security protocol glue!");
            delete ssManager;
            return nsnull;
        }
 
        rv = sXPConnect->SetDefaultSecurityManager(ssManager,
                                                   nsIXPCSecurityManager::HOOK_ALL);
        if (NS_FAILED(rv)) {
            NS_WARNING("Failed to install xpconnect security manager!");
            delete ssManager;
            return nsnull;
        }

        gScriptSecMan = ssManager;
    }
    return gScriptSecMan;
}

// Currently this nsGenericFactory constructor is used only from FastLoad
// (XPCOM object deserialization) code, when "creating" the system principal
// singleton.
nsSystemPrincipal *
nsScriptSecurityManager::SystemPrincipalSingletonConstructor()
{
    nsIPrincipal *sysprin = nsnull;
    if (gScriptSecMan)
        NS_ADDREF(sysprin = gScriptSecMan->mSystemPrincipal);
    return NS_STATIC_CAST(nsSystemPrincipal*, sysprin);
}

nsresult
nsScriptSecurityManager::InitPolicies()
{
    // Clear any policies cached on XPConnect wrappers
    NS_ENSURE_STATE(sXPConnect);
    nsresult rv = sXPConnect->ClearAllWrappedNativeSecurityPolicies();
    if (NS_FAILED(rv)) return rv;

    //-- Clear mOriginToPolicyMap: delete mapped DomainEntry items,
    //-- whose dtor decrements refcount of stored DomainPolicy object
    delete mOriginToPolicyMap;
    
    //-- Marks all the survivor DomainPolicy objects (those cached
    //-- by nsPrincipal objects) as invalid: they will be released
    //-- on first nsPrincipal::GetSecurityPolicy() attempt.
    DomainPolicy::InvalidateAll();
    
    //-- Release old default policy
    if(mDefaultPolicy) {
        mDefaultPolicy->Drop();
        mDefaultPolicy = nsnull;
    }
    
    //-- Initialize a new mOriginToPolicyMap
    mOriginToPolicyMap =
      new nsObjectHashtable(nsnull, nsnull, DeleteDomainEntry, nsnull);
    if (!mOriginToPolicyMap)
        return NS_ERROR_OUT_OF_MEMORY;

    //-- Create, refcount and initialize a new default policy 
    mDefaultPolicy = new DomainPolicy();
    if (!mDefaultPolicy)
        return NS_ERROR_OUT_OF_MEMORY;

    mDefaultPolicy->Hold();
    if (!mDefaultPolicy->Init())
        return NS_ERROR_UNEXPECTED;

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
    char* policyCurrent = policyNames.BeginWriting();
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

        nsCAutoString sitesPrefName(
            NS_LITERAL_CSTRING(sPolicyPrefix) +
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

        if (!domainPolicy->Init())
        {
            delete domainPolicy;
            return NS_ERROR_UNEXPECTED;
        }
        domainPolicy->Hold();
        //-- Parse list of sites and create an entry in mOriginToPolicyMap for each
        char* domainStart = domainList.BeginWriting();
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
                {
                    domainPolicy->Drop();
                    return NS_ERROR_OUT_OF_MEMORY;
                }
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
        domainPolicy->Drop();
        if (NS_FAILED(rv))
            return rv;
    }

    // Reset the "dirty" flag
    mPolicyPrefsChanged = PR_FALSE;

#ifdef DEBUG_CAPS_HACKER
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
    nsCAutoString policyPrefix(NS_LITERAL_CSTRING(sPolicyPrefix) +
                               nsDependentCString(aPolicyName) +
                               NS_LITERAL_CSTRING("."));
    PRUint32 prefixLength = policyPrefix.Length() - 1; // subtract the '.'

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
        const char* start = prefNames[currentPref] + prefixLength + 1;
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
                secLevel.capability = NS_strdup(prefValue);
                if (!secLevel.capability)
                    break;
                mCapabilities->Put(&secLevelKey, 
                                   secLevel.capability);
            }
        }

        *end = '\0';
        // Find or store this class in the classes table
        ClassPolicy* cpolicy = 
          NS_STATIC_CAST(ClassPolicy*,
                         PL_DHashTableOperate(aDomainPolicy, start,
                                              PL_DHASH_ADD));
        if (!cpolicy)
            break;

        // If this is the wildcard class (class '*'), save it in mWildcardPolicy
        // (we leave it stored in the hashtable too to take care of the cleanup)
        if ((*start == '*') && (end == start + 1)) {
            aDomainPolicy->mWildcardPolicy = cpolicy;

            // Make sure that cpolicy knows about aDomainPolicy so it can reset
            // the mWildcardPolicy pointer as needed if it gets moved in the
            // hashtable.
            cpolicy->mDomainWeAreWildcardFor = aDomainPolicy;
        }

        // Get the property name
        start = end + 1;
        end = PL_strchr(start, '.');
        if (end)
            *end = '\0';

        JSAutoRequest ar(cx);

        JSString* propertyKey = ::JS_InternString(cx, start);
        if (!propertyKey)
            return NS_ERROR_OUT_OF_MEMORY;

        // Store this property in the class policy
        const void* ppkey =
          NS_REINTERPRET_CAST(const void*, STRING_TO_JSVAL(propertyKey));
        PropertyPolicy* ppolicy = 
          NS_STATIC_CAST(PropertyPolicy*,
                         PL_DHashTableOperate(cpolicy->mPolicy, ppkey,
                                              PL_DHASH_ADD));
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


// XXXbz We should really just get a prefbranch to handle this...
nsresult
nsScriptSecurityManager::GetPrincipalPrefNames(const char* prefBase,
                                               nsCString& grantedPref,
                                               nsCString& deniedPref,
                                               nsCString& subjectNamePref)
{
    char* lastDot = PL_strrchr(prefBase, '.');
    if (!lastDot) return NS_ERROR_FAILURE;
    PRInt32 prefLen = lastDot - prefBase + 1;

    grantedPref.Assign(prefBase, prefLen);
    deniedPref.Assign(prefBase, prefLen);
    subjectNamePref.Assign(prefBase, prefLen);

#define GRANTED "granted"
#define DENIED "denied"
#define SUBJECTNAME "subjectName"

    grantedPref.AppendLiteral(GRANTED);
    if (grantedPref.Length() != prefLen + sizeof(GRANTED) - 1) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    deniedPref.AppendLiteral(DENIED);
    if (deniedPref.Length() != prefLen + sizeof(DENIED) - 1) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    subjectNamePref.AppendLiteral(SUBJECTNAME);
    if (subjectNamePref.Length() != prefLen + sizeof(SUBJECTNAME) - 1) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

#undef SUBJECTNAME
#undef DENIED
#undef GRANTED
    
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
        PRInt32 prefNameLen = PL_strlen(aPrefNames[c]) - 
            (NS_ARRAY_LENGTH(idSuffix) - 1);
        if (PL_strcasecmp(aPrefNames[c] + prefNameLen, idSuffix) != 0)
            continue;

        nsXPIDLCString id;
        if (NS_FAILED(mSecurityPref->SecurityGetCharPref(aPrefNames[c], getter_Copies(id))))
            return NS_ERROR_FAILURE;

        nsCAutoString grantedPrefName;
        nsCAutoString deniedPrefName;
        nsCAutoString subjectNamePrefName;
        nsresult rv = GetPrincipalPrefNames(aPrefNames[c],
                                            grantedPrefName,
                                            deniedPrefName,
                                            subjectNamePrefName);
        if (rv == NS_ERROR_OUT_OF_MEMORY)
            return rv;
        if (NS_FAILED(rv))
            continue;

        nsXPIDLCString grantedList;
        mSecurityPref->SecurityGetCharPref(grantedPrefName.get(),
                                           getter_Copies(grantedList));
        nsXPIDLCString deniedList;
        mSecurityPref->SecurityGetCharPref(deniedPrefName.get(),
                                           getter_Copies(deniedList));
        nsXPIDLCString subjectName;
        mSecurityPref->SecurityGetCharPref(subjectNamePrefName.get(),
                                           getter_Copies(subjectName));

        //-- Delete prefs if their value is the empty string
        if (id.IsEmpty() || (grantedList.IsEmpty() && deniedList.IsEmpty()))
        {
            mSecurityPref->SecurityClearUserPref(aPrefNames[c]);
            mSecurityPref->SecurityClearUserPref(grantedPrefName.get());
            mSecurityPref->SecurityClearUserPref(deniedPrefName.get());
            mSecurityPref->SecurityClearUserPref(subjectNamePrefName.get());
            continue;
        }

        //-- Create a principal based on the prefs
        static const char certificateName[] = "capability.principal.certificate";
        static const char codebaseName[] = "capability.principal.codebase";
        static const char codebaseTrustedName[] = "capability.principal.codebaseTrusted";

        PRBool isCert = PR_FALSE;
        PRBool isTrusted = PR_FALSE;
        
        if (PL_strncmp(aPrefNames[c], certificateName,
                       sizeof(certificateName) - 1) == 0)
        {
            isCert = PR_TRUE;
        }
        else if (PL_strncmp(aPrefNames[c], codebaseName,
                            sizeof(codebaseName) - 1) == 0)
        {
            isTrusted = (PL_strncmp(aPrefNames[c], codebaseTrustedName,
                                    sizeof(codebaseTrustedName) - 1) == 0);
        }
        else
        {
          NS_ERROR("Not a codebase or a certificate?!");
        }

        nsRefPtr<nsPrincipal> newPrincipal = new nsPrincipal();
        if (!newPrincipal)
            return NS_ERROR_OUT_OF_MEMORY;

        rv = newPrincipal->InitFromPersistent(aPrefNames[c], id, subjectName,
                                              EmptyCString(),
                                              grantedList, deniedList, nsnull, 
                                              isCert, isTrusted);
        if (NS_SUCCEEDED(rv))
            mPrincipals.Put(newPrincipal, newPrincipal);
    }
    return NS_OK;
}

const char nsScriptSecurityManager::sJSEnabledPrefName[] =
    "javascript.enabled";
const char nsScriptSecurityManager::sJSMailEnabledPrefName[] =
    "javascript.allow.mailnews";
#ifdef XPC_IDISPATCH_SUPPORT
const char nsScriptSecurityManager::sXPCDefaultGrantAllName[] =
    "security.classID.allowByDefault";
#endif

inline void
nsScriptSecurityManager::JSEnabledPrefChanged(nsISecurityPref* aSecurityPref)
{
    PRBool temp;
    nsresult rv = mSecurityPref->SecurityGetBoolPref(sJSEnabledPrefName, &temp);
    // JavaScript defaults to enabled in failure cases.
    mIsJavaScriptEnabled = NS_FAILED(rv) || temp;

    rv = mSecurityPref->SecurityGetBoolPref(sJSMailEnabledPrefName, &temp);
    // JavaScript in Mail defaults to enabled in failure cases.
    mIsMailJavaScriptEnabled = NS_FAILED(rv) || temp;

#ifdef XPC_IDISPATCH_SUPPORT
    rv = mSecurityPref->SecurityGetBoolPref(sXPCDefaultGrantAllName, &temp);
    // Granting XPC Priveleges defaults to disabled in failure cases.
    mXPCDefaultGrantAll = NS_SUCCEEDED(rv) && temp;
#endif
}

nsresult
nsScriptSecurityManager::InitPrefs()
{
    nsresult rv;
    nsCOMPtr<nsIPrefService> prefService(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = prefService->GetBranch(nsnull, getter_AddRefs(mPrefBranch));
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIPrefBranch2> prefBranchInternal(do_QueryInterface(mPrefBranch, &rv));
    NS_ENSURE_SUCCESS(rv, rv);
    mSecurityPref = do_QueryInterface(mPrefBranch, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // Set the initial value of the "javascript.enabled" prefs
    JSEnabledPrefChanged(mSecurityPref);
    // set observer callbacks in case the value of the prefs change
    prefBranchInternal->AddObserver(sJSEnabledPrefName, this, PR_FALSE);
    prefBranchInternal->AddObserver(sJSMailEnabledPrefName, this, PR_FALSE);
#ifdef XPC_IDISPATCH_SUPPORT
    prefBranchInternal->AddObserver(sXPCDefaultGrantAllName, this, PR_FALSE);
#endif
    PRUint32 prefCount;
    char** prefNames;

    // Set a callback for policy pref changes
    prefBranchInternal->AddObserver(sPolicyPrefix, this, PR_FALSE);

    //-- Initialize the principals database from prefs
    rv = mPrefBranch->GetChildList(sPrincipalPrefix, &prefCount, &prefNames);
    if (NS_SUCCEEDED(rv) && prefCount > 0)
    {
        rv = InitPrincipals(prefCount, (const char**)prefNames, mSecurityPref);
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(prefCount, prefNames);
        NS_ENSURE_SUCCESS(rv, rv);
    }
    //-- Set a callback for principal changes
    prefBranchInternal->AddObserver(sPrincipalPrefix, this, PR_FALSE);

    return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// The following code prints the contents of the policy DB to the console.
#ifdef DEBUG_CAPS_HACKER

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
        if (!cx)
            cx = GetSafeJSContext();
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
