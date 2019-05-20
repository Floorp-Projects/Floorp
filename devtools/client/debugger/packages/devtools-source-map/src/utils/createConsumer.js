/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
const { SourceMapConsumer } = require("source-map");

async function createConsumer(map: any, sourceMapUrl: any): SourceMapConsumer {
  return new SourceMapConsumer(map, sourceMapUrl);
}

module.exports = {
  createConsumer,
};
