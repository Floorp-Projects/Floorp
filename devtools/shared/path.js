/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");

/*
 * Returns the directory name of the path
 */
exports.dirname = path => {
  return Services.io.newURI(
    ".", null, Services.io.newURI(path, null, null)).spec;
}

/*
 * Join all the arguments together and normalize the resulting URI.
 * The initial path must be an full URI with a protocol (i.e. http://).
 */
exports.joinURI = (initialPath, ...paths) => {
  let uri;

  try {
    uri = Services.io.newURI(initialPath, null, null);
  }
  catch(e) {
    return;
  }

  for(let path of paths) {
    if (path) {
      uri = Services.io.newURI(path, null, uri);
    }
  }

  return uri.spec;
}
