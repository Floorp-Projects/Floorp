/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  getUrlDetails,
  // eslint-disable-next-line mozilla/reject-some-requires
} = require("devtools/client/netmonitor/src/utils/request-utils");

module.exports = function({ resource }) {
  resource.urlDetails = getUrlDetails(resource.url);
  resource.startedMs = Date.parse(resource.startedDateTime);
  return resource;
};
