/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// TODO This file aims to implement a Chrome-API-free replacement for
// devtools/client/shared/unicode-url.js, so that it can be used in the
// Launchpad.
//
// Currently this is just a dummpy mock of
// devtools/client/shared/unicode-url.js, no actual functionaly involved.
// Eventually we'll want to implement it. Once implemented, we should keep the
// feature the same as devtools/client/shared/unicode-url.js.

"use strict";

function getUnicodeHostname(hostname) {
  return hostname;
}

function getUnicodeUrlPath(urlPath) {
  return urlPath;
}

function getUnicodeUrl(url) {
  return url;
}

module.exports = {
  getUnicodeHostname,
  getUnicodeUrlPath,
  getUnicodeUrl,
};
