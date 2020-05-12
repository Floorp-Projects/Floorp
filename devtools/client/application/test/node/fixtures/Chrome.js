/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.exports = {
  Cc: {},
  Ci: {
    // sw states from
    // mozilla-central/source/dom/interfaces/base/nsIServiceWorkerManager.idl
    nsIServiceWorkerInfo: {
      STATE_PARSED: 0,
      STATE_INSTALLING: 1,
      STATE_INSTALLED: 2,
      STATE_ACTIVATING: 3,
      STATE_ACTIVATED: 4,
      STATE_REDUNDANT: 5,
      STATE_UNKNOWN: 6,
    },
  },
  Cu: {},
};
