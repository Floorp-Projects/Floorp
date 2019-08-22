/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const sourceMapRequests = new Map();

function clearSourceMaps() {
  sourceMapRequests.clear();
}

function getSourceMap(generatedSourceId: string): ?Promise<SourceMapConsumer> {
  return sourceMapRequests.get(generatedSourceId);
}

function setSourceMap(generatedId, request) {
  sourceMapRequests.set(generatedId, request);
}

module.exports = {
  clearSourceMaps,
  getSourceMap,
  setSourceMap,
};
