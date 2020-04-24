/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

exports.resolveSourceURL = resolveSourceURL;
function resolveSourceURL(sourceURL, global) {
  if (sourceURL) {
    try {
      return new URL(sourceURL, global?.location?.href || undefined).href;
    } catch (err) {}
  }

  return null;
}
