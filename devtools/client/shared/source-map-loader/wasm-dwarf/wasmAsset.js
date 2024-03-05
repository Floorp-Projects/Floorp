/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

async function getDwarfToWasmData() {
  const response = await fetch(
    "resource://devtools/client/shared/source-map-loader/wasm-dwarf/dwarf_to_json.wasm"
  );

  return response.arrayBuffer();
}

module.exports = {
  getDwarfToWasmData,
};
