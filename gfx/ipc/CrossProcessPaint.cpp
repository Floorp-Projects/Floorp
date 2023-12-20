/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CrossProcessPaint.h"

#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/ContentProcessManager.h"
#include "mozilla/dom/ImageBitmap.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/PWindowGlobalParent.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/dom/WindowGlobalActorsBinding.h"
#include "mozilla/gfx/DrawEventRecorder.h"
#include "mozilla/gfx/InlineTranslator.h"
#include "mozilla/Logging.h"
#include "mozilla/PresShell.h"

#include "gfxPlatform.h"

#include "nsContentUtils.h"
#include "nsIDocShell.h"
#include "nsPresContext.h"

static mozilla::LazyLogModule gCrossProcessPaintLog("CrossProcessPaint");
static mozilla::LazyLogModule gPaintFragmentLog("PaintFragment");

#define CPP_LOG(msg, ...) \
  MOZ_LOG(gCrossProcessPaintLog, LogLevel::Debug, (msg, ##__VA_ARGS__))
#define PF_LOG(msg, ...) \
  MOZ_LOG(gPaintFragmentLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

namespace mozilla {
namespace gfx {

using namespace mozilla::ipc;

/// The minimum scale we allow tabs to be rasterized at.
static const float kMinPaintScale = 0.05f;

/* static */
PaintFragment PaintFragment::Record(dom::BrowsingContext* aBc,
                                    const Maybe<IntRect>& aRect, float aScale,
                                    nscolor aBackgroundColor,
                                    CrossProcessPaintFlags aFlags) {
  nsIDocShell* ds = aBc->GetDocShell();
  if (!ds) {
    PF_LOG("Couldn't find docshell.\n");
    return PaintFragment{};
  }

  RefPtr<nsPresContext> presContext = ds->GetPresContext();
  if (!presContext) {
    PF_LOG("Couldn't find PresContext.\n");
    return PaintFragment{};
  }

  CSSIntRect rect;
  if (!aRect) {
    nsCOMPtr<nsIWidget> widget =
        nsContentUtils::WidgetForDocument(presContext->Document());

    // TODO: Apply some sort of clipping to visible bounds here (Bug 1562720)
    LayoutDeviceIntRect boundsDevice = widget->GetBounds();
    boundsDevice.MoveTo(0, 0);
    nsRect boundsAu = LayoutDevicePixel::ToAppUnits(
        boundsDevice, presContext->AppUnitsPerDevPixel());
    rect = gfx::RoundedOut(CSSPixel::FromAppUnits(boundsAu));
  } else {
    rect = CSSIntRect::FromUnknownRect(*aRect);
  }

  if (rect.IsEmpty()) {
    // TODO: Should we return an empty surface here?
    PF_LOG("Empty rect to paint.\n");
    return PaintFragment{};
  }

  // FIXME: Shouldn't the surface size be in device rather than CSS pixels?
  CSSIntSize surfaceSize = rect.Size();
  surfaceSize.width *= aScale;
  surfaceSize.height *= aScale;

  CPP_LOG(
      "Recording "
      "[browsingContext=%p, "
      "rect=(%d, %d) x (%d, %d), "
      "scale=%f, "
      "color=(%u, %u, %u, %u)]\n",
      aBc, rect.x, rect.y, rect.width, rect.height, aScale,
      NS_GET_R(aBackgroundColor), NS_GET_G(aBackgroundColor),
      NS_GET_B(aBackgroundColor), NS_GET_A(aBackgroundColor));

  // Check for invalid sizes
  if (surfaceSize.width <= 0 || surfaceSize.height <= 0 ||
      !Factory::CheckSurfaceSize(surfaceSize.ToUnknownSize())) {
    PF_LOG("Invalid surface size of (%d x %d).\n", surfaceSize.width,
           surfaceSize.height);
    return PaintFragment{};
  }

  // Flush any pending notifications
  nsContentUtils::FlushLayoutForTree(ds->GetWindow());

  // Initialize the recorder
  SurfaceFormat format = SurfaceFormat::B8G8R8A8;
  RefPtr<DrawTarget> referenceDt = Factory::CreateDrawTarget(
      gfxPlatform::GetPlatform()->GetSoftwareBackend(), IntSize(1, 1), format);

  // TODO: This may OOM crash if the content is complex enough
  RefPtr<DrawEventRecorderMemory> recorder =
      MakeAndAddRef<DrawEventRecorderMemory>(nullptr);
  RefPtr<DrawTarget> dt = Factory::CreateRecordingDrawTarget(
      recorder, referenceDt,
      IntRect(IntPoint(0, 0), surfaceSize.ToUnknownSize()));
  if (!dt || !dt->IsValid()) {
    PF_LOG("Failed to create drawTarget.\n");
    return PaintFragment{};
  }

  RenderDocumentFlags renderDocFlags = RenderDocumentFlags::None;
  if (!(aFlags & CrossProcessPaintFlags::DrawView)) {
    renderDocFlags |= RenderDocumentFlags::IgnoreViewportScrolling |
                      RenderDocumentFlags::DocumentRelative;
    if (aFlags & CrossProcessPaintFlags::ResetScrollPosition) {
      renderDocFlags |= RenderDocumentFlags::ResetViewportScrolling;
    }
  }
  if (aFlags & CrossProcessPaintFlags::UseHighQualityScaling) {
    renderDocFlags |= RenderDocumentFlags::UseHighQualityScaling;
  }

  // Perform the actual rendering
  {
    nsRect r = CSSPixel::ToAppUnits(rect);

    // This matches what nsDeviceContext::CreateRenderingContext does.
    if (presContext->IsPrintingOrPrintPreview()) {
      dt->AddUserData(&sDisablePixelSnapping, (void*)0x1, nullptr);
    }

    gfxContext thebes(dt);
    thebes.SetMatrix(Matrix::Scaling(aScale, aScale));
    thebes.SetCrossProcessPaintScale(aScale);
    RefPtr<PresShell> presShell = presContext->PresShell();
    Unused << presShell->RenderDocument(r, renderDocFlags, aBackgroundColor,
                                        &thebes);
  }

  if (!recorder->mOutputStream.mValid) {
    recorder->DetachResources();
    return PaintFragment{};
  }

  ByteBuf recording = ByteBuf((uint8_t*)recorder->mOutputStream.mData,
                              recorder->mOutputStream.mLength,
                              recorder->mOutputStream.mCapacity);
  recorder->mOutputStream.mData = nullptr;
  recorder->mOutputStream.mLength = 0;
  recorder->mOutputStream.mCapacity = 0;

  PaintFragment fragment{
      surfaceSize.ToUnknownSize(),
      std::move(recording),
      std::move(recorder->TakeDependentSurfaces()),
  };

  recorder->DetachResources();
  return fragment;
}

bool PaintFragment::IsEmpty() const {
  return !mRecording.mData || mRecording.mLen == 0 || mSize == IntSize(0, 0);
}

PaintFragment::PaintFragment(IntSize aSize, ByteBuf&& aRecording,
                             nsTHashSet<uint64_t>&& aDependencies)
    : mSize(aSize),
      mRecording(std::move(aRecording)),
      mDependencies(std::move(aDependencies)) {}

static dom::TabId GetTabId(dom::WindowGlobalParent* aWGP) {
  // There is no unique TabId for a given WindowGlobalParent, as multiple
  // WindowGlobalParents share the same PBrowser actor. However, we only
  // ever queue one paint per PBrowser by just using the current
  // WindowGlobalParent for a PBrowser. So we can interchange TabId and
  // WindowGlobalParent when dealing with resolving surfaces.
  RefPtr<dom::BrowserParent> browserParent = aWGP->GetBrowserParent();
  return browserParent ? browserParent->GetTabId() : dom::TabId(0);
}

/* static */
bool CrossProcessPaint::Start(dom::WindowGlobalParent* aRoot,
                              const dom::DOMRect* aRect, float aScale,
                              nscolor aBackgroundColor,
                              CrossProcessPaintFlags aFlags,
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

  if (rect && rect->IsEmpty()) {
    return false;
  }

  dom::TabId rootId = GetTabId(aRoot);

  RefPtr<CrossProcessPaint> resolver =
      new CrossProcessPaint(aScale, rootId, aFlags);
  RefPtr<CrossProcessPaint::ResolvePromise> promise;
  if (aRoot->IsInProcess()) {
    RefPtr<dom::WindowGlobalChild> childActor = aRoot->GetChildActor();
    if (!childActor) {
      return false;
    }

    // `BrowsingContext()` cannot be nullptr.
    RefPtr<dom::BrowsingContext> bc = childActor->BrowsingContext();

    promise = resolver->Init();
    resolver->mPendingFragments += 1;
    resolver->ReceiveFragment(
        aRoot,
        PaintFragment::Record(bc, rect, aScale, aBackgroundColor, aFlags));
  } else {
    promise = resolver->Init();
    resolver->QueuePaint(aRoot, rect, aBackgroundColor, aFlags);
  }

  promise->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [promise = RefPtr{aPromise}, rootId](ResolvedFragmentMap&& aFragments) {
        RefPtr<RecordedDependentSurface> root = aFragments.Get(rootId);
        CPP_LOG("Resolved all fragments.\n");

        // Create the destination draw target
        RefPtr<DrawTarget> drawTarget =
            gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
                root->mSize, SurfaceFormat::B8G8R8A8);
        if (!drawTarget || !drawTarget->IsValid()) {
          CPP_LOG("Couldn't create (%d x %d) surface for fragment %" PRIu64
                  ".\n",
                  root->mSize.width, root->mSize.height, (uint64_t)rootId);
          promise->MaybeReject(NS_ERROR_FAILURE);
          return;
        }

        // Translate the recording using our child tabs
        {
          InlineTranslator translator(drawTarget, nullptr);
          translator.SetDependentSurfaces(&aFragments);
          if (!translator.TranslateRecording((char*)root->mRecording.mData,
                                             root->mRecording.mLen)) {
            CPP_LOG("Couldn't translate recording for fragment %" PRIu64 ".\n",
                    (uint64_t)rootId);
            promise->MaybeReject(NS_ERROR_FAILURE);
            return;
          }
        }

        RefPtr<SourceSurface> snapshot = drawTarget->Snapshot();
        if (!snapshot) {
          promise->MaybeReject(NS_ERROR_FAILURE);
          return;
        }

        ErrorResult rv;
        RefPtr<dom::ImageBitmap> bitmap =
            dom::ImageBitmap::CreateFromSourceSurface(
                promise->GetParentObject(), snapshot, rv);

        if (!rv.Failed()) {
          CPP_LOG("Success, fulfilling promise.\n");
          promise->MaybeResolve(bitmap);
        } else {
          CPP_LOG("Couldn't create ImageBitmap for SourceSurface.\n");
          promise->MaybeReject(std::move(rv));
        }
      },
      [promise = RefPtr{aPromise}](const nsresult& aRv) {
        promise->MaybeReject(aRv);
      });

  return true;
}

/* static */
RefPtr<CrossProcessPaint::ResolvePromise> CrossProcessPaint::Start(
    nsTHashSet<uint64_t>&& aDependencies) {
  MOZ_ASSERT(!aDependencies.IsEmpty());
  RefPtr<CrossProcessPaint> resolver =
      new CrossProcessPaint(1.0, dom::TabId(0), CrossProcessPaintFlags::None);

  RefPtr<CrossProcessPaint::ResolvePromise> promise = resolver->Init();

  PaintFragment rootFragment;
  rootFragment.mDependencies = std::move(aDependencies);

  resolver->QueueDependencies(rootFragment.mDependencies);
  resolver->mReceivedFragments.InsertOrUpdate(dom::TabId(0),
                                              std::move(rootFragment));

  resolver->MaybeResolve();

  return promise;
}

CrossProcessPaint::CrossProcessPaint(float aScale, dom::TabId aRoot,
                                     CrossProcessPaintFlags aFlags)
    : mRoot{aRoot}, mScale{aScale}, mPendingFragments{0}, mFlags{aFlags} {}

CrossProcessPaint::~CrossProcessPaint() { Clear(NS_ERROR_ABORT); }

void CrossProcessPaint::ReceiveFragment(dom::WindowGlobalParent* aWGP,
                                        PaintFragment&& aFragment) {
  if (IsCleared()) {
    CPP_LOG("Ignoring fragment from %p.\n", aWGP);
    return;
  }

  dom::TabId surfaceId = GetTabId(aWGP);

  MOZ_ASSERT(mPendingFragments > 0);
  MOZ_ASSERT(!mReceivedFragments.Contains(surfaceId));

  // Double check our invariants to protect against a compromised content
  // process
  if (mPendingFragments == 0 || mReceivedFragments.Contains(surfaceId) ||
      aFragment.IsEmpty()) {
    CPP_LOG("Dropping invalid fragment from %p.\n", aWGP);
    LostFragment(aWGP);
    return;
  }

  CPP_LOG("Receiving fragment from %p(%" PRIu64 ").\n", aWGP,
          (uint64_t)surfaceId);

  // Queue paints for child tabs
  QueueDependencies(aFragment.mDependencies);

  mReceivedFragments.InsertOrUpdate(surfaceId, std::move(aFragment));
  mPendingFragments -= 1;

  // Resolve this paint if we have received all pending fragments
  MaybeResolve();
}

void CrossProcessPaint::LostFragment(dom::WindowGlobalParent* aWGP) {
  if (IsCleared()) {
    CPP_LOG("Ignoring lost fragment from %p.\n", aWGP);
    return;
  }

  Clear(NS_ERROR_LOSS_OF_SIGNIFICANT_DATA);
}

void CrossProcessPaint::QueueDependencies(
    const nsTHashSet<uint64_t>& aDependencies) {
  dom::ContentProcessManager* cpm = dom::ContentProcessManager::GetSingleton();
  if (!cpm) {
    CPP_LOG(
        "Skipping QueueDependencies with no"
        " current ContentProcessManager.\n");
    return;
  }
  for (const auto& key : aDependencies) {
    auto dependency = dom::TabId(key);

    // Get the current BrowserParent of the remote browser that was marked
    // as a dependency
    dom::ContentParentId cpId = cpm->GetTabProcessId(dependency);
    RefPtr<dom::BrowserParent> browser =
        cpm->GetBrowserParentByProcessAndTabId(cpId, dependency);
    if (!browser) {
      CPP_LOG("Skipping dependency %" PRIu64
              " with no current BrowserParent.\n",
              (uint64_t)dependency);
      continue;
    }

    // Note that if the remote document is currently being cloned, it's possible
    // that the BrowserParent isn't the one for the cloned document, but the
    // BrowsingContext should be persisted/consistent.
    QueuePaint(browser->GetBrowsingContext());
  }
}

void CrossProcessPaint::QueuePaint(dom::WindowGlobalParent* aWGP,
                                   const Maybe<IntRect>& aRect,
                                   nscolor aBackgroundColor,
                                   CrossProcessPaintFlags aFlags) {
  MOZ_ASSERT(!mReceivedFragments.Contains(GetTabId(aWGP)));

  CPP_LOG("Queueing paint for WindowGlobalParent(%p).\n", aWGP);

  aWGP->DrawSnapshotInternal(this, aRect, mScale, aBackgroundColor,
                             (uint32_t)aFlags);
  mPendingFragments += 1;
}

void CrossProcessPaint::QueuePaint(dom::CanonicalBrowsingContext* aBc) {
  RefPtr<GenericNonExclusivePromise> clonePromise = aBc->GetClonePromise();

  if (!clonePromise) {
    RefPtr<dom::WindowGlobalParent> wgp = aBc->GetCurrentWindowGlobal();
    if (!wgp) {
      CPP_LOG("Skipping BrowsingContext(%p) with no current WGP.\n", aBc);
      return;
    }

    // TODO: Apply some sort of clipping to visible bounds here (Bug 1562720)
    QueuePaint(wgp, Nothing(), NS_RGBA(0, 0, 0, 0), GetFlagsForDependencies());
    return;
  }

  CPP_LOG("Queueing paint for BrowsingContext(%p).\n", aBc);
  // In the case it's still in the process of cloning the remote document, we
  // should defer the snapshot request after the cloning has been finished.
  mPendingFragments += 1;
  clonePromise->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [self = RefPtr{this}, bc = RefPtr{aBc}]() {
        RefPtr<dom::WindowGlobalParent> wgp = bc->GetCurrentWindowGlobal();
        if (!wgp) {
          CPP_LOG("Skipping BrowsingContext(%p) with no current WGP.\n",
                  bc.get());
          return;
        }
        MOZ_ASSERT(!self->mReceivedFragments.Contains(GetTabId(wgp)));

        // TODO: Apply some sort of clipping to visible bounds here (Bug
        // 1562720)
        wgp->DrawSnapshotInternal(self, Nothing(), self->mScale,
                                  NS_RGBA(0, 0, 0, 0),
                                  (uint32_t)self->GetFlagsForDependencies());
      },
      [self = RefPtr{this}]() {
        CPP_LOG(
            "Abort painting for BrowsingContext(%p) because cloning remote "
            "document failed.\n",
            self.get());
        self->Clear(NS_ERROR_FAILURE);
      });
}

void CrossProcessPaint::Clear(nsresult aStatus) {
  mPendingFragments = 0;
  mReceivedFragments.Clear();
  mPromise.RejectIfExists(aStatus, __func__);
}

bool CrossProcessPaint::IsCleared() const { return mPromise.IsEmpty(); }

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
  ResolvedFragmentMap resolved;
  {
    nsresult rv = ResolveInternal(mRoot, &resolved);
    if (NS_FAILED(rv)) {
      CPP_LOG("Couldn't resolve.\n");
      Clear(rv);
      return;
    }
  }

  CPP_LOG("Resolved all fragments.\n");

  mPromise.ResolveIfExists(std::move(resolved), __func__);
  Clear(NS_OK);
}

nsresult CrossProcessPaint::ResolveInternal(dom::TabId aTabId,
                                            ResolvedFragmentMap* aResolved) {
  // We should not have resolved this paint already
  MOZ_ASSERT(!aResolved->GetWeak(aTabId));

  CPP_LOG("Resolving fragment %" PRIu64 ".\n", (uint64_t)aTabId);

  Maybe<PaintFragment> fragment = mReceivedFragments.Extract(aTabId);
  if (!fragment) {
    return NS_ERROR_LOSS_OF_SIGNIFICANT_DATA;
  }

  // Rasterize all the dependencies first so that we can resolve this fragment
  for (const auto& key : fragment->mDependencies) {
    auto dependency = dom::TabId(key);

    nsresult rv = ResolveInternal(dependency, aResolved);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  RefPtr<RecordedDependentSurface> surface = new RecordedDependentSurface{
      fragment->mSize, std::move(fragment->mRecording)};
  aResolved->InsertOrUpdate(aTabId, std::move(surface));
  return NS_OK;
}

}  // namespace gfx
}  // namespace mozilla
