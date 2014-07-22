/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 "use strict";

module.metadata = {
  "stability": "unstable"
};

let usingJSON = false;
let hash = {}, bestMatchingLocale = null;
try {
  let data = require("@l10n/data");
  hash = data.hash;
  bestMatchingLocale = data.bestMatchingLocale;
  usingJSON = true;
}
catch(e) {}

exports.usingJSON = usingJSON;

// Returns the translation for a given key, if available.
exports.get = function get(k) {
  return k in hash ? hash[k] : null;
}

// Returns the full length locale code: ja-JP-mac, en-US or fr
exports.locale = function locale() {
  return bestMatchingLocale;
}

// Returns the short locale code: ja, en, fr
exports.language = function language() {
  return bestMatchingLocale ? bestMatchingLocale.split("-")[0].toLowerCase()
                            : null;
}
