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

using namespace mozilla::hal;

namespace mozilla {
namespace dom {
namespace system {

NS_IMPL_QUERY_INTERFACE_INHERITED1(AudioChannelManager, DOMEventTargetHelper,
                                   nsIDOMEventListener)
NS_IMPL_ADDREF_INHERITED(AudioChannelManager, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(AudioChannelManager, DOMEventTargetHelper)

AudioChannelManager::AudioChannelManager()
  : mState(SWITCH_STATE_UNKNOWN)
  , mVolumeChannel(AUDIO_CHANNEL_DEFAULT)
{
  RegisterSwitchObserver(SWITCH_HEADPHONES, this);
  mState = GetCurrentSwitchState(SWITCH_HEADPHONES);
  SetIsDOMBinding();
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
AudioChannelManager::WrapObject(JSContext* aCx)
{
  return AudioChannelManagerBinding::Wrap(aCx, this);
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

  AudioChannelType oldVolumeChannel = mVolumeChannel;
  // Only normal channel doesn't need permission.
  if (aChannel.EqualsASCII("normal")) {
    mVolumeChannel = AUDIO_CHANNEL_NORMAL;
  } else {
    nsCOMPtr<nsIPermissionManager> permissionManager =
      do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
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

    if (aChannel.EqualsASCII("content")) {
      mVolumeChannel = AUDIO_CHANNEL_CONTENT;
    } else if (aChannel.EqualsASCII("notification")) {
      mVolumeChannel = AUDIO_CHANNEL_NOTIFICATION;
    } else if (aChannel.EqualsASCII("alarm")) {
      mVolumeChannel = AUDIO_CHANNEL_ALARM;
    } else if (aChannel.EqualsASCII("telephony")) {
      mVolumeChannel = AUDIO_CHANNEL_TELEPHONY;
    } else if (aChannel.EqualsASCII("ringer")) {
      mVolumeChannel = AUDIO_CHANNEL_RINGER;
    }
  }

  if (oldVolumeChannel == mVolumeChannel) {
    return true;
  }
  NotifyVolumeControlChannelChanged();
  return true;
}

bool
AudioChannelManager::GetVolumeControlChannel(nsAString & aChannel)
{
  if (mVolumeChannel == AUDIO_CHANNEL_NORMAL) {
    aChannel.AssignASCII("normal");
  } else if (mVolumeChannel == AUDIO_CHANNEL_CONTENT) {
    aChannel.AssignASCII("content");
  } else if (mVolumeChannel == AUDIO_CHANNEL_NOTIFICATION) {
    aChannel.AssignASCII("notification");
  } else if (mVolumeChannel == AUDIO_CHANNEL_ALARM) {
    aChannel.AssignASCII("alarm");
  } else if (mVolumeChannel == AUDIO_CHANNEL_TELEPHONY) {
    aChannel.AssignASCII("telephony");
  } else if (mVolumeChannel == AUDIO_CHANNEL_RINGER) {
    aChannel.AssignASCII("ringer");
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

  AudioChannelService* service = AudioChannelService::GetAudioChannelService();
  if (isActive) {
    service->SetDefaultVolumeControlChannel(mVolumeChannel, isActive);
  } else {
    service->SetDefaultVolumeControlChannel(AUDIO_CHANNEL_DEFAULT, isActive);
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
