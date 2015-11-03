/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcprivate.h"
#include "nsString.h"
#include "nsIObjectOutputStream.h"
#include "nsIObjectInputStream.h"
#include "nsJSPrincipals.h"
#include "plstr.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsMemory.h"
#include "nsStringBuffer.h"

#include "mozilla/dom/StructuredCloneTags.h"
// for mozilla::dom::workers::kJSPrincipalsDebugToken
#include "mozilla/dom/workers/Workers.h"
#include "mozilla/ipc/BackgroundUtils.h"

using namespace mozilla;
using namespace mozilla::ipc;

NS_IMETHODIMP_(MozExternalRefCountType)
nsJSPrincipals::AddRef()
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_PRECONDITION(int32_t(refcount) >= 0, "illegal refcnt");
  nsrefcnt count = ++refcount;
  NS_LOG_ADDREF(this, count, "nsJSPrincipals", sizeof(*this));
  return count;
}

NS_IMETHODIMP_(MozExternalRefCountType)
nsJSPrincipals::Release()
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_PRECONDITION(0 != refcount, "dup release");
  nsrefcnt count = --refcount;
  NS_LOG_RELEASE(this, count, "nsJSPrincipals");
  if (count == 0) {
    delete this;
  }

  return count;
}

/* static */ bool
nsJSPrincipals::Subsume(JSPrincipals *jsprin, JSPrincipals *other)
{
    bool result;
    nsresult rv = nsJSPrincipals::get(jsprin)->Subsumes(nsJSPrincipals::get(other), &result);
    return NS_SUCCEEDED(rv) && result;
}

/* static */ void
nsJSPrincipals::Destroy(JSPrincipals *jsprin)
{
    // The JS runtime can call this method during the last GC when
    // nsScriptSecurityManager is destroyed. So we must not assume here that
    // the security manager still exists.

    nsJSPrincipals *nsjsprin = nsJSPrincipals::get(jsprin);

    // We need to destroy the nsIPrincipal. We'll do this by adding
    // to the refcount and calling release

#ifdef NS_BUILD_REFCNT_LOGGING
    // The refcount logging considers AddRef-to-1 to indicate creation,
    // so trick it into thinking it's otherwise, but balance the
    // Release() we do below.
    nsjsprin->refcount++;
    nsjsprin->AddRef();
    nsjsprin->refcount--;
#else
    nsjsprin->refcount++;
#endif
    nsjsprin->Release();
}

#ifdef DEBUG

// Defined here so one can do principals->dump() in the debugger
JS_EXPORT_API(void)
JSPrincipals::dump()
{
    if (debugToken == nsJSPrincipals::DEBUG_TOKEN) {
      nsAutoCString str;
      static_cast<nsJSPrincipals *>(this)->GetScriptLocation(str);
      fprintf(stderr, "nsIPrincipal (%p) = %s\n", static_cast<void*>(this), str.get());
    } else if (debugToken == dom::workers::kJSPrincipalsDebugToken) {
        fprintf(stderr, "Web Worker principal singleton (%p)\n", this);
    } else {
        fprintf(stderr,
                "!!! JSPrincipals (%p) is not nsJSPrincipals instance - bad token: "
                "actual=0x%x expected=0x%x\n",
                this, unsigned(debugToken), unsigned(nsJSPrincipals::DEBUG_TOKEN));
    }
}

#endif

/* static */ bool
nsJSPrincipals::ReadPrincipals(JSContext* aCx, JSStructuredCloneReader* aReader,
                               JSPrincipals** aOutPrincipals)
{
    uint32_t tag;
    uint32_t unused;
    if (!JS_ReadUint32Pair(aReader, &tag, &unused)) {
        return false;
    }

    if (!(tag == SCTAG_DOM_NULL_PRINCIPAL ||
          tag == SCTAG_DOM_SYSTEM_PRINCIPAL ||
          tag == SCTAG_DOM_CONTENT_PRINCIPAL)) {
        xpc::Throw(aCx, NS_ERROR_DOM_DATA_CLONE_ERR);
        return false;
    }

    return ReadKnownPrincipalType(aCx, aReader, tag, aOutPrincipals);
}

/* static */ bool
nsJSPrincipals::ReadKnownPrincipalType(JSContext* aCx,
                                       JSStructuredCloneReader* aReader,
                                       uint32_t aTag,
                                       JSPrincipals** aOutPrincipals)
{
    MOZ_ASSERT(aTag == SCTAG_DOM_NULL_PRINCIPAL ||
               aTag == SCTAG_DOM_SYSTEM_PRINCIPAL ||
               aTag == SCTAG_DOM_CONTENT_PRINCIPAL);

    if (NS_WARN_IF(!NS_IsMainThread())) {
        xpc::Throw(aCx, NS_ERROR_UNCATCHABLE_EXCEPTION);
        return false;
    }

    PrincipalInfo info;
    if (aTag == SCTAG_DOM_SYSTEM_PRINCIPAL) {
        info = SystemPrincipalInfo();
    } else if (aTag == SCTAG_DOM_NULL_PRINCIPAL) {
        info = NullPrincipalInfo();
    } else {
        uint32_t suffixLength, specLength;
        if (!JS_ReadUint32Pair(aReader, &suffixLength, &specLength)) {
            return false;
        }

        nsAutoCString suffix;
        suffix.SetLength(suffixLength);
        if (!JS_ReadBytes(aReader, suffix.BeginWriting(), suffixLength)) {
            return false;
        }

        nsAutoCString spec;
        spec.SetLength(specLength);
        if (!JS_ReadBytes(aReader, spec.BeginWriting(), specLength)) {
            return false;
        }

        PrincipalOriginAttributes attrs;
        attrs.PopulateFromSuffix(suffix);
        info = ContentPrincipalInfo(attrs, spec);
    }

    nsresult rv;
    nsCOMPtr<nsIPrincipal> prin = PrincipalInfoToPrincipal(info, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
        xpc::Throw(aCx, NS_ERROR_DOM_DATA_CLONE_ERR);
        return false;
    }

    *aOutPrincipals = get(prin.forget().take());
    return true;
}

bool
nsJSPrincipals::write(JSContext* aCx, JSStructuredCloneWriter* aWriter)
{
    PrincipalInfo info;
    if (NS_WARN_IF(NS_FAILED(PrincipalToPrincipalInfo(this, &info)))) {
        xpc::Throw(aCx, NS_ERROR_DOM_DATA_CLONE_ERR);
        return false;
    }

    if (info.type() == PrincipalInfo::TNullPrincipalInfo) {
        return JS_WriteUint32Pair(aWriter, SCTAG_DOM_NULL_PRINCIPAL, 0);
    }
    if (info.type() == PrincipalInfo::TSystemPrincipalInfo) {
        return JS_WriteUint32Pair(aWriter, SCTAG_DOM_SYSTEM_PRINCIPAL, 0);
    }

    MOZ_ASSERT(info.type() == PrincipalInfo::TContentPrincipalInfo);
    const ContentPrincipalInfo& cInfo = info;
    nsAutoCString suffix;
    cInfo.attrs().CreateSuffix(suffix);
    return JS_WriteUint32Pair(aWriter, SCTAG_DOM_CONTENT_PRINCIPAL, 0) &&
           JS_WriteUint32Pair(aWriter, suffix.Length(), cInfo.spec().Length()) &&
           JS_WriteBytes(aWriter, suffix.get(), suffix.Length()) &&
           JS_WriteBytes(aWriter, cInfo.spec().get(), cInfo.spec().Length());
}
