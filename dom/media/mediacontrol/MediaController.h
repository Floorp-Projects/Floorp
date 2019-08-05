/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mediacontroller_h__
#define mozilla_dom_mediacontroller_h__

#include "nsDataHashtable.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace dom {

class BrowsingContext;

/**
 * MediaController is a class which is used to control media in the content
 * process. It's a basic interface class and you should implement you own class
 * to inherit this class. Every controller would correspond to a browsing
 * context. For example, TabMediaController corresponds to the top level
 * browsing context. In the future, we might implement MediaSessionController
 * which could correspond to any browsing context, depending on which browsing
 * context has active media session.
 */
class MediaController {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaController);

  explicit MediaController(uint64_t aContextId)
      : mBrowsingContextId(aContextId) {}

  virtual void Play() = 0;
  virtual void Pause() = 0;
  virtual void Stop() = 0;
  virtual void Shutdown() = 0;

  virtual void NotifyMediaActiveChanged(bool aActive) = 0;
  virtual void NotifyMediaAudibleChanged(bool aAudible) = 0;

  bool IsPlaying() const { return mIsPlaying; }
  uint64_t Id() const { return mBrowsingContextId; }
  virtual uint64_t ControlledMediaNum() const { return 0; }
  virtual bool IsAudible() const { return false; }

 protected:
  virtual ~MediaController() = default;

  already_AddRefed<BrowsingContext> GetContext() const;

  uint64_t mBrowsingContextId;
  bool mIsPlaying = false;
};

/**
 * TabMediaController is used to control all media in a tab. It can only be used
 * in Chrome process. Everytime media starts in the tab, it would increase the
 * number of controlled media, and also would decrease the number when media
 * stops. The media it controls might be in different content processes, so we
 * keep tracking the top level browsing context in the tab, which can be used to
 * propagate conmmands to remote content processes.
 */
class TabMediaController final : public MediaController {
 public:
  explicit TabMediaController(uint64_t aContextId);

  void Play() override;
  void Pause() override;
  void Stop() override;
  void Shutdown() override;

  uint64_t ControlledMediaNum() const override;
  bool IsAudible() const override;

  void NotifyMediaActiveChanged(bool aActive) override;
  void NotifyMediaAudibleChanged(bool aAudible) override;

 protected:
  ~TabMediaController();

 private:
  void IncreaseControlledMediaNum();
  void DecreaseControlledMediaNum();

  void Activate();
  void Deactivate();

  int64_t mControlledMediaNum = 0;
  bool mAudible = false;
};

}  // namespace dom
}  // namespace mozilla

#endif
