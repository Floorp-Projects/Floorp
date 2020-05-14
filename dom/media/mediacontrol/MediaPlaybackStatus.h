/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MEDIACONTROL_MEDIAPLAYBACKSTATUS_H_
#define DOM_MEDIA_MEDIACONTROL_MEDIAPLAYBACKSTATUS_H_

#include "ContentMediaController.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "nsDataHashtable.h"
#include "nsISupportsImpl.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

/**
 * MediaPlaybackStatus is an internal module for the media controller, it
 * represents a tab's media related status, such like "does the tab contain any
 * controlled media? is the tab playing? is the tab audible?".
 *
 * The reason we need this class is that we would like to encapsulate the
 * details of determining the tab's media status. A tab can contains multiple
 * browsing contexts, and each browsing context can have different media status.
 * The final media status would be decided by checking all those context status.
 *
 * Use `UpdateMediaXXXState()` to update controlled media status, and use
 * `IsXXX()` methods to acquire the playback status of the tab.
 */
class MediaPlaybackStatus final {
 public:
  void UpdateMediaPlaybackState(uint64_t aContextId, MediaPlaybackState aState);
  void UpdateMediaAudibleState(uint64_t aContextId, MediaAudibleState aState);

  bool IsPlaying() const;
  bool IsAudible() const;
  bool IsAnyMediaBeingControlled() const;

 private:
  /**
   * This internal class stores detailed media status of controlled media for
   * a browsing context.
   */
  class ContextMediaInfo final {
   public:
    explicit ContextMediaInfo(uint64_t aContextId) : mContextId(aContextId) {}
    ~ContextMediaInfo() = default;

    void IncreaseControlledMediaNum() {
      MOZ_DIAGNOSTIC_ASSERT(mControlledMediaNum < UINT_MAX);
      mControlledMediaNum++;
    }
    void DecreaseControlledMediaNum() {
      MOZ_DIAGNOSTIC_ASSERT(mControlledMediaNum > 0);
      mControlledMediaNum--;
    }
    void IncreasePlayingMediaNum() {
      MOZ_DIAGNOSTIC_ASSERT(mPlayingMediaNum < mControlledMediaNum);
      mPlayingMediaNum++;
    }
    void DecreasePlayingMediaNum() {
      MOZ_DIAGNOSTIC_ASSERT(mPlayingMediaNum > 0);
      mPlayingMediaNum--;
    }
    void IncreaseAudibleMediaNum() {
      MOZ_DIAGNOSTIC_ASSERT(mAudibleMediaNum < mPlayingMediaNum);
      mAudibleMediaNum++;
    }
    void DecreaseAudibleMediaNum() {
      MOZ_DIAGNOSTIC_ASSERT(mAudibleMediaNum > 0);
      mAudibleMediaNum--;
    }
    bool IsPlaying() const { return mPlayingMediaNum > 0; }
    bool IsAudible() const { return mAudibleMediaNum > 0; }
    bool IsAnyMediaBeingControlled() const { return mControlledMediaNum > 0; }
    uint64_t Id() const { return mContextId; }

   private:
    /**
     * The possible value for those three numbers should follow this rule,
     * mControlledMediaNum >= mPlayingMediaNum >= mAudibleMediaNum
     */
    uint32_t mControlledMediaNum = 0;
    uint32_t mAudibleMediaNum = 0;
    uint32_t mPlayingMediaNum = 0;
    uint64_t mContextId = 0;
  };

  ContextMediaInfo& GetNotNullContextInfo(uint64_t aContextId);
  void DestroyContextInfo(uint64_t aContextId);

  // This contains all the media status of browsing contexts within a tab.
  nsDataHashtable<nsUint64HashKey, UniquePtr<ContextMediaInfo>> mContextInfoMap;
};

}  // namespace dom
}  // namespace mozilla

#endif  //  DOM_MEDIA_MEDIACONTROL_MEDIAPLAYBACKSTATUS_H_
