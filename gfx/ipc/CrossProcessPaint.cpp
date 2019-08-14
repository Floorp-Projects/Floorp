/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CrossProcessPaint.h"

#include "mozilla/dom/ContentProcessManager.h"
#include "mozilla/dom/ImageBitmap.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/PWindowGlobalParent.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/dom/WindowGlobalChild.h"
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
                                    const Maybe<IntRect>& aRect, float aScale,
                                    nscolor aBackgroundColor) {
  if (!aDocShell) {
    PF_LOG("Couldn't find DocShell.\n");
    return PaintFragment{};
  }

  RefPtr<nsPresContext> presContext = aDocShell->GetPresContext();
  if (!presContext) {
    PF_LOG("Couldn't find PresContext.\n");
    return PaintFragment{};
  }

  IntRect rect;
  if (!aRect) {
    nsCOMPtr<nsIWidget> widget =
        nsContentUtils::WidgetForDocument(aDocShell->GetDocument());

    // TODO: Apply some sort of clipping to visible bounds here (Bug 1562720)
    LayoutDeviceIntRect boundsDevice = widget->GetBounds();
    boundsDevice.MoveTo(0, 0);
    nsRect boundsAu = LayoutDevicePixel::ToAppUnits(
        boundsDevice, presContext->AppUnitsPerDevPixel());
    rect = gfx::RoundedOut(CSSPixel::FromAppUnits(boundsAu).ToUnknownRect());
  } else {
    rect = *aRect;
  }

  if (rect.IsEmpty()) {
    // TODO: Should we return an empty surface here?
    PF_LOG("Empty rect to paint.\n");
    return PaintFragment{};
  }

  IntSize surfaceSize = rect.Size();
  surfaceSize.width *= aScale;
  surfaceSize.height *= aScale;

  CPP_LOG(
      "Recording "
      "[docshell=%p, "
      "rect=(%d, %d) x (%d, %d), "
      "scale=%f, "
      "color=(%u, %u, %u, %u)]\n",
      aDocShell, rect.x, rect.y, rect.width, rect.height, aScale,
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

  // Initialize the recorder
  SurfaceFormat format = SurfaceFormat::B8G8R8A8;
  RefPtr<DrawTarget> referenceDt = Factory::CreateDrawTarget(
      gfxPlatform::GetPlatform()->GetSoftwareBackend(), IntSize(1, 1), format);

  // TODO: This may OOM crash if the content is complex enough
  RefPtr<DrawEventRecorderMemory> recorder =
      MakeAndAddRef<DrawEventRecorderMemory>(nullptr);
  RefPtr<DrawTarget> dt = Factory::CreateRecordingDrawTarget(
      recorder, referenceDt, IntRect(IntPoint(0, 0), surfaceSize));

  // Perform the actual rendering
  {
    nsRect r(nsPresContext::CSSPixelsToAppUnits(rect.x),
             nsPresContext::CSSPixelsToAppUnits(rect.y),
             nsPresContext::CSSPixelsToAppUnits(rect.width),
             nsPresContext::CSSPixelsToAppUnits(rect.height));

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
bool CrossProcessPaint::Start(dom::WindowGlobalParent* aRoot,
                              const dom::DOMRect* aRect, float aScale,
                              nscolor aBackgroundColor,
                              dom::Promise* aPromise) {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  aScale = std::max(aScale, kMinPaintScale);

  CPP_LOG(
      "Starting paint. "
      "[wgp=%p, "
      "scale=%f, "
      "color=(%u, %u, %u, %u)]\n",
      aRoot, aScale, NS_GET_R(aBackgroundColor), NS_GET_G(aBackgroundColor),
      NS_GET_B(aBackgroundColor), NS_GET_A(aBackgroundColor));

  Maybe<IntRect> rect;
  if (aRect) {
    rect =
        Some(IntRect::RoundOut((float)aRect->X(), (float)aRect->Y(),
                               (float)aRect->Width(), (float)aRect->Height()));
  }

  RefPtr<CrossProcessPaint> resolver =
      new CrossProcessPaint(aPromise, aScale, aRoot);

  if (aRoot->IsInProcess()) {
    RefPtr<dom::WindowGlobalChild> childActor = aRoot->GetChildActor();
    if (!childActor) {
      return false;
    }

    // `BrowsingContext()` cannot be nullptr.
    nsCOMPtr<nsIDocShell> docShell =
        childActor->BrowsingContext()->GetDocShell();
    if (!docShell) {
      return false;
    }

    resolver->mPendingFragments += 1;
    resolver->ReceiveFragment(
        aRoot, PaintFragment::Record(docShell, rect, aScale, aBackgroundColor));
  } else {
    resolver->QueuePaint(aRoot, rect, aBackgroundColor);
  }
  return true;
}

CrossProcessPaint::CrossProcessPaint(dom::Promise* aPromise, float aScale,
                                     dom::WindowGlobalParent* aRoot)
    : mPromise{aPromise}, mRoot{aRoot}, mScale{aScale}, mPendingFragments{0} {}

CrossProcessPaint::~CrossProcessPaint() {}

static dom::TabId GetTabId(dom::WindowGlobalParent* aWGP) {
  // There is no unique TabId for a given WindowGlobalParent, as multiple
  // WindowGlobalParents share the same PBrowser actor. However, we only
  // ever queue one paint per PBrowser by just using the current
  // WindowGlobalParent for a PBrowser. So we can interchange TabId and
  // WindowGlobalParent when dealing with resolving surfaces.
  RefPtr<dom::BrowserParent> browserParent = aWGP->GetBrowserParent();
  return browserParent ? browserParent->GetTabId() : dom::TabId(0);
}

void CrossProcessPaint::ReceiveFragment(dom::WindowGlobalParent* aWGP,
                                        PaintFragment&& aFragment) {
  if (IsCleared()) {
    CPP_LOG("Ignoring fragment from %p.\n", aWGP);
    return;
  }

  dom::TabId surfaceId = GetTabId(aWGP);

  MOZ_ASSERT(mPendingFragments > 0);
  MOZ_ASSERT(!mReceivedFragments.GetValue(surfaceId));
  MOZ_ASSERT(!aFragment.IsEmpty());

  // Double check our invariants to protect against a compromised content
  // process
  if (mPendingFragments == 0 || mReceivedFragments.GetValue(surfaceId) ||
      aFragment.IsEmpty()) {
    CPP_LOG("Dropping invalid fragment from %p.\n", aWGP);
    LostFragment(aWGP);
    return;
  }

  CPP_LOG("Receiving fragment from %p(%llu).\n", aWGP, (uint64_t)surfaceId);

  // Queue paints for child tabs
  for (auto iter = aFragment.mDependencies.Iter(); !iter.Done(); iter.Next()) {
    auto dependency = dom::TabId(iter.Get()->GetKey());

    // Get the current WindowGlobalParent of the remote browser that was marked
    // as a dependency
    dom::ContentProcessManager* cpm =
        dom::ContentProcessManager::GetSingleton();
    dom::ContentParentId cpId = cpm->GetTabProcessId(dependency);
    RefPtr<dom::BrowserParent> browser =
        cpm->GetBrowserParentByProcessAndTabId(cpId, dependency);
    RefPtr<dom::WindowGlobalParent> wgp =
        browser->GetBrowsingContext()->GetCurrentWindowGlobal();

    if (!wgp) {
      CPP_LOG("Skipping dependency %llu with no current WGP.\n",
              (uint64_t)dependency);
      continue;
    }

    // TODO: Apply some sort of clipping to visible bounds here (Bug 1562720)
    QueuePaint(wgp, Nothing());
  }

  mReceivedFragments.Put(surfaceId, std::move(aFragment));
  mPendingFragments -= 1;

  // Resolve this paint if we have received all pending fragments
  MaybeResolve();
}

void CrossProcessPaint::LostFragment(dom::WindowGlobalParent* aWGP) {
  if (IsCleared()) {
    CPP_LOG("Ignoring lost fragment from %p.\n", aWGP);
    return;
  }

  mPromise->MaybeReject(NS_ERROR_LOSS_OF_SIGNIFICANT_DATA);
  Clear();
}

void CrossProcessPaint::QueuePaint(dom::WindowGlobalParent* aWGP,
                                   const Maybe<IntRect>& aRect,
                                   nscolor aBackgroundColor) {
  MOZ_ASSERT(!mReceivedFragments.GetValue(GetTabId(aWGP)));

  CPP_LOG("Queueing paint for %p.\n", aWGP);

  // TODO - Don't apply the background color to all paints (Bug 1562722)
  aWGP->DrawSnapshotInternal(this, aRect, mScale, aBackgroundColor);
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
  {
    nsresult rv = ResolveInternal(GetTabId(mRoot), &resolved);
    if (NS_FAILED(rv)) {
      CPP_LOG("Couldn't resolve.\n");

      mPromise->MaybeReject(rv);
      Clear();
      return;
    }
  }

  // Grab the result from the resolved table.
  RefPtr<SourceSurface> root = resolved.Get(GetTabId(mRoot));
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

nsresult CrossProcessPaint::ResolveInternal(dom::TabId aTabId,
                                            ResolvedSurfaceMap* aResolved) {
  // We should not have resolved this paint already
  MOZ_ASSERT(!aResolved->GetWeak(aTabId));

  CPP_LOG("Resolving fragment %llu.\n", (uint64_t)aTabId);

  Maybe<PaintFragment> fragment = mReceivedFragments.GetAndRemove(aTabId);
  if (!fragment) {
    return NS_ERROR_LOSS_OF_SIGNIFICANT_DATA;
  }

  // Rasterize all the dependencies first so that we can resolve this fragment
  for (auto iter = fragment->mDependencies.Iter(); !iter.Done(); iter.Next()) {
    auto dependency = dom::TabId(iter.Get()->GetKey());

    nsresult rv = ResolveInternal(dependency, aResolved);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  // Create the destination draw target
  RefPtr<DrawTarget> drawTarget =
      gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
          fragment->mSize, SurfaceFormat::B8G8R8A8);
  if (!drawTarget || !drawTarget->IsValid()) {
    CPP_LOG("Couldn't create (%d x %d) surface for fragment %llu.\n",
            fragment->mSize.width, fragment->mSize.height, (uint64_t)aTabId);
    return NS_ERROR_FAILURE;
  }

  // Translate the recording using our child tabs
  {
    InlineTranslator translator(drawTarget, nullptr);
    translator.SetExternalSurfaces(aResolved);
    if (!translator.TranslateRecording((char*)fragment->mRecording.mData,
                                       fragment->mRecording.mLen)) {
      CPP_LOG("Couldn't translate recording for fragment %llu.\n",
              (uint64_t)aTabId);
      return NS_ERROR_FAILURE;
    }
  }

  RefPtr<SourceSurface> snapshot = drawTarget->Snapshot();
  if (!snapshot) {
    CPP_LOG("Couldn't get snapshot for fragment %llu.\n", (uint64_t)aTabId);
    return NS_ERROR_FAILURE;
  }

  // We are done with the resolved images of our dependencies, let's remove
  // them
  for (auto iter = fragment->mDependencies.Iter(); !iter.Done(); iter.Next()) {
    auto dependency = iter.Get()->GetKey();
    aResolved->Remove(dependency);
  }

  aResolved->Put(aTabId, snapshot);
  return NS_OK;
}

}  // namespace gfx
}  // namespace mozilla
