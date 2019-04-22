/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
nsJSPrincipals::AddRef() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(int32_t(refcount) >= 0, "illegal refcnt");
  nsrefcnt count = ++refcount;
  NS_LOG_ADDREF(this, count, "nsJSPrincipals", sizeof(*this));
  return count;
}

NS_IMETHODIMP_(MozExternalRefCountType)
nsJSPrincipals::Release() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(0 != refcount, "dup release");
  nsrefcnt count = --refcount;
  NS_LOG_RELEASE(this, count, "nsJSPrincipals");
  if (count == 0) {
    delete this;
  }

  return count;
}

/* static */
bool nsJSPrincipals::Subsume(JSPrincipals* jsprin, JSPrincipals* other) {
  bool result;
  nsresult rv = nsJSPrincipals::get(jsprin)->Subsumes(
      nsJSPrincipals::get(other), &result);
  return NS_SUCCEEDED(rv) && result;
}

/* static */
void nsJSPrincipals::Destroy(JSPrincipals* jsprin) {
  // The JS runtime can call this method during the last GC when
  // nsScriptSecurityManager is destroyed. So we must not assume here that
  // the security manager still exists.

  nsJSPrincipals* nsjsprin = nsJSPrincipals::get(jsprin);

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
JS_PUBLIC_API void JSPrincipals::dump() {
  if (debugToken == nsJSPrincipals::DEBUG_TOKEN) {
    nsAutoCString str;
    nsresult rv = static_cast<nsJSPrincipals*>(this)->GetScriptLocation(str);
    fprintf(stderr, "nsIPrincipal (%p) = %s\n", static_cast<void*>(this),
            NS_SUCCEEDED(rv) ? str.get() : "(unknown)");
  } else if (debugToken == dom::workerinternals::kJSPrincipalsDebugToken) {
    fprintf(stderr, "Web Worker principal singleton (%p)\n", this);
  } else if (debugToken ==
             mozilla::dom::WorkletPrincipal::kJSPrincipalsDebugToken) {
    fprintf(stderr, "Web Worklet principal singleton (%p)\n", this);
  } else {
    fprintf(stderr,
            "!!! JSPrincipals (%p) is not nsJSPrincipals instance - bad token: "
            "actual=0x%x expected=0x%x\n",
            this, unsigned(debugToken), unsigned(nsJSPrincipals::DEBUG_TOKEN));
  }
}

#endif

/* static */
bool nsJSPrincipals::ReadPrincipals(JSContext* aCx,
                                    JSStructuredCloneReader* aReader,
                                    JSPrincipals** aOutPrincipals) {
  uint32_t tag;
  uint32_t unused;
  if (!JS_ReadUint32Pair(aReader, &tag, &unused)) {
    return false;
  }

  if (!(tag == SCTAG_DOM_NULL_PRINCIPAL || tag == SCTAG_DOM_SYSTEM_PRINCIPAL ||
        tag == SCTAG_DOM_CONTENT_PRINCIPAL ||
        tag == SCTAG_DOM_EXPANDED_PRINCIPAL ||
        tag == SCTAG_DOM_WORKER_PRINCIPAL)) {
    xpc::Throw(aCx, NS_ERROR_DOM_DATA_CLONE_ERR);
    return false;
  }

  return ReadKnownPrincipalType(aCx, aReader, tag, aOutPrincipals);
}

static bool ReadPrincipalInfo(
    JSStructuredCloneReader* aReader, OriginAttributes& aAttrs,
    nsACString& aSpec, nsACString& aOriginNoSuffix, nsACString& aBaseDomain,
    nsTArray<ContentSecurityPolicy>* aPolicies = nullptr) {
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

  uint32_t originNoSuffixLength, policyCount;
  if (!JS_ReadUint32Pair(aReader, &originNoSuffixLength, &policyCount)) {
    return false;
  }

  if (!aPolicies) {
    MOZ_ASSERT(policyCount == 0);
  }

  if (!aOriginNoSuffix.SetLength(originNoSuffixLength, fallible)) {
    return false;
  }

  if (!JS_ReadBytes(aReader, aOriginNoSuffix.BeginWriting(),
                    originNoSuffixLength)) {
    return false;
  }

  for (uint32_t i = 0; i < policyCount; i++) {
    uint32_t policyLength, reportAndMeta;
    if (!JS_ReadUint32Pair(aReader, &policyLength, &reportAndMeta)) {
      return false;
    }
    bool reportOnly = reportAndMeta & 1;
    bool deliveredViaMetaTag = reportAndMeta & 2;

    nsAutoCString policyStr;
    if (!policyStr.SetLength(policyLength, fallible)) {
      return false;
    }

    if (!JS_ReadBytes(aReader, policyStr.BeginWriting(), policyLength)) {
      return false;
    }

    if (aPolicies) {
      aPolicies->AppendElement(ContentSecurityPolicy(
          NS_ConvertUTF8toUTF16(policyStr), reportOnly, deliveredViaMetaTag));
    }
  }

  uint32_t baseDomainIsVoid, baseDomainLength;
  if (!JS_ReadUint32Pair(aReader, &baseDomainIsVoid, &baseDomainLength)) {
    return false;
  }

  MOZ_ASSERT(baseDomainIsVoid == 0 || baseDomainIsVoid == 1);

  if (baseDomainIsVoid) {
    MOZ_ASSERT(baseDomainLength == 0);

    aBaseDomain.SetIsVoid(true);
    return true;
  }

  if (!aBaseDomain.SetLength(baseDomainLength, fallible)) {
    return false;
  }

  if (!JS_ReadBytes(aReader, aBaseDomain.BeginWriting(), baseDomainLength)) {
    return false;
  }

  return true;
}

static bool ReadPrincipalInfo(JSStructuredCloneReader* aReader, uint32_t aTag,
                              PrincipalInfo& aInfo) {
  if (aTag == SCTAG_DOM_SYSTEM_PRINCIPAL) {
    aInfo = SystemPrincipalInfo();
  } else if (aTag == SCTAG_DOM_NULL_PRINCIPAL) {
    OriginAttributes attrs;
    nsAutoCString spec;
    nsAutoCString originNoSuffix;
    nsAutoCString baseDomain;
    if (!ReadPrincipalInfo(aReader, attrs, spec, originNoSuffix, baseDomain)) {
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
      expanded.allowlist().AppendElement(sub);
    }

    aInfo = expanded;
  } else if (aTag == SCTAG_DOM_CONTENT_PRINCIPAL) {
    OriginAttributes attrs;
    nsAutoCString spec;
    nsAutoCString originNoSuffix;
    nsAutoCString baseDomain;
    nsTArray<ContentSecurityPolicy> policies;
    if (!ReadPrincipalInfo(aReader, attrs, spec, originNoSuffix, baseDomain,
                           &policies)) {
      return false;
    }

#ifdef FUZZING
    if (originNoSuffix.IsEmpty()) {
      return false;
    }
#endif

    MOZ_DIAGNOSTIC_ASSERT(!originNoSuffix.IsEmpty());

    // XXX: Do we care about mDomain for structured clone?
    aInfo = ContentPrincipalInfo(attrs, originNoSuffix, spec, Nothing(),
                                 std::move(policies), baseDomain);
  } else {
#ifdef FUZZING
    return false;
#else
    MOZ_CRASH("unexpected principal structured clone tag");
#endif
  }

  return true;
}

static StaticRefPtr<nsIPrincipal> sActiveWorkerPrincipal;

nsJSPrincipals::AutoSetActiveWorkerPrincipal::AutoSetActiveWorkerPrincipal(
    nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(!sActiveWorkerPrincipal);
  sActiveWorkerPrincipal = aPrincipal;
}

nsJSPrincipals::AutoSetActiveWorkerPrincipal::~AutoSetActiveWorkerPrincipal() {
  sActiveWorkerPrincipal = nullptr;
}

/* static */
bool nsJSPrincipals::ReadKnownPrincipalType(JSContext* aCx,
                                            JSStructuredCloneReader* aReader,
                                            uint32_t aTag,
                                            JSPrincipals** aOutPrincipals) {
  MOZ_ASSERT(aTag == SCTAG_DOM_NULL_PRINCIPAL ||
             aTag == SCTAG_DOM_SYSTEM_PRINCIPAL ||
             aTag == SCTAG_DOM_CONTENT_PRINCIPAL ||
             aTag == SCTAG_DOM_EXPANDED_PRINCIPAL ||
             aTag == SCTAG_DOM_WORKER_PRINCIPAL);

  if (NS_WARN_IF(!NS_IsMainThread())) {
    xpc::Throw(aCx, NS_ERROR_UNCATCHABLE_EXCEPTION);
    return false;
  }

  if (aTag == SCTAG_DOM_WORKER_PRINCIPAL) {
    // When reading principals which were written on a worker thread, we need to
    // know the principal of the worker which did the write.
    if (!sActiveWorkerPrincipal) {
      xpc::Throw(aCx, NS_ERROR_DOM_DATA_CLONE_ERR);
      return false;
    }
    RefPtr<nsJSPrincipals> retval = get(sActiveWorkerPrincipal);
    retval.forget(aOutPrincipals);
    return true;
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

static bool WritePrincipalInfo(
    JSStructuredCloneWriter* aWriter, const OriginAttributes& aAttrs,
    const nsCString& aSpec, const nsCString& aOriginNoSuffix,
    const nsCString& aBaseDomain,
    const nsTArray<ContentSecurityPolicy>* aPolicies = nullptr) {
  nsAutoCString suffix;
  aAttrs.CreateSuffix(suffix);
  size_t policyCount = aPolicies ? aPolicies->Length() : 0;

  if (!(JS_WriteUint32Pair(aWriter, suffix.Length(), aSpec.Length()) &&
        JS_WriteBytes(aWriter, suffix.get(), suffix.Length()) &&
        JS_WriteBytes(aWriter, aSpec.get(), aSpec.Length()) &&
        JS_WriteUint32Pair(aWriter, aOriginNoSuffix.Length(), policyCount) &&
        JS_WriteBytes(aWriter, aOriginNoSuffix.get(),
                      aOriginNoSuffix.Length()))) {
    return false;
  }

  for (uint32_t i = 0; i < policyCount; i++) {
    nsCString policy;
    CopyUTF16toUTF8((*aPolicies)[i].policy(), policy);
    uint32_t reportAndMeta =
        ((*aPolicies)[i].reportOnlyFlag() ? 1 : 0) |
        ((*aPolicies)[i].deliveredViaMetaTagFlag() ? 2 : 0);
    if (!(JS_WriteUint32Pair(aWriter, policy.Length(), reportAndMeta) &&
          JS_WriteBytes(aWriter, PromiseFlatCString(policy).get(),
                        policy.Length()))) {
      return false;
    }
  }

  if (aBaseDomain.IsVoid()) {
    return JS_WriteUint32Pair(aWriter, 1, 0);
  }

  return JS_WriteUint32Pair(aWriter, 0, aBaseDomain.Length()) &&
         JS_WriteBytes(aWriter, aBaseDomain.get(), aBaseDomain.Length());
}

static bool WritePrincipalInfo(JSStructuredCloneWriter* aWriter,
                               const PrincipalInfo& aInfo) {
  if (aInfo.type() == PrincipalInfo::TNullPrincipalInfo) {
    const NullPrincipalInfo& nullInfo = aInfo;
    return JS_WriteUint32Pair(aWriter, SCTAG_DOM_NULL_PRINCIPAL, 0) &&
           WritePrincipalInfo(aWriter, nullInfo.attrs(), nullInfo.spec(),
                              EmptyCString(), EmptyCString());
  }
  if (aInfo.type() == PrincipalInfo::TSystemPrincipalInfo) {
    return JS_WriteUint32Pair(aWriter, SCTAG_DOM_SYSTEM_PRINCIPAL, 0);
  }
  if (aInfo.type() == PrincipalInfo::TExpandedPrincipalInfo) {
    const ExpandedPrincipalInfo& expanded = aInfo;
    if (!JS_WriteUint32Pair(aWriter, SCTAG_DOM_EXPANDED_PRINCIPAL, 0) ||
        !JS_WriteUint32Pair(aWriter, expanded.allowlist().Length(), 0)) {
      return false;
    }

    for (uint32_t i = 0; i < expanded.allowlist().Length(); i++) {
      if (!WritePrincipalInfo(aWriter, expanded.allowlist()[i])) {
        return false;
      }
    }
    return true;
  }

  MOZ_ASSERT(aInfo.type() == PrincipalInfo::TContentPrincipalInfo);
  const ContentPrincipalInfo& cInfo = aInfo;
  return JS_WriteUint32Pair(aWriter, SCTAG_DOM_CONTENT_PRINCIPAL, 0) &&
         WritePrincipalInfo(aWriter, cInfo.attrs(), cInfo.spec(),
                            cInfo.originNoSuffix(), cInfo.baseDomain(),
                            &(cInfo.securityPolicies()));
}

bool nsJSPrincipals::write(JSContext* aCx, JSStructuredCloneWriter* aWriter) {
  PrincipalInfo info;
  if (NS_WARN_IF(NS_FAILED(PrincipalToPrincipalInfo(this, &info)))) {
    xpc::Throw(aCx, NS_ERROR_DOM_DATA_CLONE_ERR);
    return false;
  }

  return WritePrincipalInfo(aWriter, info);
}
