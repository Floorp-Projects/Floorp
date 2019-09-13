/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "MediaHardwareKeysEvent.h"

using namespace mozilla::dom;

TEST(MediaHardwareKeysEvent, TestAddOrRemoveListener)
{
  RefPtr<MediaHardwareKeysEventSource> source =
      new MediaHardwareKeysEventSource();
  ASSERT_TRUE(source->GetListenersNum() == 0);

  RefPtr<MediaHardwareKeysEventListener> listener =
      new MediaHardwareKeysEventListener();

  source->AddListener(listener);
  ASSERT_TRUE(source->GetListenersNum() == 1);

  source->RemoveListener(listener);
  ASSERT_TRUE(source->GetListenersNum() == 0);
}
