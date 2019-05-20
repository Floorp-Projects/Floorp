/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
// @flow

const fs = require("fs");

const assets = require("../../assets");

function setAssetRootURL(assetRoot: string): void {
  // No-op on Node
}

async function getDwarfToWasmData(): Promise<ArrayBuffer> {
  const data = await new Promise((res, rej) => {
    fs.readFile(assets["dwarf_to_json.wasm"], (err, result) => {
      if (err) {
        return rej(err);
      }
      res(result);
    });
  });

  return data.buffer;
}

module.exports = {
  setAssetRootURL,
  getDwarfToWasmData,
};
