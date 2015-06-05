/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Test module to check presense of user defined globals.
// Related to bug 827792.

exports.getCom = function() {
  return com;
};
