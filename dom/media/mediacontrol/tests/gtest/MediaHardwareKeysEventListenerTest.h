/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MEDIAHARDWAREKEYSEVENTLISTENERTEST_H_
#define DOM_MEDIA_MEDIAHARDWAREKEYSEVENTLISTENERTEST_H_

#include "MediaControlKeysEvent.h"
#include "mozilla/Maybe.h"

namespace mozilla {
namespace dom {

class MediaHardwareKeysEventListenerTest
    : public MediaControlKeysEventListener {
 public:
  NS_INLINE_DECL_REFCOUNTING(MediaHardwareKeysEventListenerTest, override)

  void Clear() { mReceivedEvent = mozilla::Nothing(); }

  void OnKeyPressed(MediaControlKeysEvent aKeyEvent) override {
    mReceivedEvent = mozilla::Some(aKeyEvent);
  }
  bool IsResultEqualTo(MediaControlKeysEvent aResult) const {
    if (mReceivedEvent) {
      return *mReceivedEvent == aResult;
    }
    return false;
  }
  bool IsReceivedResult() const { return mReceivedEvent.isSome(); }

 private:
  ~MediaHardwareKeysEventListenerTest() = default;
  mozilla::Maybe<MediaControlKeysEvent> mReceivedEvent;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_MEDIA_MEDIAHARDWAREKEYSEVENTLISTENERTEST_H_
