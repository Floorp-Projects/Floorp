/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

let root;
function setAssetRootURL(assetRoot) {
  root = assetRoot;
}

async function getDwarfToWasmData(name) {
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
