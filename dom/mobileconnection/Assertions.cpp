/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MozMobileConnectionBinding.h"
#include "nsIMobileConnectionService.h"

namespace mozilla {
namespace dom {

#define ASSERT_NETWORK_SELECTION_MODE_EQUALITY(webidlState, xpidlState) \
  static_assert(static_cast<int32_t>(MobileNetworkSelectionMode::webidlState) == nsIMobileConnection::xpidlState, \
                "MobileNetworkSelectionMode::" #webidlState " should equal to nsIMobileConnection::" #xpidlState)

ASSERT_NETWORK_SELECTION_MODE_EQUALITY(Automatic, NETWORK_SELECTION_MODE_AUTOMATIC);
ASSERT_NETWORK_SELECTION_MODE_EQUALITY(Manual, NETWORK_SELECTION_MODE_MANUAL);

#undef ASSERT_NETWORK_SELECTION_MODE_EQUALITY

} // namespace dom
} // namespace mozilla

