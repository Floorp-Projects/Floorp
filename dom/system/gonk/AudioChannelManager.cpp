/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIDOMClassInfo.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventListener.h"
#include "nsPIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIPermissionManager.h"
#include "nsIInterfaceRequestorUtils.h"
#include "AudioChannelManager.h"
#include "mozilla/dom/AudioChannelManagerBinding.h"
#include "mozilla/Services.h"

using namespace mozilla::hal;

namespace mozilla {
namespace dom {
namespace system {

NS_IMPL_QUERY_INTERFACE_INHERITED(AudioChannelManager, DOMEventTargetHelper,
                                  nsIDOMEventListener)
NS_IMPL_ADDREF_INHERITED(AudioChannelManager, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(AudioChannelManager, DOMEventTargetHelper)

AudioChannelManager::AudioChannelManager()
  : mState(SWITCH_STATE_UNKNOWN)
  , mVolumeChannel(-1)
{
  RegisterSwitchObserver(SWITCH_HEADPHONES, this);
  mState = GetCurrentSwitchState(SWITCH_HEADPHONES);
}

AudioChannelManager::~AudioChannelManager()
{
  UnregisterSwitchObserver(SWITCH_HEADPHONES, this);

  nsCOMPtr<EventTarget> target = do_QueryInterface(GetOwner());
  NS_ENSURE_TRUE_VOID(target);

  target->RemoveSystemEventListener(NS_LITERAL_STRING("visibilitychange"),
                                    this,
                                    /* useCapture = */ true);
}

void
AudioChannelManager::Init(nsPIDOMWindow* aWindow)
{
  BindToOwner(aWindow->IsOuterWindow() ?
              aWindow->GetCurrentInnerWindow() : aWindow);

  nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(GetOwner());
  NS_ENSURE_TRUE_VOID(target);

  target->AddSystemEventListener(NS_LITERAL_STRING("visibilitychange"),
                                 this,
                                 /* useCapture = */ true,
                                 /* wantsUntrusted = */ false);
}

JSObject*
AudioChannelManager::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return AudioChannelManagerBinding::Wrap(aCx, this, aGivenProto);
}

void
AudioChannelManager::Notify(const SwitchEvent& aEvent)
{
  mState = aEvent.status();

  DispatchTrustedEvent(NS_LITERAL_STRING("headphoneschange"));
}

bool
AudioChannelManager::SetVolumeControlChannel(const nsAString& aChannel)
{
  if (aChannel.EqualsASCII("publicnotification")) {
    return false;
  }

  AudioChannel newChannel = AudioChannelService::GetAudioChannel(aChannel);

  // Only normal channel doesn't need permission.
  if (newChannel != AudioChannel::Normal) {
    nsCOMPtr<nsIPermissionManager> permissionManager =
      services::GetPermissionManager();
    if (!permissionManager) {
      return false;
    }
    uint32_t perm = nsIPermissionManager::UNKNOWN_ACTION;
    permissionManager->TestPermissionFromWindow(GetOwner(),
      nsCString(NS_LITERAL_CSTRING("audio-channel-") +
      NS_ConvertUTF16toUTF8(aChannel)).get(), &perm);
    if (perm != nsIPermissionManager::ALLOW_ACTION) {
      return false;
    }
  }

  if (mVolumeChannel == (int32_t)newChannel) {
    return true;
  }

  mVolumeChannel = (int32_t)newChannel;

  NotifyVolumeControlChannelChanged();
  return true;
}

bool
AudioChannelManager::GetVolumeControlChannel(nsAString & aChannel)
{
  if (mVolumeChannel >= 0) {
    AudioChannelService::GetAudioChannelString(
                                      static_cast<AudioChannel>(mVolumeChannel),
                                      aChannel);
  } else {
    aChannel.AssignASCII("");
  }

  return true;
}

void
AudioChannelManager::NotifyVolumeControlChannelChanged()
{
  nsCOMPtr<nsIDocShell> docshell = do_GetInterface(GetOwner());
  NS_ENSURE_TRUE_VOID(docshell);

  bool isActive = false;
  docshell->GetIsActive(&isActive);

  nsRefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
  if (isActive) {
    service->SetDefaultVolumeControlChannel(mVolumeChannel, isActive);
  } else {
    service->SetDefaultVolumeControlChannel(-1, isActive);
  }
}

NS_IMETHODIMP
AudioChannelManager::HandleEvent(nsIDOMEvent* aEvent)
{
  nsAutoString type;
  aEvent->GetType(type);

  if (type.EqualsLiteral("visibilitychange")) {
    NotifyVolumeControlChannelChanged();
  }
  return NS_OK;
}

} // namespace system
} // namespace dom
} // namespace mozilla
