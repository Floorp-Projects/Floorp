/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_system_AudioChannelManager_h
#define mozilla_dom_system_AudioChannelManager_h

#include "mozilla/HalTypes.h"
#include "nsIAudioChannelManager.h"
#include "nsDOMEventTargetHelper.h"

namespace mozilla {
namespace hal {
class SwitchEvent;
typedef Observer<SwitchEvent> SwitchObserver;
} // namespace hal

namespace dom {
namespace system {

class AudioChannelManager : public nsDOMEventTargetHelper
                          , public nsIAudioChannelManager
                          , public hal::SwitchObserver
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIAUDIOCHANNELMANAGER
  NS_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper::)

  AudioChannelManager();
  virtual ~AudioChannelManager();

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AudioChannelManager,
                                           nsDOMEventTargetHelper)
  void Notify(const hal::SwitchEvent& aEvent);

  void Init(nsPIDOMWindow* aWindow);
private:
  hal::SwitchState mState;
};

} // namespace system
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_system_AudioChannelManager_h
