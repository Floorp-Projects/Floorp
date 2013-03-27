/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "xpcprivate.h"
#include "XPCWrapper.h"
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
#include "nsError.h"
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
#include "nsIFileURL.h"
#include "nsIZipReader.h"
#include "nsIXPConnect.h"
#include "nsIScriptGlobalObject.h"
#include "nsPIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIPrompt.h"
#include "nsIWindowWatcher.h"
#include "nsIConsoleService.h"
#include "nsISecurityCheckedComponent.h"
#include "nsIJSRuntimeService.h"
#include "nsIObserverService.h"
#include "nsIContent.h"
#include "nsAutoPtr.h"
#include "nsDOMJSUtils.h"
#include "nsAboutProtocolUtils.h"
#include "nsIClassInfo.h"
#include "nsIURIFixup.h"
#include "nsCDefaultURIFixup.h"
#include "nsIChromeRegistry.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/StandardInteger.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"
#include "nsContentUtils.h"

// This should be probably defined on some other place... but I couldn't find it
#define WEBAPPS_PERM_NAME "webapps-manage"

using namespace mozilla;
using namespace mozilla::dom;

static NS_DEFINE_CID(kZipReaderCID, NS_ZIPREADER_CID);

nsIIOService    *nsScriptSecurityManager::sIOService = nullptr;
nsIXPConnect    *nsScriptSecurityManager::sXPConnect = nullptr;
nsIThreadJSContextStack *nsScriptSecurityManager::sJSContextStack = nullptr;
nsIStringBundle *nsScriptSecurityManager::sStrBundle = nullptr;
JSRuntime       *nsScriptSecurityManager::sRuntime   = 0;
bool nsScriptSecurityManager::sStrictFileOriginPolicy = true;

bool
nsScriptSecurityManager::SubjectIsPrivileged()
{
    JSContext *cx = GetCurrentJSContext();
    if (cx && xpc::IsUniversalXPConnectEnabled(cx))
        return true;
    bool isSystem = false;
    return NS_SUCCEEDED(SubjectPrincipalIsSystem(&isSystem)) && isSystem;
}

///////////////////////////
// Convenience Functions //
///////////////////////////
// Result of this function should not be freed.
static inline const PRUnichar *
IDToString(JSContext *cx, jsid id)
{
    if (JSID_IS_STRING(id))
        return JS_GetInternedStringChars(JSID_TO_STRING(id));

    JSAutoRequest ar(cx);
    JS::Value idval;
    if (!JS_IdToValue(cx, id, &idval))
        return nullptr;
    JSString *str = JS_ValueToString(cx, idval);
    if(!str)
        return nullptr;
    return JS_GetStringCharsZ(cx, str);
}

class nsAutoInPrincipalDomainOriginSetter {
public:
    nsAutoInPrincipalDomainOriginSetter() {
        ++sInPrincipalDomainOrigin;
    }
    ~nsAutoInPrincipalDomainOriginSetter() {
        --sInPrincipalDomainOrigin;
    }
    static uint32_t sInPrincipalDomainOrigin;
};
uint32_t nsAutoInPrincipalDomainOriginSetter::sInPrincipalDomainOrigin;

static
nsresult
GetOriginFromURI(nsIURI* aURI, nsACString& aOrigin)
{
  if (nsAutoInPrincipalDomainOriginSetter::sInPrincipalDomainOrigin > 1) {
      // Allow a single recursive call to GetPrincipalDomainOrigin, since that
      // might be happening on a different principal from the first call.  But
      // after that, cut off the recursion; it just indicates that something
      // we're doing in this method causes us to reenter a security check here.
      return NS_ERROR_NOT_AVAILABLE;
  }

  nsAutoInPrincipalDomainOriginSetter autoSetter;

  nsCOMPtr<nsIURI> uri = NS_GetInnermostURI(aURI);
  NS_ENSURE_TRUE(uri, NS_ERROR_UNEXPECTED);

  nsAutoCString hostPort;

  nsresult rv = uri->GetHostPort(hostPort);
  if (NS_SUCCEEDED(rv)) {
    nsAutoCString scheme;
    rv = uri->GetScheme(scheme);
    NS_ENSURE_SUCCESS(rv, rv);
    aOrigin = scheme + NS_LITERAL_CSTRING("://") + hostPort;
  }
  else {
    // Some URIs (e.g., nsSimpleURI) don't support host. Just
    // get the full spec.
    rv = uri->GetSpec(aOrigin);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

static
nsresult
GetPrincipalDomainOrigin(nsIPrincipal* aPrincipal,
                         nsACString& aOrigin)
{

  nsCOMPtr<nsIURI> uri;
  aPrincipal->GetDomain(getter_AddRefs(uri));
  if (!uri) {
    aPrincipal->GetURI(getter_AddRefs(uri));
  }
  NS_ENSURE_TRUE(uri, NS_ERROR_UNEXPECTED);

  return GetOriginFromURI(uri, aOrigin);
}

static nsIScriptContext *
GetScriptContext(JSContext *cx)
{
    return GetScriptContextFromJSContext(cx);
}

inline void SetPendingException(JSContext *cx, const char *aMsg)
{
    JSAutoRequest ar(cx);
    JS_ReportError(cx, "%s", aMsg);
}

inline void SetPendingException(JSContext *cx, const PRUnichar *aMsg)
{
    JSAutoRequest ar(cx);
    JS_ReportError(cx, "%hs", aMsg);
}

// DomainPolicy members
uint32_t DomainPolicy::sGeneration = 0;

// Helper class to get stuff from the ClassInfo and not waste extra time with
// virtual method calls for things it has already gotten
class ClassInfoData
{
public:
    ClassInfoData(nsIClassInfo *aClassInfo, const char *aName)
        : mClassInfo(aClassInfo),
          mName(const_cast<char *>(aName)),
          mDidGetFlags(false),
          mMustFreeName(false)
    {
    }

    ~ClassInfoData()
    {
        if (mMustFreeName)
            nsMemory::Free(mName);
    }

    uint32_t GetFlags()
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

            mDidGetFlags = true;
        }

        return mFlags;
    }

    bool IsDOMClass()
    {
        return !!(GetFlags() & nsIClassInfo::DOM_OBJECT);
    }

    const char* GetName()
    {
        if (!mName) {
            if (mClassInfo) {
                mClassInfo->GetClassDescription(&mName);
            }

            if (mName) {
                mMustFreeName = true;
            } else {
                mName = const_cast<char *>("UnnamedClass");
            }
        }

        return mName;
    }

private:
    nsIClassInfo *mClassInfo; // WEAK
    uint32_t mFlags;
    char *mName;
    bool mDidGetFlags;
    bool mMustFreeName;
};

class AutoCxPusher {
public:
    AutoCxPusher(nsIJSContextStack *aStack, JSContext *cx)
        : mStack(aStack), mContext(cx)
    {
        if (NS_FAILED(mStack->Push(mContext))) {
            mStack = nullptr;
        }
    }

    ~AutoCxPusher()
    {
        if (mStack) {
            mStack->Pop(nullptr);
        }
    }

private:
    nsCOMPtr<nsIJSContextStack> mStack;
    JSContext *mContext;
};

JSContext *
nsScriptSecurityManager::GetCurrentJSContext()
{
    // Get JSContext from stack.
    JSContext *cx;
    if (NS_FAILED(sJSContextStack->Peek(&cx)))
        return nullptr;
    return cx;
}

JSContext *
nsScriptSecurityManager::GetSafeJSContext()
{
    // Get JSContext from stack.
    return sJSContextStack->GetSafeJSContext();
}

/* static */
bool
nsScriptSecurityManager::SecurityCompareURIs(nsIURI* aSourceURI,
                                             nsIURI* aTargetURI)
{
    return NS_SecurityCompareURIs(aSourceURI, aTargetURI, sStrictFileOriginPolicy);
}

// SecurityHashURI is consistent with SecurityCompareURIs because NS_SecurityHashURI
// is consistent with NS_SecurityCompareURIs.  See nsNetUtil.h.
uint32_t
nsScriptSecurityManager::SecurityHashURI(nsIURI* aURI)
{
    return NS_SecurityHashURI(aURI);
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
    // as nsDocument::Reset and XULDocument::StartDocumentLoad.
    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDocShell> docShell;
    NS_QueryNotificationCallbacks(aChannel, docShell);

    if (docShell) {
        return GetDocShellCodebasePrincipal(uri, docShell, aPrincipal);
    }

    return GetCodebasePrincipalInternal(uri, UNKNOWN_APP_ID,
        /* isInBrowserElement */ false, aPrincipal);
}

NS_IMETHODIMP
nsScriptSecurityManager::IsSystemPrincipal(nsIPrincipal* aPrincipal,
                                           bool* aIsSystem)
{
    *aIsSystem = (aPrincipal == mSystemPrincipal);
    return NS_OK;
}

NS_IMETHODIMP_(nsIPrincipal *)
nsScriptSecurityManager::GetCxSubjectPrincipal(JSContext *cx)
{
    NS_ASSERTION(cx == GetCurrentJSContext(),
                 "Uh, cx is not the current JS context!");

    nsresult rv = NS_ERROR_FAILURE;
    nsIPrincipal *principal = GetSubjectPrincipal(cx, &rv);
    if (NS_FAILED(rv))
        return nullptr;

    return principal;
}

////////////////////
// Policy Storage //
////////////////////

// Table of security levels
static bool
DeleteCapability(nsHashKey *aKey, void *aData, void* closure)
{
    NS_Free(aData);
    return true;
}

//-- Per-Domain Policy - applies to one or more protocols or hosts
struct DomainEntry
{
    DomainEntry(const char* aOrigin,
                DomainPolicy* aDomainPolicy) : mOrigin(aOrigin),
                                               mDomainPolicy(aDomainPolicy),
                                               mNext(nullptr)
    {
        mDomainPolicy->Hold();
    }

    ~DomainEntry()
    {
        mDomainPolicy->Drop();
    }

    bool Matches(const char *anOrigin)
    {
        int len = strlen(anOrigin);
        int thisLen = mOrigin.Length();
        if (len < thisLen)
            return false;
        if (mOrigin.RFindChar(':', thisLen-1, 1) != -1)
        //-- Policy applies to all URLs of this scheme, compare scheme only
            return mOrigin.EqualsIgnoreCase(anOrigin, thisLen);

        //-- Policy applies to a particular host; compare domains
        if (!mOrigin.Equals(anOrigin + (len - thisLen)))
            return false;
        if (len == thisLen)
            return true;
        char charBefore = anOrigin[len-thisLen-1];
        return (charBefore == '.' || charBefore == ':' || charBefore == '/');
    }

    nsCString         mOrigin;
    DomainPolicy*     mDomainPolicy;
    DomainEntry*      mNext;
};

static bool
DeleteDomainEntry(nsHashKey *aKey, void *aData, void* closure)
{
    DomainEntry *entry = (DomainEntry*) aData;
    do
    {
        DomainEntry *next = entry->mNext;
        delete entry;
        entry = next;
    } while (entry);
    return true;
}

/////////////////////////////
// nsScriptSecurityManager //
/////////////////////////////

////////////////////////////////////
// Methods implementing ISupports //
////////////////////////////////////
NS_IMPL_ISUPPORTS4(nsScriptSecurityManager,
                   nsIScriptSecurityManager,
                   nsIXPCSecurityManager,
                   nsIChannelEventSink,
                   nsIObserver)

///////////////////////////////////////////////////
// Methods implementing nsIScriptSecurityManager //
///////////////////////////////////////////////////

///////////////// Security Checks /////////////////

JSBool
nsScriptSecurityManager::ContentSecurityPolicyPermitsJSAction(JSContext *cx)
{
    // Get the security manager
    nsScriptSecurityManager *ssm =
        nsScriptSecurityManager::GetScriptSecurityManager();

    NS_ASSERTION(ssm, "Failed to get security manager service");
    if (!ssm)
        return JS_FALSE;

    nsresult rv;
    nsIPrincipal* subjectPrincipal = ssm->GetSubjectPrincipal(cx, &rv);

    NS_ASSERTION(NS_SUCCEEDED(rv), "CSP: Failed to get nsIPrincipal from js context");
    if (NS_FAILED(rv))
        return JS_FALSE; // Not just absence of principal, but failure.

    if (!subjectPrincipal)
        return JS_TRUE;

    nsCOMPtr<nsIContentSecurityPolicy> csp;
    rv = subjectPrincipal->GetCsp(getter_AddRefs(csp));
    NS_ASSERTION(NS_SUCCEEDED(rv), "CSP: Failed to get CSP from principal.");

    // don't do anything unless there's a CSP
    if (!csp)
        return JS_TRUE;

    bool evalOK = true;
    rv = csp->GetAllowsEval(&evalOK);

    if (NS_FAILED(rv))
    {
        NS_WARNING("CSP: failed to get allowsEval");
        return JS_TRUE; // fail open to not break sites.
    }

    if (!evalOK) {
        // get the script filename, script sample, and line number
        // to log with the violation
        nsAutoString fileName;
        unsigned lineNum = 0;
        NS_NAMED_LITERAL_STRING(scriptSample, "call to eval() or related function blocked by CSP");

        JSScript *script;
        if (JS_DescribeScriptedCaller(cx, &script, &lineNum)) {
            if (const char *file = JS_GetScriptFilename(cx, script)) {
                CopyUTF8toUTF16(nsDependentCString(file), fileName);
            }
        }

        csp->LogViolationDetails(nsIContentSecurityPolicy::VIOLATION_TYPE_EVAL,
                                 fileName,
                                 scriptSample,
                                 lineNum);
    }

    return evalOK;
}


JSBool
nsScriptSecurityManager::CheckObjectAccess(JSContext *cx, JSHandleObject obj,
                                           JSHandleId id, JSAccessMode mode,
                                           JSMutableHandleValue vp)
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
    // vp can be a primitive, in that case, we use obj as the target
    // object.
    JSObject* target = JSVAL_IS_PRIMITIVE(vp) ? obj : JSVAL_TO_OBJECT(vp);

    // Do the same-origin check -- this sets a JS exception if the check fails.
    // Pass the parent object's class name, as we have no class-info for it.
    nsresult rv =
        ssm->CheckPropertyAccess(cx, target, js::GetObjectClass(obj)->name, id,
                                 (mode & JSACC_WRITE) ?
                                 (int32_t)nsIXPCSecurityManager::ACCESS_SET_PROPERTY :
                                 (int32_t)nsIXPCSecurityManager::ACCESS_GET_PROPERTY);

    if (NS_FAILED(rv))
        return JS_FALSE; // Security check failed (XXX was an error reported?)

    return JS_TRUE;
}

NS_IMETHODIMP
nsScriptSecurityManager::CheckPropertyAccess(JSContext* cx,
                                             JSObject* aJSObject,
                                             const char* aClassName,
                                             jsid aProperty,
                                             uint32_t aAction)
{
    return CheckPropertyAccessImpl(aAction, nullptr, cx, aJSObject,
                                   nullptr, nullptr,
                                   aClassName, aProperty, nullptr);
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
                                            nsIURI* aTargetURI,
                                            bool reportError)
{
    if (!SecurityCompareURIs(aSourceURI, aTargetURI))
    {
         if (reportError) {
            ReportError(nullptr, NS_LITERAL_STRING("CheckSameOriginError"),
                     aSourceURI, aTargetURI);
         }
         return NS_ERROR_DOM_BAD_URI;
    }
    return NS_OK;
}

nsresult
nsScriptSecurityManager::CheckPropertyAccessImpl(uint32_t aAction,
                                                 nsAXPCNativeCallContext* aCallContext,
                                                 JSContext* cx, JSObject* aJSObject,
                                                 nsISupports* aObj,
                                                 nsIClassInfo* aClassInfo,
                                                 const char* aClassName, jsid aProperty,
                                                 void** aCachedClassPolicy)
{
    nsresult rv;
    nsIPrincipal* subjectPrincipal = GetSubjectPrincipal(cx, &rv);
    if (NS_FAILED(rv))
        return rv;

    if (!subjectPrincipal || subjectPrincipal == mSystemPrincipal)
        // We have native code or the system principal: just allow access
        return NS_OK;

    nsCOMPtr<nsIPrincipal> objectPrincipal;

    // Hold the class info data here so we don't have to go back to virtual
    // methods all the time
    ClassInfoData classInfoData(aClassInfo, aClassName);

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
            rv = NS_ERROR_DOM_PROP_ACCESS_DENIED;
            break;

        case SCRIPT_SECURITY_ALL_ACCESS:
            rv = NS_OK;
            break;

        case SCRIPT_SECURITY_SAME_ORIGIN_ACCESS:
            {
                nsCOMPtr<nsIPrincipal> principalHolder;
                if(aJSObject)
                {
                    objectPrincipal = doGetObjectPrincipal(aJSObject);
                    if (!objectPrincipal)
                        rv = NS_ERROR_DOM_SECURITY_ERR;
                }
                else
                {
                    NS_ERROR("CheckPropertyAccessImpl called without a target object or URL");
                    return NS_ERROR_FAILURE;
                }
                if(NS_SUCCEEDED(rv))
                    rv = CheckSameOriginDOMProp(subjectPrincipal, objectPrincipal,
                                                aAction);
                break;
            }
        default:
            NS_ERROR("Bad Security Level Value");
            return NS_ERROR_FAILURE;
        }
    }
    else // if SECURITY_ACCESS_LEVEL_FLAG is false, securityLevel is a capability
    {
        rv = SubjectIsPrivileged() ? NS_OK : NS_ERROR_DOM_SECURITY_ERR;
    }

    if (NS_SUCCEEDED(rv))
    {
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
        const nsIID* objIID = nullptr;
        rv = aCallContext->GetCalleeWrapper(getter_AddRefs(wrapper));
        if (NS_SUCCEEDED(rv) && wrapper)
            rv = wrapper->FindInterfaceWithMember(aProperty, getter_AddRefs(interfaceInfo));
        if (NS_SUCCEEDED(rv) && interfaceInfo)
            rv = interfaceInfo->GetIIDShared(&objIID);
        if (NS_SUCCEEDED(rv) && objIID)
        {
            switch (aAction)
            {
            case nsIXPCSecurityManager::ACCESS_GET_PROPERTY:
                checkedComponent->CanGetProperty(objIID,
                                                 IDToString(cx, aProperty),
                                                 getter_Copies(objectSecurityLevel));
                break;
            case nsIXPCSecurityManager::ACCESS_SET_PROPERTY:
                checkedComponent->CanSetProperty(objIID,
                                                 IDToString(cx, aProperty),
                                                 getter_Copies(objectSecurityLevel));
                break;
            case nsIXPCSecurityManager::ACCESS_CALL_METHOD:
                checkedComponent->CanCallMethod(objIID,
                                                IDToString(cx, aProperty),
                                                getter_Copies(objectSecurityLevel));
            }
        }
    }
    rv = CheckXPCPermissions(cx, aObj, aJSObject, subjectPrincipal,
                             objectSecurityLevel);

    if (NS_FAILED(rv)) //-- Security tests failed, access is denied, report error
    {
        nsAutoString stringName;
        switch(aAction)
        {
        case nsIXPCSecurityManager::ACCESS_GET_PROPERTY:
            stringName.AssignLiteral("GetPropertyDeniedOrigins");
            break;
        case nsIXPCSecurityManager::ACCESS_SET_PROPERTY:
            stringName.AssignLiteral("SetPropertyDeniedOrigins");
            break;
        case nsIXPCSecurityManager::ACCESS_CALL_METHOD:
            stringName.AssignLiteral("CallMethodDeniedOrigins");
        }

        // Null out objectPrincipal for now, so we don't leak information about
        // it.  Whenever we can report different error strings to content and
        // the UI we can take this out again.
        objectPrincipal = nullptr;

        NS_ConvertUTF8toUTF16 className(classInfoData.GetName());
        nsAutoCString subjectOrigin;
        nsAutoCString subjectDomain;
        if (!nsAutoInPrincipalDomainOriginSetter::sInPrincipalDomainOrigin) {
            nsCOMPtr<nsIURI> uri, domain;
            subjectPrincipal->GetURI(getter_AddRefs(uri));
            if (uri) { // Object principal might be expanded
                GetOriginFromURI(uri, subjectOrigin);
            }
            subjectPrincipal->GetDomain(getter_AddRefs(domain));
            if (domain) {
                GetOriginFromURI(domain, subjectDomain);
            }
        } else {
            subjectOrigin.AssignLiteral("the security manager");
        }
        NS_ConvertUTF8toUTF16 subjectOriginUnicode(subjectOrigin);
        NS_ConvertUTF8toUTF16 subjectDomainUnicode(subjectDomain);

        nsAutoCString objectOrigin;
        nsAutoCString objectDomain;
        if (!nsAutoInPrincipalDomainOriginSetter::sInPrincipalDomainOrigin &&
            objectPrincipal) {
            nsCOMPtr<nsIURI> uri, domain;
            objectPrincipal->GetURI(getter_AddRefs(uri));
            if (uri) { // Object principal might be system
                GetOriginFromURI(uri, objectOrigin);
            }
            objectPrincipal->GetDomain(getter_AddRefs(domain));
            if (domain) {
                GetOriginFromURI(domain, objectDomain);
            }
        }
        NS_ConvertUTF8toUTF16 objectOriginUnicode(objectOrigin);
        NS_ConvertUTF8toUTF16 objectDomainUnicode(objectDomain);

        nsXPIDLString errorMsg;
        const PRUnichar *formatStrings[] =
        {
            subjectOriginUnicode.get(),
            className.get(),
            IDToString(cx, aProperty),
            objectOriginUnicode.get(),
            subjectDomainUnicode.get(),
            objectDomainUnicode.get()
        };

        uint32_t length = ArrayLength(formatStrings);

        // XXXbz Our localization system is stupid and can't handle not showing
        // some strings that get passed in.  Which means that we have to get
        // our length precisely right: it has to be exactly the number of
        // strings our format string wants.  This means we'll have to move
        // strings in the array as needed, sadly...
        if (nsAutoInPrincipalDomainOriginSetter::sInPrincipalDomainOrigin ||
            !objectPrincipal) {
            stringName.AppendLiteral("OnlySubject");
            length -= 3;
        } else {
            // default to a length that doesn't include the domains, then
            // increase it as needed.
            length -= 2;
            if (!subjectDomainUnicode.IsEmpty()) {
                stringName.AppendLiteral("SubjectDomain");
                length += 1;
            }
            if (!objectDomainUnicode.IsEmpty()) {
                stringName.AppendLiteral("ObjectDomain");
                length += 1;
                if (length != ArrayLength(formatStrings)) {
                    // We have an object domain but not a subject domain.
                    // Scoot our string over one slot.  See the XXX comment
                    // above for why we need to do this.
                    formatStrings[length-1] = formatStrings[length];
                }
            }
        }
        
        // We need to keep our existing failure rv and not override it
        // with a likely success code from the following string bundle
        // call in order to throw the correct security exception later.
        nsresult rv2 = sStrBundle->FormatStringFromName(stringName.get(),
                                                        formatStrings,
                                                        length,
                                                        getter_Copies(errorMsg));
        if (NS_FAILED(rv2)) {
            // Might just be missing the string...  Do our best
            errorMsg = stringName;
        }

        SetPendingException(cx, errorMsg.get());
    }

    return rv;
}

/* static */
nsresult
nsScriptSecurityManager::CheckSameOriginPrincipal(nsIPrincipal* aSubject,
                                                  nsIPrincipal* aObject)
{
    /*
    ** Get origin of subject and object and compare.
    */
    if (aSubject == aObject)
        return NS_OK;

    if (!AppAttributesEqual(aSubject, aObject)) {
        return NS_ERROR_DOM_PROP_ACCESS_DENIED;
    }

    // Default to false, and change if that turns out wrong.
    bool subjectSetDomain = false;
    bool objectSetDomain = false;
    
    nsCOMPtr<nsIURI> subjectURI;
    nsCOMPtr<nsIURI> objectURI;

    aSubject->GetDomain(getter_AddRefs(subjectURI));
    if (!subjectURI) {
        aSubject->GetURI(getter_AddRefs(subjectURI));
    } else {
        subjectSetDomain = true;
    }

    aObject->GetDomain(getter_AddRefs(objectURI));
    if (!objectURI) {
        aObject->GetURI(getter_AddRefs(objectURI));
    } else {
        objectSetDomain = true;
    }

    if (SecurityCompareURIs(subjectURI, objectURI))
    {   // If either the subject or the object has changed its principal by
        // explicitly setting document.domain then the other must also have
        // done so in order to be considered the same origin. This prevents
        // DNS spoofing based on document.domain (154930)

        // If both or neither explicitly set their domain, allow the access
        if (subjectSetDomain == objectSetDomain)
            return NS_OK;
    }

    /*
    ** Access tests failed, so now report error.
    */
    return NS_ERROR_DOM_PROP_ACCESS_DENIED;
}

// It's important that
//
//   CheckSameOriginPrincipal(A, B) == NS_OK
//
// imply
//
//   HashPrincipalByOrigin(A) == HashPrincipalByOrigin(B)
//
// if principals A and B could ever be used as keys in a hashtable.
// Violation of this invariant leads to spurious failures of hashtable
// lookups.  See bug 454850.

/*static*/ uint32_t
nsScriptSecurityManager::HashPrincipalByOrigin(nsIPrincipal* aPrincipal)
{
    nsCOMPtr<nsIURI> uri;
    aPrincipal->GetDomain(getter_AddRefs(uri));
    if (!uri)
        aPrincipal->GetURI(getter_AddRefs(uri));
    return SecurityHashURI(uri);
}

/* static */ bool
nsScriptSecurityManager::AppAttributesEqual(nsIPrincipal* aFirst,
                                            nsIPrincipal* aSecond)
{
    MOZ_ASSERT(aFirst && aSecond, "Don't pass null pointers!");

    uint32_t firstAppId = nsIScriptSecurityManager::UNKNOWN_APP_ID;
    if (!aFirst->GetUnknownAppId()) {
        firstAppId = aFirst->GetAppId();
    }

    uint32_t secondAppId = nsIScriptSecurityManager::UNKNOWN_APP_ID;
    if (!aSecond->GetUnknownAppId()) {
        secondAppId = aSecond->GetAppId();
    }

    return ((firstAppId == secondAppId) &&
            (aFirst->GetIsInBrowserElement() == aSecond->GetIsInBrowserElement()));
}

nsresult
nsScriptSecurityManager::CheckSameOriginDOMProp(nsIPrincipal* aSubject,
                                                nsIPrincipal* aObject,
                                                uint32_t aAction)
{
    nsresult rv;
    bool subsumes;
    rv = aSubject->Subsumes(aObject, &subsumes);
    if (NS_SUCCEEDED(rv) && !subsumes) {
        rv = NS_ERROR_DOM_PROP_ACCESS_DENIED;
    }
    
    if (NS_SUCCEEDED(rv))
        return NS_OK;

    /*
    * Content can't ever touch chrome (we check for UniversalXPConnect later)
    */
    if (aObject == mSystemPrincipal)
        return NS_ERROR_DOM_PROP_ACCESS_DENIED;

    /*
    ** Access tests failed, so now report error.
    */
    return NS_ERROR_DOM_PROP_ACCESS_DENIED;
}

nsresult
nsScriptSecurityManager::LookupPolicy(nsIPrincipal* aPrincipal,
                                      ClassInfoData& aClassData,
                                      jsid aProperty,
                                      uint32_t aAction,
                                      ClassPolicy** aCachedClassPolicy,
                                      SecurityLevel* result)
{
    nsresult rv;
    result->level = SCRIPT_SECURITY_UNDEFINED_ACCESS;

    DomainPolicy* dpolicy = nullptr;
    //-- Initialize policies if necessary
    if (mPolicyPrefsChanged)
    {
        if (!mPrefInitialized) {
            rv = InitPrefs();
            NS_ENSURE_SUCCESS(rv, rv);
        }
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
        if (nsCOMPtr<nsIExpandedPrincipal> exp = do_QueryInterface(aPrincipal)) 
        {
            // For expanded principals domain origin is not defined so let's just
            // use the default policy
            dpolicy = mDefaultPolicy;
        }
        else
        {
            nsAutoCString origin;
            rv = GetPrincipalDomainOrigin(aPrincipal, origin);
            NS_ENSURE_SUCCESS(rv, rv);
 
            char *start = origin.BeginWriting();
            const char *nextToLastDot = nullptr;
            const char *lastDot = nullptr;
            const char *colon = nullptr;
            char *p = start;

            //-- search domain (stop at the end of the string or at the 3rd slash)
            for (uint32_t slashes=0; *p; p++)
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
                nsAutoCString scheme(start, colon-start+1);
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
        }

        aPrincipal->SetSecurityPolicy((void*)dpolicy);
    }

    ClassPolicy* cpolicy = nullptr;

    if ((dpolicy == mDefaultPolicy) && aCachedClassPolicy)
    {
        // No per-domain policy for this principal (the more common case)
        // so look for a cached class policy from the object wrapper
        cpolicy = *aCachedClassPolicy;
    }

    if (!cpolicy)
    { //-- No cached policy for this class, need to look it up
        cpolicy = static_cast<ClassPolicy*>
                             (PL_DHashTableOperate(dpolicy,
                                                      aClassData.GetName(),
                                                      PL_DHASH_LOOKUP));

        if (PL_DHASH_ENTRY_IS_FREE(cpolicy))
            cpolicy = NO_POLICY_FOR_CLASS;

        if ((dpolicy == mDefaultPolicy) && aCachedClassPolicy)
            *aCachedClassPolicy = cpolicy;
    }

    NS_ASSERTION(JSID_IS_INT(aProperty) || JSID_IS_OBJECT(aProperty) ||
                 JSID_IS_STRING(aProperty), "Property must be a valid id");

    // Only atomized strings are stored in the policies' hash tables.
    if (!JSID_IS_STRING(aProperty))
        return NS_OK;

    JSString *propertyKey = JSID_TO_STRING(aProperty);

    // We look for a PropertyPolicy in the following places:
    // 1)  The ClassPolicy for our class we got from our DomainPolicy
    // 2)  The mWildcardPolicy of our DomainPolicy
    // 3)  The ClassPolicy for our class we got from mDefaultPolicy
    // 4)  The mWildcardPolicy of our mDefaultPolicy
    PropertyPolicy* ppolicy = nullptr;
    if (cpolicy != NO_POLICY_FOR_CLASS)
    {
        ppolicy = static_cast<PropertyPolicy*>
                             (PL_DHashTableOperate(cpolicy->mPolicy,
                                                      propertyKey,
                                                      PL_DHASH_LOOKUP));
    }

    // If there is no class policy for this property, and we have a wildcard
    // policy, try that.
    if (dpolicy->mWildcardPolicy &&
        (!ppolicy || PL_DHASH_ENTRY_IS_FREE(ppolicy)))
    {
        ppolicy =
            static_cast<PropertyPolicy*>
                       (PL_DHashTableOperate(dpolicy->mWildcardPolicy->mPolicy,
                                                propertyKey,
                                                PL_DHASH_LOOKUP));
    }

    // If dpolicy is not the defauly policy and there's no class or wildcard
    // policy for this property, check the default policy for this class and
    // the default wildcard policy
    if (dpolicy != mDefaultPolicy &&
        (!ppolicy || PL_DHASH_ENTRY_IS_FREE(ppolicy)))
    {
        cpolicy = static_cast<ClassPolicy*>
                             (PL_DHashTableOperate(mDefaultPolicy,
                                                      aClassData.GetName(),
                                                      PL_DHASH_LOOKUP));

        if (PL_DHASH_ENTRY_IS_BUSY(cpolicy))
        {
            ppolicy =
                static_cast<PropertyPolicy*>
                           (PL_DHashTableOperate(cpolicy->mPolicy,
                                                    propertyKey,
                                                    PL_DHASH_LOOKUP));
        }

        if ((!ppolicy || PL_DHASH_ENTRY_IS_FREE(ppolicy)) &&
            mDefaultPolicy->mWildcardPolicy)
        {
            ppolicy =
              static_cast<PropertyPolicy*>
                         (PL_DHashTableOperate(mDefaultPolicy->mWildcardPolicy->mPolicy,
                                                  propertyKey,
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
    // UniversalXPConnect capability trump the above check.
    bool isFile = false;
    bool isRes = false;
    if (NS_FAILED(aURI->SchemeIs("file", &isFile)) ||
        NS_FAILED(aURI->SchemeIs("resource", &isRes)))
        return NS_ERROR_FAILURE;
    if (isFile || isRes)
    {
        if (SubjectIsPrivileged())
            return NS_OK;
    }

    // Report error.
    nsAutoCString spec;
    if (NS_FAILED(aURI->GetAsciiSpec(spec)))
        return NS_ERROR_FAILURE;
    nsAutoCString msg("Access to '");
    msg.Append(spec);
    msg.AppendLiteral("' from script denied");
    SetPendingException(cx, msg.get());
    return NS_ERROR_DOM_BAD_URI;
}

/**
 * Helper method to handle cases where a flag passed to
 * CheckLoadURIWithPrincipal means denying loading if the given URI has certain
 * nsIProtocolHandler flags set.
 * @return if success, access is allowed. Otherwise, deny access
 */
static nsresult
DenyAccessIfURIHasFlags(nsIURI* aURI, uint32_t aURIFlags)
{
    NS_PRECONDITION(aURI, "Must have URI!");
    
    bool uriHasFlags;
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
                                                   uint32_t aFlags)
{
    NS_PRECONDITION(aPrincipal, "CheckLoadURIWithPrincipal must have a principal");
    // If someone passes a flag that we don't understand, we should
    // fail, because they may need a security check that we don't
    // provide.
    NS_ENSURE_FALSE(aFlags & ~(nsIScriptSecurityManager::LOAD_IS_AUTOMATIC_DOCUMENT_REPLACEMENT |
                               nsIScriptSecurityManager::ALLOW_CHROME |
                               nsIScriptSecurityManager::DISALLOW_SCRIPT |
                               nsIScriptSecurityManager::DISALLOW_INHERIT_PRINCIPAL |
                               nsIScriptSecurityManager::DONT_REPORT_ERRORS),
                    NS_ERROR_UNEXPECTED);
    NS_ENSURE_ARG_POINTER(aPrincipal);
    NS_ENSURE_ARG_POINTER(aTargetURI);

    // If DISALLOW_INHERIT_PRINCIPAL is set, we prevent loading of URIs which
    // would do such inheriting. That would be URIs that do not have their own
    // security context. We do this even for the system principal.
    if (aFlags & nsIScriptSecurityManager::DISALLOW_INHERIT_PRINCIPAL) {
        nsresult rv =
            DenyAccessIfURIHasFlags(aTargetURI,
                                    nsIProtocolHandler::URI_INHERITS_SECURITY_CONTEXT);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    if (aPrincipal == mSystemPrincipal) {
        // Allow access
        return NS_OK;
    }

    nsCOMPtr<nsIURI> sourceURI;
    aPrincipal->GetURI(getter_AddRefs(sourceURI));
    if (!sourceURI) {
        nsCOMPtr<nsIExpandedPrincipal> expanded = do_QueryInterface(aPrincipal);
        if (expanded) {
            nsTArray< nsCOMPtr<nsIPrincipal> > *whiteList;
            expanded->GetWhiteList(&whiteList);
            for (uint32_t i = 0; i < whiteList->Length(); ++i) {
                nsresult rv = CheckLoadURIWithPrincipal((*whiteList)[i],
                                                        aTargetURI,
                                                        aFlags);
                if (NS_SUCCEEDED(rv)) {
                    // Allow access if it succeeded with one of the white listed principals
                    return NS_OK;
                }
            }
            return NS_OK;
        }
        NS_ERROR("Non-system principals or expanded principal passed to CheckLoadURIWithPrincipal "
                 "must have a URI!");
        return NS_ERROR_UNEXPECTED;
    }
    
    // Automatic loads are not allowed from certain protocols.
    if (aFlags & nsIScriptSecurityManager::LOAD_IS_AUTOMATIC_DOCUMENT_REPLACEMENT) {
        nsresult rv =
            DenyAccessIfURIHasFlags(sourceURI,
                                    nsIProtocolHandler::URI_FORBIDS_AUTOMATIC_DOCUMENT_REPLACEMENT);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    // If either URI is a nested URI, get the base URI
    nsCOMPtr<nsIURI> sourceBaseURI = NS_GetInnermostURI(sourceURI);
    nsCOMPtr<nsIURI> targetBaseURI = NS_GetInnermostURI(aTargetURI);

    //-- get the target scheme
    nsAutoCString targetScheme;
    nsresult rv = targetBaseURI->GetScheme(targetScheme);
    if (NS_FAILED(rv)) return rv;

    //-- Some callers do not allow loading javascript:
    if ((aFlags & nsIScriptSecurityManager::DISALLOW_SCRIPT) &&
         targetScheme.EqualsLiteral("javascript"))
    {
       return NS_ERROR_DOM_BAD_URI;
    }

    NS_NAMED_LITERAL_STRING(errorTag, "CheckLoadURIError");
    bool reportErrors = !(aFlags & nsIScriptSecurityManager::DONT_REPORT_ERRORS);

    // Check for uris that are only loadable by principals that subsume them
    bool hasFlags;
    rv = NS_URIChainHasFlags(targetBaseURI,
                             nsIProtocolHandler::URI_LOADABLE_BY_SUBSUMERS,
                             &hasFlags);
    NS_ENSURE_SUCCESS(rv, rv);

    if (hasFlags) {
        return aPrincipal->CheckMayLoad(targetBaseURI, true, false);
    }

    //-- get the source scheme
    nsAutoCString sourceScheme;
    rv = sourceBaseURI->GetScheme(sourceScheme);
    if (NS_FAILED(rv)) return rv;

    if (sourceScheme.LowerCaseEqualsLiteral(NS_NULLPRINCIPAL_SCHEME)) {
        // A null principal can target its own URI.
        if (sourceURI == aTargetURI) {
            return NS_OK;
        }
    }
    else if (targetScheme.Equals(sourceScheme,
                                 nsCaseInsensitiveCStringComparator()))
    {
        // every scheme can access another URI from the same scheme,
        // as long as they don't represent null principals...
        // Or they don't require an special permission to do so
        // See bug#773886

        bool hasFlags;
        rv = NS_URIChainHasFlags(targetBaseURI,
                                 nsIProtocolHandler::URI_CROSS_ORIGIN_NEEDS_WEBAPPS_PERM,
                                 &hasFlags);
        NS_ENSURE_SUCCESS(rv, rv);

        if (hasFlags) {
            // In this case, we allow opening only if the source and target URIS
            // are on the same domain, or the opening URI has the webapps
            // permision granted
            if (!SecurityCompareURIs(sourceBaseURI,targetBaseURI) &&
                !nsContentUtils::IsExactSitePermAllow(aPrincipal,WEBAPPS_PERM_NAME)){
                return NS_ERROR_DOM_BAD_URI;
            }
        }
        return NS_OK;
    }

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
        if (reportErrors) {
            ReportError(nullptr, errorTag, sourceURI, aTargetURI);
        }
        return rv;
    }

    // Check for chrome target URI
    rv = NS_URIChainHasFlags(targetBaseURI,
                             nsIProtocolHandler::URI_IS_UI_RESOURCE,
                             &hasFlags);
    NS_ENSURE_SUCCESS(rv, rv);
    if (hasFlags) {
        if (aFlags & nsIScriptSecurityManager::ALLOW_CHROME) {
            if (!targetScheme.EqualsLiteral("chrome")) {
                // for now don't change behavior for resource: or moz-icon:
                return NS_OK;
            }

            // allow load only if chrome package is whitelisted
            nsCOMPtr<nsIXULChromeRegistry> reg(do_GetService(
                                                 NS_CHROMEREGISTRY_CONTRACTID));
            if (reg) {
                bool accessAllowed = false;
                reg->AllowContentToAccess(targetBaseURI, &accessAllowed);
                if (accessAllowed) {
                    return NS_OK;
                }
            }
        }

        // resource: and chrome: are equivalent, securitywise
        // That's bogus!!  Fix this.  But watch out for
        // the view-source stylesheet?
        bool sourceIsChrome;
        rv = NS_URIChainHasFlags(sourceBaseURI,
                                 nsIProtocolHandler::URI_IS_UI_RESOURCE,
                                 &sourceIsChrome);
        NS_ENSURE_SUCCESS(rv, rv);
        if (sourceIsChrome) {
            return NS_OK;
        }
        if (reportErrors) {
            ReportError(nullptr, errorTag, sourceURI, aTargetURI);
        }
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
        bool sourceIsChrome;
        rv = NS_URIChainHasFlags(sourceURI,
                                 nsIProtocolHandler::URI_IS_UI_RESOURCE,
                                 &sourceIsChrome);
        NS_ENSURE_SUCCESS(rv, rv);
        if (sourceIsChrome) {
            return NS_OK;
        }

        // Now check capability policies
        static const char loadURIPrefGroup[] = "checkloaduri";
        ClassInfoData nameData(nullptr, loadURIPrefGroup);

        SecurityLevel secLevel;
        rv = LookupPolicy(aPrincipal, nameData, sEnabledID,
                          nsIXPCSecurityManager::ACCESS_GET_PROPERTY, 
                          nullptr, &secLevel);
        if (NS_SUCCEEDED(rv) && secLevel.level == SCRIPT_SECURITY_ALL_ACCESS)
        {
            // OK for this site!
            return NS_OK;
        }

        if (reportErrors) {
            ReportError(nullptr, errorTag, sourceURI, aTargetURI);
        }
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
                                 ArrayLength(formatStrings),
                                 getter_Copies(message));
        if (NS_SUCCEEDED(rv)) {
            nsCOMPtr<nsIConsoleService> console(
              do_GetService("@mozilla.org/consoleservice;1"));
            NS_ENSURE_TRUE(console, NS_ERROR_FAILURE);

            console->LogStringMessage(message.get());
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
    nsAutoCString sourceSpec;
    rv = aSource->GetAsciiSpec(sourceSpec);
    NS_ENSURE_SUCCESS(rv, rv);

    // Get the target URL spec
    nsAutoCString targetSpec;
    rv = aTarget->GetAsciiSpec(targetSpec);
    NS_ENSURE_SUCCESS(rv, rv);

    // Localize the error message
    nsXPIDLString message;
    NS_ConvertASCIItoUTF16 ucsSourceSpec(sourceSpec);
    NS_ConvertASCIItoUTF16 ucsTargetSpec(targetSpec);
    const PRUnichar *formatStrings[] = { ucsSourceSpec.get(), ucsTargetSpec.get() };
    rv = sStrBundle->FormatStringFromName(PromiseFlatString(messageTag).get(),
                                          formatStrings,
                                          ArrayLength(formatStrings),
                                          getter_Copies(message));
    NS_ENSURE_SUCCESS(rv, rv);

    // If a JS context was passed in, set a JS exception.
    // Otherwise, print the error message directly to the JS console
    // and to standard output
    if (cx)
    {
        SetPendingException(cx, message.get());
    }
    else // Print directly to the console
    {
        nsCOMPtr<nsIConsoleService> console(
            do_GetService("@mozilla.org/consoleservice;1"));
        NS_ENSURE_TRUE(console, NS_ERROR_FAILURE);

        console->LogStringMessage(message.get());
    }
    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::CheckLoadURIStrWithPrincipal(nsIPrincipal* aPrincipal,
                                                      const nsACString& aTargetURIStr,
                                                      uint32_t aFlags)
{
    nsresult rv;
    nsCOMPtr<nsIURI> target;
    rv = NS_NewURI(getter_AddRefs(target), aTargetURIStr,
                   nullptr, nullptr, sIOService);
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

    uint32_t flags[] = {
        nsIURIFixup::FIXUP_FLAG_NONE,
        nsIURIFixup::FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP,
        nsIURIFixup::FIXUP_FLAGS_MAKE_ALTERNATE_URI,
        nsIURIFixup::FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP |
        nsIURIFixup::FIXUP_FLAGS_MAKE_ALTERNATE_URI
    };

    for (uint32_t i = 0; i < ArrayLength(flags); ++i) {
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
        GetFunctionObjectPrincipal(aCx, (JSObject *)aFunObj, &rv);

    // If subject is null, get a principal from the function object's scope.
    if (NS_SUCCEEDED(rv) && !subject)
    {
#ifdef DEBUG
        {
            JS_ASSERT(JS_ObjectIsFunction(aCx, (JSObject *)aFunObj));
            JSFunction *fun = JS_GetObjectFunction((JSObject *)aFunObj);
            JSScript *script = JS_GetFunctionScript(aCx, fun);

            NS_ASSERTION(!script, "Null principal for non-native function!");
        }
#endif

        subject = doGetObjectPrincipal((JSObject*)aFunObj);
    }

    if (!subject)
        return NS_ERROR_FAILURE;

    if (subject == mSystemPrincipal)
        // This is the system principal: just allow access
        return NS_OK;

    // Check if the principal the function was compiled under is
    // allowed to execute scripts.

    bool result;
    rv = CanExecuteScripts(aCx, subject, true, &result);
    if (NS_FAILED(rv))
      return rv;

    if (!result)
      return NS_ERROR_DOM_SECURITY_ERR;

    if (!aTargetObj) {
        // We're done here
        return NS_OK;
    }

    /*
    ** Get origin of subject and object and compare.
    */
    JSObject* obj = (JSObject*)aTargetObj;
    nsIPrincipal* object = doGetObjectPrincipal(obj);

    if (!object)
        return NS_ERROR_FAILURE;        

    bool subsumes;
    rv = subject->Subsumes(object, &subsumes);
    if (NS_SUCCEEDED(rv) && !subsumes) {
        rv = NS_ERROR_DOM_PROP_ACCESS_DENIED;
    }
    return rv;
}

NS_IMETHODIMP
nsScriptSecurityManager::CanExecuteScripts(JSContext* cx,
                                           nsIPrincipal *aPrincipal,
                                           bool *result)
{
    return CanExecuteScripts(cx, aPrincipal, false, result);
}

nsresult
nsScriptSecurityManager::CanExecuteScripts(JSContext* cx,
                                           nsIPrincipal *aPrincipal,
                                           bool aAllowIfNoScriptContext,
                                           bool *result)
{
    *result = false; 

    if (aPrincipal == mSystemPrincipal)
    {
        // Even if JavaScript is disabled, we must still execute system scripts
        *result = true;
        return NS_OK;
    }

    // Same thing for nsExpandedPrincipal, which is pseudo-privileged.
    nsCOMPtr<nsIExpandedPrincipal> ep = do_QueryInterface(aPrincipal);
    if (ep)
    {
        *result = true;
        return NS_OK;
    }

    //-- See if the current window allows JS execution
    nsIScriptContext *scriptContext = GetScriptContext(cx);
    if (!scriptContext) {
        if (aAllowIfNoScriptContext) {
            *result = true;
            return NS_OK;
        }
        return NS_ERROR_FAILURE;
    }

    if (!scriptContext->GetScriptsEnabled()) {
        // No scripting on this context, folks
        *result = false;
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

    if (docshell) {
      rv = docshell->GetCanExecuteScripts(result);
      if (NS_FAILED(rv)) return rv;
      if (!*result) return NS_OK;
    }

    // OK, the docshell doesn't have script execution explicitly disabled.
    // Check whether our URI is an "about:" URI that allows scripts.  If it is,
    // we need to allow JS to run.  In this case, don't apply the JS enabled
    // pref or policies.  On failures, just press on and don't do this special
    // case.
    nsCOMPtr<nsIURI> principalURI;
    aPrincipal->GetURI(getter_AddRefs(principalURI));
    if (!principalURI) {
        // Broken principal of some sort.  Disallow.
        *result = false;
        return NS_ERROR_UNEXPECTED;
    }
        
    bool isAbout;
    rv = principalURI->SchemeIs("about", &isAbout);
    if (NS_SUCCEEDED(rv) && isAbout) {
        nsCOMPtr<nsIAboutModule> module;
        rv = NS_GetAboutModule(principalURI, getter_AddRefs(module));
        if (NS_SUCCEEDED(rv)) {
            uint32_t flags;
            rv = module->GetURIFlags(principalURI, &flags);
            if (NS_SUCCEEDED(rv) &&
                (flags & nsIAboutModule::ALLOW_SCRIPT)) {
                *result = true;
                return NS_OK;              
            }
        }
    }

    *result = mIsJavaScriptEnabled;
    if (!*result)
        return NS_OK; // Do not run scripts

    //-- Check for a per-site policy
    static const char jsPrefGroupName[] = "javascript";
    ClassInfoData nameData(nullptr, jsPrefGroupName);

    SecurityLevel secLevel;
    rv = LookupPolicy(aPrincipal, nameData, sEnabledID,
                      nsIXPCSecurityManager::ACCESS_GET_PROPERTY, 
                      nullptr, &secLevel);
    if (NS_FAILED(rv) || secLevel.level == SCRIPT_SECURITY_NO_ACCESS)
    {
        *result = false;
        return rv;
    }

    //-- Nobody vetoed, so allow the JS to run.
    *result = true;
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
        return nullptr;
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
nsScriptSecurityManager::SubjectPrincipalIsSystem(bool* aIsSystem)
{
    NS_ENSURE_ARG_POINTER(aIsSystem);
    *aIsSystem = false;

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
        *aIsSystem = true;
        return NS_OK;
    }

    return mSystemPrincipal->Equals(subject, aIsSystem);
}

nsresult
nsScriptSecurityManager::CreateCodebasePrincipal(nsIURI* aURI, uint32_t aAppId,
                                                 bool aInMozBrowser,
                                                 nsIPrincipal **result)
{
    // I _think_ it's safe to not create null principals here based on aURI.
    // At least all the callers would do the right thing in those cases, as far
    // as I can tell.  --bz

    nsCOMPtr<nsIURIWithPrincipal> uriPrinc = do_QueryInterface(aURI);
    if (uriPrinc) {
        nsCOMPtr<nsIPrincipal> principal;
        uriPrinc->GetPrincipal(getter_AddRefs(principal));
        if (!principal || principal == mSystemPrincipal) {
            return CallCreateInstance(NS_NULLPRINCIPAL_CONTRACTID, result);
        }

        principal.forget(result);

        return NS_OK;
    }

    nsRefPtr<nsPrincipal> codebase = new nsPrincipal();
    if (!codebase)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = codebase->Init(aURI, aAppId, aInMozBrowser);
    if (NS_FAILED(rv))
        return rv;

    NS_ADDREF(*result = codebase);

    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::GetSimpleCodebasePrincipal(nsIURI* aURI,
                                                    nsIPrincipal** aPrincipal)
{
  return GetCodebasePrincipalInternal(aURI,
                                      nsIScriptSecurityManager::UNKNOWN_APP_ID,
                                      false, aPrincipal);
}

NS_IMETHODIMP
nsScriptSecurityManager::GetNoAppCodebasePrincipal(nsIURI* aURI,
                                                   nsIPrincipal** aPrincipal)
{
  return GetCodebasePrincipalInternal(aURI,  nsIScriptSecurityManager::NO_APP_ID,
                                      false, aPrincipal);
}

NS_IMETHODIMP
nsScriptSecurityManager::GetCodebasePrincipal(nsIURI* aURI,
                                              nsIPrincipal** aPrincipal)
{
  return GetNoAppCodebasePrincipal(aURI, aPrincipal);
}

NS_IMETHODIMP
nsScriptSecurityManager::GetAppCodebasePrincipal(nsIURI* aURI,
                                                 uint32_t aAppId,
                                                 bool aInMozBrowser,
                                                 nsIPrincipal** aPrincipal)
{
  NS_ENSURE_TRUE(aAppId != nsIScriptSecurityManager::UNKNOWN_APP_ID,
                 NS_ERROR_INVALID_ARG);

  return GetCodebasePrincipalInternal(aURI, aAppId, aInMozBrowser, aPrincipal);
}

NS_IMETHODIMP
nsScriptSecurityManager::GetDocShellCodebasePrincipal(nsIURI* aURI,
                                                      nsIDocShell* aDocShell,
                                                      nsIPrincipal** aPrincipal)
{
  return GetCodebasePrincipalInternal(aURI,
                                      aDocShell->GetAppId(),
                                      aDocShell->GetIsInBrowserElement(),
                                      aPrincipal);
}

nsresult
nsScriptSecurityManager::GetCodebasePrincipalInternal(nsIURI *aURI,
                                                      uint32_t aAppId,
                                                      bool aInMozBrowser,
                                                      nsIPrincipal **result)
{
    NS_ENSURE_ARG(aURI);

    bool inheritsPrincipal;
    nsresult rv =
        NS_URIChainHasFlags(aURI,
                            nsIProtocolHandler::URI_INHERITS_SECURITY_CONTEXT,
                            &inheritsPrincipal);
    if (NS_FAILED(rv) || inheritsPrincipal) {
        return CallCreateInstance(NS_NULLPRINCIPAL_CONTRACTID, result);
    }

    nsCOMPtr<nsIPrincipal> principal;
    rv = CreateCodebasePrincipal(aURI, aAppId, aInMozBrowser,
                                 getter_AddRefs(principal));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_IF_ADDREF(*result = principal);

    return NS_OK;
}

// static
nsIPrincipal*
nsScriptSecurityManager::GetScriptPrincipal(JSScript *script,
                                            nsresult* rv)
{
    NS_PRECONDITION(rv, "Null out param");
    *rv = NS_OK;
    if (!script)
    {
        return nullptr;
    }
    JSPrincipals *jsp = JS_GetScriptPrincipals(script);
    if (!jsp) {
        *rv = NS_ERROR_FAILURE;
        NS_ERROR("Script compiled without principals!");
        return nullptr;
    }
    return nsJSPrincipals::get(jsp);
}

// static
nsIPrincipal*
nsScriptSecurityManager::GetFunctionObjectPrincipal(JSContext *cx,
                                                    JSObject *obj,
                                                    nsresult *rv)
{
    NS_PRECONDITION(rv, "Null out param");

    *rv = NS_OK;

    if (!JS_ObjectIsFunction(cx, obj))
    {
        // Protect against pseudo-functions (like SJOWs).
        nsIPrincipal *result = doGetObjectPrincipal(obj);
        if (!result)
            *rv = NS_ERROR_FAILURE;
        return result;
    }

    JSFunction *fun = JS_GetObjectFunction(obj);
    JSScript *script = JS_GetFunctionScript(cx, fun);

    if (!script)
    {
        // A native function: skip it in order to find its scripted caller.
        return nullptr;
    }

    if (!js::IsOriginalScriptFunction(fun))
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

        nsIPrincipal *result = doGetObjectPrincipal(obj);
        if (!result)
            *rv = NS_ERROR_FAILURE;
        return result;
    }

    return GetScriptPrincipal(script, rv);
}

nsIPrincipal*
nsScriptSecurityManager::GetSubjectPrincipal(JSContext *cx,
                                             nsresult* rv)
{
    *rv = NS_OK;
    JSCompartment *compartment = js::GetContextCompartment(cx);

    // The context should always be in a compartment, either one it has entered
    // or the one associated with its global.
    MOZ_ASSERT(!!compartment);

    JSPrincipals *principals = JS_GetCompartmentPrincipals(compartment);
    return nsJSPrincipals::get(principals);
}

NS_IMETHODIMP
nsScriptSecurityManager::GetObjectPrincipal(JSContext *aCx, JSObject *aObj,
                                            nsIPrincipal **result)
{
    *result = doGetObjectPrincipal(aObj);
    if (!*result)
        return NS_ERROR_FAILURE;
    NS_ADDREF(*result);
    return NS_OK;
}

// static
nsIPrincipal*
nsScriptSecurityManager::doGetObjectPrincipal(JSObject *aObj)
{
    JSCompartment *compartment = js::GetObjectCompartment(aObj);
    JSPrincipals *principals = JS_GetCompartmentPrincipals(compartment);
    nsIPrincipal *principal = nsJSPrincipals::get(principals);

    // We leave the old code in for a little while to make sure that pulling
    // object principals directly off the compartment always gives an equivalent
    // result (from a security perspective).
#ifdef DEBUG
    nsIPrincipal *old = old_doGetObjectPrincipal(aObj);
    MOZ_ASSERT(NS_SUCCEEDED(CheckSameOriginPrincipal(principal, old)));
#endif

    return principal;
}

#ifdef DEBUG
// static
nsIPrincipal*
nsScriptSecurityManager::old_doGetObjectPrincipal(JSObject *aObj,
                                                  bool aAllowShortCircuit)
{
    NS_ASSERTION(aObj, "Bad call to doGetObjectPrincipal()!");
    nsIPrincipal* result = nullptr;

    JSObject* origObj = aObj;
    js::Class *jsClass = js::GetObjectClass(aObj);

    // A common case seen in this code is that we enter this function
    // with aObj being a Function object, whose parent is a Call
    // object. Neither of those have object principals, so we can skip
    // those objects here before we enter the below loop. That way we
    // avoid wasting time checking properties of their classes etc in
    // the loop.

    if (jsClass == &js::FunctionClass) {
        aObj = js::GetObjectParent(aObj);

        if (!aObj)
            return nullptr;

        jsClass = js::GetObjectClass(aObj);

        if (jsClass == &js::CallClass) {
            aObj = js::GetObjectParentMaybeScope(aObj);

            if (!aObj)
                return nullptr;

            jsClass = js::GetObjectClass(aObj);
        }
    }

    do {
        // Note: jsClass is set before this loop, and also at the
        // *end* of this loop.
        
        if (IS_WRAPPER_CLASS(jsClass)) {
            result = sXPConnect->GetPrincipal(aObj,
                                              aAllowShortCircuit);
            if (result) {
                break;
            }
        } else {
            nsISupports *priv;
            if (!(~jsClass->flags & (JSCLASS_HAS_PRIVATE |
                                     JSCLASS_PRIVATE_IS_NSISUPPORTS))) {
                priv = (nsISupports *) js::GetObjectPrivate(aObj);
            } else if (!UnwrapDOMObjectToISupports(aObj, priv)) {
                priv = nullptr;
            }

            if (aAllowShortCircuit) {
                nsCOMPtr<nsIXPConnectWrappedNative> xpcWrapper =
                    do_QueryInterface(priv);

                NS_ASSERTION(!xpcWrapper ||
                             !strcmp(jsClass->name, "XPCNativeWrapper"),
                             "Uh, an nsIXPConnectWrappedNative with the "
                             "wrong JSClass or getObjectOps hooks!");
            }

            nsCOMPtr<nsIScriptObjectPrincipal> objPrin =
                do_QueryInterface(priv);

            if (objPrin) {
                result = objPrin->GetPrincipal();

                if (result) {
                    break;
                }
            }
        }

        aObj = js::GetObjectParentMaybeScope(aObj);

        if (!aObj)
            break;

        jsClass = js::GetObjectClass(aObj);
    } while (1);

    if (aAllowShortCircuit) {
        nsIPrincipal *principal = old_doGetObjectPrincipal(origObj, false);

        // Because of inner window reuse, we can have objects with one principal
        // living in a scope with a different (but same-origin) principal. So
        // just check same-origin here.
        NS_ASSERTION(NS_SUCCEEDED(CheckSameOriginPrincipal(result, principal)),
                     "Principal mismatch.  Not good");
    }

    return result;
}
#endif /* DEBUG */

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
// XXX Special case for nsIXPCException ?
    ClassInfoData objClassInfo = ClassInfoData(aClassInfo, nullptr);
    if (objClassInfo.IsDOMClass())
    {
        return NS_OK;
    }

    //--See if the object advertises a non-default level of access
    //  using nsISecurityCheckedComponent
    nsCOMPtr<nsISecurityCheckedComponent> checkedComponent =
        do_QueryInterface(aObj);

    nsXPIDLCString objectSecurityLevel;
    if (checkedComponent)
        checkedComponent->CanCreateWrapper((nsIID *)&aIID, getter_Copies(objectSecurityLevel));

    nsresult rv = CheckXPCPermissions(cx, aObj, nullptr, nullptr, objectSecurityLevel);
    if (NS_FAILED(rv))
    {
        //-- Access denied, report an error
        NS_ConvertUTF8toUTF16 strName("CreateWrapperDenied");
        nsAutoCString origin;
        nsresult rv2;
        nsIPrincipal* subjectPrincipal = doGetSubjectPrincipal(&rv2);
        if (NS_SUCCEEDED(rv2) && subjectPrincipal) {
            GetPrincipalDomainOrigin(subjectPrincipal, origin);
        }
        NS_ConvertUTF8toUTF16 originUnicode(origin);
        NS_ConvertUTF8toUTF16 className(objClassInfo.GetName());
        const PRUnichar* formatStrings[] = {
            className.get(),
            originUnicode.get()
        };
        uint32_t length = ArrayLength(formatStrings);
        if (originUnicode.IsEmpty()) {
            --length;
        } else {
            strName.AppendLiteral("ForOrigin");
        }
        nsXPIDLString errorMsg;
        // We need to keep our existing failure rv and not override it
        // with a likely success code from the following string bundle
        // call in order to throw the correct security exception later.
        rv2 = sStrBundle->FormatStringFromName(strName.get(),
                                               formatStrings,
                                               length,
                                               getter_Copies(errorMsg));
        NS_ENSURE_SUCCESS(rv2, rv2);

        SetPendingException(cx, errorMsg.get());
    }

    return rv;
}

NS_IMETHODIMP
nsScriptSecurityManager::CanCreateInstance(JSContext *cx,
                                           const nsCID &aCID)
{
    nsresult rv = CheckXPCPermissions(nullptr, nullptr, nullptr, nullptr, nullptr);
    if (NS_FAILED(rv))
    {
        //-- Access denied, report an error
        nsAutoCString errorMsg("Permission denied to create instance of class. CID=");
        char cidStr[NSID_LENGTH];
        aCID.ToProvidedString(cidStr);
        errorMsg.Append(cidStr);
        SetPendingException(cx, errorMsg.get());
    }
    return rv;
}

NS_IMETHODIMP
nsScriptSecurityManager::CanGetService(JSContext *cx,
                                       const nsCID &aCID)
{
    nsresult rv = CheckXPCPermissions(nullptr, nullptr, nullptr, nullptr, nullptr);
    if (NS_FAILED(rv))
    {
        //-- Access denied, report an error
        nsAutoCString errorMsg("Permission denied to get service. CID=");
        char cidStr[NSID_LENGTH];
        aCID.ToProvidedString(cidStr);
        errorMsg.Append(cidStr);
        SetPendingException(cx, errorMsg.get());
    }

    return rv;
}


NS_IMETHODIMP
nsScriptSecurityManager::CanAccess(uint32_t aAction,
                                   nsAXPCNativeCallContext* aCallContext,
                                   JSContext* cx,
                                   JSObject* aJSObject,
                                   nsISupports* aObj,
                                   nsIClassInfo* aClassInfo,
                                   jsid aPropertyName,
                                   void** aPolicy)
{
    return CheckPropertyAccessImpl(aAction, aCallContext, cx,
                                   aJSObject, aObj, aClassInfo,
                                   nullptr, aPropertyName, aPolicy);
}

nsresult
nsScriptSecurityManager::CheckXPCPermissions(JSContext* cx,
                                             nsISupports* aObj, JSObject* aJSObject,
                                             nsIPrincipal* aSubjectPrincipal,
                                             const char* aObjectSecurityLevel)
{
    // Check if the subject is privileged.
    if (SubjectIsPrivileged())
        return NS_OK;

    //-- If the object implements nsISecurityCheckedComponent, it has a non-default policy.
    if (aObjectSecurityLevel)
    {
        if (PL_strcasecmp(aObjectSecurityLevel, "allAccess") == 0)
            return NS_OK;
        if (cx && PL_strcasecmp(aObjectSecurityLevel, "sameOrigin") == 0)
        {
            nsresult rv;
            if (!aJSObject)
            {
                nsCOMPtr<nsIXPConnectWrappedJS> xpcwrappedjs =
                    do_QueryInterface(aObj);
                if (xpcwrappedjs)
                {
                    rv = xpcwrappedjs->GetJSObject(&aJSObject);
                    NS_ENSURE_SUCCESS(rv, rv);
                }
            }

            if (!aSubjectPrincipal)
            {
                // No subject principal passed in. Compute it.
                aSubjectPrincipal = GetSubjectPrincipal(cx, &rv);
                NS_ENSURE_SUCCESS(rv, rv);
            }
            if (aSubjectPrincipal && aJSObject)
            {
                nsIPrincipal* objectPrincipal = doGetObjectPrincipal(aJSObject);

                // Only do anything if we have both a subject and object
                // principal.
                if (objectPrincipal)
                {
                    bool subsumes;
                    rv = aSubjectPrincipal->Subsumes(objectPrincipal, &subsumes);
                    NS_ENSURE_SUCCESS(rv, rv);
                    if (subsumes)
                        return NS_OK;
                }
            }
        }
        else if (PL_strcasecmp(aObjectSecurityLevel, "noAccess") != 0)
        {
            if (SubjectIsPrivileged())
                return NS_OK;
        }
    }

    //-- Access tests failed
    return NS_ERROR_DOM_XPCONNECT_ACCESS_DENIED;
}

/////////////////////////////////////////////
// Method implementing nsIChannelEventSink //
/////////////////////////////////////////////
NS_IMETHODIMP
nsScriptSecurityManager::AsyncOnChannelRedirect(nsIChannel* oldChannel, 
                                                nsIChannel* newChannel,
                                                uint32_t redirFlags,
                                                nsIAsyncVerifyRedirectCallback *cb)
{
    nsCOMPtr<nsIPrincipal> oldPrincipal;
    GetChannelPrincipal(oldChannel, getter_AddRefs(oldPrincipal));

    nsCOMPtr<nsIURI> newURI;
    newChannel->GetURI(getter_AddRefs(newURI));
    nsCOMPtr<nsIURI> newOriginalURI;
    newChannel->GetOriginalURI(getter_AddRefs(newOriginalURI));

    NS_ENSURE_STATE(oldPrincipal && newURI && newOriginalURI);

    const uint32_t flags =
        nsIScriptSecurityManager::LOAD_IS_AUTOMATIC_DOCUMENT_REPLACEMENT |
        nsIScriptSecurityManager::DISALLOW_SCRIPT;
    nsresult rv = CheckLoadURIWithPrincipal(oldPrincipal, newURI, flags);
    if (NS_SUCCEEDED(rv) && newOriginalURI != newURI) {
        rv = CheckLoadURIWithPrincipal(oldPrincipal, newOriginalURI, flags);
    }

    if (NS_FAILED(rv))
        return rv;

    cb->OnRedirectVerifyCallback(NS_OK);
    return NS_OK;
}


/////////////////////////////////////
// Method implementing nsIObserver //
/////////////////////////////////////
const char sJSEnabledPrefName[] = "javascript.enabled";
const char sFileOriginPolicyPrefName[] =
    "security.fileuri.strict_origin_policy";
static const char sPolicyPrefix[] = "capability.policy.";

static const char* kObservedPrefs[] = {
  sJSEnabledPrefName,
  sFileOriginPolicyPrefName,
  sPolicyPrefix,
  nullptr
};


NS_IMETHODIMP
nsScriptSecurityManager::Observe(nsISupports* aObject, const char* aTopic,
                                 const PRUnichar* aMessage)
{
    nsresult rv = NS_OK;
    NS_ConvertUTF16toUTF8 messageStr(aMessage);
    const char *message = messageStr.get();

    static const char jsPrefix[] = "javascript.";
    static const char securityPrefix[] = "security.";
    if ((PL_strncmp(message, jsPrefix, sizeof(jsPrefix)-1) == 0) ||
        (PL_strncmp(message, securityPrefix, sizeof(securityPrefix)-1) == 0) )
    {
        ScriptSecurityPrefChanged();
    }
    else if (PL_strncmp(message, sPolicyPrefix, sizeof(sPolicyPrefix)-1) == 0)
    {
        // This will force re-initialization of the pref table
        mPolicyPrefsChanged = true;
    }
    return rv;
}

/////////////////////////////////////////////
// Constructor, Destructor, Initialization //
/////////////////////////////////////////////
nsScriptSecurityManager::nsScriptSecurityManager(void)
    : mOriginToPolicyMap(nullptr),
      mDefaultPolicy(nullptr),
      mCapabilities(nullptr),
      mPrefInitialized(false),
      mIsJavaScriptEnabled(false),
      mPolicyPrefsChanged(true)
{
    MOZ_STATIC_ASSERT(sizeof(intptr_t) == sizeof(void*),
                      "intptr_t and void* have different lengths on this platform. "
                      "This may cause a security failure with the SecurityLevel union.");
}

nsresult nsScriptSecurityManager::Init()
{
    nsXPConnect* xpconnect = nsXPConnect::GetXPConnect();
     if (!xpconnect)
        return NS_ERROR_FAILURE;

    NS_ADDREF(sXPConnect = xpconnect);
    NS_ADDREF(sJSContextStack = xpconnect);

    JSContext* cx = GetSafeJSContext();
    if (!cx) return NS_ERROR_FAILURE;   // this can happen of xpt loading fails
    
    ::JS_BeginRequest(cx);
    if (sEnabledID == JSID_VOID)
        sEnabledID = INTERNED_STRING_TO_JSID(cx, ::JS_InternString(cx, "enabled"));
    ::JS_EndRequest(cx);

    InitPrefs();

    nsresult rv = CallGetService(NS_IOSERVICE_CONTRACTID, &sIOService);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIStringBundleService> bundleService =
        mozilla::services::GetStringBundleService();
    if (!bundleService)
        return NS_ERROR_FAILURE;

    rv = bundleService->CreateBundle("chrome://global/locale/security/caps.properties", &sStrBundle);
    NS_ENSURE_SUCCESS(rv, rv);

    // Create our system principal singleton
    nsRefPtr<nsSystemPrincipal> system = new nsSystemPrincipal();
    NS_ENSURE_TRUE(system, NS_ERROR_OUT_OF_MEMORY);

    mSystemPrincipal = system;

    //-- Register security check callback in the JS engine
    //   Currently this is used to control access to function.caller
    nsCOMPtr<nsIJSRuntimeService> runtimeService =
        do_QueryInterface(sXPConnect, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = runtimeService->GetRuntime(&sRuntime);
    NS_ENSURE_SUCCESS(rv, rv);

    static const JSSecurityCallbacks securityCallbacks = {
        CheckObjectAccess,
        ContentSecurityPolicyPermitsJSAction
    };

    MOZ_ASSERT(!JS_GetSecurityCallbacks(sRuntime));
    JS_SetSecurityCallbacks(sRuntime, &securityCallbacks);
    JS_InitDestroyPrincipalsCallback(sRuntime, nsJSPrincipals::Destroy);

    JS_SetTrustedPrincipals(sRuntime, system);

    return NS_OK;
}

static StaticRefPtr<nsScriptSecurityManager> gScriptSecMan;

jsid nsScriptSecurityManager::sEnabledID   = JSID_VOID;

nsScriptSecurityManager::~nsScriptSecurityManager(void)
{
    Preferences::RemoveObservers(this, kObservedPrefs);
    delete mOriginToPolicyMap;
    if(mDefaultPolicy)
        mDefaultPolicy->Drop();
    delete mCapabilities;
}

void
nsScriptSecurityManager::Shutdown()
{
    if (sRuntime) {
        JS_SetSecurityCallbacks(sRuntime, NULL);
        JS_SetTrustedPrincipals(sRuntime, NULL);
        sRuntime = nullptr;
    }
    sEnabledID = JSID_VOID;

    NS_IF_RELEASE(sIOService);
    NS_IF_RELEASE(sXPConnect);
    NS_IF_RELEASE(sJSContextStack);
    NS_IF_RELEASE(sStrBundle);
}

nsScriptSecurityManager *
nsScriptSecurityManager::GetScriptSecurityManager()
{
    if (!gScriptSecMan)
    {
        nsRefPtr<nsScriptSecurityManager> ssManager = new nsScriptSecurityManager();

        nsresult rv;
        rv = ssManager->Init();
        NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to initialize nsScriptSecurityManager");
        if (NS_FAILED(rv)) {
            return nullptr;
        }
 
        rv = sXPConnect->SetDefaultSecurityManager(ssManager,
                                                   nsIXPCSecurityManager::HOOK_ALL);
        if (NS_FAILED(rv)) {
            NS_WARNING("Failed to install xpconnect security manager!");
            return nullptr;
        }

        ClearOnShutdown(&gScriptSecMan);
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
    nsIPrincipal *sysprin = nullptr;
    if (gScriptSecMan)
        NS_ADDREF(sysprin = gScriptSecMan->mSystemPrincipal);
    return static_cast<nsSystemPrincipal*>(sysprin);
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
        mDefaultPolicy = nullptr;
    }
    
    //-- Initialize a new mOriginToPolicyMap
    mOriginToPolicyMap =
      new nsObjectHashtable(nullptr, nullptr, DeleteDomainEntry, nullptr);
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
          new nsObjectHashtable(nullptr, nullptr, DeleteCapability, nullptr);
        if (!mCapabilities)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    // Get a JS context - we need it to create internalized strings later.
    JSContext* cx = GetSafeJSContext();
    NS_ASSERTION(cx, "failed to get JS context");
    AutoCxPusher autoPusher(sJSContextStack, cx);
    rv = InitDomainPolicy(cx, "default", mDefaultPolicy);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAdoptingCString policyNames =
        Preferences::GetCString("capability.policy.policynames");

    nsAdoptingCString defaultPolicyNames =
        Preferences::GetCString("capability.policy.default_policynames");
    policyNames += NS_LITERAL_CSTRING(" ") + defaultPolicyNames;

    //-- Initialize domain policies
    char* policyCurrent = policyNames.BeginWriting();
    bool morePolicies = true;
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

        nsAutoCString sitesPrefName(
            NS_LITERAL_CSTRING(sPolicyPrefix) +
            nsDependentCString(nameBegin) +
            NS_LITERAL_CSTRING(".sites"));
        nsAdoptingCString domainList =
            Preferences::GetCString(sitesPrefName.get());
        if (!domainList) {
            continue;
        }

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
        char* lastDot = nullptr;
        char* nextToLastDot = nullptr;
        bool moreDomains = true;
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
                lastDot = nextToLastDot = nullptr;
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
    mPolicyPrefsChanged = false;

    return NS_OK;
}


nsresult
nsScriptSecurityManager::InitDomainPolicy(JSContext* cx,
                                          const char* aPolicyName,
                                          DomainPolicy* aDomainPolicy)
{
    nsresult rv;
    nsAutoCString policyPrefix(NS_LITERAL_CSTRING(sPolicyPrefix) +
                               nsDependentCString(aPolicyName) +
                               NS_LITERAL_CSTRING("."));
    uint32_t prefixLength = policyPrefix.Length() - 1; // subtract the '.'

    uint32_t prefCount;
    char** prefNames;
    nsIPrefBranch* branch = Preferences::GetRootBranch();
    NS_ASSERTION(branch, "failed to get the root pref branch");
    rv = branch->GetChildList(policyPrefix.get(), &prefCount, &prefNames);
    if (NS_FAILED(rv)) return rv;
    if (prefCount == 0)
        return NS_OK;

    //-- Populate the policy
    uint32_t currentPref = 0;
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
        nsAdoptingCString prefValue =
            Preferences::GetCString(prefNames[currentPref]);
        if (!prefValue) {
            continue;
        }

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
                reinterpret_cast<char*>(mCapabilities->Get(&secLevelKey));
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
          static_cast<ClassPolicy*>
                     (PL_DHashTableOperate(aDomainPolicy, start,
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
        PropertyPolicy* ppolicy = 
          static_cast<PropertyPolicy*>
                     (PL_DHashTableOperate(cpolicy->mPolicy, propertyKey,
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

inline void
nsScriptSecurityManager::ScriptSecurityPrefChanged()
{
    // JavaScript defaults to enabled in failure cases.
    mIsJavaScriptEnabled = true;

    sStrictFileOriginPolicy = true;

    nsresult rv;
    if (!mPrefInitialized) {
        rv = InitPrefs();
        if (NS_FAILED(rv))
            return;
    }

    mIsJavaScriptEnabled =
        Preferences::GetBool(sJSEnabledPrefName, mIsJavaScriptEnabled);

    sStrictFileOriginPolicy =
        Preferences::GetBool(sFileOriginPolicyPrefName, false);
}

nsresult
nsScriptSecurityManager::InitPrefs()
{
    nsIPrefBranch* branch = Preferences::GetRootBranch();
    NS_ENSURE_TRUE(branch, NS_ERROR_FAILURE);

    mPrefInitialized = true;

    // Set the initial value of the "javascript.enabled" prefs
    ScriptSecurityPrefChanged();

    // set observer callbacks in case the value of the prefs change
    Preferences::AddStrongObservers(this, kObservedPrefs);

    return NS_OK;
}

namespace mozilla {

void
GetExtendedOrigin(nsIURI* aURI, uint32_t aAppId, bool aInMozBrowser,
                  nsACString& aExtendedOrigin)
{
  MOZ_ASSERT(aURI);
  MOZ_ASSERT(aAppId != nsIScriptSecurityManager::UNKNOWN_APP_ID);

  if (aAppId == nsIScriptSecurityManager::UNKNOWN_APP_ID) {
    aAppId = nsIScriptSecurityManager::NO_APP_ID;
  }

  nsAutoCString origin;
  nsPrincipal::GetOriginForURI(aURI, getter_Copies(origin));

  // Fallback.
  if (aAppId == nsIScriptSecurityManager::NO_APP_ID && !aInMozBrowser) {
    aExtendedOrigin.Assign(origin);
    return;
  }

  // aExtendedOrigin = appId + "+" + { 't', 'f' } "+" + origin;
  aExtendedOrigin.Truncate();
  aExtendedOrigin.AppendInt(aAppId);
  aExtendedOrigin.Append('+');
  aExtendedOrigin.Append(aInMozBrowser ? 't' : 'f');
  aExtendedOrigin.Append('+');
  aExtendedOrigin.Append(origin);

  return;
}

} // namespace mozilla

NS_IMETHODIMP
nsScriptSecurityManager::GetExtendedOrigin(nsIURI* aURI,
                                           uint32_t aAppId,
                                           bool aInMozBrowser,
                                           nsACString& aExtendedOrigin)
{
  MOZ_ASSERT(aAppId != nsIScriptSecurityManager::UNKNOWN_APP_ID);

  mozilla::GetExtendedOrigin(aURI, aAppId, aInMozBrowser, aExtendedOrigin);
  return NS_OK;
}
