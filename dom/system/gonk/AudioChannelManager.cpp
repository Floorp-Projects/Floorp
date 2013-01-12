/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Hal.h"
#include "mozilla/HalTypes.h"
#include "AudioChannelManager.h"
#include "nsIDOMClassInfo.h"

using namespace mozilla::hal;

#ifdef MOZ_AUDIO_CHANNEL_MANAGER
DOMCI_DATA(AudioChannelManager, mozilla::dom::system::AudioChannelManager)
#endif

namespace mozilla {
namespace dom {
namespace system {

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(AudioChannelManager,
                                                  nsDOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(AudioChannelManager,
                                                nsDOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(AudioChannelManager, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(AudioChannelManager, nsDOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(AudioChannelManager)
  NS_INTERFACE_MAP_ENTRY(nsIAudioChannelManager)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(AudioChannelManager)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

AudioChannelManager::AudioChannelManager()
  : mState(SWITCH_STATE_UNKNOWN)
{
  RegisterSwitchObserver(SWITCH_HEADPHONES, this);
  mState = GetCurrentSwitchState(SWITCH_HEADPHONES);
}

AudioChannelManager::~AudioChannelManager()
{
  UnregisterSwitchObserver(SWITCH_HEADPHONES, this);
}

void
AudioChannelManager::Init(nsPIDOMWindow* aWindow)
{
  BindToOwner(aWindow->IsOuterWindow() ?
    aWindow->GetCurrentInnerWindow() : aWindow);
}

/* readonly attribute boolean headphones; */
NS_IMETHODIMP
AudioChannelManager::GetHeadphones(bool *aHeadphones)
{
  *aHeadphones = mState == SWITCH_STATE_ON ? true : false;
  return NS_OK;
}

NS_IMPL_EVENT_HANDLER(AudioChannelManager, headphoneschange)

void
AudioChannelManager::Notify(const SwitchEvent& aEvent)
{
  if (aEvent.status() == SWITCH_STATE_ON ||
      aEvent.status() == SWITCH_STATE_HEADSET ||
      aEvent.status() == SWITCH_STATE_HEADPHONE) {
    mState = SWITCH_STATE_ON;
  } else {
    mState = SWITCH_STATE_OFF;
  }

  DispatchTrustedEvent(NS_LITERAL_STRING("headphoneschange"));
}

} // namespace system
} // namespace dom
} // namespace mozilla
