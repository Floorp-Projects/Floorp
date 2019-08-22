/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { convertToJSON } = require("./src/convertToJSON");
const { setAssetRootURL } = require("./src/wasmAsset");
const { getWasmXScopes, clearWasmXScopes } = require("./src/wasmXScopes");

module.exports = {
  convertToJSON,
  setAssetRootURL,
  getWasmXScopes,
  clearWasmXScopes,
};
