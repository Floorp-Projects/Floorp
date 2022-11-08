/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const {
  getWasmXScopes,
} = require("resource://devtools/client/shared/source-map-loader/wasm-dwarf/wasmXScopes");
const {
  getSourceMap,
} = require("resource://devtools/client/shared/source-map-loader/utils/sourceMapRequests");
const {
  generatedToOriginalId,
} = require("resource://devtools/client/shared/source-map-loader/utils/index");

// Returns expanded stack frames details based on the generated location.
// The function return null if not information was found.
async function getOriginalStackFrames(generatedLocation) {
  const wasmXScopes = await getWasmXScopes(generatedLocation.sourceId, {
    getSourceMap,
    generatedToOriginalId,
  });
  if (!wasmXScopes) {
    return null;
  }

  const scopes = wasmXScopes.search(generatedLocation);
  if (scopes.length === 0) {
    console.warn("Something wrong with debug data: none original frames found");
    return null;
  }
  return scopes;
}

module.exports = {
  getOriginalStackFrames,
};
