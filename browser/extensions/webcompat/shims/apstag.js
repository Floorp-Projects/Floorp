/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1713698 - Shim Amazon Transparent Ad Marketplace's apstag.js
 *
 * Some sites such as politico.com rely on Amazon TAM tracker to serve ads,
 * breaking functionality like galleries if it is blocked. This shim helps
 * mitigate major breakage in that case.
 */

if (!window.apstag?._getSlotIdToNameMapping) {
  const _Q = window.apstag?._Q || [];

  const newBid = config => {
    return {
      amznbid: "",
      amzniid: "",
      amznp: "",
      amznsz: "0x0",
      size: "0x0",
      slotID: config.slotID,
    };
  };

  window.apstag = {
    _Q,
    _getSlotIdToNameMapping() {},
    bids() {},
    debug() {},
    deleteId() {},
    fetchBids(cfg, cb) {
      if (!Array.isArray(cfg?.slots)) {
        return;
      }
      cb(cfg.slots.map(s => newBid(s)));
    },
    init() {},
    punt() {},
    renderImp() {},
    renewId() {},
    setDisplayBids() {},
    targetingKeys: () => [],
    thirdPartyData: {},
    updateId() {},
  };

  window.apstagLOADED = true;

  _Q.push = function(prefix, args) {
    try {
      switch (prefix) {
        case "f":
          window.apstag.fetchBids(...args);
          break;
        case "i":
          window.apstag.init(...args);
          break;
      }
    } catch (e) {
      console.trace(e);
    }
  };

  for (const cmd of _Q) {
    _Q.push(cmd);
  }
}
