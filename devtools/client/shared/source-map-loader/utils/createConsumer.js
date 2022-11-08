/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const {
  SourceMapConsumer,
} = require("resource://devtools/client/shared/vendor/source-map/source-map.js");

async function createConsumer(map, sourceMapUrl) {
  return new SourceMapConsumer(map, sourceMapUrl);
}

module.exports = {
  createConsumer,
};
