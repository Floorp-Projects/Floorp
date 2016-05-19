/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const URL = require("URL");

/*
 * Join all the arguments together and normalize the resulting URI.
 * The initial path must be an full URI with a protocol (i.e. http://).
 */
exports.joinURI = (initialPath, ...paths) => {
  let url;

  try {
    url = new URL(initialPath);
  }
  catch (e) {
    return;
  }

  for (let path of paths) {
    if (path) {
      url = new URL(path, url);
    }
  }

  return url.href;
};
