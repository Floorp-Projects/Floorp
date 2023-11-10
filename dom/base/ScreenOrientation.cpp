/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScreenOrientation.h"
#include "nsIDocShell.h"
#include "mozilla/dom/Document.h"
#include "nsGlobalWindowInner.h"
#include "nsSandboxFlags.h"
#include "nsScreen.h"

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/Hal.h"
#include "mozilla/Preferences.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/StaticPrefs_browser.h"
#include "nsContentUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_INHERITED(ScreenOrientation, DOMEventTargetHelper,
                                   mScreen);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ScreenOrientation)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(ScreenOrientation, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(ScreenOrientation, DOMEventTargetHelper)

static OrientationType InternalOrientationToType(
    hal::ScreenOrientation aOrientation) {
  switch (aOrientation) {
    case hal::ScreenOrientation::PortraitPrimary:
      return OrientationType::Portrait_primary;
    case hal::ScreenOrientation::PortraitSecondary:
      return OrientationType::Portrait_secondary;
    case hal::ScreenOrientation::LandscapePrimary:
      return OrientationType::Landscape_primary;
    case hal::ScreenOrientation::LandscapeSecondary:
      return OrientationType::Landscape_secondary;
    default:
      MOZ_CRASH("Bad aOrientation value");
  }
}

static hal::ScreenOrientation OrientationTypeToInternal(
    OrientationType aOrientation) {
  switch (aOrientation) {
    case OrientationType::Portrait_primary:
      return hal::ScreenOrientation::PortraitPrimary;
    case OrientationType::Portrait_secondary:
      return hal::ScreenOrientation::PortraitSecondary;
    case OrientationType::Landscape_primary:
      return hal::ScreenOrientation::LandscapePrimary;
    case OrientationType::Landscape_secondary:
      return hal::ScreenOrientation::LandscapeSecondary;
    default:
      MOZ_CRASH("Bad aOrientation value");
  }
}

ScreenOrientation::ScreenOrientation(nsPIDOMWindowInner* aWindow,
                                     nsScreen* aScreen)
    : DOMEventTargetHelper(aWindow), mScreen(aScreen) {
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aScreen);

  mAngle = aScreen->GetOrientationAngle();
  mType = InternalOrientationToType(aScreen->GetOrientationType());

  Document* doc = GetResponsibleDocument();
  BrowsingContext* bc = doc ? doc->GetBrowsingContext() : nullptr;
  if (bc && !bc->IsDiscarded() && !bc->InRDMPane()) {
    MOZ_ALWAYS_SUCCEEDS(bc->SetCurrentOrientation(mType, mAngle));
  }
}

ScreenOrientation::~ScreenOrientation() {
  if (mTriedToLockDeviceOrientation) {
    UnlockDeviceOrientation();
  } else {
    CleanupFullscreenListener();
  }

  MOZ_ASSERT(!mFullscreenListener);
}

class ScreenOrientation::FullscreenEventListener final
    : public nsIDOMEventListener {
  ~FullscreenEventListener() = default;

 public:
  FullscreenEventListener() = default;

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER
};

class ScreenOrientation::VisibleEventListener final
    : public nsIDOMEventListener {
  ~VisibleEventListener() = default;

 public:
  VisibleEventListener() = default;

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER
};

class ScreenOrientation::LockOrientationTask final : public nsIRunnable {
  ~LockOrientationTask();

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  LockOrientationTask(ScreenOrientation* aScreenOrientation, Promise* aPromise,
                      hal::ScreenOrientation aOrientationLock,
                      Document* aDocument, bool aIsFullscreen);

 protected:
  bool OrientationLockContains(OrientationType aOrientationType);

  RefPtr<ScreenOrientation> mScreenOrientation;
  RefPtr<Promise> mPromise;
  hal::ScreenOrientation mOrientationLock;
  WeakPtr<Document> mDocument;
  bool mIsFullscreen;
};

NS_IMPL_ISUPPORTS(ScreenOrientation::LockOrientationTask, nsIRunnable)

ScreenOrientation::LockOrientationTask::LockOrientationTask(
    ScreenOrientation* aScreenOrientation, Promise* aPromise,
    hal::ScreenOrientation aOrientationLock, Document* aDocument,
    bool aIsFullscreen)
    : mScreenOrientation(aScreenOrientation),
      mPromise(aPromise),
      mOrientationLock(aOrientationLock),
      mDocument(aDocument),
      mIsFullscreen(aIsFullscreen) {
  MOZ_ASSERT(aScreenOrientation);
  MOZ_ASSERT(aPromise);
  MOZ_ASSERT(aDocument);
}

ScreenOrientation::LockOrientationTask::~LockOrientationTask() = default;

bool ScreenOrientation::LockOrientationTask::OrientationLockContains(
    OrientationType aOrientationType) {
  return bool(mOrientationLock & OrientationTypeToInternal(aOrientationType));
}

NS_IMETHODIMP
ScreenOrientation::LockOrientationTask::Run() {
  if (!mPromise) {
    return NS_OK;
  }

  if (!mDocument) {
    mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindowInner> owner = mScreenOrientation->GetOwner();
  if (!owner || !owner->IsFullyActive()) {
    mPromise->MaybeRejectWithAbortError("The document is not fully active.");
    return NS_OK;
  }

  // Step to lock the orientation as defined in the spec.
  if (mDocument->GetOrientationPendingPromise() != mPromise) {
    // The document's pending promise is not associated with this task
    // to lock orientation. There has since been another request to
    // lock orientation, thus we don't need to do anything. Old promise
    // should be been rejected.
    return NS_OK;
  }

  if (mDocument->Hidden()) {
    // Active orientation lock is not the document's orientation lock.
    mPromise->MaybeResolveWithUndefined();
    mDocument->ClearOrientationPendingPromise();
    return NS_OK;
  }

  if (mOrientationLock == hal::ScreenOrientation::None) {
    mScreenOrientation->UnlockDeviceOrientation();
    mPromise->MaybeResolveWithUndefined();
    mDocument->ClearOrientationPendingPromise();
    return NS_OK;
  }

  BrowsingContext* bc = mDocument->GetBrowsingContext();
  if (!bc) {
    mPromise->MaybeResolveWithUndefined();
    mDocument->ClearOrientationPendingPromise();
    return NS_OK;
  }

  OrientationType previousOrientationType = bc->GetCurrentOrientationType();
  mScreenOrientation->LockDeviceOrientation(mOrientationLock, mIsFullscreen)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [self = RefPtr{this}, previousOrientationType](
              const GenericNonExclusivePromise::ResolveOrRejectValue& aValue) {
            if (self->mPromise->State() != Promise::PromiseState::Pending) {
              // mPromise is already resolved or rejected by
              // DispatchChangeEventAndResolvePromise() or
              // AbortInProcessOrientationPromises().
              return;
            }

            if (!self->mDocument) {
              self->mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
              return;
            }

            if (self->mDocument->GetOrientationPendingPromise() !=
                self->mPromise) {
              // mPromise is old promise now and document has new promise by
              // later `orientation.lock` call. Old promise is already rejected
              // by AbortInProcessOrientationPromises()
              return;
            }
            if (aValue.IsResolve()) {
              // LockDeviceOrientation won't change orientation, so change
              // event isn't fired.
              if (BrowsingContext* bc = self->mDocument->GetBrowsingContext()) {
                OrientationType currentOrientationType =
                    bc->GetCurrentOrientationType();
                if ((previousOrientationType == currentOrientationType &&
                     self->OrientationLockContains(currentOrientationType)) ||
                    (self->mOrientationLock ==
                         hal::ScreenOrientation::Default &&
                     bc->GetCurrentOrientationAngle() == 0)) {
                  // Orientation lock will not cause an orientation change, so
                  // we need to manually resolve the promise here.
                  self->mPromise->MaybeResolveWithUndefined();
                  self->mDocument->ClearOrientationPendingPromise();
                }
              }
              return;
            }
            self->mPromise->MaybeReject(aValue.RejectValue());
            self->mDocument->ClearOrientationPendingPromise();
          });

  return NS_OK;
}

already_AddRefed<Promise> ScreenOrientation::Lock(
    OrientationLockType aOrientation, ErrorResult& aRv) {
  hal::ScreenOrientation orientation = hal::ScreenOrientation::None;

  switch (aOrientation) {
    case OrientationLockType::Any:
      orientation = hal::ScreenOrientation::PortraitPrimary |
                    hal::ScreenOrientation::PortraitSecondary |
                    hal::ScreenOrientation::LandscapePrimary |
                    hal::ScreenOrientation::LandscapeSecondary;
      break;
    case OrientationLockType::Natural:
      orientation |= hal::ScreenOrientation::Default;
      break;
    case OrientationLockType::Landscape:
      orientation = hal::ScreenOrientation::LandscapePrimary |
                    hal::ScreenOrientation::LandscapeSecondary;
      break;
    case OrientationLockType::Portrait:
      orientation = hal::ScreenOrientation::PortraitPrimary |
                    hal::ScreenOrientation::PortraitSecondary;
      break;
    case OrientationLockType::Portrait_primary:
      orientation = hal::ScreenOrientation::PortraitPrimary;
      break;
    case OrientationLockType::Portrait_secondary:
      orientation = hal::ScreenOrientation::PortraitSecondary;
      break;
    case OrientationLockType::Landscape_primary:
      orientation = hal::ScreenOrientation::LandscapePrimary;
      break;
    case OrientationLockType::Landscape_secondary:
      orientation = hal::ScreenOrientation::LandscapeSecondary;
      break;
    default:
      NS_WARNING("Unexpected orientation type");
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
  }

  return LockInternal(orientation, aRv);
}

// Wait for document entered fullscreen.
class FullscreenWaitListener final : public nsIDOMEventListener {
 private:
  ~FullscreenWaitListener() = default;

 public:
  FullscreenWaitListener() = default;

  NS_DECL_ISUPPORTS

  // When we have pending fullscreen request, we will wait for the completion or
  // cancel of it.
  RefPtr<GenericPromise> Promise(Document* aDocument) {
    if (aDocument->Fullscreen()) {
      return GenericPromise::CreateAndResolve(true, __func__);
    }

    if (NS_FAILED(InstallEventListener(aDocument))) {
      return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
    }

    MOZ_ASSERT(aDocument->HasPendingFullscreenRequests());
    return mHolder.Ensure(__func__);
  }

  NS_IMETHODIMP HandleEvent(Event* aEvent) override {
    nsAutoString eventType;
    aEvent->GetType(eventType);

    if (eventType.EqualsLiteral("pagehide")) {
      mHolder.Reject(NS_ERROR_FAILURE, __func__);
      CleanupEventListener();
      return NS_OK;
    }

    MOZ_ASSERT(eventType.EqualsLiteral("fullscreenchange") ||
               eventType.EqualsLiteral("fullscreenerror") ||
               eventType.EqualsLiteral("pagehide"));
    if (mDocument->Fullscreen()) {
      mHolder.Resolve(true, __func__);
    } else {
      mHolder.Reject(NS_ERROR_FAILURE, __func__);
    }
    CleanupEventListener();
    return NS_OK;
  }

 private:
  nsresult InstallEventListener(Document* aDoc) {
    if (mDocument) {
      return NS_OK;
    }

    mDocument = aDoc;
    nsresult rv = aDoc->AddSystemEventListener(u"fullscreenchange"_ns, this,
                                               /* aUseCapture = */ true);
    if (NS_FAILED(rv)) {
      CleanupEventListener();
      return rv;
    }

    rv = aDoc->AddSystemEventListener(u"fullscreenerror"_ns, this,
                                      /* aUseCapture = */ true);
    if (NS_FAILED(rv)) {
      CleanupEventListener();
      return rv;
    }

    nsPIDOMWindowOuter* window = aDoc->GetWindow();
    nsCOMPtr<EventTarget> target = do_QueryInterface(window);
    if (!target) {
      CleanupEventListener();
      return NS_ERROR_FAILURE;
    }
    rv = target->AddSystemEventListener(u"pagehide"_ns, this,
                                        /* aUseCapture = */ true,
                                        /* aWantsUntrusted = */ false);
    if (NS_FAILED(rv)) {
      CleanupEventListener();
      return rv;
    }

    return NS_OK;
  }

  void CleanupEventListener() {
    if (!mDocument) {
      return;
    }
    RefPtr<FullscreenWaitListener> kungFuDeathGrip(this);
    mDocument->RemoveSystemEventListener(u"fullscreenchange"_ns, this, true);
    mDocument->RemoveSystemEventListener(u"fullscreenerror"_ns, this, true);
    nsPIDOMWindowOuter* window = mDocument->GetWindow();
    nsCOMPtr<EventTarget> target = do_QueryInterface(window);
    if (target) {
      target->RemoveSystemEventListener(u"pagehide"_ns, this, true);
    }
    mDocument = nullptr;
  }

  MozPromiseHolder<GenericPromise> mHolder;
  RefPtr<Document> mDocument;
};

NS_IMPL_ISUPPORTS(FullscreenWaitListener, nsIDOMEventListener)

void ScreenOrientation::AbortInProcessOrientationPromises(
    BrowsingContext* aBrowsingContext) {
  MOZ_ASSERT(aBrowsingContext);

  aBrowsingContext = aBrowsingContext->Top();
  aBrowsingContext->PreOrderWalk([](BrowsingContext* aContext) {
    nsIDocShell* docShell = aContext->GetDocShell();
    if (docShell) {
      Document* doc = docShell->GetDocument();
      if (doc) {
        Promise* promise = doc->GetOrientationPendingPromise();
        if (promise) {
          promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
          doc->ClearOrientationPendingPromise();
        }
      }
    }
  });
}

already_AddRefed<Promise> ScreenOrientation::LockInternal(
    hal::ScreenOrientation aOrientation, ErrorResult& aRv) {
  // Steps to apply an orientation lock as defined in spec.

  // Step 1.
  // Let document be this's relevant global object's associated Document.

  Document* doc = GetResponsibleDocument();
  if (NS_WARN_IF(!doc)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  // Step 2.
  // If document is not fully active, return a promise rejected with an
  // "InvalidStateError" DOMException.

  nsCOMPtr<nsPIDOMWindowInner> owner = GetOwner();
  if (NS_WARN_IF(!owner)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  nsCOMPtr<nsIDocShell> docShell = owner->GetDocShell();
  if (NS_WARN_IF(!docShell)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> go = do_QueryInterface(owner);
  MOZ_ASSERT(go);
  RefPtr<Promise> p = Promise::Create(go, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  if (!owner->IsFullyActive()) {
    p->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return p.forget();
  }

  // Step 3.
  // If document has the sandboxed orientation lock browsing context flag set,
  // or doesn't meet the pre-lock conditions, or locking would be a security
  // risk, return a promise rejected with a "SecurityError" DOMException and
  // abort these steps.

  LockPermission perm = GetLockOrientationPermission(true);
  if (perm == LOCK_DENIED) {
    p->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
    return p.forget();
  }

  // Step 4.
  // If the user agent does not support locking the screen orientation to
  // orientation, return a promise rejected with a "NotSupportedError"
  // DOMException and abort these steps.

#if !defined(MOZ_WIDGET_ANDROID) && !defined(XP_WIN)
  // User agent does not support locking the screen orientation.
  p->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  return p.forget();
#else
  // Bypass locking screen orientation if preference is false
  if (!StaticPrefs::dom_screenorientation_allow_lock()) {
    p->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return p.forget();
  }

  RefPtr<BrowsingContext> bc = docShell->GetBrowsingContext();
  bc = bc ? bc->Top() : nullptr;
  if (!bc) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  bc->SetOrientationLock(aOrientation, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  AbortInProcessOrientationPromises(bc);
  dom::ContentChild::GetSingleton()->SendAbortOtherOrientationPendingPromises(
      bc);

  if (!doc->SetOrientationPendingPromise(p)) {
    p->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
    return p.forget();
  }

  if (perm == LOCK_ALLOWED || doc->Fullscreen()) {
    nsCOMPtr<nsIRunnable> lockOrientationTask = new LockOrientationTask(
        this, p, aOrientation, doc, perm == FULLSCREEN_LOCK_ALLOWED);
    aRv = NS_DispatchToMainThread(lockOrientationTask);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    return p.forget();
  }

  MOZ_ASSERT(perm == FULLSCREEN_LOCK_ALLOWED);

  // Full screen state is pending. We have to wait for the completion.
  RefPtr<FullscreenWaitListener> listener = new FullscreenWaitListener();
  RefPtr<Promise> promise = p;
  listener->Promise(doc)->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [self = RefPtr{this}, promise = std::move(promise), aOrientation,
       document =
           RefPtr{doc}](const GenericPromise::ResolveOrRejectValue& aValue) {
        if (aValue.IsResolve()) {
          nsCOMPtr<nsIRunnable> lockOrientationTask = new LockOrientationTask(
              self, promise, aOrientation, document, true);
          nsresult rv = NS_DispatchToMainThread(lockOrientationTask);
          if (NS_SUCCEEDED(rv)) {
            return;
          }
        }
        // Pending full screen request is canceled or causes an error.
        if (document->GetOrientationPendingPromise() != promise) {
          // The document's pending promise is not associated with
          // this promise.
          return;
        }
        // pre-lock conditions aren't matched.
        promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
        document->ClearOrientationPendingPromise();
      });

  return p.forget();
#endif
}

RefPtr<GenericNonExclusivePromise> ScreenOrientation::LockDeviceOrientation(
    hal::ScreenOrientation aOrientation, bool aIsFullscreen) {
  if (!GetOwner()) {
    return GenericNonExclusivePromise::CreateAndReject(NS_ERROR_DOM_ABORT_ERR,
                                                       __func__);
  }

  nsCOMPtr<EventTarget> target = GetOwner()->GetDoc();
  // We need to register a listener so we learn when we leave fullscreen
  // and when we will have to unlock the screen.
  // This needs to be done before LockScreenOrientation call to make sure
  // the locking can be unlocked.
  if (aIsFullscreen && !target) {
    return GenericNonExclusivePromise::CreateAndReject(NS_ERROR_DOM_ABORT_ERR,
                                                       __func__);
  }

  // We are fullscreen and lock has been accepted.
  if (aIsFullscreen) {
    if (!mFullscreenListener) {
      mFullscreenListener = new FullscreenEventListener();
    }

    nsresult rv = target->AddSystemEventListener(u"fullscreenchange"_ns,
                                                 mFullscreenListener,
                                                 /* aUseCapture = */ true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return GenericNonExclusivePromise::CreateAndReject(NS_ERROR_DOM_ABORT_ERR,
                                                         __func__);
    }
  }

  mTriedToLockDeviceOrientation = true;
  return hal::LockScreenOrientation(aOrientation);
}

void ScreenOrientation::Unlock(ErrorResult& aRv) {
  if (RefPtr<Promise> p = LockInternal(hal::ScreenOrientation::None, aRv)) {
    // Don't end up reporting unhandled promise rejection since
    // screen.orientation.unlock doesn't return promise.
    MOZ_ALWAYS_TRUE(p->SetAnyPromiseIsHandled());
  }
}

void ScreenOrientation::UnlockDeviceOrientation() {
  hal::UnlockScreenOrientation();
  CleanupFullscreenListener();
}

void ScreenOrientation::CleanupFullscreenListener() {
  if (!mFullscreenListener || !GetOwner()) {
    mFullscreenListener = nullptr;
    return;
  }

  // Remove event listener in case of fullscreen lock.
  if (nsCOMPtr<EventTarget> target = GetOwner()->GetDoc()) {
    target->RemoveSystemEventListener(u"fullscreenchange"_ns,
                                      mFullscreenListener,
                                      /* useCapture */ true);
  }

  mFullscreenListener = nullptr;
}

OrientationType ScreenOrientation::DeviceType(CallerType aCallerType) const {
  if (nsContentUtils::ShouldResistFingerprinting(
          aCallerType, GetOwnerGlobal(), RFPTarget::ScreenOrientation)) {
    return OrientationType::Landscape_primary;
  }
  return mType;
}

uint16_t ScreenOrientation::DeviceAngle(CallerType aCallerType) const {
  if (nsContentUtils::ShouldResistFingerprinting(
          aCallerType, GetOwnerGlobal(), RFPTarget::ScreenOrientation)) {
    return 0;
  }
  return mAngle;
}

OrientationType ScreenOrientation::GetType(CallerType aCallerType,
                                           ErrorResult& aRv) const {
  if (nsContentUtils::ShouldResistFingerprinting(
          aCallerType, GetOwnerGlobal(), RFPTarget::ScreenOrientation)) {
    return OrientationType::Landscape_primary;
  }

  Document* doc = GetResponsibleDocument();
  BrowsingContext* bc = doc ? doc->GetBrowsingContext() : nullptr;
  if (!bc) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return OrientationType::Portrait_primary;
  }

  return bc->GetCurrentOrientationType();
}

uint16_t ScreenOrientation::GetAngle(CallerType aCallerType,
                                     ErrorResult& aRv) const {
  if (nsContentUtils::ShouldResistFingerprinting(
          aCallerType, GetOwnerGlobal(), RFPTarget::ScreenOrientation)) {
    return 0;
  }

  Document* doc = GetResponsibleDocument();
  BrowsingContext* bc = doc ? doc->GetBrowsingContext() : nullptr;
  if (!bc) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return 0;
  }

  return bc->GetCurrentOrientationAngle();
}

ScreenOrientation::LockPermission
ScreenOrientation::GetLockOrientationPermission(bool aCheckSandbox) const {
  nsCOMPtr<nsPIDOMWindowInner> owner = GetOwner();
  if (!owner) {
    return LOCK_DENIED;
  }

  // Chrome can always lock the screen orientation.
  if (owner->GetBrowsingContext()->IsChrome()) {
    return LOCK_ALLOWED;
  }

  nsCOMPtr<Document> doc = owner->GetDoc();
  if (!doc || doc->Hidden()) {
    return LOCK_DENIED;
  }

  // Sandboxed without "allow-orientation-lock"
  if (aCheckSandbox && doc->GetSandboxFlags() & SANDBOXED_ORIENTATION_LOCK) {
    return LOCK_DENIED;
  }

  if (Preferences::GetBool(
          "dom.screenorientation.testing.non_fullscreen_lock_allow", false)) {
    return LOCK_ALLOWED;
  }

  // Other content must be fullscreen in order to lock orientation.
  return doc->Fullscreen() || doc->HasPendingFullscreenRequests()
             ? FULLSCREEN_LOCK_ALLOWED
             : LOCK_DENIED;
}

Document* ScreenOrientation::GetResponsibleDocument() const {
  nsCOMPtr<nsPIDOMWindowInner> owner = GetOwner();
  if (!owner) {
    return nullptr;
  }

  return owner->GetDoc();
}

void ScreenOrientation::MaybeChanged() {
  Document* doc = GetResponsibleDocument();
  if (!doc || doc->ShouldResistFingerprinting(RFPTarget::ScreenOrientation)) {
    return;
  }

  BrowsingContext* bc = doc->GetBrowsingContext();
  if (!bc) {
    return;
  }

  hal::ScreenOrientation orientation = mScreen->GetOrientationType();
  if (orientation != hal::ScreenOrientation::PortraitPrimary &&
      orientation != hal::ScreenOrientation::PortraitSecondary &&
      orientation != hal::ScreenOrientation::LandscapePrimary &&
      orientation != hal::ScreenOrientation::LandscapeSecondary) {
    // The platform may notify of some other values from
    // an orientation lock, but we only care about real
    // changes to screen orientation which result in one of
    // the values we care about.
    return;
  }

  OrientationType previousOrientation = mType;
  mAngle = mScreen->GetOrientationAngle();
  mType = InternalOrientationToType(orientation);

  DebugOnly<nsresult> rv;
  if (mScreen && mType != previousOrientation) {
    // Use of mozorientationchange is deprecated.
    rv = mScreen->DispatchTrustedEvent(u"mozorientationchange"_ns);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "DispatchTrustedEvent failed");
  }

  if (doc->Hidden() && !mVisibleListener) {
    mVisibleListener = new VisibleEventListener();
    rv = doc->AddSystemEventListener(u"visibilitychange"_ns, mVisibleListener,
                                     /* aUseCapture = */ true);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "AddSystemEventListener failed");
    return;
  }

  if (mType != bc->GetCurrentOrientationType()) {
    rv = bc->SetCurrentOrientation(mType, mAngle);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "SetCurrentOrientation failed");

    nsCOMPtr<nsIRunnable> runnable = DispatchChangeEventAndResolvePromise();
    rv = NS_DispatchToMainThread(runnable);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "NS_DispatchToMainThread failed");
  }
}

void ScreenOrientation::UpdateActiveOrientationLock(
    hal::ScreenOrientation aOrientation) {
  if (aOrientation == hal::ScreenOrientation::None) {
    hal::UnlockScreenOrientation();
  } else {
    hal::LockScreenOrientation(aOrientation)
        ->Then(
            GetMainThreadSerialEventTarget(), __func__,
            [](const GenericNonExclusivePromise::ResolveOrRejectValue& aValue) {
              NS_WARNING_ASSERTION(aValue.IsResolve(),
                                   "hal::LockScreenOrientation failed");
            });
  }
}

nsCOMPtr<nsIRunnable>
ScreenOrientation::DispatchChangeEventAndResolvePromise() {
  RefPtr<Document> doc = GetResponsibleDocument();
  RefPtr<ScreenOrientation> self = this;
  return NS_NewRunnableFunction(
      "dom::ScreenOrientation::DispatchChangeEvent", [self, doc]() {
        DebugOnly<nsresult> rv = self->DispatchTrustedEvent(u"change"_ns);
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "DispatchTrustedEvent failed");
        if (doc) {
          Promise* pendingPromise = doc->GetOrientationPendingPromise();
          if (pendingPromise) {
            pendingPromise->MaybeResolveWithUndefined();
            doc->ClearOrientationPendingPromise();
          }
        }
      });
}

JSObject* ScreenOrientation::WrapObject(JSContext* aCx,
                                        JS::Handle<JSObject*> aGivenProto) {
  return ScreenOrientation_Binding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_ISUPPORTS(ScreenOrientation::VisibleEventListener, nsIDOMEventListener)

NS_IMETHODIMP
ScreenOrientation::VisibleEventListener::HandleEvent(Event* aEvent) {
  // Document may have become visible, if the page is visible, run the steps
  // following the "now visible algorithm" as specified.
  MOZ_ASSERT(aEvent->GetCurrentTarget());
  nsCOMPtr<nsINode> eventTargetNode =
      nsINode::FromEventTarget(aEvent->GetCurrentTarget());
  if (!eventTargetNode || !eventTargetNode->IsDocument() ||
      eventTargetNode->AsDocument()->Hidden()) {
    return NS_OK;
  }

  RefPtr<Document> doc = eventTargetNode->AsDocument();
  auto* win = nsGlobalWindowInner::Cast(doc->GetInnerWindow());
  if (!win) {
    return NS_OK;
  }

  ScreenOrientation* orientation = win->Screen()->Orientation();
  MOZ_ASSERT(orientation);

  doc->RemoveSystemEventListener(u"visibilitychange"_ns, this, true);

  BrowsingContext* bc = doc->GetBrowsingContext();
  if (bc && bc->GetCurrentOrientationType() !=
                orientation->DeviceType(CallerType::System)) {
    nsresult result =
        bc->SetCurrentOrientation(orientation->DeviceType(CallerType::System),
                                  orientation->DeviceAngle(CallerType::System));
    NS_ENSURE_SUCCESS(result, result);

    nsCOMPtr<nsIRunnable> runnable =
        orientation->DispatchChangeEventAndResolvePromise();
    MOZ_TRY(NS_DispatchToMainThread(runnable));
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS(ScreenOrientation::FullscreenEventListener,
                  nsIDOMEventListener)

NS_IMETHODIMP
ScreenOrientation::FullscreenEventListener::HandleEvent(Event* aEvent) {
#ifdef DEBUG
  nsAutoString eventType;
  aEvent->GetType(eventType);

  MOZ_ASSERT(eventType.EqualsLiteral("fullscreenchange"));
#endif

  EventTarget* target = aEvent->GetCurrentTarget();
  MOZ_ASSERT(target);
  MOZ_ASSERT(target->IsNode());
  RefPtr<Document> doc = nsINode::FromEventTarget(target)->AsDocument();
  MOZ_ASSERT(doc);

  // We have to make sure that the event we got is the event sent when
  // fullscreen is disabled because we could get one when fullscreen
  // got enabled if the lock call is done at the same moment.
  if (doc->Fullscreen()) {
    return NS_OK;
  }

  BrowsingContext* bc = doc->GetBrowsingContext();
  bc = bc ? bc->Top() : nullptr;
  if (bc) {
    bc->SetOrientationLock(hal::ScreenOrientation::None, IgnoreErrors());
  }

  hal::UnlockScreenOrientation();

  target->RemoveSystemEventListener(u"fullscreenchange"_ns, this, true);
  return NS_OK;
}
