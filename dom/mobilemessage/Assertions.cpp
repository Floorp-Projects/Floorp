/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MozMobileMessageManagerBinding.h"
#include "nsISmsService.h"

namespace mozilla {
namespace dom {
namespace mobilemessage {

#define ASSERT_SMS_EQUALITY(webidlType, webidlState, xpidlState) \
  static_assert(static_cast<uint32_t>(webidlType::webidlState) == nsISmsService::xpidlState, \
  #webidlType "::" #webidlState " should equal to nsISmsService::" #xpidlState)

/**
 * Enum TypeOfNumber
 */
#define ASSERT_SMS_TYPE_OF_NUMBER_EQUALITY(webidlState, xpidlState) \
  ASSERT_SMS_EQUALITY(TypeOfNumber, webidlState, xpidlState)

ASSERT_SMS_TYPE_OF_NUMBER_EQUALITY(Unknown, TYPE_OF_NUMBER_UNKNOWN);
ASSERT_SMS_TYPE_OF_NUMBER_EQUALITY(International, TYPE_OF_NUMBER_INTERNATIONAL);
ASSERT_SMS_TYPE_OF_NUMBER_EQUALITY(National, TYPE_OF_NUMBER_NATIONAL);
ASSERT_SMS_TYPE_OF_NUMBER_EQUALITY(Network_specific, TYPE_OF_NUMBER_NETWORK_SPECIFIC);
ASSERT_SMS_TYPE_OF_NUMBER_EQUALITY(Dedicated_access_short_code, TYPE_OF_NUMBER_DEDICATED_ACCESS_SHORT_CODE);

#undef ASSERT_SMS_TYPE_OF_NUMBER_EQUALITY

/**
 * Enum NumberPlanIdentification
 */
#define ASSERT_SMS_NUMBER_PLAN_IDENTIFICATION_EQUALITY(webidlState, xpidlState) \
  ASSERT_SMS_EQUALITY(NumberPlanIdentification, webidlState, xpidlState)

ASSERT_SMS_NUMBER_PLAN_IDENTIFICATION_EQUALITY(Unknown, NUMBER_PLAN_IDENTIFICATION_UNKNOWN);
ASSERT_SMS_NUMBER_PLAN_IDENTIFICATION_EQUALITY(Isdn, NUMBER_PLAN_IDENTIFICATION_ISDN);
ASSERT_SMS_NUMBER_PLAN_IDENTIFICATION_EQUALITY(Data, NUMBER_PLAN_IDENTIFICATION_DATA);
ASSERT_SMS_NUMBER_PLAN_IDENTIFICATION_EQUALITY(Telex, NUMBER_PLAN_IDENTIFICATION_TELEX);
ASSERT_SMS_NUMBER_PLAN_IDENTIFICATION_EQUALITY(National, NUMBER_PLAN_IDENTIFICATION_NATIONAL);
ASSERT_SMS_NUMBER_PLAN_IDENTIFICATION_EQUALITY(Private, NUMBER_PLAN_IDENTIFICATION_PRIVATE);

#undef ASSERT_SMS_EQUALITY

} // namespace mobilemessage
} // namespace dom
} // namespace mozilla
