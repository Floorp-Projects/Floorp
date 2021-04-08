/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1449347 - Rich Relevance
 */

if (!window.r3_common) {
  const noopfn = () => {};

  window.rr_flush_onload = noopfn;
  window.r3 = noopfn;
  window.RR = noopfn;

  window.r3_home = function() {};

  window.r3_search = function() {};
  window.r3_search.prototype = {
    setTerms: noopfn,
  };

  window.r3_common = function() {};
  window.r3_common.prototype = {
    addContext: noopfn,
    addItemId: noopfn,
    addPlacementType: noopfn,
    setApiKey: noopfn,
    setBaseUrl: noopfn,
    setClickthruServer: noopfn,
    setSessionId: noopfn,
    setUserId: noopfn,
  };
}
