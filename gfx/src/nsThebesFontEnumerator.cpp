/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsThebesFontEnumerator.h"
#include <stdint.h>              // for uint32_t
#include "gfxPlatform.h"         // for gfxPlatform
#include "mozilla/Assertions.h"  // for MOZ_ASSERT_HELPER2
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/Promise.h"  // for mozilla::dom::Promise
#include "nsCOMPtr.h"             // for nsCOMPtr
#include "nsDebug.h"              // for NS_ENSURE_ARG_POINTER
#include "nsError.h"              // for NS_OK, NS_FAILED, nsresult
#include "nsAtom.h"               // for nsAtom, NS_Atomize
#include "nsID.h"
#include "nsString.h"  // for nsAutoCString, nsAutoString, etc
#include "nsTArray.h"  // for nsTArray, nsTArray_Impl, etc
#include "nscore.h"    // for char16_t, NS_IMETHODIMP

using mozilla::MakeUnique;
using mozilla::Runnable;
using mozilla::UniquePtr;

NS_IMPL_ISUPPORTS(nsThebesFontEnumerator, nsIFontEnumerator)

nsThebesFontEnumerator::nsThebesFontEnumerator() = default;

NS_IMETHODIMP
nsThebesFontEnumerator::EnumerateAllFonts(nsTArray<nsString>& aResult) {
  return EnumerateFonts(nullptr, nullptr, aResult);
}

NS_IMETHODIMP
nsThebesFontEnumerator::EnumerateFonts(const char* aLangGroup,
                                       const char* aGeneric,
                                       nsTArray<nsString>& aResult) {
  nsAutoCString generic;
  if (aGeneric)
    generic.Assign(aGeneric);
  else
    generic.SetIsVoid(true);

  RefPtr<nsAtom> langGroupAtom;
  if (aLangGroup) {
    nsAutoCString lowered;
    lowered.Assign(aLangGroup);
    ToLowerCase(lowered);
    langGroupAtom = NS_Atomize(lowered);
  }

  return gfxPlatform::GetPlatform()->GetFontList(langGroupAtom, generic,
                                                 aResult);
}

struct EnumerateFontsPromise final {
  explicit EnumerateFontsPromise(mozilla::dom::Promise* aPromise)
      : mPromise(aPromise) {
    MOZ_ASSERT(aPromise);
    MOZ_ASSERT(NS_IsMainThread());
  }

  RefPtr<mozilla::dom::Promise> mPromise;
};

class EnumerateFontsResult final : public Runnable {
 public:
  EnumerateFontsResult(nsresult aRv,
                       UniquePtr<EnumerateFontsPromise> aEnumerateFontsPromise,
                       nsTArray<nsString> aFontList)
      : Runnable("EnumerateFontsResult"),
        mRv(aRv),
        mEnumerateFontsPromise(std::move(aEnumerateFontsPromise)),
        mFontList(std::move(aFontList)),
        mWorkerThread(do_GetCurrentThread()) {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread());

    if (NS_FAILED(mRv)) {
      mEnumerateFontsPromise->mPromise->MaybeReject(mRv);
    } else {
      mEnumerateFontsPromise->mPromise->MaybeResolve(mFontList);
    }

    mWorkerThread->Shutdown();

    return NS_OK;
  }

 private:
  nsresult mRv;
  UniquePtr<EnumerateFontsPromise> mEnumerateFontsPromise;
  nsTArray<nsString> mFontList;
  nsCOMPtr<nsIThread> mWorkerThread;
};

class EnumerateFontsTask final : public Runnable {
 public:
  EnumerateFontsTask(nsAtom* aLangGroupAtom, const nsAutoCString& aGeneric,
                     UniquePtr<EnumerateFontsPromise> aEnumerateFontsPromise,
                     nsIEventTarget* aMainThreadTarget)
      : Runnable("EnumerateFontsTask"),
        mLangGroupAtom(aLangGroupAtom),
        mGeneric(aGeneric),
        mEnumerateFontsPromise(std::move(aEnumerateFontsPromise)),
        mMainThreadTarget(aMainThreadTarget) {
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_IMETHOD Run() override {
    MOZ_ASSERT(!NS_IsMainThread());

    nsTArray<nsString> fontList;

    nsresult rv = gfxPlatform::GetPlatform()->GetFontList(mLangGroupAtom,
                                                          mGeneric, fontList);
    nsCOMPtr<nsIRunnable> runnable = new EnumerateFontsResult(
        rv, std::move(mEnumerateFontsPromise), std::move(fontList));
    mMainThreadTarget->Dispatch(runnable.forget());

    return NS_OK;
  }

 private:
  RefPtr<nsAtom> mLangGroupAtom;
  nsAutoCStringN<16> mGeneric;
  UniquePtr<EnumerateFontsPromise> mEnumerateFontsPromise;
  RefPtr<nsIEventTarget> mMainThreadTarget;
};

NS_IMETHODIMP
nsThebesFontEnumerator::EnumerateAllFontsAsync(
    JSContext* aCx, JS::MutableHandle<JS::Value> aRval) {
  return EnumerateFontsAsync(nullptr, nullptr, aCx, aRval);
}

NS_IMETHODIMP
nsThebesFontEnumerator::EnumerateFontsAsync(
    const char* aLangGroup, const char* aGeneric, JSContext* aCx,
    JS::MutableHandle<JS::Value> aRval) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIGlobalObject> global = xpc::CurrentNativeGlobal(aCx);
  NS_ENSURE_TRUE(global, NS_ERROR_UNEXPECTED);

  mozilla::ErrorResult errv;
  RefPtr<mozilla::dom::Promise> promise =
      mozilla::dom::Promise::Create(global, errv);
  if (errv.Failed()) {
    return errv.StealNSResult();
  }

  auto enumerateFontsPromise = MakeUnique<EnumerateFontsPromise>(promise);

  nsCOMPtr<nsIThread> thread;
  nsresult rv = NS_NewNamedThread("FontEnumThread", getter_AddRefs(thread));
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<nsAtom> langGroupAtom;
  if (aLangGroup) {
    nsAutoCStringN<16> lowered;
    lowered.Assign(aLangGroup);
    ToLowerCase(lowered);
    langGroupAtom = NS_Atomize(lowered);
  }

  nsAutoCString generic;
  if (aGeneric) {
    generic.Assign(aGeneric);
  } else {
    generic.SetIsVoid(true);
  }

  nsCOMPtr<nsIEventTarget> target = global->SerialEventTarget();
  nsCOMPtr<nsIRunnable> runnable = new EnumerateFontsTask(
      langGroupAtom, generic, std::move(enumerateFontsPromise), target);
  thread->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);

  if (!ToJSValue(aCx, promise, aRval)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsThebesFontEnumerator::HaveFontFor(const char* aLangGroup, bool* aResult) {
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = true;
  return NS_OK;
}

NS_IMETHODIMP
nsThebesFontEnumerator::GetDefaultFont(const char* aLangGroup,
                                       const char* aGeneric,
                                       char16_t** aResult) {
  if (NS_WARN_IF(!aResult) || NS_WARN_IF(!aLangGroup) ||
      NS_WARN_IF(!aGeneric)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aResult = nullptr;
  nsAutoCString defaultFontName(gfxPlatform::GetPlatform()->GetDefaultFontName(
      nsDependentCString(aLangGroup), nsDependentCString(aGeneric)));
  if (!defaultFontName.IsEmpty()) {
    *aResult = UTF8ToNewUnicode(defaultFontName);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsThebesFontEnumerator::GetStandardFamilyName(const char16_t* aName,
                                              char16_t** aResult) {
  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_ARG_POINTER(aName);

  nsAutoString name(aName);
  if (name.IsEmpty()) {
    *aResult = nullptr;
    return NS_OK;
  }

  nsAutoCString family;
  gfxPlatform::GetPlatform()->GetStandardFamilyName(
      NS_ConvertUTF16toUTF8(aName), family);
  if (family.IsEmpty()) {
    *aResult = nullptr;
    return NS_OK;
  }
  *aResult = UTF8ToNewUnicode(family);
  return NS_OK;
}
