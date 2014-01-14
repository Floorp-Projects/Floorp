/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsScriptSecurityManager.h"

#include "mozilla/ArrayUtils.h"

#include "js/OldDebugAPI.h"
#include "xpcprivate.h"
#include "XPCWrapper.h"
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
#include "DomainPolicy.h"
#include "nsXPIDLString.h"
#include "nsCRT.h"
#include "nsCRTGlue.h"
#include "nsError.h"
#include "nsDOMCID.h"
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
#include <stdint.h>
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"
#include "nsContentUtils.h"
#include "nsCxPusher.h"
#include "nsJSUtils.h"

// This should be probably defined on some other place... but I couldn't find it
#define WEBAPPS_PERM_NAME "webapps-manage"

using namespace mozilla;
using namespace mozilla::dom;

static NS_DEFINE_CID(kZipReaderCID, NS_ZIPREADER_CID);

nsIIOService    *nsScriptSecurityManager::sIOService = nullptr;
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
static inline const char16_t *
IDToString(JSContext *cx, jsid id_)
{
    JS::RootedId id(cx, id_);
    if (JSID_IS_STRING(id))
        return JS_GetInternedStringChars(JSID_TO_STRING(id));

    JS::Rooted<JS::Value> idval(cx);
    if (!JS_IdToValue(cx, id, idval.address()))
        return nullptr;
    JSString *str = JS::ToString(cx, idval);
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

inline void SetPendingException(JSContext *cx, const char *aMsg)
{
    JS_ReportError(cx, "%s", aMsg);
}

inline void SetPendingException(JSContext *cx, const char16_t *aMsg)
{
    JS_ReportError(cx, "%hs", aMsg);
}

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

JSContext *
nsScriptSecurityManager::GetCurrentJSContext()
{
    // Get JSContext from stack.
    return nsXPConnect::XPConnect()->GetCurrentJSContext();
}

JSContext *
nsScriptSecurityManager::GetSafeJSContext()
{
    // Get JSContext from stack.
    return nsXPConnect::XPConnect()->GetSafeJSContext();
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

bool
nsScriptSecurityManager::ContentSecurityPolicyPermitsJSAction(JSContext *cx)
{
    // Get the security manager
    nsScriptSecurityManager *ssm =
        nsScriptSecurityManager::GetScriptSecurityManager();

    NS_ASSERTION(ssm, "Failed to get security manager service");
    if (!ssm)
        return false;

    nsresult rv;
    nsIPrincipal* subjectPrincipal = ssm->GetSubjectPrincipal(cx, &rv);

    NS_ASSERTION(NS_SUCCEEDED(rv), "CSP: Failed to get nsIPrincipal from js context");
    if (NS_FAILED(rv))
        return false; // Not just absence of principal, but failure.

    if (!subjectPrincipal)
        return true;

    nsCOMPtr<nsIContentSecurityPolicy> csp;
    rv = subjectPrincipal->GetCsp(getter_AddRefs(csp));
    NS_ASSERTION(NS_SUCCEEDED(rv), "CSP: Failed to get CSP from principal.");

    // don't do anything unless there's a CSP
    if (!csp)
        return true;

    bool evalOK = true;
    bool reportViolation = false;
    rv = csp->GetAllowsEval(&reportViolation, &evalOK);

    if (NS_FAILED(rv))
    {
        NS_WARNING("CSP: failed to get allowsEval");
        return true; // fail open to not break sites.
    }

    if (reportViolation) {
        nsAutoString fileName;
        unsigned lineNum = 0;
        NS_NAMED_LITERAL_STRING(scriptSample, "call to eval() or related function blocked by CSP");

        JS::RootedScript script(cx);
        if (JS_DescribeScriptedCaller(cx, &script, &lineNum)) {
            if (const char *file = JS_GetScriptFilename(cx, script)) {
                CopyUTF8toUTF16(nsDependentCString(file), fileName);
            }
        }
        csp->LogViolationDetails(nsIContentSecurityPolicy::VIOLATION_TYPE_EVAL,
                                 fileName,
                                 scriptSample,
                                 lineNum,
                                 EmptyString(),
                                 EmptyString());
    }

    return evalOK;
}

bool
nsScriptSecurityManager::CheckObjectAccess(JSContext *cx, JS::Handle<JSObject*> obj,
                                           JS::Handle<jsid> id, JSAccessMode mode,
                                           JS::MutableHandle<JS::Value> vp)
{
    // Get the security manager
    nsScriptSecurityManager *ssm =
        nsScriptSecurityManager::GetScriptSecurityManager();

    NS_WARN_IF_FALSE(ssm, "Failed to get security manager service");
    if (!ssm)
        return false;

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
        return false; // Security check failed (XXX was an error reported?)

    return true;
}

// static
bool
nsScriptSecurityManager::JSPrincipalsSubsume(JSPrincipals *first,
                                             JSPrincipals *second)
{
    return nsJSPrincipals::get(first)->Subsumes(nsJSPrincipals::get(second));
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
                                   aClassName, aProperty);
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
                                                 const char* aClassName, jsid aProperty)
{
    nsresult rv;
    JS::RootedObject jsObject(cx, aJSObject);
    JS::RootedId property(cx, aProperty);
    nsIPrincipal* subjectPrincipal = GetSubjectPrincipal(cx, &rv);
    if (NS_FAILED(rv))
        return rv;

    if (!subjectPrincipal || subjectPrincipal == mSystemPrincipal)
        // We have native code or the system principal: just allow access
        return NS_OK;

    nsCOMPtr<nsIPrincipal> objectPrincipal;

    // Even though security wrappers have handled our cross-origin access policy
    // since FF4, we've still always called into this function, and have relied
    // on the CAPS policy mechanism to manually approve each cross-origin property
    // based on a whitelist in all.js. This whitelist has drifted out of sync
    // with the canonical one in AccessCheck.cpp, but it's always been more
    // permissive, so we never noticed. This machinery is going away, so for now
    // let's just skip the check for cross-origin property accesses that we know
    // we get right.
    //
    // Note that while it would be nice to just rely on aClassName here, it
    // isn't set reliably by callers. :-(
    const char *className = aClassName;
    if (!className && jsObject) {
        className = JS_GetClass(jsObject)->name;
    }
    if (className &&
        (!strcmp(className, "Window") || !strcmp(className, "Location")))
    {
        return NS_OK;
    }

    // Hold the class info data here so we don't have to go back to virtual
    // methods all the time
    ClassInfoData classInfoData(aClassInfo, aClassName);

    // If we were called from somewhere other than XPConnect
    // (no XPC call context), assume this is a DOM class. Otherwise,
    // ask the ClassInfo.
    if (aCallContext && !classInfoData.IsDOMClass()) {
        rv = NS_ERROR_DOM_PROP_ACCESS_DENIED;
    } else {
        if (!jsObject) {
            NS_ERROR("CheckPropertyAccessImpl called without a target object or URL");
            return NS_ERROR_FAILURE;
        }
        nsCOMPtr<nsIPrincipal> principalHolder;
        objectPrincipal = doGetObjectPrincipal(jsObject);
        if (!objectPrincipal)
            rv = NS_ERROR_DOM_SECURITY_ERR;

        if (NS_SUCCEEDED(rv))
            rv = CheckSameOriginDOMProp(subjectPrincipal, objectPrincipal,
                                        aAction);
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
            rv = wrapper->FindInterfaceWithMember(property, getter_AddRefs(interfaceInfo));
        if (NS_SUCCEEDED(rv) && interfaceInfo)
            rv = interfaceInfo->GetIIDShared(&objIID);
        if (NS_SUCCEEDED(rv) && objIID)
        {
            switch (aAction)
            {
            case nsIXPCSecurityManager::ACCESS_GET_PROPERTY:
                checkedComponent->CanGetProperty(objIID,
                                                 IDToString(cx, property),
                                                 getter_Copies(objectSecurityLevel));
                break;
            case nsIXPCSecurityManager::ACCESS_SET_PROPERTY:
                checkedComponent->CanSetProperty(objIID,
                                                 IDToString(cx, property),
                                                 getter_Copies(objectSecurityLevel));
                break;
            case nsIXPCSecurityManager::ACCESS_CALL_METHOD:
                checkedComponent->CanCallMethod(objIID,
                                                IDToString(cx, property),
                                                getter_Copies(objectSecurityLevel));
            }
        }
    }
    rv = CheckXPCPermissions(cx, aObj, jsObject, subjectPrincipal,
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
        const char16_t *formatStrings[] =
        {
            subjectOriginUnicode.get(),
            className.get(),
            IDToString(cx, property),
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
            // None of our whitelisted principals worked.
            return NS_ERROR_DOM_BAD_URI;
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
        const char16_t* formatStrings[] = { ucsTargetScheme.get() };
        rv = sStrBundle->
            FormatStringFromName(MOZ_UTF16("ProtocolFlagError"),
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
    const char16_t *formatStrings[] = { ucsSourceSpec.get(), ucsTargetSpec.get() };
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
    if (rv == NS_ERROR_DOM_BAD_URI) {
        // Don't warn because NS_ERROR_DOM_BAD_URI is one of the expected
        // return values.
        return rv;
    }
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
        nsIURIFixup::FIXUP_FLAG_FIX_SCHEME_TYPOS,
        nsIURIFixup::FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP,
        nsIURIFixup::FIXUP_FLAGS_MAKE_ALTERNATE_URI,
        nsIURIFixup::FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP |
        nsIURIFixup::FIXUP_FLAGS_MAKE_ALTERNATE_URI
    };

    for (uint32_t i = 0; i < ArrayLength(flags); ++i) {
        rv = fixup->CreateFixupURI(aTargetURIStr, flags[i], nullptr,
                                   getter_AddRefs(target));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = CheckLoadURIWithPrincipal(aPrincipal, target, aFlags);
        if (rv == NS_ERROR_DOM_BAD_URI) {
            // Don't warn because NS_ERROR_DOM_BAD_URI is one of the expected
            // return values.
            return rv;
        }
        NS_ENSURE_SUCCESS(rv, rv);
    }

    return rv;
}

bool
nsScriptSecurityManager::ScriptAllowed(JSObject *aGlobal)
{
    MOZ_ASSERT(aGlobal);
    MOZ_ASSERT(JS_IsGlobalObject(aGlobal) || js::IsOuterObject(aGlobal));
    AutoJSContext cx;
    JS::RootedObject global(cx, js::UncheckedUnwrap(aGlobal, /* stopAtOuter = */ false));

    // Check the bits on the compartment private.
    return xpc::Scriptability::Get(aGlobal).Allowed();
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
nsScriptSecurityManager::GetObjectPrincipal(const JS::Value &aObjectVal,
                                            JSContext *aCx,
                                            nsIPrincipal **result)
{
    NS_ENSURE_TRUE(aObjectVal.isObject(), NS_ERROR_FAILURE);
    JS::RootedObject obj(aCx, &aObjectVal.toObject());
    nsCOMPtr<nsIPrincipal> principal = doGetObjectPrincipal(obj);
    principal.forget(result);
    return NS_OK;
}

// static
nsIPrincipal*
nsScriptSecurityManager::doGetObjectPrincipal(JSObject *aObj)
{
    JSCompartment *compartment = js::GetObjectCompartment(aObj);
    JSPrincipals *principals = JS_GetCompartmentPrincipals(compartment);
    return nsJSPrincipals::get(principals);
}

////////////////////////////////////////////////
// Methods implementing nsIXPCSecurityManager //
////////////////////////////////////////////////

NS_IMETHODIMP
nsScriptSecurityManager::CanCreateWrapper(JSContext *cx,
                                          const nsIID &aIID,
                                          nsISupports *aObj,
                                          nsIClassInfo *aClassInfo)
{
// XXX Special case for nsIXPCException ?
    ClassInfoData objClassInfo = ClassInfoData(aClassInfo, nullptr);
    if (objClassInfo.IsDOMClass())
    {
        return NS_OK;
    }

    // We give remote-XUL whitelisted domains a free pass here. See bug 932906.
    if (!xpc::AllowXBLScope(js::GetContextCompartment(cx)))
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
        const char16_t* formatStrings[] = {
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
    nsresult rv = CheckXPCPermissions(cx, nullptr, nullptr, nullptr, nullptr);
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
    nsresult rv = CheckXPCPermissions(cx, nullptr, nullptr, nullptr, nullptr);
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
                                   jsid aPropertyName)
{
    return CheckPropertyAccessImpl(aAction, aCallContext, cx,
                                   aJSObject, aObj, aClassInfo,
                                   nullptr, aPropertyName);
}

nsresult
nsScriptSecurityManager::CheckXPCPermissions(JSContext* cx,
                                             nsISupports* aObj, JSObject* aJSObject,
                                             nsIPrincipal* aSubjectPrincipal,
                                             const char* aObjectSecurityLevel)
{
    MOZ_ASSERT(cx);
    JS::RootedObject jsObject(cx, aJSObject);
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
            if (!jsObject)
            {
                nsCOMPtr<nsIXPConnectWrappedJS> xpcwrappedjs =
                    do_QueryInterface(aObj);
                if (xpcwrappedjs)
                {
                    jsObject = xpcwrappedjs->GetJSObject();
                    NS_ENSURE_STATE(jsObject);
                }
            }

            if (!aSubjectPrincipal)
            {
                // No subject principal passed in. Compute it.
                aSubjectPrincipal = GetSubjectPrincipal(cx, &rv);
                NS_ENSURE_SUCCESS(rv, rv);
            }
            if (aSubjectPrincipal && jsObject)
            {
                nsIPrincipal* objectPrincipal = doGetObjectPrincipal(jsObject);

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

static const char* kObservedPrefs[] = {
  sJSEnabledPrefName,
  sFileOriginPolicyPrefName,
  nullptr
};


NS_IMETHODIMP
nsScriptSecurityManager::Observe(nsISupports* aObject, const char* aTopic,
                                 const char16_t* aMessage)
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
    return rv;
}

/////////////////////////////////////////////
// Constructor, Destructor, Initialization //
/////////////////////////////////////////////
nsScriptSecurityManager::nsScriptSecurityManager(void)
    : mPrefInitialized(false)
    , mIsJavaScriptEnabled(false)
{
    static_assert(sizeof(intptr_t) == sizeof(void*),
                  "intptr_t and void* have different lengths on this platform. "
                  "This may cause a security failure with the SecurityLevel union.");
}

nsresult nsScriptSecurityManager::Init()
{
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
    rv = nsXPConnect::XPConnect()->GetRuntime(&sRuntime);
    NS_ENSURE_SUCCESS(rv, rv);

    static const JSSecurityCallbacks securityCallbacks = {
        CheckObjectAccess,
        ContentSecurityPolicyPermitsJSAction,
        JSPrincipalsSubsume,
    };

    MOZ_ASSERT(!JS_GetSecurityCallbacks(sRuntime));
    JS_SetSecurityCallbacks(sRuntime, &securityCallbacks);
    JS_InitDestroyPrincipalsCallback(sRuntime, nsJSPrincipals::Destroy);

    JS_SetTrustedPrincipals(sRuntime, system);

    return NS_OK;
}

static StaticRefPtr<nsScriptSecurityManager> gScriptSecMan;

nsScriptSecurityManager::~nsScriptSecurityManager(void)
{
    Preferences::RemoveObservers(this, kObservedPrefs);
    if (mDomainPolicy)
        mDomainPolicy->Deactivate();
    MOZ_ASSERT(!mDomainPolicy);
}

void
nsScriptSecurityManager::Shutdown()
{
    if (sRuntime) {
        JS_SetSecurityCallbacks(sRuntime, nullptr);
        JS_SetTrustedPrincipals(sRuntime, nullptr);
        sRuntime = nullptr;
    }

    NS_IF_RELEASE(sIOService);
    NS_IF_RELEASE(sStrBundle);
}

nsScriptSecurityManager *
nsScriptSecurityManager::GetScriptSecurityManager()
{
    if (!gScriptSecMan && nsXPConnect::XPConnect())
    {
        nsRefPtr<nsScriptSecurityManager> ssManager = new nsScriptSecurityManager();

        nsresult rv;
        rv = ssManager->Init();
        if (NS_FAILED(rv)) {
            return nullptr;
        }
 
        rv = nsXPConnect::XPConnect()->
            SetDefaultSecurityManager(ssManager);
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
GetJarPrefix(uint32_t aAppId, bool aInMozBrowser, nsACString& aJarPrefix)
{
  MOZ_ASSERT(aAppId != nsIScriptSecurityManager::UNKNOWN_APP_ID);

  if (aAppId == nsIScriptSecurityManager::UNKNOWN_APP_ID) {
    aAppId = nsIScriptSecurityManager::NO_APP_ID;
  }

  aJarPrefix.Truncate();

  // Fallback.
  if (aAppId == nsIScriptSecurityManager::NO_APP_ID && !aInMozBrowser) {
    return;
  }

  // aJarPrefix = appId + "+" + { 't', 'f' } + "+";
  aJarPrefix.AppendInt(aAppId);
  aJarPrefix.Append('+');
  aJarPrefix.Append(aInMozBrowser ? 't' : 'f');
  aJarPrefix.Append('+');

  return;
}

} // namespace mozilla

NS_IMETHODIMP
nsScriptSecurityManager::GetJarPrefix(uint32_t aAppId,
                                      bool aInMozBrowser,
                                      nsACString& aJarPrefix)
{
  MOZ_ASSERT(aAppId != nsIScriptSecurityManager::UNKNOWN_APP_ID);

  mozilla::GetJarPrefix(aAppId, aInMozBrowser, aJarPrefix);
  return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::GetDomainPolicyActive(bool *aRv)
{
    *aRv = !!mDomainPolicy;
    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::ActivateDomainPolicy(nsIDomainPolicy** aRv)
{
    // We only allow one domain policy at a time. The holder of the previous
    // policy must explicitly deactivate it first.
    if (mDomainPolicy) {
        return NS_ERROR_SERVICE_NOT_AVAILABLE;
    }

    mDomainPolicy = new DomainPolicy();
    nsCOMPtr<nsIDomainPolicy> ptr = mDomainPolicy;
    ptr.forget(aRv);
    return NS_OK;
}

// Intentionally non-scriptable. Script must have a reference to the
// nsIDomainPolicy to deactivate it.
void
nsScriptSecurityManager::DeactivateDomainPolicy()
{
    mDomainPolicy = nullptr;
}

NS_IMETHODIMP
nsScriptSecurityManager::PolicyAllowsScript(nsIURI* aURI, bool *aRv)
{
    nsresult rv;

    // Compute our rule. If we don't have any domain policy set up that might
    // provide exceptions to this rule, we're done.
    *aRv = mIsJavaScriptEnabled;
    if (!mDomainPolicy) {
        return NS_OK;
    }

    // We have a domain policy. Grab the appropriate set of exceptions to the
    // rule (either the blacklist or the whitelist, depending on whether script
    // is enabled or disabled by default).
    nsCOMPtr<nsIDomainSet> exceptions;
    nsCOMPtr<nsIDomainSet> superExceptions;
    if (*aRv) {
        mDomainPolicy->GetBlacklist(getter_AddRefs(exceptions));
        mDomainPolicy->GetSuperBlacklist(getter_AddRefs(superExceptions));
    } else {
        mDomainPolicy->GetWhitelist(getter_AddRefs(exceptions));
        mDomainPolicy->GetSuperWhitelist(getter_AddRefs(superExceptions));
    }

    bool contains;
    rv = exceptions->Contains(aURI, &contains);
    NS_ENSURE_SUCCESS(rv, rv);
    if (contains) {
        *aRv = !*aRv;
        return NS_OK;
    }
    rv = superExceptions->ContainsSuperDomain(aURI, &contains);
    NS_ENSURE_SUCCESS(rv, rv);
    if (contains) {
        *aRv = !*aRv;
    }

    return NS_OK;
}
