/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SpeakerManager.h"

#include "mozilla/Services.h"

#include "mozilla/dom/Event.h"

#include "AudioChannelService.h"
#include "nsIDocShell.h"
#include "nsIDOMClassInfo.h"
#include "nsIDOMEventListener.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPermissionManager.h"
#include "SpeakerManagerService.h"

namespace mozilla {
namespace dom {

NS_IMPL_QUERY_INTERFACE_INHERITED(SpeakerManager, DOMEventTargetHelper,
                                  nsIDOMEventListener)
NS_IMPL_ADDREF_INHERITED(SpeakerManager, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(SpeakerManager, DOMEventTargetHelper)

SpeakerManager::SpeakerManager()
  : mForcespeaker(false)
  , mVisible(false)
{
  SpeakerManagerService *service =
    SpeakerManagerService::GetOrCreateSpeakerManagerService();
  MOZ_ASSERT(service);
  service->RegisterSpeakerManager(this);
}

SpeakerManager::~SpeakerManager()
{
  SpeakerManagerService *service = SpeakerManagerService::GetOrCreateSpeakerManagerService();
  MOZ_ASSERT(service);

  service->UnRegisterSpeakerManager(this);
  nsCOMPtr<EventTarget> target = do_QueryInterface(GetOwner());
  NS_ENSURE_TRUE_VOID(target);

  target->RemoveSystemEventListener(NS_LITERAL_STRING("visibilitychange"),
                                    this,
                                    /* useCapture = */ true);
}

bool
SpeakerManager::Speakerforced()
{
  // If a background app calls forcespeaker=true that doesn't change anything.
  // 'speakerforced' remains false everywhere.
  if (mForcespeaker && !mVisible) {
    return false;
  }
  SpeakerManagerService *service = SpeakerManagerService::GetOrCreateSpeakerManagerService();
  MOZ_ASSERT(service);
  return service->GetSpeakerStatus();

}

bool
SpeakerManager::Forcespeaker()
{
  return mForcespeaker;
}

void
SpeakerManager::SetForcespeaker(bool aEnable)
{
  SpeakerManagerService *service = SpeakerManagerService::GetOrCreateSpeakerManagerService();
  MOZ_ASSERT(service);

  service->ForceSpeaker(aEnable, mVisible);
  mForcespeaker = aEnable;
}

void
SpeakerManager::DispatchSimpleEvent(const nsAString& aStr)
{
  MOZ_ASSERT(NS_IsMainThread(), "Not running on main thread");
  nsresult rv = CheckInnerWindowCorrectness();
  if (NS_FAILED(rv)) {
    return;
  }

  RefPtr<Event> event = NS_NewDOMEvent(this, nullptr, nullptr);
  event->InitEvent(aStr, false, false);
  event->SetTrusted(true);

  rv = DispatchDOMEvent(nullptr, event, nullptr, nullptr);
  if (NS_FAILED(rv)) {
    NS_ERROR("Failed to dispatch the event!!!");
    return;
  }
}

void
SpeakerManager::Init(nsPIDOMWindowInner* aWindow)
{
  BindToOwner(aWindow);

  nsCOMPtr<nsIDocShell> docshell = GetOwner()->GetDocShell();
  NS_ENSURE_TRUE_VOID(docshell);
  docshell->GetIsActive(&mVisible);

  nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(GetOwner());
  NS_ENSURE_TRUE_VOID(target);

  target->AddSystemEventListener(NS_LITERAL_STRING("visibilitychange"),
                                 this,
                                 /* useCapture = */ true,
                                 /* wantsUntrusted = */ false);
}

nsPIDOMWindowInner*
SpeakerManager::GetParentObject() const
{
  return GetOwner();
}

/* static */ already_AddRefed<SpeakerManager>
SpeakerManager::Constructor(const GlobalObject& aGlobal, ErrorResult& aRv)
{
  nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(aGlobal.GetAsSupports());
  if (!sgo) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowInner> ownerWindow = do_QueryInterface(aGlobal.GetAsSupports());
  if (!ownerWindow) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIPermissionManager> permMgr = services::GetPermissionManager();
  NS_ENSURE_TRUE(permMgr, nullptr);

  uint32_t permission = nsIPermissionManager::DENY_ACTION;
  nsresult rv =
    permMgr->TestPermissionFromWindow(ownerWindow, "speaker-control",
                                      &permission);
  NS_ENSURE_SUCCESS(rv, nullptr);

  if (permission != nsIPermissionManager::ALLOW_ACTION) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  RefPtr<SpeakerManager> object = new SpeakerManager();
  object->Init(ownerWindow);
  return object.forget();
}

JSObject*
SpeakerManager::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MozSpeakerManagerBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMETHODIMP
SpeakerManager::HandleEvent(nsIDOMEvent* aEvent)
{
  nsAutoString type;
  aEvent->GetType(type);

  if (!type.EqualsLiteral("visibilitychange")) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocShell> docshell = do_GetInterface(GetOwner());
  NS_ENSURE_TRUE(docshell, NS_ERROR_FAILURE);
  docshell->GetIsActive(&mVisible);

  // If an app that has called forcespeaker=true is switched
  // from the background to the foreground 'speakerforced'
  // switches to true in all apps. I.e. the app doesn't have to
  // call forcespeaker=true again when it comes into foreground.
  SpeakerManagerService *service =
    SpeakerManagerService::GetOrCreateSpeakerManagerService();
  MOZ_ASSERT(service);

  if (mVisible && mForcespeaker) {
    service->ForceSpeaker(mForcespeaker, mVisible);
  }
  // If an application that has called forcespeaker=true, but no audio is
  // currently playing in the app itself, if application switch to
  // the background, we switch 'speakerforced' to false.
  if (!mVisible && mForcespeaker) {
    RefPtr<AudioChannelService> audioChannelService =
      AudioChannelService::GetOrCreate();
    if (audioChannelService && !audioChannelService->AnyAudioChannelIsActive()) {
      service->ForceSpeaker(false, mVisible);
    }
  }
  return NS_OK;
}

void
SpeakerManager::SetAudioChannelActive(bool isActive)
{
  if (mForcespeaker) {
    SpeakerManagerService *service =
      SpeakerManagerService::GetOrCreateSpeakerManagerService();
    MOZ_ASSERT(service);
    service->ForceSpeaker(isActive, mVisible);
  }
}

} // namespace dom
} // namespace mozilla
