/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MEDIACONTROL_MEDIAPLAYBACKSTATUS_H_
#define DOM_MEDIA_MEDIACONTROL_MEDIAPLAYBACKSTATUS_H_

#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "nsISupportsImpl.h"
#include "nsTArray.h"
#include "nsTHashMap.h"

namespace mozilla::dom {

/**
 * This enum is used to update controlled media state to the media controller in
 * the chrome process.
 * `eStarted`: media has successfully registered to the content media controller
 * `ePlayed` : media has started playing
 * `ePaused` : media has paused playing, but still can be resumed by content
 *             media controller
 * `eStopped`: media has unregistered from the content media controller, we can
 *             not control it anymore
 */
enum class MediaPlaybackState : uint32_t {
  eStarted,
  ePlayed,
  ePaused,
  eStopped,
};

/**
 * This enum is used to update controlled media audible audible state to the
 * media controller in the chrome process.
 */
enum class MediaAudibleState : bool {
  eInaudible = false,
  eAudible = true,
};

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
 *
 * As we know each context's audible state, we can decide which context should
 * owns the audio focus when multiple contexts are all playing audible media at
 * the same time. In that cases, the latest context that plays media would own
 * the audio focus. When the context owning the audio focus is destroyed, we
 * would see if there is another other context still playing audible media, and
 * switch the audio focus to another context.
 */
class MediaPlaybackStatus final {
 public:
  void UpdateMediaPlaybackState(uint64_t aContextId, MediaPlaybackState aState);
  void UpdateMediaAudibleState(uint64_t aContextId, MediaAudibleState aState);

  bool IsPlaying() const;
  bool IsAudible() const;
  bool IsAnyMediaBeingControlled() const;

  Maybe<uint64_t> GetAudioFocusOwnerContextId() const;

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
#ifndef FUZZING_SNAPSHOT
      MOZ_DIAGNOSTIC_ASSERT(mControlledMediaNum < UINT_MAX);
#endif
      mControlledMediaNum++;
    }
    void DecreaseControlledMediaNum() {
#ifndef FUZZING_SNAPSHOT
      MOZ_DIAGNOSTIC_ASSERT(mControlledMediaNum > 0);
#endif
      mControlledMediaNum--;
    }
    void IncreasePlayingMediaNum() {
#ifndef FUZZING_SNAPSHOT
      MOZ_DIAGNOSTIC_ASSERT(mPlayingMediaNum < mControlledMediaNum);
#endif
      mPlayingMediaNum++;
    }
    void DecreasePlayingMediaNum() {
#ifndef FUZZING_SNAPSHOT
      MOZ_DIAGNOSTIC_ASSERT(mPlayingMediaNum > 0);
#endif
      mPlayingMediaNum--;
    }
    void IncreaseAudibleMediaNum() {
#ifndef FUZZING_SNAPSHOT
      MOZ_DIAGNOSTIC_ASSERT(mAudibleMediaNum < mPlayingMediaNum);
#endif
      mAudibleMediaNum++;
    }
    void DecreaseAudibleMediaNum() {
#ifndef FUZZING_SNAPSHOT
      MOZ_DIAGNOSTIC_ASSERT(mAudibleMediaNum > 0);
#endif
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

  void ChooseNewContextToOwnAudioFocus();
  void SetOwningAudioFocusContextId(Maybe<uint64_t>&& aContextId);
  bool IsContextOwningAudioFocus(uint64_t aContextId) const;
  bool ShouldRequestAudioFocusForInfo(const ContextMediaInfo& aInfo) const;
  bool ShouldAbandonAudioFocusForInfo(const ContextMediaInfo& aInfo) const;

  // This contains all the media status of browsing contexts within a tab.
  nsTHashMap<uint64_t, UniquePtr<ContextMediaInfo>> mContextInfoMap;
  Maybe<uint64_t> mOwningAudioFocusContextId;
};

}  // namespace mozilla::dom

#endif  //  DOM_MEDIA_MEDIACONTROL_MEDIAPLAYBACKSTATUS_H_
