/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_system_AudioChannelManager_h
#define mozilla_dom_system_AudioChannelManager_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/Hal.h"
#include "mozilla/HalTypes.h"
#include "AudioChannelService.h"

namespace mozilla {
namespace hal {
class SwitchEvent;
typedef Observer<SwitchEvent> SwitchObserver;
} // namespace hal

namespace dom {
namespace system {

class AudioChannelManager MOZ_FINAL
  : public DOMEventTargetHelper
  , public hal::SwitchObserver
  , public nsIDOMEventListener
{
public:
  AudioChannelManager();
  virtual ~AudioChannelManager();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMEVENTLISTENER

  void Notify(const hal::SwitchEvent& aEvent);

  void Init(nsPIDOMWindow* aWindow);

  /**
   * WebIDL Interface
   */

  nsPIDOMWindow* GetParentObject() const
  {
     return GetOwner();
  }

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  bool Headphones() const
  {
    // Bug 929139 - Remove the assert check for SWITCH_STATE_UNKNOWN.
    // If any devices (ex: emulator) didn't have the corresponding sys node for
    // headset switch state then GonkSwitch will report the unknown state.
    // So it is possible to get unknown state here.
    return mState != hal::SWITCH_STATE_OFF &&
           mState != hal::SWITCH_STATE_UNKNOWN;
  }

  bool SetVolumeControlChannel(const nsAString& aChannel);

  bool GetVolumeControlChannel(nsAString& aChannel);

  IMPL_EVENT_HANDLER(headphoneschange)

private:
  void NotifyVolumeControlChannelChanged();

  hal::SwitchState mState;
  AudioChannelType mVolumeChannel;
};

} // namespace system
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_system_AudioChannelManager_h
