/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MEDIAKEYLISTENERTEST_H_
#define DOM_MEDIA_MEDIAKEYLISTENERTEST_H_

#include "MediaControlKeySource.h"
#include "mozilla/Maybe.h"

namespace mozilla {
namespace dom {

class MediaKeyListenerTest : public MediaControlKeyListener {
 public:
  NS_INLINE_DECL_REFCOUNTING(MediaKeyListenerTest, override)

  void Clear() { mReceivedKey = mozilla::Nothing(); }

  void OnKeyPressed(MediaControlKey aKey) override {
    mReceivedKey = mozilla::Some(aKey);
  }
  bool IsResultEqualTo(MediaControlKey aResult) const {
    if (mReceivedKey) {
      return *mReceivedKey == aResult;
    }
    return false;
  }
  bool IsReceivedResult() const { return mReceivedKey.isSome(); }

 private:
  ~MediaKeyListenerTest() = default;
  mozilla::Maybe<MediaControlKey> mReceivedKey;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_MEDIA_MEDIAKEYLISTENERTEST_H_
