/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CrossProcessPaint.h"

#include "mozilla/dom/ContentProcessManager.h"
#include "mozilla/dom/ImageBitmap.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/gfx/DrawEventRecorder.h"
#include "mozilla/gfx/InlineTranslator.h"
#include "mozilla/PresShell.h"

#include "gfxPlatform.h"

#include "nsContentUtils.h"
#include "nsGlobalWindowInner.h"
#include "nsIDocShell.h"
#include "nsPresContext.h"

#define ENABLE_PAINT_LOG 0
// #define ENABLE_PAINT_LOG 1

#if ENABLE_PAINT_LOG
#  define PF_LOG(...) printf_stderr("PaintFragment: " __VA_ARGS__)
#  define CPP_LOG(...) printf_stderr("CrossProcessPaint: " __VA_ARGS__)
#else
#  define PF_LOG(...)
#  define CPP_LOG(...)
#endif

namespace mozilla {
namespace gfx {

using namespace mozilla::ipc;

/// The minimum scale we allow tabs to be rasterized at.
static const float kMinPaintScale = 0.05f;

/* static */
PaintFragment PaintFragment::Record(nsIDocShell* aDocShell,
                                    const IntRect& aRect, float aScale,
                                    nscolor aBackgroundColor) {
  IntSize surfaceSize = aRect.Size();
  surfaceSize.width *= aScale;
  surfaceSize.height *= aScale;

  CPP_LOG(
      "Recording "
      "[docshell=%p, "
      "rect=(%d, %d) x (%d, %d), "
      "scale=%f, "
      "color=(%u, %u, %u, %u)]\n",
      aDocShell, aRect.x, aRect.y, aRect.width, aRect.height, aScale,
      NS_GET_R(aBackgroundColor), NS_GET_G(aBackgroundColor),
      NS_GET_B(aBackgroundColor), NS_GET_A(aBackgroundColor));

  // Check for invalid sizes
  if (surfaceSize.width <= 0 || surfaceSize.height <= 0 ||
      !Factory::CheckSurfaceSize(surfaceSize)) {
    PF_LOG("Invalid surface size of (%d x %d).\n", surfaceSize.width,
           surfaceSize.height);
    return PaintFragment{};
  }

  // Flush any pending notifications
  nsContentUtils::FlushLayoutForTree(aDocShell->GetWindow());

  // Grab the presentation shell to render
  RefPtr<nsPresContext> presContext;
  if (aDocShell) {
    presContext = aDocShell->GetPresContext();
  }
  if (!presContext) {
    PF_LOG("Couldn't find PresContext.\n");
    return PaintFragment{};
  }

  // Initialize the recorder
  SurfaceFormat format = SurfaceFormat::B8G8R8A8;
  RefPtr<DrawTarget> referenceDt = Factory::CreateDrawTarget(
      gfxPlatform::GetPlatform()->GetSoftwareBackend(), IntSize(1, 1), format);

  // TODO: This may OOM crash if the content is complex enough
  RefPtr<DrawEventRecorderMemory> recorder =
      MakeAndAddRef<DrawEventRecorderMemory>(nullptr);
  RefPtr<DrawTarget> dt =
      Factory::CreateRecordingDrawTarget(recorder, referenceDt, surfaceSize);

  // Perform the actual rendering
  {
    nsRect r(nsPresContext::CSSPixelsToAppUnits(aRect.x),
             nsPresContext::CSSPixelsToAppUnits(aRect.y),
             nsPresContext::CSSPixelsToAppUnits(aRect.width),
             nsPresContext::CSSPixelsToAppUnits(aRect.height));

    RefPtr<gfxContext> thebes = gfxContext::CreateOrNull(dt);
    thebes->SetMatrix(Matrix::Scaling(aScale, aScale));
    RefPtr<PresShell> presShell = presContext->PresShell();
    Unused << presShell->RenderDocument(r, RenderDocumentFlags::None,
                                        aBackgroundColor, thebes);
  }

  ByteBuf recording = ByteBuf((uint8_t*)recorder->mOutputStream.mData,
                              recorder->mOutputStream.mLength,
                              recorder->mOutputStream.mCapacity);
  recorder->mOutputStream.mData = nullptr;
  recorder->mOutputStream.mLength = 0;
  recorder->mOutputStream.mCapacity = 0;

  return PaintFragment{
      surfaceSize,
      std::move(recording),
      std::move(recorder->TakeDependentSurfaces()),
  };
}

bool PaintFragment::IsEmpty() const {
  return !mRecording.mData || mRecording.mLen == 0 || mSize == IntSize(0, 0);
}

PaintFragment::PaintFragment(IntSize aSize, ByteBuf&& aRecording,
                             nsTHashtable<nsUint64HashKey>&& aDependencies)
    : mSize(aSize),
      mRecording(std::move(aRecording)),
      mDependencies(std::move(aDependencies)) {}

/* static */
void CrossProcessPaint::StartLocal(nsIDocShell* aRoot, const IntRect& aRect,
                                   float aScale, nscolor aBackgroundColor,
                                   dom::Promise* aPromise) {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  aScale = std::max(aScale, kMinPaintScale);

  CPP_LOG(
      "Starting local paint. "
      "[docshell=%p, "
      "rect=(%d, %d) x (%d, %d), "
      "scale=%f, "
      "color=(%u, %u, %u, %u)]\n",
      aRoot, aRect.x, aRect.y, aRect.width, aRect.height, aScale,
      NS_GET_R(aBackgroundColor), NS_GET_G(aBackgroundColor),
      NS_GET_B(aBackgroundColor), NS_GET_A(aBackgroundColor));

  RefPtr<CrossProcessPaint> resolver =
      new CrossProcessPaint(aPromise, aScale, aBackgroundColor, dom::TabId(0));
  resolver->ReceiveFragment(
      dom::TabId(0),
      PaintFragment::Record(aRoot, aRect, aScale, aBackgroundColor));
}

/* static */
void CrossProcessPaint::StartRemote(dom::TabId aRoot, const IntRect& aRect,
                                    float aScale, nscolor aBackgroundColor,
                                    dom::Promise* aPromise) {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  aScale = std::max(aScale, kMinPaintScale);

  CPP_LOG(
      "Starting remote paint. "
      "[tab=%llu, "
      "rect=(%d, %d) x (%d, %d), "
      "scale=%f, "
      "color=(%u, %u, %u, %u)]\n",
      (uint64_t)aRoot, aRect.x, aRect.y, aRect.width, aRect.height, aScale,
      NS_GET_R(aBackgroundColor), NS_GET_G(aBackgroundColor),
      NS_GET_B(aBackgroundColor), NS_GET_A(aBackgroundColor));

  RefPtr<CrossProcessPaint> resolver =
      new CrossProcessPaint(aPromise, aScale, aBackgroundColor, aRoot);
  resolver->QueueRootPaint(aRoot, aRect, aScale, aBackgroundColor);
}

CrossProcessPaint::CrossProcessPaint(dom::Promise* aPromise, float aScale,
                                     nscolor aBackgroundColor,
                                     dom::TabId aRootId)
    : mPromise{aPromise},
      mRootId{aRootId},
      mScale{aScale},
      mBackgroundColor{aBackgroundColor},
      mPendingFragments{1} {}

CrossProcessPaint::~CrossProcessPaint() {}

void CrossProcessPaint::ReceiveFragment(dom::TabId aId,
                                        PaintFragment&& aFragment) {
  if (IsCleared()) {
    CPP_LOG("Ignoring fragment from %llu.\n", (uint64_t)aId);
    return;
  }

  MOZ_ASSERT(mPendingFragments > 0);
  MOZ_ASSERT(!mReceivedFragments.GetValue(aId));
  MOZ_ASSERT(!aFragment.IsEmpty());

  // Double check our invariants to protect against a compromised content
  // process
  if (mPendingFragments == 0 || mReceivedFragments.GetValue(aId) ||
      aFragment.IsEmpty()) {
    CPP_LOG("Dropping invalid fragment from %llu.\n", (uint64_t)aId);
    LostFragment(aId);
    return;
  }

  CPP_LOG("Receiving fragment from %llu.\n", (uint64_t)aId);

  // Queue paints for child tabs
  for (auto iter = aFragment.mDependencies.Iter(); !iter.Done(); iter.Next()) {
    auto dependency = iter.Get()->GetKey();
    QueueSubPaint(dom::TabId(dependency));
  }

  mReceivedFragments.Put(aId, std::move(aFragment));
  mPendingFragments -= 1;

  // Resolve this paint if we have received all pending fragments
  MaybeResolve();
}

void CrossProcessPaint::LostFragment(dom::TabId aId) {
  if (IsCleared()) {
    CPP_LOG("Ignoring lost fragment from %llu.\n", (uint64_t)aId);
    return;
  }

  mPromise->MaybeReject(NS_ERROR_FAILURE);
  Clear();
}

void CrossProcessPaint::QueueRootPaint(dom::TabId aId, const IntRect& aRect,
                                       float aScale, nscolor aBackgroundColor) {
  MOZ_ASSERT(!mReceivedFragments.GetValue(aId));
  MOZ_ASSERT(mPendingFragments == 1);

  CPP_LOG("Queueing root paint for %llu.\n", (uint64_t)aId);

  dom::ContentProcessManager* cpm = dom::ContentProcessManager::GetSingleton();

  dom::ContentParentId cpId = cpm->GetTabProcessId(aId);
  RefPtr<dom::BrowserParent> tab =
      cpm->GetBrowserParentByProcessAndTabId(cpId, aId);
  tab->RequestRootPaint(this, aRect, aScale, aBackgroundColor);

  // This will always be the first paint, so the constructor will already have
  // incremented one pending fragment
}

void CrossProcessPaint::QueueSubPaint(dom::TabId aId) {
  MOZ_ASSERT(!mReceivedFragments.GetValue((uint64_t)aId));

  CPP_LOG("Queueing sub paint for %llu.\n", (uint64_t)aId);

  dom::ContentProcessManager* cpm = dom::ContentProcessManager::GetSingleton();

  dom::ContentParentId cpId = cpm->GetTabProcessId(aId);
  RefPtr<dom::BrowserParent> tab =
      cpm->GetBrowserParentByProcessAndTabId(cpId, aId);
  tab->RequestSubPaint(this, mScale, mBackgroundColor);

  mPendingFragments += 1;
}

void CrossProcessPaint::Clear() {
  mPromise = nullptr;
  mPendingFragments = 0;
  mReceivedFragments.Clear();
}

bool CrossProcessPaint::IsCleared() const { return !mPromise; }

void CrossProcessPaint::MaybeResolve() {
  // Don't do anything if we aren't ready, experienced an error, or already
  // resolved this paint
  if (IsCleared() || mPendingFragments > 0) {
    CPP_LOG("Not ready to resolve yet, have %u fragments left.\n",
            mPendingFragments);
    return;
  }

  CPP_LOG("Starting to resolve fragments.\n");

  // Resolve the paint fragments from the bottom up
  ResolvedSurfaceMap resolved;
  if (!ResolveInternal(mRootId, &resolved)) {
    CPP_LOG("Couldn't resolve.\n");

    mPromise->MaybeReject(NS_ERROR_FAILURE);
    Clear();
    return;
  }

  // Grab the result from the resolved table
  RefPtr<SourceSurface> root = resolved.Get(mRootId);
  CPP_LOG("Resolved all fragments.\n");

  ErrorResult rv;
  RefPtr<dom::ImageBitmap> bitmap = dom::ImageBitmap::CreateFromSourceSurface(
      mPromise->GetParentObject(), root, rv);

  if (!rv.Failed()) {
    CPP_LOG("Success, fulfilling promise.\n");
    mPromise->MaybeResolve(bitmap);
  } else {
    CPP_LOG("Couldn't create ImageBitmap for SourceSurface.\n");
    mPromise->MaybeReject(rv);
  }
  Clear();
}

bool CrossProcessPaint::ResolveInternal(dom::TabId aId,
                                        ResolvedSurfaceMap* aResolved) {
  // We should not have resolved this paint already
  MOZ_ASSERT(!aResolved->GetWeak(aId));

  CPP_LOG("Resolving fragment %llu.\n", (uint64_t)aId);

  Maybe<PaintFragment> fragment = mReceivedFragments.GetAndRemove(aId);

  // Rasterize all the dependencies first so that we can resolve this fragment
  for (auto iter = fragment->mDependencies.Iter(); !iter.Done(); iter.Next()) {
    auto dependency = iter.Get()->GetKey();
    if (!ResolveInternal(dom::TabId(dependency), aResolved)) {
      return false;
    }
  }

  // Create the destination draw target
  RefPtr<DrawTarget> drawTarget =
      gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
          fragment->mSize, SurfaceFormat::B8G8R8A8);
  if (!drawTarget || !drawTarget->IsValid()) {
    CPP_LOG("Couldn't create (%d x %d) surface for fragment %llu.\n",
            fragment->mSize.width, fragment->mSize.height, (uint64_t)aId);
    return false;
  }

  // Translate the recording using our child tabs
  {
    InlineTranslator translator(drawTarget, nullptr);
    translator.SetExternalSurfaces(aResolved);
    if (!translator.TranslateRecording((char*)fragment->mRecording.mData,
                                       fragment->mRecording.mLen)) {
      CPP_LOG("Couldn't translate recording for fragment %llu.\n",
              (uint64_t)aId);
      return false;
    }
  }

  RefPtr<SourceSurface> snapshot = drawTarget->Snapshot();
  if (!snapshot) {
    CPP_LOG("Couldn't get snapshot for fragment %llu.\n", (uint64_t)aId);
    return false;
  }

  // We are done with the resolved images of our dependencies, let's remove
  // them
  for (auto iter = fragment->mDependencies.Iter(); !iter.Done(); iter.Next()) {
    auto dependency = iter.Get()->GetKey();
    aResolved->Remove(dependency);
  }

  aResolved->Put(aId, snapshot);
  return true;
}

}  // namespace gfx
}  // namespace mozilla
