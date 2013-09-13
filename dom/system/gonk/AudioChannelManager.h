/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_system_AudioChannelManager_h
#define mozilla_dom_system_AudioChannelManager_h

#include "mozilla/Hal.h"
#include "mozilla/HalTypes.h"
#include "nsDOMEventTargetHelper.h"
#include "AudioChannelService.h"

namespace mozilla {
namespace hal {
class SwitchEvent;
typedef Observer<SwitchEvent> SwitchObserver;
} // namespace hal

namespace dom {
namespace system {

class AudioChannelManager MOZ_FINAL
  : public nsDOMEventTargetHelper
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

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  bool Headphones() const
  {
    MOZ_ASSERT(mState != hal::SWITCH_STATE_UNKNOWN);
    return mState != hal::SWITCH_STATE_OFF;
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
