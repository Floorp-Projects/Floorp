/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file mocks the functions from the OT sdk that we use. This is to provide
 * an interface that tests can mock out, without needing to maintain a copy of
 * the sdk or load one from the network.
 */
(function (window) {
  "use strict";

  if (!window.OT) {
    window.OT = {};
  }

  window.OT.checkSystemRequirements = function() {
    return true;
  };

})(window);

