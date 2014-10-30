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

#define ASSERT_MOBILE_RADIO_STATE_EQUALITY(webidlState, xpidlState) \
  static_assert(static_cast<int32_t>(MobileRadioState::webidlState) == nsIMobileConnection::xpidlState, \
                "MobileRadioState::" #webidlState " should equal to nsIMobileConnection::" #xpidlState)

ASSERT_MOBILE_RADIO_STATE_EQUALITY(Enabling, MOBILE_RADIO_STATE_ENABLING);
ASSERT_MOBILE_RADIO_STATE_EQUALITY(Enabled, MOBILE_RADIO_STATE_ENABLED);
ASSERT_MOBILE_RADIO_STATE_EQUALITY(Disabling, MOBILE_RADIO_STATE_DISABLING);
ASSERT_MOBILE_RADIO_STATE_EQUALITY(Disabled, MOBILE_RADIO_STATE_DISABLED);

#undef ASSERT_MOBILE_RADIO_STATE_EQUALITY

#define ASSERT_PREFERRED_NETWORK_TYPE_EQUALITY(webidlState, xpidlState) \
  static_assert(static_cast<int32_t>(MobilePreferredNetworkType::webidlState) == nsIMobileConnection::xpidlState, \
                "MobilePreferredNetworkType::" #webidlState " should equal to nsIMobileConnection::" #xpidlState)

ASSERT_PREFERRED_NETWORK_TYPE_EQUALITY(Wcdma_gsm, PREFERRED_NETWORK_TYPE_WCDMA_GSM);
ASSERT_PREFERRED_NETWORK_TYPE_EQUALITY(Gsm, PREFERRED_NETWORK_TYPE_GSM_ONLY);
ASSERT_PREFERRED_NETWORK_TYPE_EQUALITY(Wcdma, PREFERRED_NETWORK_TYPE_WCDMA_ONLY);
ASSERT_PREFERRED_NETWORK_TYPE_EQUALITY(Wcdma_gsm_auto, PREFERRED_NETWORK_TYPE_WCDMA_GSM_AUTO);
ASSERT_PREFERRED_NETWORK_TYPE_EQUALITY(Cdma_evdo, PREFERRED_NETWORK_TYPE_CDMA_EVDO);
ASSERT_PREFERRED_NETWORK_TYPE_EQUALITY(Cdma, PREFERRED_NETWORK_TYPE_CDMA_ONLY);
ASSERT_PREFERRED_NETWORK_TYPE_EQUALITY(Evdo, PREFERRED_NETWORK_TYPE_EVDO_ONLY);
ASSERT_PREFERRED_NETWORK_TYPE_EQUALITY(Wcdma_gsm_cdma_evdo, PREFERRED_NETWORK_TYPE_WCDMA_GSM_CDMA_EVDO);
ASSERT_PREFERRED_NETWORK_TYPE_EQUALITY(Lte_cdma_evdo, PREFERRED_NETWORK_TYPE_LTE_CDMA_EVDO);
ASSERT_PREFERRED_NETWORK_TYPE_EQUALITY(Lte_wcdma_gsm, PREFERRED_NETWORK_TYPE_LTE_WCDMA_GSM);
ASSERT_PREFERRED_NETWORK_TYPE_EQUALITY(Lte_wcdma_gsm_cdma_evdo, PREFERRED_NETWORK_TYPE_LTE_WCDMA_GSM_CDMA_EVDO);
ASSERT_PREFERRED_NETWORK_TYPE_EQUALITY(Lte, PREFERRED_NETWORK_TYPE_LTE_ONLY);

#undef ASSERT_PREFERRED_NETWORK_TYPE_EQUALITY

} // namespace dom
} // namespace mozilla
