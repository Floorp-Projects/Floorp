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
#include "nsIJARURI.h"
#include "nspr.h"
#include "nsJSPrincipals.h"
#include "nsSystemPrincipal.h"
#include "nsPrincipal.h"
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
#include "nsIProperties.h"
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
#include "nsAutoPtr.h"

static NS_DEFINE_CID(kZipReaderCID, NS_ZIPREADER_CID);

nsIIOService    *nsScriptSecurityManager::sIOService = nsnull;
nsIXPConnect    *nsScriptSecurityManager::sXPConnect = nsnull;
nsIStringBundle *nsScriptSecurityManager::sStrBundle = nsnull;

///////////////////////////
// Convenience Functions //
///////////////////////////
// Result of this function should not be freed.
static inline const PRUnichar *
JSValIDToString(JSContext *cx, const jsval idval)
{
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
    JSString *str = JS_NewStringCopyZ(cx, aMsg);
    if (str)
        JS_SetPendingException(cx, STRING_TO_JSVAL(str));
}

inline void SetPendingException(JSContext *cx, const PRUnichar *aMsg)
{
    JSString *str = JS_NewUCStringCopyZ(cx,
                        NS_REINTERPRET_CAST(const jschar*, aMsg));
    if (str)
        JS_SetPendingException(cx, STRING_TO_JSVAL(str));
}

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

NS_IMETHODIMP
nsScriptSecurityManager::SecurityCompareURIs(nsIURI* aSourceURI,
                                             nsIURI* aTargetURI,
                                             PRBool* result)
{
    *result = PR_FALSE;

    if (aSourceURI == aTargetURI)
    {
        *result = PR_TRUE;
        return NS_OK;
    }

    if (!aTargetURI) 
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
    nsresult rv = targetBaseURI->GetScheme(targetScheme);
    nsCAutoString sourceScheme;
    if (NS_SUCCEEDED(rv))
        rv = sourceBaseURI->GetScheme(sourceScheme);
    if (NS_SUCCEEDED(rv) && targetScheme.Equals(sourceScheme))
    {
        if (targetScheme.EqualsLiteral("file"))
        {
            // All file: urls are considered to have the same origin.
            *result = PR_TRUE;
        }
        else if (targetScheme.EqualsLiteral("imap") ||
                 targetScheme.EqualsLiteral("mailbox") ||
                 targetScheme.EqualsLiteral("news"))
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
                    NS_ENSURE_STATE(sIOService);

                    PRInt32 defaultPort;
                    nsCOMPtr<nsIProtocolHandler> protocolHandler;
                    rv = sIOService->GetProtocolHandler(sourceScheme.get(),
                                                        getter_AddRefs(protocolHandler));
                    if (NS_FAILED(rv))
                    {
                        *result = PR_FALSE;
                        return NS_OK;
                    }
                    
                    rv = protocolHandler->GetDefaultPort(&defaultPort);
                    if (NS_FAILED(rv) || defaultPort == -1)
                        return NS_OK; // No default port for this scheme

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
    // If *vp is not a primitive, some new JS engine call to this hook was
    // added, but we can handle that case too -- if a primitive value in a
    // property of obj is being accessed, we should use obj as the target
    // object.
    NS_ASSERTION(!JSVAL_IS_PRIMITIVE(*vp), "unexpected target property value");
    JSObject* target = JSVAL_IS_PRIMITIVE(*vp) ? obj : JSVAL_TO_OBJECT(*vp);

    // Do the same-origin check -- this sets a JS exception if the check fails.
    // Pass the parent object's class name, as we have no class-info for it.
    nsresult rv =
        ssm->CheckPropertyAccess(cx, target, JS_GetClass(cx, obj)->name, id,
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
    nsCOMPtr<nsIPrincipal> subjectPrincipal;
    if (NS_FAILED(GetSubjectPrincipal(cx, getter_AddRefs(subjectPrincipal))))
        return NS_ERROR_FAILURE;

    if (!subjectPrincipal || subjectPrincipal == mSystemPrincipal)
        // We have native code or the system principal: just allow access
        return NS_OK;

    nsresult rv;
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
    rv = LookupPolicy(subjectPrincipal, classInfoData.GetName(), aProperty, aAction, 
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

    nsCOMPtr<nsIURI> subjectURI;
    nsCOMPtr<nsIURI> objectURI;
    aSubject->GetDomain(getter_AddRefs(subjectURI));
    if (!subjectURI) {
      aSubject->GetURI(getter_AddRefs(subjectURI));
    }

    aObject->GetDomain(getter_AddRefs(objectURI));
    if (!objectURI) {
      aObject->GetURI(getter_AddRefs(objectURI));
    }

    PRBool isSameOrigin = PR_FALSE;
    nsresult rv = SecurityCompareURIs(subjectURI, objectURI, &isSameOrigin);
    NS_ENSURE_SUCCESS(rv, rv);

    if (isSameOrigin)
    {   // If either the subject or the object has changed its principal by
        // explicitly setting document.domain then the other must also have
        // done so in order to be considered the same origin. This prevents
        // DNS spoofing based on document.domain (154930)

        // But this restriction does not apply to CheckConnect calls, since
        // that's called for data-only load checks like XMLHTTPRequest, where
        // the target document has not yet loaded and can't have set its domain
        // (bug 163950)
        if (aIsCheckConnect)
            return NS_OK;

        nsCOMPtr<nsIURI> subjectDomain;
        aSubject->GetDomain(getter_AddRefs(subjectDomain));

        nsCOMPtr<nsIURI> objectDomain;
        aObject->GetDomain(getter_AddRefs(objectDomain));

        // If both or neither explicitly set their domain, allow the access
        if (!subjectDomain == !objectDomain)
            return NS_OK;
    }

    // Allow access to about:blank
    nsXPIDLCString origin;
    rv = aObject->GetOrigin(getter_Copies(origin));
    NS_ENSURE_SUCCESS(rv, rv);
    if (nsCRT::strcasecmp(origin, "about:blank") == 0)
        return NS_OK;

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
    {
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
nsScriptSecurityManager::LookupPolicy(nsIPrincipal* aPrincipal,
                                     const char* aClassName, jsval aProperty,
                                     PRUint32 aAction,
                                     ClassPolicy** aCachedClassPolicy,
                                     SecurityLevel* result)
{
    nsresult rv;
    result->level = SCRIPT_SECURITY_UNDEFINED_ACCESS;

    //-- Initialize policies if necessary
    if (mPolicyPrefsChanged)
    {
        rv = InitPolicies();
        if (NS_FAILED(rv))
            return rv;
    }

    DomainPolicy* dpolicy = nsnull;
    aPrincipal->GetSecurityPolicy((void**)&dpolicy);

    if (!dpolicy && mOriginToPolicyMap)
    {
        //-- Look up the relevant domain policy, if any
#ifdef DEBUG_CAPS_LookupPolicy
        printf("DomainLookup ");
#endif

        nsXPIDLCString origin;
        if (NS_FAILED(rv = aPrincipal->GetOrigin(getter_Copies(origin))))
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
                                                      aClassName,
                                                      PL_DHASH_LOOKUP));

        if (PL_DHASH_ENTRY_IS_FREE(cpolicy))
            cpolicy = NO_POLICY_FOR_CLASS;

        if ((dpolicy == mDefaultPolicy) && aCachedClassPolicy)
            *aCachedClassPolicy = cpolicy;
    }
    PropertyPolicy* ppolicy = nsnull;
    if (cpolicy != NO_POLICY_FOR_CLASS)
    {
        ppolicy = NS_STATIC_CAST(PropertyPolicy*,
                                 PL_DHashTableOperate(cpolicy->mPolicy,
                                                      (void*)aProperty,
                                                      PL_DHASH_LOOKUP));
    }
    else
    {
        // If there's no per-domain policy and no default policy, we're done
        if (dpolicy == mDefaultPolicy)
            return NS_OK;

        // This class is not present in the domain policy, check its wildcard policy
        if (dpolicy->mWildcardPolicy)
        {
            ppolicy =
              NS_STATIC_CAST(PropertyPolicy*,
                             PL_DHashTableOperate(dpolicy->mWildcardPolicy->mPolicy,
                                                  (void*)aProperty,
                                                  PL_DHASH_LOOKUP));
        }

        // If there's no wildcard policy, check the default policy for this class
        if (!ppolicy || PL_DHASH_ENTRY_IS_FREE(ppolicy))
        {
            cpolicy = NS_STATIC_CAST(ClassPolicy*,
                                     PL_DHashTableOperate(mDefaultPolicy,
                                                          aClassName,
                                                          PL_DHASH_LOOKUP));

            if (PL_DHASH_ENTRY_IS_BUSY(cpolicy))
            {
                ppolicy =
                  NS_STATIC_CAST(PropertyPolicy*,
                                 PL_DHashTableOperate(cpolicy->mPolicy,
                                                      (void*)aProperty,
                                                      PL_DHASH_LOOKUP));
            }
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
    nsCOMPtr<nsIPrincipal> principal;
    if (NS_FAILED(GetSubjectPrincipal(cx, getter_AddRefs(principal))))
        return NS_ERROR_FAILURE;

    // Native code can load all URIs.
    if (!principal)
        return NS_OK;

    // The system principal can load all URIs.
    if (principal == mSystemPrincipal)
        return NS_OK;

    // Otherwise, principal should have a codebase URI that we can use to
    // do the remaining tests.
    nsCOMPtr<nsIURI> uri;
    if (NS_FAILED(principal->GetURI(getter_AddRefs(uri))))
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

// static
nsresult
nsScriptSecurityManager::GetBaseURIScheme(nsIURI* aURI,
                                          nsCString& aScheme)
{
    if (!aURI)
       return NS_ERROR_FAILURE;

    nsresult rv;
    nsCOMPtr<nsIURI> uri(aURI);

    //-- get the source scheme
    rv = uri->GetScheme(aScheme);
    if (NS_FAILED(rv)) return rv;

    //-- If uri is a view-source URI, drill down to the base URI
    nsCAutoString path;
    while (aScheme.EqualsLiteral("view-source"))
    {
        rv = uri->GetPath(path);
        if (NS_FAILED(rv)) return rv;
        rv = NS_NewURI(getter_AddRefs(uri), path, nsnull, nsnull, sIOService);
        if (NS_FAILED(rv)) return rv;
        rv = uri->GetScheme(aScheme);
        if (NS_FAILED(rv)) return rv;
    }

    //-- If uri is a jar URI, drill down again
    nsCOMPtr<nsIJARURI> jarURI;
    PRBool isJAR = PR_FALSE;
    while ((jarURI = do_QueryInterface(uri)))
    {
        jarURI->GetJARFile(getter_AddRefs(uri));
        isJAR = PR_TRUE;
    }
    if (!uri) return NS_ERROR_FAILURE;
    if (isJAR)
    {
        rv = uri->GetScheme(aScheme);
        if (NS_FAILED(rv)) return rv;
    }

    //-- if uri is an about uri, distinguish 'safe' and 'unsafe' about URIs
    if(aScheme.EqualsLiteral("about"))
    {
        nsCAutoString path;
        if(NS_FAILED(uri->GetPath(path)))
            return NS_ERROR_FAILURE;
        if (path.EqualsLiteral("blank")   ||
            path.IsEmpty()                ||
            path.EqualsLiteral("mozilla") ||
            path.EqualsLiteral("logo")    ||
            path.EqualsLiteral("credits"))
        {
            aScheme = NS_LITERAL_CSTRING("about safe");
            return NS_OK;
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::CheckLoadURI(nsIURI *aSourceURI, nsIURI *aTargetURI,
                                      PRUint32 aFlags)
{
    NS_PRECONDITION(aSourceURI, "CheckLoadURI called with null source URI");
    NS_ENSURE_ARG_POINTER(aSourceURI);
    
    nsCOMPtr<nsIPrincipal> sourcePrincipal;
    nsresult rv = CreateCodebasePrincipal(aSourceURI,
                                          getter_AddRefs(sourcePrincipal));
    NS_ENSURE_SUCCESS(rv, rv);
    return CheckLoadURIWithPrincipal(sourcePrincipal, aTargetURI, aFlags);
}

NS_IMETHODIMP
nsScriptSecurityManager::CheckLoadURIWithPrincipal(nsIPrincipal* aPrincipal,
                                                   nsIURI *aTargetURI,
                                                   PRUint32 aFlags)
{
    NS_PRECONDITION(aPrincipal, "CheckLoadURIWithPrincipal must have a principal");
    NS_ENSURE_ARG_POINTER(aPrincipal);

    if (aPrincipal == mSystemPrincipal) {
        // Allow access
        return NS_OK;
    }
    
    nsCOMPtr<nsIURI> sourceURI;
    aPrincipal->GetURI(getter_AddRefs(sourceURI));

    NS_ASSERTION(sourceURI, "Non-system principals passed to CheckLoadURIWithPrincipal must have a URI!");
    
    //-- get the source scheme
    nsCAutoString sourceScheme;
    nsresult rv = GetBaseURIScheme(sourceURI, sourceScheme);
    if (NS_FAILED(rv)) return rv;

    // Some loads are not allowed from mail/news messages
    if ((aFlags & nsIScriptSecurityManager::DISALLOW_FROM_MAIL) &&
        (sourceScheme.LowerCaseEqualsLiteral("mailbox") ||
         sourceScheme.LowerCaseEqualsLiteral("imap")    ||
         sourceScheme.LowerCaseEqualsLiteral("news")))
    {
        return NS_ERROR_DOM_BAD_URI;
    }

    //-- get the target scheme
    nsCAutoString targetScheme;
    rv = GetBaseURIScheme(aTargetURI, targetScheme);
    if (NS_FAILED(rv)) return rv;

    if (nsCRT::strcasecmp(targetScheme.get(), sourceScheme.get()) == 0)
    {
        // every scheme can access another URI from the same scheme
        return NS_OK;
    }

    //-- Some callers do not allow loading javascript: or data: URLs
    if ((aFlags & nsIScriptSecurityManager::DISALLOW_SCRIPT_OR_DATA) &&
        (targetScheme.Equals("javascript") || targetScheme.Equals("data")))
    {
       return NS_ERROR_DOM_BAD_URI;
    }

    //-- If the schemes don't match, the policy is specified in this table.
    enum Action { AllowProtocol, DenyProtocol, PrefControlled, ChromeProtocol};
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
        { "about",           ChromeProtocol },
        { "mailto",          AllowProtocol  },
        { "aim",             AllowProtocol  },
        { "data",            AllowProtocol  },
        { "keyword",         DenyProtocol   },
        { "resource",        ChromeProtocol },
        { "gopher",          AllowProtocol  },
        { "datetime",        DenyProtocol   },
        { "finger",          AllowProtocol  },
        { "res",             DenyProtocol   },
        { "x-jsd",           ChromeProtocol }
    };

    NS_NAMED_LITERAL_STRING(errorTag, "CheckLoadURIError");
    for (unsigned i=0; i < sizeof(protocolList)/sizeof(protocolList[0]); i++)
    {
        if (targetScheme.LowerCaseEqualsASCII(protocolList[i].name))
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
                    mSecurityPref->SecurityGetBoolPref("security.checkloaduri",
                                                       &doCheck);
                    if (doCheck)
                    {
                        // resource: and chrome: are equivalent, securitywise
                        if (sourceScheme.EqualsLiteral("chrome") ||
                            sourceScheme.EqualsLiteral("resource"))
                            return NS_OK;

                        ReportError(nsnull, errorTag, sourceURI, aTargetURI);
                        return NS_ERROR_DOM_BAD_URI;
                    }
                    return NS_OK;
                }
            case ChromeProtocol:
                if (aFlags & nsIScriptSecurityManager::ALLOW_CHROME)
                    return NS_OK;
                // resource: and chrome: are equivalent, securitywise
                if (sourceScheme.EqualsLiteral("chrome") ||
                    sourceScheme.EqualsLiteral("resource"))
                    return NS_OK;
                ReportError(nsnull, errorTag, sourceURI, aTargetURI);
                return NS_ERROR_DOM_BAD_URI;
            case DenyProtocol:
                // Deny access
                ReportError(nsnull, errorTag, sourceURI, aTargetURI);
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
    NS_ConvertASCIItoUCS2 ucsSourceSpec(sourceSpec);
    NS_ConvertASCIItoUCS2 ucsTargetSpec(targetSpec);
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
    nsresult rv = NS_NewURI(getter_AddRefs(source),
                            nsDependentCString(aSourceURIStr),
                            nsnull, nsnull, sIOService);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIURI> target;
    rv = NS_NewURI(getter_AddRefs(target),
                   nsDependentCString(aTargetURIStr),
                   nsnull, nsnull, sIOService);
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
    {
#ifdef DEBUG
        {
            JSFunction *fun =
                (JSFunction *)JS_GetPrivate(aCx, (JSObject *)aFunObj);
            JSScript *script = JS_GetFunctionScript(aCx, fun);

            NS_ASSERTION(!script, "Null principal for non-native function!");
        }
#endif

        rv = doGetObjectPrincipal(aCx, (JSObject*)aFunObj,
                                  getter_AddRefs(subject));
    }

    if (NS_FAILED(rv)) return rv;
    if (!subject) return NS_ERROR_FAILURE;


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
    nsCOMPtr<nsIPrincipal> object;
    if (NS_FAILED(doGetObjectPrincipal(aCx, obj, getter_AddRefs(object))))
        return NS_ERROR_FAILURE;
    if (subject == object)
        return NS_OK;

    return CheckSameOriginPrincipalInternal(subject, object, PR_TRUE);
}

nsresult
nsScriptSecurityManager::GetRootDocShell(JSContext *cx, nsIDocShell **result)
{
    nsresult rv;
    *result = nsnull;
    nsIScriptContext *scriptContext = GetScriptContext(cx);
    if (!scriptContext)
        return NS_ERROR_FAILURE;

    nsIScriptGlobalObject *globalObject = scriptContext->GetGlobalObject();
    if (!globalObject)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDocShellTreeItem> docshellTreeItem =
        do_QueryInterface(globalObject->GetDocShell(), &rv);
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
    if (!mIsJavaScriptEnabled)
    {
        nsCOMPtr<nsIURI> principalURI;
        aPrincipal->GetURI(getter_AddRefs(principalURI));
        if (principalURI)
        {
            PRBool isChrome = PR_FALSE;
            principalURI->SchemeIs("chrome", &isChrome);
            if (isChrome)
            {
                *result = PR_TRUE;
                return NS_OK;              
            }
        }
    }

    //-- See if the current window allows JS execution
    nsIScriptContext *scriptContext = GetScriptContext(cx);
    if (!scriptContext) return NS_ERROR_FAILURE;
    nsIScriptGlobalObject *globalObject = scriptContext->GetGlobalObject();
    if (!globalObject) return NS_ERROR_FAILURE;

    nsresult rv;
    nsCOMPtr<nsIDocShell> docshell = globalObject->GetDocShell();
    nsCOMPtr<nsIDocShellTreeItem> globalObjTreeItem = do_QueryInterface(docshell);
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

    SecurityLevel secLevel;
    rv = LookupPolicy(aPrincipal, (char*)jsPrefGroupName, sEnabledID,
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
        nsRefPtr<nsSystemPrincipal> system = new nsSystemPrincipal();
        if (!system)
            return NS_ERROR_OUT_OF_MEMORY;

        nsresult rv = system->Init();
        if (NS_FAILED(rv))
            return rv;

        mSystemPrincipal = system;
    }

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
nsScriptSecurityManager::GetCertificatePrincipal(const char* aCertID,
                                                 nsIURI* aURI,
                                                 nsIPrincipal **result)
{
    // Create a certificate principal out of the certificate ID
    // and URI given to us.  We will use this principal to test
    // equality when doing our hashtable lookups below.
    nsRefPtr<nsPrincipal> certificate = new nsPrincipal();
    if (!certificate)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = certificate->Init(aCertID, aURI);
    NS_ENSURE_SUCCESS(rv, rv);

    // Check to see if we already have this principal.
    nsCOMPtr<nsIPrincipal> fromTable;
    mPrincipals.Get(certificate, getter_AddRefs(fromTable));
    if (fromTable) {
        // Bingo.  We found the certificate in the table, which means
        // that it has escalated priveleges.
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
            nsXPIDLCString granted;
            nsXPIDLCString denied;
            rv = fromTable->GetPreferences(getter_Copies(prefName),
                                           getter_Copies(id),
                                           getter_Copies(granted),
                                           getter_Copies(denied));
            if (NS_SUCCEEDED(rv)) {
                certificate = new nsPrincipal();
                if (!certificate)
                    return NS_ERROR_OUT_OF_MEMORY;

                rv = certificate->InitFromPersistent(prefName, id,
                                                     granted, denied,
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
    nsRefPtr<nsPrincipal> codebase = new nsPrincipal();
    if (!codebase)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = codebase->Init(nsnull, aURI);
    if (NS_FAILED(rv))
        return rv;

    NS_ADDREF(*result = codebase);

    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::GetCodebasePrincipal(nsIURI *aURI,
                                              nsIPrincipal **result)
{
    nsresult rv;
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

nsresult
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
    {
        if (JS_GetFunctionObject(fun) != obj)
        {
            // Function is a clone, its prototype was precompiled from
            // brutally shared chrome. For this case only, get the
            // principals from the clone's scope since there's no
            // reliable principals compiled into the function.
            return doGetObjectPrincipal(cx, obj, result);
        }

        if (NS_FAILED(GetScriptPrincipal(cx, script,
                                         getter_AddRefs(scriptPrincipal))))
            return NS_ERROR_FAILURE;

    }

    NS_IF_ADDREF(*result = scriptPrincipal);

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

    nsresult rv = GetFunctionObjectPrincipal(cx, obj, result);

#ifdef DEBUG
    if (NS_SUCCEEDED(rv) && !*result)
    {
        JSFunction *fun = (JSFunction *)JS_GetPrivate(cx, obj);
        JSScript *script = JS_GetFunctionScript(cx, fun);

        NS_ASSERTION(!script, "Null principal for non-native function!");
    }
#endif

    return rv;
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
        nsIScriptContext *scriptContext = GetScriptContext(cx);
        if (scriptContext)
        {
            nsCOMPtr<nsIScriptObjectPrincipal> globalData =
                do_QueryInterface(scriptContext->GetGlobalObject());
            NS_ENSURE_TRUE(globalData, NS_ERROR_FAILURE);

            NS_IF_ADDREF(*result = globalData->GetPrincipal());
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

            if (objPrin && (*result = objPrin->GetPrincipal()))
            {
                NS_ADDREF(*result);
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
    //-- Save to mPrincipals
    mPrincipals.Put(aToSave, aToSave);

    //-- Save to prefs
    nsXPIDLCString idPrefName;
    nsXPIDLCString id;
    nsXPIDLCString grantedList;
    nsXPIDLCString deniedList;
    nsresult rv = aToSave->GetPreferences(getter_Copies(idPrefName),
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
        rv = aPrincipal->GetCommonName(getter_Copies(val));
    else
        rv = aPrincipal->GetOrigin(getter_Copies(val));

    if (NS_FAILED(rv))
        return PR_FALSE;

    NS_ConvertUTF8toUTF16 location(val.get());
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
        nsXPIDLCString val;
        PRBool hasCert;
        nsresult rv;
        principal->GetHasCertificate(&hasCert);
        if (hasCert)
            rv = principal->GetCommonName(getter_Copies(val));
        else
            rv = principal->GetOrigin(getter_Copies(val));

        if (NS_FAILED(rv))
            return rv;

        NS_ConvertUTF8toUTF16 location(val.get());
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
        nsCOMPtr<nsIZipReader> systemCertZip = do_CreateInstance(kZipReaderCID, &rv);
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
        SetPendingException(cx, mSystemCertificate ? msg1 : msg2);
        return NS_ERROR_FAILURE;
    }

    //-- Get the target principal
    nsCOMPtr<nsIPrincipal> objectPrincipal;
    rv =  GetCertificatePrincipal(certificateID, nsnull, getter_AddRefs(objectPrincipal));
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
    nsCOMPtr<nsIPrincipal> subjectPrincipal;
    if (NS_FAILED(GetSubjectPrincipal(cx, getter_AddRefs(subjectPrincipal))))
        return NS_ERROR_FAILURE;

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
    jsval cidVal = STRING_TO_JSVAL(::JS_InternString(cx, cid.get()));

    SecurityLevel securityLevel;
    rv = LookupPolicy(subjectPrincipal, "ClassID", cidVal,
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
static const char sPolicyPrefix[] = "capability.policy.";

NS_IMETHODIMP
nsScriptSecurityManager::Observe(nsISupports* aObject, const char* aTopic,
                                 const PRUnichar* aMessage)
{
    nsresult rv = NS_OK;
    NS_ConvertUCS2toUTF8 messageStr(aMessage);
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

    rv = bundleService->CreateBundle("chrome://communicator/locale/security/caps.properties", &sStrBundle);
    NS_ENSURE_SUCCESS(rv, rv);

    //-- Register security check callback in the JS engine
    //   Currently this is used to control access to function.caller
    nsCOMPtr<nsIJSRuntimeService> runtimeService =
        do_GetService("@mozilla.org/js/xpc/RuntimeService;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    JSRuntime *rt;
    rv = runtimeService->GetRuntime(&rt);
    NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG
    JSCheckAccessOp oldCallback =
#endif
        JS_SetCheckObjectAccessCallback(rt, CheckObjectAccess);

    // For now, assert that no callback was set previously
    NS_ASSERTION(!oldCallback, "Someone already set a JS CheckObjectAccess callback");
    return NS_OK;
}

static nsScriptSecurityManager *gScriptSecMan = nsnull;

jsval nsScriptSecurityManager::sEnabledID   = JSVAL_VOID;

nsScriptSecurityManager::~nsScriptSecurityManager(void)
{
    delete mOriginToPolicyMap;
    delete mDefaultPolicy;
    delete mCapabilities;
    gScriptSecMan = nsnull;
}

void
nsScriptSecurityManager::Shutdown()
{
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
        gScriptSecMan->GetSystemPrincipal(&sysprin);
    return NS_STATIC_CAST(nsSystemPrincipal*, sysprin);
}

nsresult
nsScriptSecurityManager::InitPolicies()
{
    // Reset the "dirty" flag
    mPolicyPrefsChanged = PR_FALSE;

    // Clear any policies cached on XPConnect wrappers
    NS_ENSURE_STATE(sXPConnect);
    nsresult rv = sXPConnect->ClearAllWrappedNativeSecurityPolicies();
    if (NS_FAILED(rv)) return rv;

    //-- Reset mOriginToPolicyMap
    delete mOriginToPolicyMap;
    mOriginToPolicyMap =
      new nsObjectHashtable(nsnull, nsnull, DeleteDomainEntry, nsnull);

    //-- Reset and initialize the default policy
    delete mDefaultPolicy;
    mDefaultPolicy = new DomainPolicy();
    if (!mOriginToPolicyMap || !mDefaultPolicy)
        return NS_ERROR_OUT_OF_MEMORY;

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
                    delete domainPolicy;
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
        if (NS_FAILED(rv))
            return rv;
    }

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
          NS_STATIC_CAST(ClassPolicy*,
                         PL_DHashTableOperate(aDomainPolicy, start,
                                              PL_DHASH_ADD));
        if (!cpolicy)
            break;

        // If this is the wildcard class (class '*'), save it in mWildcardPolicy
        // (we leave it stored in the hashtable too to take care of the cleanup)
        if ((*start == '*') && (end == start + 1))
            aDomainPolicy->mWildcardPolicy = cpolicy;

        // Get the property name
        start = end + 1;
        end = PL_strchr(start, '.');
        if (end)
            *end = '\0';

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
        if (NS_FAILED(rv))
            continue;

        nsXPIDLCString grantedList;
        mSecurityPref->SecurityGetCharPref(grantedPrefName, getter_Copies(grantedList));
        nsXPIDLCString deniedList;
        mSecurityPref->SecurityGetCharPref(deniedPrefName, getter_Copies(deniedList));

        //-- Delete prefs if their value is the empty string
        if (id.IsEmpty() || (grantedList.IsEmpty() && deniedList.IsEmpty()))
        {
            mSecurityPref->SecurityClearUserPref(aPrefNames[c]);
            mSecurityPref->SecurityClearUserPref(grantedPrefName);
            mSecurityPref->SecurityClearUserPref(deniedPrefName);
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

        rv = newPrincipal->InitFromPersistent(aPrefNames[c], id.get(),
                                              grantedList, deniedList,
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
    nsCOMPtr<nsIPrefBranchInternal> prefBranchInternal(do_QueryInterface(mPrefBranch, &rv));
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
        NS_ENSURE_SUCCESS(rv, rv);
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(prefCount, prefNames);
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

