/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScreenOrientation.h"
#include "nsIDocShell.h"
#include "mozilla/dom/Document.h"
#include "nsGlobalWindow.h"
#include "nsSandboxFlags.h"
#include "nsScreen.h"

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/Hal.h"
#include "mozilla/Preferences.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/Promise.h"
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
    case hal::eScreenOrientation_PortraitPrimary:
      return OrientationType::Portrait_primary;
    case hal::eScreenOrientation_PortraitSecondary:
      return OrientationType::Portrait_secondary;
    case hal::eScreenOrientation_LandscapePrimary:
      return OrientationType::Landscape_primary;
    case hal::eScreenOrientation_LandscapeSecondary:
      return OrientationType::Landscape_secondary;
    default:
      MOZ_CRASH("Bad aOrientation value");
  }
}

static hal::ScreenOrientation OrientationTypeToInternal(
    OrientationType aOrientation) {
  switch (aOrientation) {
    case OrientationType::Portrait_primary:
      return hal::eScreenOrientation_PortraitPrimary;
    case OrientationType::Portrait_secondary:
      return hal::eScreenOrientation_PortraitSecondary;
    case OrientationType::Landscape_primary:
      return hal::eScreenOrientation_LandscapePrimary;
    case OrientationType::Landscape_secondary:
      return hal::eScreenOrientation_LandscapeSecondary;
    default:
      MOZ_CRASH("Bad aOrientation value");
  }
}

ScreenOrientation::ScreenOrientation(nsPIDOMWindowInner* aWindow,
                                     nsScreen* aScreen)
    : DOMEventTargetHelper(aWindow), mScreen(aScreen) {
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aScreen);

  hal::RegisterScreenConfigurationObserver(this);

  hal::ScreenConfiguration config;
  hal::GetCurrentScreenConfiguration(&config);
  mType = InternalOrientationToType(config.orientation());
  mAngle = config.angle();

  Document* doc = GetResponsibleDocument();
  BrowsingContext* bc = doc ? doc->GetBrowsingContext() : nullptr;
  if (bc && !bc->InRDMPane()) {
    bc->SetCurrentOrientation(mType, mAngle);
  }
}

ScreenOrientation::~ScreenOrientation() {
  UnlockDeviceOrientation();
  hal::UnregisterScreenConfigurationObserver(this);
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
  nsCOMPtr<Document> mDocument;
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
  return mOrientationLock & OrientationTypeToInternal(aOrientationType);
}

NS_IMETHODIMP
ScreenOrientation::LockOrientationTask::Run() {
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

  if (mOrientationLock == hal::eScreenOrientation_None) {
    mScreenOrientation->UnlockDeviceOrientation();
    mPromise->MaybeResolveWithUndefined();
    mDocument->ClearOrientationPendingPromise();
    return NS_OK;
  }

  ErrorResult rv;
  bool result = mScreenOrientation->LockDeviceOrientation(mOrientationLock,
                                                          mIsFullscreen, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  if (NS_WARN_IF(!result)) {
    mPromise->MaybeReject(NS_ERROR_UNEXPECTED);
    mDocument->ClearOrientationPendingPromise();
    return NS_OK;
  }

  BrowsingContext* bc = mDocument->GetBrowsingContext();
  if (OrientationLockContains(bc->GetCurrentOrientationType()) ||
      (mOrientationLock == hal::eScreenOrientation_Default &&
       bc->GetCurrentOrientationAngle() == 0)) {
    // Orientation lock will not cause an orientation change.
    mPromise->MaybeResolveWithUndefined();
    mDocument->ClearOrientationPendingPromise();
  }

  return NS_OK;
}

already_AddRefed<Promise> ScreenOrientation::Lock(
    OrientationLockType aOrientation, ErrorResult& aRv) {
  hal::ScreenOrientation orientation = hal::eScreenOrientation_None;

  switch (aOrientation) {
    case OrientationLockType::Any:
      orientation = hal::eScreenOrientation_PortraitPrimary |
                    hal::eScreenOrientation_PortraitSecondary |
                    hal::eScreenOrientation_LandscapePrimary |
                    hal::eScreenOrientation_LandscapeSecondary;
      break;
    case OrientationLockType::Natural:
      orientation |= hal::eScreenOrientation_Default;
      break;
    case OrientationLockType::Landscape:
      orientation = hal::eScreenOrientation_LandscapePrimary |
                    hal::eScreenOrientation_LandscapeSecondary;
      break;
    case OrientationLockType::Portrait:
      orientation = hal::eScreenOrientation_PortraitPrimary |
                    hal::eScreenOrientation_PortraitSecondary;
      break;
    case OrientationLockType::Portrait_primary:
      orientation = hal::eScreenOrientation_PortraitPrimary;
      break;
    case OrientationLockType::Portrait_secondary:
      orientation = hal::eScreenOrientation_PortraitSecondary;
      break;
    case OrientationLockType::Landscape_primary:
      orientation = hal::eScreenOrientation_LandscapePrimary;
      break;
    case OrientationLockType::Landscape_secondary:
      orientation = hal::eScreenOrientation_LandscapeSecondary;
      break;
    default:
      NS_WARNING("Unexpected orientation type");
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
  }

  return LockInternal(orientation, aRv);
}

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

  Document* doc = GetResponsibleDocument();
  if (NS_WARN_IF(!doc)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowInner> owner = GetOwner();
  if (NS_WARN_IF(!owner)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsCOMPtr<nsIDocShell> docShell = owner->GetDocShell();
  if (NS_WARN_IF(!docShell)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> go = do_QueryInterface(owner);
  MOZ_ASSERT(go);
  RefPtr<Promise> p = Promise::Create(go, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

#if !defined(MOZ_WIDGET_ANDROID)
  // User agent does not support locking the screen orientation.
  p->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  return p.forget();
#else
  LockPermission perm = GetLockOrientationPermission(true);
  if (perm == LOCK_DENIED) {
    p->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
    return p.forget();
  }

  RefPtr<BrowsingContext> bc = docShell->GetBrowsingContext();
  bc = bc ? bc->Top() : nullptr;
  if (!bc) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  bc->SetOrientationLock(aOrientation);
  AbortInProcessOrientationPromises(bc);
  dom::ContentChild::GetSingleton()->SendAbortOtherOrientationPendingPromises(
      bc);

  if (!doc->SetOrientationPendingPromise(p)) {
    p->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
    return p.forget();
  }

  nsCOMPtr<nsIRunnable> lockOrientationTask = new LockOrientationTask(
      this, p, aOrientation, doc, perm == FULLSCREEN_LOCK_ALLOWED);
  aRv = NS_DispatchToMainThread(lockOrientationTask);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return p.forget();
#endif
}

bool ScreenOrientation::LockDeviceOrientation(
    hal::ScreenOrientation aOrientation, bool aIsFullscreen, ErrorResult& aRv) {
  if (!GetOwner()) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return false;
  }

  nsCOMPtr<EventTarget> target = GetOwner()->GetDoc();
  // We need to register a listener so we learn when we leave fullscreen
  // and when we will have to unlock the screen.
  // This needs to be done before LockScreenOrientation call to make sure
  // the locking can be unlocked.
  if (aIsFullscreen && !target) {
    return false;
  }

  if (NS_WARN_IF(!hal::LockScreenOrientation(aOrientation))) {
    return false;
  }

  // We are fullscreen and lock has been accepted.
  if (aIsFullscreen) {
    if (!mFullscreenListener) {
      mFullscreenListener = new FullscreenEventListener();
    }

    aRv = target->AddSystemEventListener(NS_LITERAL_STRING("fullscreenchange"),
                                         mFullscreenListener,
                                         /* useCapture = */ true);
    if (NS_WARN_IF(aRv.Failed())) {
      return false;
    }
  }

  return true;
}

void ScreenOrientation::Unlock(ErrorResult& aRv) {
  RefPtr<Promise> p = LockInternal(hal::eScreenOrientation_None, aRv);
}

void ScreenOrientation::UnlockDeviceOrientation() {
  hal::UnlockScreenOrientation();

  if (!mFullscreenListener || !GetOwner()) {
    mFullscreenListener = nullptr;
    return;
  }

  // Remove event listener in case of fullscreen lock.
  nsCOMPtr<EventTarget> target = GetOwner()->GetDoc();
  if (target) {
    target->RemoveSystemEventListener(NS_LITERAL_STRING("fullscreenchange"),
                                      mFullscreenListener,
                                      /* useCapture */ true);
  }

  mFullscreenListener = nullptr;
}

OrientationType ScreenOrientation::DeviceType(CallerType aCallerType) const {
  return nsContentUtils::ResistFingerprinting(aCallerType)
             ? OrientationType::Landscape_primary
             : mType;
}

uint16_t ScreenOrientation::DeviceAngle(CallerType aCallerType) const {
  return nsContentUtils::ResistFingerprinting(aCallerType) ? 0 : mAngle;
}

OrientationType ScreenOrientation::GetType(CallerType aCallerType,
                                           ErrorResult& aRv) const {
  if (nsContentUtils::ResistFingerprinting(aCallerType)) {
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
  if (nsContentUtils::ResistFingerprinting(aCallerType)) {
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
  return doc->Fullscreen() ? FULLSCREEN_LOCK_ALLOWED : LOCK_DENIED;
}

Document* ScreenOrientation::GetResponsibleDocument() const {
  nsCOMPtr<nsPIDOMWindowInner> owner = GetOwner();
  if (!owner) {
    return nullptr;
  }

  return owner->GetDoc();
}

void ScreenOrientation::Notify(const hal::ScreenConfiguration& aConfiguration) {
  if (ShouldResistFingerprinting()) {
    return;
  }

  Document* doc = GetResponsibleDocument();
  BrowsingContext* bc = doc ? doc->GetBrowsingContext() : nullptr;
  if (!bc) {
    return;
  }

  hal::ScreenOrientation orientation = aConfiguration.orientation();
  if (orientation != hal::eScreenOrientation_PortraitPrimary &&
      orientation != hal::eScreenOrientation_PortraitSecondary &&
      orientation != hal::eScreenOrientation_LandscapePrimary &&
      orientation != hal::eScreenOrientation_LandscapeSecondary) {
    // The platform may notify of some other values from
    // an orientation lock, but we only care about real
    // changes to screen orientation which result in one of
    // the values we care about.
    return;
  }

  OrientationType previousOrientation = mType;
  mAngle = aConfiguration.angle();
  mType = InternalOrientationToType(orientation);

  DebugOnly<nsresult> rv;
  if (mScreen && mType != previousOrientation) {
    // Use of mozorientationchange is deprecated.
    rv = mScreen->DispatchTrustedEvent(
        NS_LITERAL_STRING("mozorientationchange"));
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "DispatchTrustedEvent failed");
  }

  if (doc->Hidden() && !mVisibleListener) {
    mVisibleListener = new VisibleEventListener();
    rv = doc->AddSystemEventListener(NS_LITERAL_STRING("visibilitychange"),
                                     mVisibleListener, /* useCapture = */ true);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "AddSystemEventListener failed");
    return;
  }

  if (mType != bc->GetCurrentOrientationType()) {
    bc->SetCurrentOrientation(mType, mAngle);

    nsCOMPtr<nsIRunnable> runnable = DispatchChangeEventAndResolvePromise();
    rv = NS_DispatchToMainThread(runnable);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "NS_DispatchToMainThread failed");
  }
}

void ScreenOrientation::UpdateActiveOrientationLock(
    hal::ScreenOrientation aOrientation) {
  if (aOrientation == hal::eScreenOrientation_None) {
    hal::UnlockScreenOrientation();
  } else {
    DebugOnly<bool> ok = hal::LockScreenOrientation(aOrientation);
    NS_WARNING_ASSERTION(ok, "hal::LockScreenOrientation failed");
  }
}

nsCOMPtr<nsIRunnable>
ScreenOrientation::DispatchChangeEventAndResolvePromise() {
  RefPtr<Document> doc = GetResponsibleDocument();
  RefPtr<ScreenOrientation> self = this;
  return NS_NewRunnableFunction(
      "dom::ScreenOrientation::DispatchChangeEvent", [self, doc]() {
        DebugOnly<nsresult> rv =
            self->DispatchTrustedEvent(NS_LITERAL_STRING("change"));
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

bool ScreenOrientation::ShouldResistFingerprinting() const {
  bool resist = false;
  if (nsCOMPtr<nsPIDOMWindowInner> owner = GetOwner()) {
    resist = nsContentUtils::ShouldResistFingerprinting(owner->GetDocShell());
  }
  return resist;
}

NS_IMPL_ISUPPORTS(ScreenOrientation::VisibleEventListener, nsIDOMEventListener)

NS_IMETHODIMP
ScreenOrientation::VisibleEventListener::HandleEvent(Event* aEvent) {
  // Document may have become visible, if the page is visible, run the steps
  // following the "now visible algorithm" as specified.
  nsCOMPtr<EventTarget> target = aEvent->GetCurrentTarget();
  MOZ_ASSERT(target);

  nsCOMPtr<Document> doc = do_QueryInterface(target);
  if (!doc || doc->Hidden()) {
    return NS_OK;
  }

  auto* win = nsGlobalWindowInner::Cast(doc->GetInnerWindow());
  if (!win) {
    return NS_OK;
  }

  ErrorResult rv;
  nsScreen* screen = win->GetScreen(rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  MOZ_ASSERT(screen);
  ScreenOrientation* orientation = screen->Orientation();
  MOZ_ASSERT(orientation);

  target->RemoveSystemEventListener(NS_LITERAL_STRING("visibilitychange"), this,
                                    true);

  BrowsingContext* bc = doc->GetBrowsingContext();
  if (bc && bc->GetCurrentOrientationType() !=
                orientation->DeviceType(CallerType::System)) {
    bc->SetCurrentOrientation(orientation->DeviceType(CallerType::System),
                              orientation->DeviceAngle(CallerType::System));

    nsCOMPtr<nsIRunnable> runnable =
        orientation->DispatchChangeEventAndResolvePromise();
    rv = NS_DispatchToMainThread(runnable);
    if (NS_WARN_IF(rv.Failed())) {
      return rv.StealNSResult();
    }
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

  nsCOMPtr<EventTarget> target = aEvent->GetCurrentTarget();
  MOZ_ASSERT(target);

  nsCOMPtr<Document> doc = do_QueryInterface(target);
  MOZ_ASSERT(doc);

  // We have to make sure that the event we got is the event sent when
  // fullscreen is disabled because we could get one when fullscreen
  // got enabled if the lock call is done at the same moment.
  if (doc->Fullscreen()) {
    return NS_OK;
  }

  hal::UnlockScreenOrientation();

  target->RemoveSystemEventListener(NS_LITERAL_STRING("fullscreenchange"), this,
                                    true);
  return NS_OK;
}
