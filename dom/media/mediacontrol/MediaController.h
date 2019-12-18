/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MEDIACONTROL_MEDIACONTROLLER_H_
#define DOM_MEDIA_MEDIACONTROL_MEDIACONTROLLER_H_

#include "nsDataHashtable.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace dom {

class BrowsingContext;

enum class MediaControlActions : uint32_t {
  ePlay,
  ePause,
  eStop,
  /* do not use this, it's used to indicate the last value of enum */
  eActionsNum,
};

/**
 * MediaController is a class, which is used to control all media within a tab.
 * It can only be used in Chrome process and the controlled media are usually
 * in the content process (unless we disable e10s).
 *
 * Each tab would have only one media controller, they are 1-1 corresponding
 * relationship, we use tab's top-level browsing context ID to initialize the
 * controller and use that as its ID.
 *
 * Whenever controlled media started, we would notify the controller to increase
 * or decrease the amount of its controlled media when its controlled media
 * started or stopped.
 *
 * Once the controller started, which means it has controlled some media, then
 * we can use its controlling methods, such as `Play()`, `Pause()` to control
 * the media within the tab. If there is at least one controlled media playing
 * in the tab, then we would say the controller is `playing`. If there is at
 * least one controlled media is playing and audible, then we would say the
 * controller is `audible`.
 *
 * Note that, if we don't enable audio competition, then we might have multiple
 * tabs playing media at the same time, we can use the ID to query the specific
 * controller from `MediaControlService`.
 */
class MediaController final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaController);

  explicit MediaController(uint64_t aContextId);

  void Play();
  void Pause();
  void Stop();
  void Shutdown();

  uint64_t Id() const;
  bool IsPlaying() const;
  bool IsAudible() const;
  uint64_t ControlledMediaNum() const;

  // These methods are only being used to notify the state changes of controlled
  // media in ContentParent or MediaControlUtils.
  void NotifyMediaActiveChanged(bool aActive);
  void NotifyMediaAudibleChanged(bool aAudible);

 private:
  ~MediaController();

  void UpdateMediaActionToContentMediaIfNeeded(MediaControlActions aAction);
  void IncreaseControlledMediaNum();
  void DecreaseControlledMediaNum();

  void Activate();
  void Deactivate();

  uint64_t mBrowsingContextId;
  bool mIsPlaying = false;
  bool mAudible = false;
  int64_t mControlledMediaNum = 0;
};

}  // namespace dom
}  // namespace mozilla

#endif
