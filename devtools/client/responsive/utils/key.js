/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { KeyCodes } = require("devtools/client/shared/keycodes");

/**
 * Helper to check if the provided key matches one of the expected keys.
 * Keys will be prefixed with DOM_VK_ and should match a key in KeyCodes.
 *
 * @param {String} key
 *        the key to check (can be a keyCode).
 * @param {...String} keys
 *        list of possible keys allowed.
 * @return {Boolean} true if the key matches one of the keys.
 */
function isKeyIn(key, ...keys) {
  return keys.some(expectedKey => {
    return key === KeyCodes["DOM_VK_" + expectedKey];
  });
}

exports.isKeyIn = isKeyIn;
