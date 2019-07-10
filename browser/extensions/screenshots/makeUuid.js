/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.makeUuid = (function() {

  // generates a v4 UUID
  return function makeUuid() { // eslint-disable-line no-unused-vars
    // get sixteen unsigned 8 bit random values
    const randomValues = window
      .crypto
      .getRandomValues(new Uint8Array(36));

    return "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx".replace(/[xy]/g, function(c) {
      const i = Array.prototype.slice.call(arguments).slice(-2)[0]; // grab the `offset` parameter
      const r = randomValues[i] % 16|0, v = c === "x" ? r : (r & 0x3 | 0x8);
      return v.toString(16);
    });
  };
})();
null;
