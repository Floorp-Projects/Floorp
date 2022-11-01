/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function handleRequest(request, response) {
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("X-Content-Type-Options", "nosniff");
  response.write("Hello");
}
