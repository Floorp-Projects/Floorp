/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
// @flow

const { SourceMapConsumer } = require("source-map");

let root;
function setAssetRootURL(assetRoot: string): void {
  // Remove any trailing slash so we don't generate a double-slash below.
  root = assetRoot.replace(/\/$/, "");

  SourceMapConsumer.initialize({
    "lib/mappings.wasm": `${root}/source-map-mappings.wasm`,
  });
}

async function getDwarfToWasmData(name: string): Promise<ArrayBuffer> {
  if (!root) {
    throw new Error(`No wasm path - Unable to resolve ${name}`);
  }

  const response = await fetch(`${root}/dwarf_to_json.wasm`);

  return response.arrayBuffer();
}

module.exports = {
  setAssetRootURL,
  getDwarfToWasmData,
};
