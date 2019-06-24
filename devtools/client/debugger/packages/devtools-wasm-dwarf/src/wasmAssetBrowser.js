/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
// @flow

let root;
function setAssetRootURL(assetRoot: string): void {
  root = assetRoot;
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
