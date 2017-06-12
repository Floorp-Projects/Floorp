/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScreenOrientation.h"
#include "nsIDeviceSensors.h"
#include "nsIDocShell.h"
#include "nsIDocument.h"
#include "nsGlobalWindow.h"
#include "nsSandboxFlags.h"
#include "nsScreen.h"

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/Hal.h"
#include "mozilla/Preferences.h"

#include "mozilla/dom/Promise.h"
#include "nsContentUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_INHERITED(ScreenOrientation,
                                   DOMEventTargetHelper,
                                   mScreen);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ScreenOrientation)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(ScreenOrientation, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(ScreenOrientation, DOMEventTargetHelper)

static OrientationType
InternalOrientationToType(ScreenOrientationInternal aOrientation)
{
  switch (aOrientation) {
  case eScreenOrientation_PortraitPrimary:
    return OrientationType::Portrait_primary;
  case eScreenOrientation_PortraitSecondary:
    return OrientationType::Portrait_secondary;
  case eScreenOrientation_LandscapePrimary:
    return OrientationType::Landscape_primary;
  case eScreenOrientation_LandscapeSecondary:
    return OrientationType::Landscape_secondary;
  default:
    MOZ_CRASH("Bad aOrientation value");
  }
}

static ScreenOrientationInternal
OrientationTypeToInternal(OrientationType aOrientation)
{
  switch (aOrientation) {
  case OrientationType::Portrait_primary:
    return eScreenOrientation_PortraitPrimary;
  case OrientationType::Portrait_secondary:
    return eScreenOrientation_PortraitSecondary;
  case OrientationType::Landscape_primary:
    return eScreenOrientation_LandscapePrimary;
  case OrientationType::Landscape_secondary:
    return eScreenOrientation_LandscapeSecondary;
  default:
    MOZ_CRASH("Bad aOrientation value");
  }
}

ScreenOrientation::ScreenOrientation(nsPIDOMWindowInner* aWindow, nsScreen* aScreen)
  : DOMEventTargetHelper(aWindow), mScreen(aScreen)
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aScreen);

  hal::RegisterScreenConfigurationObserver(this);

  hal::ScreenConfiguration config;
  hal::GetCurrentScreenConfiguration(&config);
  mType = InternalOrientationToType(config.orientation());
  mAngle = config.angle();

  nsIDocument* doc = GetResponsibleDocument();
  if (doc) {
    doc->SetCurrentOrientation(mType, mAngle);
  }
}

ScreenOrientation::~ScreenOrientation()
{
  hal::UnregisterScreenConfigurationObserver(this);
  MOZ_ASSERT(!mFullScreenListener);
}

class ScreenOrientation::FullScreenEventListener final : public nsIDOMEventListener
{
  ~FullScreenEventListener() {}
public:
  FullScreenEventListener() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER
};

class ScreenOrientation::VisibleEventListener final : public nsIDOMEventListener
{
  ~VisibleEventListener() {}
public:
  VisibleEventListener() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER
};

class ScreenOrientation::LockOrientationTask final : public nsIRunnable
{
  ~LockOrientationTask();
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  LockOrientationTask(ScreenOrientation* aScreenOrientation,
                      Promise* aPromise,
                      ScreenOrientationInternal aOrientationLock,
                      nsIDocument* aDocument,
                      bool aIsFullScreen);
protected:
  bool OrientationLockContains(OrientationType aOrientationType);

  RefPtr<ScreenOrientation> mScreenOrientation;
  RefPtr<Promise> mPromise;
  ScreenOrientationInternal mOrientationLock;
  nsCOMPtr<nsIDocument> mDocument;
  bool mIsFullScreen;
};

NS_IMPL_ISUPPORTS(ScreenOrientation::LockOrientationTask, nsIRunnable)

ScreenOrientation::LockOrientationTask::LockOrientationTask(
  ScreenOrientation* aScreenOrientation, Promise* aPromise,
  ScreenOrientationInternal aOrientationLock,
  nsIDocument* aDocument, bool aIsFullScreen)
  : mScreenOrientation(aScreenOrientation), mPromise(aPromise),
    mOrientationLock(aOrientationLock), mDocument(aDocument),
    mIsFullScreen(aIsFullScreen)
{
  MOZ_ASSERT(aScreenOrientation);
  MOZ_ASSERT(aPromise);
  MOZ_ASSERT(aDocument);
}

ScreenOrientation::LockOrientationTask::~LockOrientationTask()
{
}

bool
ScreenOrientation::LockOrientationTask::OrientationLockContains(
  OrientationType aOrientationType)
{
  return mOrientationLock & OrientationTypeToInternal(aOrientationType);
}

NS_IMETHODIMP
ScreenOrientation::LockOrientationTask::Run()
{
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
    mDocument->SetOrientationPendingPromise(nullptr);
    return NS_OK;
  }

  if (mOrientationLock == eScreenOrientation_None) {
    mScreenOrientation->UnlockDeviceOrientation();
    mPromise->MaybeResolveWithUndefined();
    mDocument->SetOrientationPendingPromise(nullptr);
    return NS_OK;
  }

  ErrorResult rv;
  bool result = mScreenOrientation->LockDeviceOrientation(mOrientationLock,
                                                          mIsFullScreen, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  if (NS_WARN_IF(!result)) {
    mPromise->MaybeReject(NS_ERROR_UNEXPECTED);
    mDocument->SetOrientationPendingPromise(nullptr);
    return NS_OK;
  }

  if (OrientationLockContains(mDocument->CurrentOrientationType()) ||
      (mOrientationLock == eScreenOrientation_Default &&
       mDocument->CurrentOrientationAngle() == 0)) {
    // Orientation lock will not cause an orientation change.
    mPromise->MaybeResolveWithUndefined();
    mDocument->SetOrientationPendingPromise(nullptr);
  }

  return NS_OK;
}

already_AddRefed<Promise>
ScreenOrientation::Lock(OrientationLockType aOrientation, ErrorResult& aRv)
{
  ScreenOrientationInternal orientation = eScreenOrientation_None;

  switch (aOrientation) {
  case OrientationLockType::Any:
    orientation = eScreenOrientation_PortraitPrimary |
                  eScreenOrientation_PortraitSecondary |
                  eScreenOrientation_LandscapePrimary |
                  eScreenOrientation_LandscapeSecondary;
    break;
  case OrientationLockType::Natural:
    orientation |= eScreenOrientation_Default;
    break;
  case OrientationLockType::Landscape:
    orientation = eScreenOrientation_LandscapePrimary |
                  eScreenOrientation_LandscapeSecondary;
    break;
  case OrientationLockType::Portrait:
    orientation = eScreenOrientation_PortraitPrimary |
                  eScreenOrientation_PortraitSecondary;
    break;
  case OrientationLockType::Portrait_primary:
    orientation = eScreenOrientation_PortraitPrimary;
    break;
  case OrientationLockType::Portrait_secondary:
    orientation = eScreenOrientation_PortraitSecondary;
    break;
  case OrientationLockType::Landscape_primary:
    orientation = eScreenOrientation_LandscapePrimary;
    break;
  case OrientationLockType::Landscape_secondary:
    orientation = eScreenOrientation_LandscapeSecondary;
    break;
  default:
    NS_WARNING("Unexpected orientation type");
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  return LockInternal(orientation, aRv);
}

static inline void
AbortOrientationPromises(nsIDocShell* aDocShell)
{
  MOZ_ASSERT(aDocShell);

  nsIDocument* doc = aDocShell->GetDocument();
  if (doc) {
    Promise* promise = doc->GetOrientationPendingPromise();
    if (promise) {
      promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
      doc->SetOrientationPendingPromise(nullptr);
    }
  }

  int32_t childCount;
  aDocShell->GetChildCount(&childCount);
  for (int32_t i = 0; i < childCount; i++) {
    nsCOMPtr<nsIDocShellTreeItem> child;
    if (NS_SUCCEEDED(aDocShell->GetChildAt(i, getter_AddRefs(child)))) {
      nsCOMPtr<nsIDocShell> childShell(do_QueryInterface(child));
      if (childShell) {
        AbortOrientationPromises(childShell);
      }
    }
  }
}

already_AddRefed<Promise>
ScreenOrientation::LockInternal(ScreenOrientationInternal aOrientation, ErrorResult& aRv)
{
  // Steps to apply an orientation lock as defined in spec.

  nsIDocument* doc = GetResponsibleDocument();
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

#if !defined(MOZ_WIDGET_ANDROID) && !defined(MOZ_WIDGET_GONK)
  // User agent does not support locking the screen orientation.
  p->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  return p.forget();
#else
  LockPermission perm = GetLockOrientationPermission(true);
  if (perm == LOCK_DENIED) {
    p->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
    return p.forget();
  }

  nsCOMPtr<nsIDocShellTreeItem> root;
  docShell->GetSameTypeRootTreeItem(getter_AddRefs(root));
  nsCOMPtr<nsIDocShell> rootShell(do_QueryInterface(root));
  if (!rootShell) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  rootShell->SetOrientationLock(aOrientation);
  AbortOrientationPromises(rootShell);

  doc->SetOrientationPendingPromise(p);

  nsCOMPtr<nsIRunnable> lockOrientationTask =
    new LockOrientationTask(this, p, aOrientation, doc,
                            perm == FULLSCREEN_LOCK_ALLOWED);
  aRv = NS_DispatchToMainThread(lockOrientationTask);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return p.forget();
#endif
}

bool
ScreenOrientation::LockDeviceOrientation(ScreenOrientationInternal aOrientation,
                                         bool aIsFullScreen, ErrorResult& aRv)
{
  if (!GetOwner()) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return false;
  }

  nsCOMPtr<EventTarget> target = do_QueryInterface(GetOwner()->GetDoc());
  // We need to register a listener so we learn when we leave full-screen
  // and when we will have to unlock the screen.
  // This needs to be done before LockScreenOrientation call to make sure
  // the locking can be unlocked.
  if (aIsFullScreen && !target) {
    return false;
  }

  if (NS_WARN_IF(!hal::LockScreenOrientation(aOrientation))) {
    return false;
  }

  // We are fullscreen and lock has been accepted.
  if (aIsFullScreen) {
    if (!mFullScreenListener) {
      mFullScreenListener = new FullScreenEventListener();
    }

    aRv = target->AddSystemEventListener(NS_LITERAL_STRING("fullscreenchange"),
                                         mFullScreenListener, /* useCapture = */ true);
    if (NS_WARN_IF(aRv.Failed())) {
      return false;
    }
  }

  return true;
}

void
ScreenOrientation::Unlock(ErrorResult& aRv)
{
  RefPtr<Promise> p = LockInternal(eScreenOrientation_None, aRv);
}

void
ScreenOrientation::UnlockDeviceOrientation()
{
  hal::UnlockScreenOrientation();

  if (!mFullScreenListener || !GetOwner()) {
    mFullScreenListener = nullptr;
    return;
  }

  // Remove event listener in case of fullscreen lock.
  nsCOMPtr<EventTarget> target = do_QueryInterface(GetOwner()->GetDoc());
  if (target) {
    DebugOnly<nsresult> rv =
      target->RemoveSystemEventListener(NS_LITERAL_STRING("fullscreenchange"),
                                        mFullScreenListener,
                                        /* useCapture */ true);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "RemoveSystemEventListener failed");
  }

  mFullScreenListener = nullptr;
}

OrientationType
ScreenOrientation::DeviceType(CallerType aCallerType) const
{
  return nsContentUtils::ResistFingerprinting(aCallerType) ?
    OrientationType::Landscape_primary : mType;
}

uint16_t
ScreenOrientation::DeviceAngle(CallerType aCallerType) const
{
  return nsContentUtils::ResistFingerprinting(aCallerType) ? 0 : mAngle;
}

OrientationType
ScreenOrientation::GetType(CallerType aCallerType, ErrorResult& aRv) const
{
  if (nsContentUtils::ResistFingerprinting(aCallerType)) {
    return OrientationType::Landscape_primary;
  }

  nsIDocument* doc = GetResponsibleDocument();
  if (!doc) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return OrientationType::Portrait_primary;
  }

  return doc->CurrentOrientationType();
}

uint16_t
ScreenOrientation::GetAngle(CallerType aCallerType, ErrorResult& aRv) const
{
  if (nsContentUtils::ResistFingerprinting(aCallerType)) {
    return 0;
  }

  nsIDocument* doc = GetResponsibleDocument();
  if (!doc) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return 0;
  }

  return doc->CurrentOrientationAngle();
}

ScreenOrientation::LockPermission
ScreenOrientation::GetLockOrientationPermission(bool aCheckSandbox) const
{
  nsCOMPtr<nsPIDOMWindowInner> owner = GetOwner();
  if (!owner) {
    return LOCK_DENIED;
  }

  // Chrome can always lock the screen orientation.
  nsIDocShell* docShell = owner->GetDocShell();
  if (docShell && docShell->ItemType() == nsIDocShellTreeItem::typeChrome) {
    return LOCK_ALLOWED;
  }

  nsCOMPtr<nsIDocument> doc = owner->GetDoc();
  if (!doc || doc->Hidden()) {
    return LOCK_DENIED;
  }

  // Sandboxed without "allow-orientation-lock"
  if (aCheckSandbox && doc->GetSandboxFlags() & SANDBOXED_ORIENTATION_LOCK) {
    return LOCK_DENIED;
  }

  if (Preferences::GetBool("dom.screenorientation.testing.non_fullscreen_lock_allow",
                           false)) {
    return LOCK_ALLOWED;
  }

  // Other content must be full-screen in order to lock orientation.
  return doc->Fullscreen() ? FULLSCREEN_LOCK_ALLOWED : LOCK_DENIED;
}

nsIDocument*
ScreenOrientation::GetResponsibleDocument() const
{
  nsCOMPtr<nsPIDOMWindowInner> owner = GetOwner();
  if (!owner) {
    return nullptr;
  }

  return owner->GetDoc();
}

void
ScreenOrientation::Notify(const hal::ScreenConfiguration& aConfiguration)
{
  if (ShouldResistFingerprinting()) {
    return;
  }

  nsIDocument* doc = GetResponsibleDocument();
  if (!doc) {
    return;
  }

  ScreenOrientationInternal orientation = aConfiguration.orientation();
  if (orientation != eScreenOrientation_PortraitPrimary &&
      orientation != eScreenOrientation_PortraitSecondary &&
      orientation != eScreenOrientation_LandscapePrimary &&
      orientation != eScreenOrientation_LandscapeSecondary) {
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
    rv = mScreen->DispatchTrustedEvent(NS_LITERAL_STRING("mozorientationchange"));
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "DispatchTrustedEvent failed");
  }

  if (doc->Hidden() && !mVisibleListener) {
    mVisibleListener = new VisibleEventListener();
    rv = doc->AddSystemEventListener(NS_LITERAL_STRING("visibilitychange"),
                                     mVisibleListener, /* useCapture = */ true);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "AddSystemEventListener failed");
    return;
  }

  if (mType != doc->CurrentOrientationType()) {
    doc->SetCurrentOrientation(mType, mAngle);

    Promise* pendingPromise = doc->GetOrientationPendingPromise();
    if (pendingPromise) {
      pendingPromise->MaybeResolveWithUndefined();
      doc->SetOrientationPendingPromise(nullptr);
    }

    nsCOMPtr<nsIRunnable> runnable =
      NewRunnableMethod("dom::ScreenOrientation::DispatchChangeEvent",
                        this,
                        &ScreenOrientation::DispatchChangeEvent);
    rv = NS_DispatchToMainThread(runnable);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "NS_DispatchToMainThread failed");
  }
}

void
ScreenOrientation::UpdateActiveOrientationLock(ScreenOrientationInternal aOrientation)
{
  if (aOrientation == eScreenOrientation_None) {
    hal::UnlockScreenOrientation();
  } else {
    DebugOnly<bool> ok = hal::LockScreenOrientation(aOrientation);
    NS_WARNING_ASSERTION(ok, "hal::LockScreenOrientation failed");
  }
}

void
ScreenOrientation::DispatchChangeEvent()
{
  DebugOnly<nsresult> rv = DispatchTrustedEvent(NS_LITERAL_STRING("change"));
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "DispatchTrustedEvent failed");
}

JSObject*
ScreenOrientation::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ScreenOrientationBinding::Wrap(aCx, this, aGivenProto);
}

bool
ScreenOrientation::ShouldResistFingerprinting() const
{
  bool resist = false;
  if (nsCOMPtr<nsPIDOMWindowInner> owner = GetOwner()) {
    resist = nsContentUtils::ShouldResistFingerprinting(owner->GetDocShell());
  }
  return resist;
}

NS_IMPL_ISUPPORTS(ScreenOrientation::VisibleEventListener, nsIDOMEventListener)

NS_IMETHODIMP
ScreenOrientation::VisibleEventListener::HandleEvent(nsIDOMEvent* aEvent)
{
  // Document may have become visible, if the page is visible, run the steps
  // following the "now visible algorithm" as specified.
  nsCOMPtr<EventTarget> target = aEvent->InternalDOMEvent()->GetCurrentTarget();
  MOZ_ASSERT(target);

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(target);
  if (!doc || doc->Hidden()) {
    return NS_OK;
  }

  auto* win = nsGlobalWindow::Cast(doc->GetInnerWindow());
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

  rv = target->RemoveSystemEventListener(NS_LITERAL_STRING("visibilitychange"),
                                         this, true);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  if (doc->CurrentOrientationType() != orientation->DeviceType(CallerType::System)) {
    doc->SetCurrentOrientation(orientation->DeviceType(CallerType::System),
			       orientation->DeviceAngle(CallerType::System));

    Promise* pendingPromise = doc->GetOrientationPendingPromise();
    if (pendingPromise) {
      pendingPromise->MaybeResolveWithUndefined();
      doc->SetOrientationPendingPromise(nullptr);
    }

    nsCOMPtr<nsIRunnable> runnable =
      NewRunnableMethod("dom::ScreenOrientation::DispatchChangeEvent",
                        orientation,
                        &ScreenOrientation::DispatchChangeEvent);
    rv = NS_DispatchToMainThread(runnable);
    if (NS_WARN_IF(rv.Failed())) {
      return rv.StealNSResult();
    }
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS(ScreenOrientation::FullScreenEventListener, nsIDOMEventListener)

NS_IMETHODIMP
ScreenOrientation::FullScreenEventListener::HandleEvent(nsIDOMEvent* aEvent)
{
#ifdef DEBUG
  nsAutoString eventType;
  aEvent->GetType(eventType);

  MOZ_ASSERT(eventType.EqualsLiteral("fullscreenchange"));
#endif

  nsCOMPtr<EventTarget> target = aEvent->InternalDOMEvent()->GetCurrentTarget();
  MOZ_ASSERT(target);

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(target);
  MOZ_ASSERT(doc);

  // We have to make sure that the event we got is the event sent when
  // fullscreen is disabled because we could get one when fullscreen
  // got enabled if the lock call is done at the same moment.
  if (doc->Fullscreen()) {
    return NS_OK;
  }

  hal::UnlockScreenOrientation();

  nsresult rv = target->RemoveSystemEventListener(NS_LITERAL_STRING("fullscreenchange"),
                                                  this, true);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}
