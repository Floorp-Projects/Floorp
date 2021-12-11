/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1713704 - Shim Moat ad tracker
 *
 * Sites such as Forbes may gate content behind Moat ads, resulting in
 * breakage like black boxes where videos should be placed. This shim
 * helps mitigate that breakage by allowing the placement to succeed.
 */

if (!window.moatPrebidAPI?.__A) {
  const targeting = new Map();

  const slotConfig = {
    m_categories: ["moat_safe"],
    m_data: "0",
    m_safety: "safe",
  };

  window.moatPrebidApi = {
    __A() {},
    disableLogging() {},
    enableLogging() {},
    getMoatTargetingForPage: () => slotConfig,
    getMoatTargetingForSlot(slot) {
      return targeting.get(slot?.getSlotElementId());
    },
    pageDataAvailable: () => true,
    safetyDataAvailable: () => true,
    setMoatTargetingForAllSlots() {
      for (const slot of window.googletag.pubads().getSlots() || []) {
        targeting.set(slot.getSlotElementId(), slot.getTargeting());
      }
    },
    setMoatTargetingForSlot(slot) {
      targeting.set(slot?.getSlotElementId(), slotConfig);
    },
    slotDataAvailable() {
      return window.googletag?.pubads().getSlots().length > 0;
    },
  };
}
