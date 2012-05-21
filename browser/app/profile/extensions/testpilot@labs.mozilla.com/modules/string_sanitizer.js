/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

EXPORTED_SYMBOLS = ["sanitizeString", "sanitizeJSONStrings"];

function sanitizeString(input) {
  // Only allow alphanumerics, space, period, hypen, and underscore.
  // Replace any other characters with question mark.
  return input.replace(/[^a-zA-Z0-9 .\-_]/g, '?');
}

function sanitizeJSONStrings(jsonBlob) {
  // recursively goes through json and sanitizes every string it finds
  for (let x in jsonBlob) {
    if (typeof jsonBlob[x] == "string") {
      jsonBlob[x] = sanitizeString(jsonBlob[x]);
    } else if (typeof jsonBlob[x] == "object") {
      jsonBlob[x] = sanitizeJSONStrings(jsonBlob[x]);
    }
  }
  return jsonBlob;
}