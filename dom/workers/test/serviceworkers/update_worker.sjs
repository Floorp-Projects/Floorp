/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

function handleRequest(request, response) {
  // This header is necessary for making this script able to be loaded.
  response.setHeader("Content-Type", "application/javascript");

  var body = '/* ' + Date.now() + ' */';
  response.write(body);
}

