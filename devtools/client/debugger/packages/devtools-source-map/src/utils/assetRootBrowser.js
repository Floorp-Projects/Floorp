/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { SourceMapConsumer } = require("source-map");
const {
  setAssetRootURL: wasmDwarfSetAssetRootURL,
} = require("devtools-wasm-dwarf");

function setAssetRootURL(assetRoot) {
  // Remove any trailing slash so we don't generate a double-slash below.
  const root = assetRoot.replace(/\/$/, "");

  wasmDwarfSetAssetRootURL(root);

  SourceMapConsumer.initialize({
    "lib/mappings.wasm": `${root}/source-map-mappings.wasm`,
  });
}

module.exports = {
  setAssetRootURL,
};
