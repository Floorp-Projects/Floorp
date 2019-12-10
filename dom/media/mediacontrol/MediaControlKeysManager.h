/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MEDIACONTROL_MEDIACONTROLKEYSMANAGER_H_
#define DOM_MEDIA_MEDIACONTROL_MEDIACONTROLKEYSMANAGER_H_

#include "MediaControlKeysEvent.h"
#include "MediaEventSource.h"

namespace mozilla {
namespace dom {

/**
 * MediaControlKeysManager is a wrapper of MediaControlKeysEventSource, which
 * is used to manage creating and destroying a real media keys event source.
 *
 * It monitors the amount of the media controller in MediaService, and would
 * create the event source when there is any existing controller and destroy it
 * when there is no controller.
 */
class MediaControlKeysManager final : public MediaControlKeysEventSource,
                                      public MediaControlKeysEventListener {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  MediaControlKeysManager() = default;

  // MediaControlKeysEventSource methods
  bool Open() override;
  bool IsOpened() const override;

  // MediaControlKeysEventListener methods
  void OnKeyPressed(MediaControlKeysEvent aKeyEvent) override;

  // The callback function for monitoring the media controller amount changed
  // event.
  void ControllerAmountChanged(uint64_t aControllerAmount);

 private:
  ~MediaControlKeysManager();
  void StartMonitoringControlKeys();
  void StopMonitoringControlKeys();
  RefPtr<MediaControlKeysEventSource> mEventSource;
  MediaEventListener mControllerAmountChangedListener;
};

}  // namespace dom
}  // namespace mozilla

#endif
