/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_system_AudioChannelManager_h
#define mozilla_dom_system_AudioChannelManager_h

#include "mozilla/Hal.h"
#include "mozilla/HalTypes.h"
#include "nsDOMEventTargetHelper.h"

namespace mozilla {
namespace hal {
class SwitchEvent;
typedef Observer<SwitchEvent> SwitchObserver;
} // namespace hal

namespace dom {
namespace system {

class AudioChannelManager : public nsDOMEventTargetHelper
                          , public hal::SwitchObserver
{
public:
  AudioChannelManager();
  virtual ~AudioChannelManager();

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

  IMPL_EVENT_HANDLER(headphoneschange)

private:
  hal::SwitchState mState;
};

} // namespace system
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_system_AudioChannelManager_h
