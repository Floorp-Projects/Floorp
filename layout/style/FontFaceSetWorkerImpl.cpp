/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FontFaceSetWorkerImpl.h"
#include "FontPreloader.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/LoadInfo.h"
#include "nsContentPolicyUtils.h"
#include "nsFontFaceLoader.h"
#include "nsINetworkPredictor.h"
#include "nsIWebNavigation.h"

using namespace mozilla;
using namespace mozilla::css;

namespace mozilla::dom {

#define LOG(...)                                                       \
  MOZ_LOG(gfxUserFontSet::GetUserFontsLog(), mozilla::LogLevel::Debug, \
          (__VA_ARGS__))
#define LOG_ENABLED() \
  MOZ_LOG_TEST(gfxUserFontSet::GetUserFontsLog(), LogLevel::Debug)

NS_IMPL_ISUPPORTS_INHERITED0(FontFaceSetWorkerImpl, FontFaceSetImpl);

FontFaceSetWorkerImpl::FontFaceSetWorkerImpl(FontFaceSet* aOwner)
    : FontFaceSetImpl(aOwner) {}

FontFaceSetWorkerImpl::~FontFaceSetWorkerImpl() = default;

bool FontFaceSetWorkerImpl::Initialize(WorkerPrivate* aWorkerPrivate) {
  MOZ_ASSERT(aWorkerPrivate);

  RefPtr<StrongWorkerRef> workerRef =
      StrongWorkerRef::Create(aWorkerPrivate, "FontFaceSetWorkerImpl",
                              [self = RefPtr{this}] { self->Destroy(); });
  if (NS_WARN_IF(!workerRef)) {
    return false;
  }

  {
    RecursiveMutexAutoLock lock(mMutex);
    mWorkerRef = new ThreadSafeWorkerRef(workerRef);
  }

  class InitRunnable final : public WorkerMainThreadRunnable {
   public:
    InitRunnable(WorkerPrivate* aWorkerPrivate, FontFaceSetWorkerImpl* aImpl)
        : WorkerMainThreadRunnable(aWorkerPrivate,
                                   "FontFaceSetWorkerImpl :: Initialize"_ns),
          mImpl(aImpl) {}

   protected:
    ~InitRunnable() override = default;

    bool MainThreadRun() override {
      mImpl->InitializeOnMainThread();
      return true;
    }

    FontFaceSetWorkerImpl* mImpl;
  };

  IgnoredErrorResult rv;
  auto runnable = MakeRefPtr<InitRunnable>(aWorkerPrivate, this);
  runnable->Dispatch(Canceling, rv);
  return !NS_WARN_IF(rv.Failed());
}

void FontFaceSetWorkerImpl::InitializeOnMainThread() {
  MOZ_ASSERT(NS_IsMainThread());
  RecursiveMutexAutoLock lock(mMutex);

  if (!mWorkerRef) {
    return;
  }

  WorkerPrivate* workerPrivate = mWorkerRef->Private();
  nsIPrincipal* principal = workerPrivate->GetPrincipal();
  nsIPrincipal* loadingPrincipal = workerPrivate->GetLoadingPrincipal();
  nsIPrincipal* partitionedPrincipal = workerPrivate->GetPartitionedPrincipal();
  nsIPrincipal* defaultPrincipal = principal ? principal : loadingPrincipal;

  nsLoadFlags loadFlags = workerPrivate->GetLoadFlags();
  uint32_t loadType = 0;

  // Get the top-level worker.
  WorkerPrivate* topWorkerPrivate = workerPrivate;
  WorkerPrivate* parent = workerPrivate->GetParent();
  while (parent) {
    topWorkerPrivate = parent;
    parent = topWorkerPrivate->GetParent();
  }

  // If the top-level worker is a dedicated worker and has a window, and the
  // window has a docshell, the caching behavior of this worker should match
  // that of that docshell. This matches the behaviour from
  // WorkerScriptLoader::LoadScript.
  if (topWorkerPrivate->IsDedicatedWorker()) {
    nsCOMPtr<nsPIDOMWindowInner> window = topWorkerPrivate->GetWindow();
    if (window) {
      nsCOMPtr<nsIDocShell> docShell = window->GetDocShell();
      if (docShell) {
        docShell->GetDefaultLoadFlags(&loadFlags);
        docShell->GetLoadType(&loadType);
      }
    }
  }

  // Record the state of the "bypass cache" flags now. In theory the load type
  // of a docshell could change after the document is loaded, but handling that
  // doesn't seem too important. This matches the behaviour from
  // FontFaceSetDocumentImpl::Initialize.
  mBypassCache =
      ((loadType >> 16) & nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE) ||
      (loadFlags & nsIRequest::LOAD_BYPASS_CACHE);

  // Same for the "private browsing" flag.
  if (defaultPrincipal) {
    mPrivateBrowsing = defaultPrincipal->GetPrivateBrowsingId() > 0;
  }

  mStandardFontLoadPrincipal =
      MakeRefPtr<gfxFontSrcPrincipal>(defaultPrincipal, partitionedPrincipal);

  mURLExtraData =
      new URLExtraData(workerPrivate->GetBaseURI(),
                       workerPrivate->GetReferrerInfo(), defaultPrincipal);
}

void FontFaceSetWorkerImpl::Destroy() {
  RecursiveMutexAutoLock lock(mMutex);

  class DestroyRunnable final : public Runnable {
   public:
    DestroyRunnable(FontFaceSetWorkerImpl* aFontFaceSet,
                    nsTHashtable<nsPtrHashKey<nsFontFaceLoader>>&& aLoaders)
        : Runnable("FontFaceSetWorkerImpl::Destroy"),
          mFontFaceSet(aFontFaceSet),
          mLoaders(std::move(aLoaders)) {}

   protected:
    ~DestroyRunnable() override = default;

    NS_IMETHOD Run() override {
      for (const auto& key : mLoaders.Keys()) {
        key->Cancel();
      }
      return NS_OK;
    }

    // We save a reference to the FontFaceSetWorkerImpl because the loaders
    // contain a non-owning reference to it.
    RefPtr<FontFaceSetWorkerImpl> mFontFaceSet;
    nsTHashtable<nsPtrHashKey<nsFontFaceLoader>> mLoaders;
  };

  if (!mLoaders.IsEmpty() && !NS_IsMainThread()) {
    auto runnable = MakeRefPtr<DestroyRunnable>(this, std::move(mLoaders));
    NS_DispatchToMainThread(runnable);
  }

  mWorkerRef = nullptr;
  FontFaceSetImpl::Destroy();
}

bool FontFaceSetWorkerImpl::IsOnOwningThread() {
  RecursiveMutexAutoLock lock(mMutex);
  if (!mWorkerRef) {
    return false;
  }

  return mWorkerRef->Private()->IsOnCurrentThread();
}

void FontFaceSetWorkerImpl::DispatchToOwningThread(
    const char* aName, std::function<void()>&& aFunc) {
  RecursiveMutexAutoLock lock(mMutex);
  if (!mWorkerRef) {
    return;
  }

  WorkerPrivate* workerPrivate = mWorkerRef->Private();
  if (workerPrivate->IsOnCurrentThread()) {
    NS_DispatchToCurrentThread(
        NS_NewCancelableRunnableFunction(aName, std::move(aFunc)));
    return;
  }

  class FontFaceSetWorkerRunnable final : public WorkerRunnable {
   public:
    FontFaceSetWorkerRunnable(WorkerPrivate* aWorkerPrivate,
                              std::function<void()>&& aFunc)
        : WorkerRunnable(aWorkerPrivate), mFunc(std::move(aFunc)) {}

    bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
      mFunc();
      return true;
    }

   private:
    std::function<void()> mFunc;
  };

  RefPtr<FontFaceSetWorkerRunnable> runnable =
      new FontFaceSetWorkerRunnable(workerPrivate, std::move(aFunc));
  runnable->Dispatch();
}

uint64_t FontFaceSetWorkerImpl::GetInnerWindowID() {
  RecursiveMutexAutoLock lock(mMutex);
  if (!mWorkerRef) {
    return 0;
  }

  return mWorkerRef->Private()->WindowID();
}

void FontFaceSetWorkerImpl::FlushUserFontSet() {
  RecursiveMutexAutoLock lock(mMutex);

  // If there was a change to the mNonRuleFaces array, then there could
  // have been a modification to the user font set.
  bool modified = mNonRuleFacesDirty;
  mNonRuleFacesDirty = false;

  for (size_t i = 0, i_end = mNonRuleFaces.Length(); i < i_end; ++i) {
    InsertNonRuleFontFace(mNonRuleFaces[i].mFontFace, modified);
  }

  // Remove any residual families that have no font entries.
  for (auto it = mFontFamilies.Iter(); !it.Done(); it.Next()) {
    if (!it.Data()->FontListLength()) {
      it.Remove();
    }
  }

  if (modified) {
    IncrementGeneration(true);
    mHasLoadingFontFacesIsDirty = true;
    CheckLoadingStarted();
    CheckLoadingFinished();
  }
}

nsresult FontFaceSetWorkerImpl::StartLoad(gfxUserFontEntry* aUserFontEntry,
                                          uint32_t aSrcIndex) {
  RecursiveMutexAutoLock lock(mMutex);

  if (NS_WARN_IF(!mWorkerRef)) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv;

  nsCOMPtr<nsIStreamLoader> streamLoader;

  const gfxFontFaceSrc& src = aUserFontEntry->SourceAt(aSrcIndex);

  nsCOMPtr<nsILoadGroup> loadGroup(mWorkerRef->Private()->GetLoadGroup());
  nsCOMPtr<nsIChannel> channel;
  rv = FontPreloader::BuildChannel(
      getter_AddRefs(channel), src.mURI->get(), CORS_ANONYMOUS,
      dom::ReferrerPolicy::_empty /* not used */, aUserFontEntry, &src,
      mWorkerRef->Private(), loadGroup, nullptr, false);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<nsFontFaceLoader> fontLoader =
      new nsFontFaceLoader(aUserFontEntry, aSrcIndex, this, channel);

  if (LOG_ENABLED()) {
    nsCOMPtr<nsIURI> referrer =
        src.mReferrerInfo ? src.mReferrerInfo->GetOriginalReferrer() : nullptr;
    LOG("userfonts (%p) download start - font uri: (%s) referrer uri: (%s)\n",
        fontLoader.get(), src.mURI->GetSpecOrDefault().get(),
        referrer ? referrer->GetSpecOrDefault().get() : "");
  }

  rv = NS_NewStreamLoader(getter_AddRefs(streamLoader), fontLoader, fontLoader);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = channel->AsyncOpen(streamLoader);
  if (NS_FAILED(rv)) {
    fontLoader->DropChannel();  // explicitly need to break ref cycle
  }

  mLoaders.PutEntry(fontLoader);

  net::PredictorLearn(src.mURI->get(), mWorkerRef->Private()->GetBaseURI(),
                      nsINetworkPredictor::LEARN_LOAD_SUBRESOURCE, loadGroup);

  if (NS_SUCCEEDED(rv)) {
    fontLoader->StartedLoading(streamLoader);
    // let the font entry remember the loader, in case we need to cancel it
    aUserFontEntry->SetLoader(fontLoader);
  }

  return rv;
}

bool FontFaceSetWorkerImpl::IsFontLoadAllowed(const gfxFontFaceSrc& aSrc) {
  MOZ_ASSERT(aSrc.mSourceType == gfxFontFaceSrc::eSourceType_URL);
  MOZ_ASSERT(NS_IsMainThread());

  RecursiveMutexAutoLock lock(mMutex);

  if (aSrc.mUseOriginPrincipal) {
    return true;
  }

  if (NS_WARN_IF(!mWorkerRef)) {
    return false;
  }

  RefPtr<gfxFontSrcPrincipal> gfxPrincipal =
      aSrc.mURI->InheritsSecurityContext() ? nullptr
                                           : aSrc.LoadPrincipal(*this);

  nsIPrincipal* principal =
      gfxPrincipal ? gfxPrincipal->NodePrincipal() : nullptr;

  nsCOMPtr<nsILoadInfo> secCheckLoadInfo = new net::LoadInfo(
      mWorkerRef->Private()->GetLoadingPrincipal(),  // loading principal
      principal,                                     // triggering principal
      nullptr, nsILoadInfo::SEC_ONLY_FOR_EXPLICIT_CONTENTSEC_CHECK,
      nsIContentPolicy::TYPE_FONT);

  int16_t shouldLoad = nsIContentPolicy::ACCEPT;
  nsresult rv = NS_CheckContentLoadPolicy(aSrc.mURI->get(), secCheckLoadInfo,
                                          ""_ns,  // mime type
                                          &shouldLoad,
                                          nsContentUtils::GetContentPolicy());

  return NS_SUCCEEDED(rv) && NS_CP_ACCEPTED(shouldLoad);
}

nsresult FontFaceSetWorkerImpl::CreateChannelForSyncLoadFontData(
    nsIChannel** aOutChannel, gfxUserFontEntry* aFontToLoad,
    const gfxFontFaceSrc* aFontFaceSrc) {
  RecursiveMutexAutoLock lock(mMutex);
  if (NS_WARN_IF(!mWorkerRef)) {
    return NS_ERROR_FAILURE;
  }

  gfxFontSrcPrincipal* principal = aFontToLoad->GetPrincipal();

  // We only get here for data: loads, so it doesn't really matter whether we
  // use SEC_ALLOW_CROSS_ORIGIN_INHERITS_SEC_CONTEXT or not, to be more
  // restrictive we use SEC_REQUIRE_SAME_ORIGIN_INHERITS_SEC_CONTEXT.
  return NS_NewChannelWithTriggeringPrincipal(
      aOutChannel, aFontFaceSrc->mURI->get(),
      mWorkerRef->Private()->GetLoadingPrincipal(),
      principal ? principal->NodePrincipal() : nullptr,
      nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_INHERITS_SEC_CONTEXT,
      aFontFaceSrc->mUseOriginPrincipal ? nsIContentPolicy::TYPE_UA_FONT
                                        : nsIContentPolicy::TYPE_FONT);
}

nsPresContext* FontFaceSetWorkerImpl::GetPresContext() const { return nullptr; }

TimeStamp FontFaceSetWorkerImpl::GetNavigationStartTimeStamp() {
  RecursiveMutexAutoLock lock(mMutex);
  if (!mWorkerRef) {
    return TimeStamp();
  }

  return mWorkerRef->Private()->CreationTimeStamp();
}

already_AddRefed<URLExtraData> FontFaceSetWorkerImpl::GetURLExtraData() {
  RecursiveMutexAutoLock lock(mMutex);
  return RefPtr{mURLExtraData}.forget();
}

#undef LOG_ENABLED
#undef LOG

}  // namespace mozilla::dom
