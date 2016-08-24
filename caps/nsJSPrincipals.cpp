/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcpublic.h"
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
JS_PUBLIC_API(void)
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
          tag == SCTAG_DOM_CONTENT_PRINCIPAL ||
          tag == SCTAG_DOM_EXPANDED_PRINCIPAL)) {
        xpc::Throw(aCx, NS_ERROR_DOM_DATA_CLONE_ERR);
        return false;
    }

    return ReadKnownPrincipalType(aCx, aReader, tag, aOutPrincipals);
}

static bool
ReadSuffixAndSpec(JSStructuredCloneReader* aReader,
                  PrincipalOriginAttributes& aAttrs,
                  nsACString& aSpec)
{
    uint32_t suffixLength, specLength;
    if (!JS_ReadUint32Pair(aReader, &suffixLength, &specLength)) {
        return false;
    }

    nsAutoCString suffix;
    suffix.SetLength(suffixLength);
    if (!JS_ReadBytes(aReader, suffix.BeginWriting(), suffixLength)) {
        return false;
    }

    if (!aAttrs.PopulateFromSuffix(suffix)) {
        return false;
    }

    aSpec.SetLength(specLength);
    if (!JS_ReadBytes(aReader, aSpec.BeginWriting(), specLength)) {
        return false;
    }

    return true;
}

static bool
ReadPrincipalInfo(JSStructuredCloneReader* aReader,
                  uint32_t aTag,
                  PrincipalInfo& aInfo)
{
    if (aTag == SCTAG_DOM_SYSTEM_PRINCIPAL) {
        aInfo = SystemPrincipalInfo();
    } else if (aTag == SCTAG_DOM_NULL_PRINCIPAL) {
        PrincipalOriginAttributes attrs;
        nsAutoCString dummy;
        if (!ReadSuffixAndSpec(aReader, attrs, dummy)) {
            return false;
        }
        aInfo = NullPrincipalInfo(attrs);
    } else if (aTag == SCTAG_DOM_EXPANDED_PRINCIPAL) {
        uint32_t length, unused;
        if (!JS_ReadUint32Pair(aReader, &length, &unused)) {
            return false;
        }

        ExpandedPrincipalInfo expanded;

        for (uint32_t i = 0; i < length; i++) {
            uint32_t tag;
            if (!JS_ReadUint32Pair(aReader, &tag, &unused)) {
                return false;
            }

            PrincipalInfo sub;
            if (!ReadPrincipalInfo(aReader, tag, sub)) {
                return false;
            }
            expanded.whitelist().AppendElement(sub);
        }

        aInfo = expanded;
    } else if (aTag == SCTAG_DOM_CONTENT_PRINCIPAL) {
        PrincipalOriginAttributes attrs;
        nsAutoCString spec;
        if (!ReadSuffixAndSpec(aReader, attrs, spec)) {
            return false;
        }

        aInfo = ContentPrincipalInfo(attrs, spec);
    } else {
        MOZ_CRASH("unexpected principal structured clone tag");
    }

    return true;
}

/* static */ bool
nsJSPrincipals::ReadKnownPrincipalType(JSContext* aCx,
                                       JSStructuredCloneReader* aReader,
                                       uint32_t aTag,
                                       JSPrincipals** aOutPrincipals)
{
    MOZ_ASSERT(aTag == SCTAG_DOM_NULL_PRINCIPAL ||
               aTag == SCTAG_DOM_SYSTEM_PRINCIPAL ||
               aTag == SCTAG_DOM_CONTENT_PRINCIPAL ||
               aTag == SCTAG_DOM_EXPANDED_PRINCIPAL);

    if (NS_WARN_IF(!NS_IsMainThread())) {
        xpc::Throw(aCx, NS_ERROR_UNCATCHABLE_EXCEPTION);
        return false;
    }

    PrincipalInfo info;
    if (!ReadPrincipalInfo(aReader, aTag, info)) {
        return false;
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

static bool
WriteSuffixAndSpec(JSStructuredCloneWriter* aWriter,
                   const PrincipalOriginAttributes& aAttrs,
                   const nsCString& aSpec)
{
  nsAutoCString suffix;
  aAttrs.CreateSuffix(suffix);

  return JS_WriteUint32Pair(aWriter, suffix.Length(), aSpec.Length()) &&
         JS_WriteBytes(aWriter, suffix.get(), suffix.Length()) &&
         JS_WriteBytes(aWriter, aSpec.get(), aSpec.Length());
}

static bool
WritePrincipalInfo(JSStructuredCloneWriter* aWriter, const PrincipalInfo& aInfo)
{
    if (aInfo.type() == PrincipalInfo::TNullPrincipalInfo) {
        const NullPrincipalInfo& nullInfo = aInfo;
        return JS_WriteUint32Pair(aWriter, SCTAG_DOM_NULL_PRINCIPAL, 0) &&
               WriteSuffixAndSpec(aWriter, nullInfo.attrs(), EmptyCString());
    }
    if (aInfo.type() == PrincipalInfo::TSystemPrincipalInfo) {
        return JS_WriteUint32Pair(aWriter, SCTAG_DOM_SYSTEM_PRINCIPAL, 0);
    }
    if (aInfo.type() == PrincipalInfo::TExpandedPrincipalInfo) {
        const ExpandedPrincipalInfo& expanded = aInfo;
        if (!JS_WriteUint32Pair(aWriter, SCTAG_DOM_EXPANDED_PRINCIPAL, 0) ||
            !JS_WriteUint32Pair(aWriter, expanded.whitelist().Length(), 0)) {
            return false;
        }

        for (uint32_t i = 0; i < expanded.whitelist().Length(); i++) {
            if (!WritePrincipalInfo(aWriter, expanded.whitelist()[i])) {
                return false;
            }
        }
        return true;
    }

    MOZ_ASSERT(aInfo.type() == PrincipalInfo::TContentPrincipalInfo);
    const ContentPrincipalInfo& cInfo = aInfo;
    return JS_WriteUint32Pair(aWriter, SCTAG_DOM_CONTENT_PRINCIPAL, 0) &&
           WriteSuffixAndSpec(aWriter, cInfo.attrs(), cInfo.spec());
}

bool
nsJSPrincipals::write(JSContext* aCx, JSStructuredCloneWriter* aWriter)
{
    PrincipalInfo info;
    if (NS_WARN_IF(NS_FAILED(PrincipalToPrincipalInfo(this, &info)))) {
        xpc::Throw(aCx, NS_ERROR_DOM_DATA_CLONE_ERR);
        return false;
    }

    return WritePrincipalInfo(aWriter, info);
}
