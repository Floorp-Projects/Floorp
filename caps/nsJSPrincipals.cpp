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
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsMemory.h"
#include "nsStringBuffer.h"

#include "mozilla/dom/StructuredCloneTags.h"
// for mozilla::dom::workerinternals::kJSPrincipalsDebugToken
#include "mozilla/dom/workerinternals/JSSettings.h"
// for mozilla::dom::worklet::kJSPrincipalsDebugToken
#include "mozilla/dom/WorkletPrincipal.h"
#include "mozilla/ipc/BackgroundUtils.h"

using namespace mozilla;
using namespace mozilla::ipc;

NS_IMETHODIMP_(MozExternalRefCountType)
nsJSPrincipals::AddRef()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(int32_t(refcount) >= 0, "illegal refcnt");
  nsrefcnt count = ++refcount;
  NS_LOG_ADDREF(this, count, "nsJSPrincipals", sizeof(*this));
  return count;
}

NS_IMETHODIMP_(MozExternalRefCountType)
nsJSPrincipals::Release()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(0 != refcount, "dup release");
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
      nsresult rv = static_cast<nsJSPrincipals *>(this)->GetScriptLocation(str);
      fprintf(stderr, "nsIPrincipal (%p) = %s\n", static_cast<void*>(this),
              NS_SUCCEEDED(rv) ? str.get() : "(unknown)");
    } else if (debugToken == dom::workerinternals::kJSPrincipalsDebugToken) {
        fprintf(stderr, "Web Worker principal singleton (%p)\n", this);
    } else if (debugToken == mozilla::dom::WorkletPrincipal::kJSPrincipalsDebugToken) {
        fprintf(stderr, "Web Worklet principal singleton (%p)\n", this);
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
ReadPrincipalInfo(JSStructuredCloneReader* aReader,
                  OriginAttributes& aAttrs,
                  nsACString& aSpec,
                  nsACString& aOriginNoSuffix)
{
    uint32_t suffixLength, specLength;
    if (!JS_ReadUint32Pair(aReader, &suffixLength, &specLength)) {
        return false;
    }

    nsAutoCString suffix;
    if (!suffix.SetLength(suffixLength, fallible)) {
        return false;
    }

    if (!JS_ReadBytes(aReader, suffix.BeginWriting(), suffixLength)) {
        return false;
    }

    if (!aAttrs.PopulateFromSuffix(suffix)) {
        return false;
    }

    if (!aSpec.SetLength(specLength, fallible)) {
        return false;
    }

    if (!JS_ReadBytes(aReader, aSpec.BeginWriting(), specLength)) {
        return false;
    }

    uint32_t originNoSuffixLength, dummy;
    if (!JS_ReadUint32Pair(aReader, &originNoSuffixLength, &dummy)) {
        return false;
    }

    MOZ_ASSERT(dummy == 0);

    if (!aOriginNoSuffix.SetLength(originNoSuffixLength, fallible)) {
        return false;
    }

    if (!JS_ReadBytes(aReader, aOriginNoSuffix.BeginWriting(),
                      originNoSuffixLength)) {
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
        OriginAttributes attrs;
        nsAutoCString spec;
        nsAutoCString originNoSuffix;
        if (!ReadPrincipalInfo(aReader, attrs, spec, originNoSuffix)) {
            return false;
        }
        aInfo = NullPrincipalInfo(attrs, spec);
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
        OriginAttributes attrs;
        nsAutoCString spec;
        nsAutoCString originNoSuffix;
        if (!ReadPrincipalInfo(aReader, attrs, spec, originNoSuffix)) {
            return false;
        }

#ifdef FUZZING
        if (originNoSuffix.IsEmpty()) {
          return false;
        }
#endif

        MOZ_DIAGNOSTIC_ASSERT(!originNoSuffix.IsEmpty());

        aInfo = ContentPrincipalInfo(attrs, originNoSuffix, spec);
    } else {
#ifdef FUZZING
        return false;
#else
        MOZ_CRASH("unexpected principal structured clone tag");
#endif
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
WritePrincipalInfo(JSStructuredCloneWriter* aWriter,
                   const OriginAttributes& aAttrs,
                   const nsCString& aSpec,
                   const nsCString& aOriginNoSuffix)
{
  nsAutoCString suffix;
  aAttrs.CreateSuffix(suffix);

  return JS_WriteUint32Pair(aWriter, suffix.Length(), aSpec.Length()) &&
         JS_WriteBytes(aWriter, suffix.get(), suffix.Length()) &&
         JS_WriteBytes(aWriter, aSpec.get(), aSpec.Length()) &&
         JS_WriteUint32Pair(aWriter, aOriginNoSuffix.Length(), 0) &&
         JS_WriteBytes(aWriter, aOriginNoSuffix.get(),
                       aOriginNoSuffix.Length());
}

static bool
WritePrincipalInfo(JSStructuredCloneWriter* aWriter, const PrincipalInfo& aInfo)
{
    if (aInfo.type() == PrincipalInfo::TNullPrincipalInfo) {
        const NullPrincipalInfo& nullInfo = aInfo;
        return JS_WriteUint32Pair(aWriter, SCTAG_DOM_NULL_PRINCIPAL, 0) &&
               WritePrincipalInfo(aWriter, nullInfo.attrs(), nullInfo.spec(),
                                  EmptyCString());
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
           WritePrincipalInfo(aWriter, cInfo.attrs(), cInfo.spec(),
                              cInfo.originNoSuffix());
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
