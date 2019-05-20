/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* eslint camelcase: 0*/

const { getDwarfToWasmData } = require("./wasmAsset.js");

let cachedWasmModule;
let utf8Decoder;

function convertDwarf(wasm, instance) {
  const { memory, alloc_mem, free_mem, convert_dwarf } = instance.exports;
  const wasmPtr = alloc_mem(wasm.byteLength);
  new Uint8Array(memory.buffer, wasmPtr, wasm.byteLength).set(
    new Uint8Array(wasm)
  );
  const resultPtr = alloc_mem(12);
  const enableXScopes = true;
  const success = convert_dwarf(
    wasmPtr,
    wasm.byteLength,
    resultPtr,
    resultPtr + 4,
    enableXScopes
  );
  free_mem(wasmPtr);
  const resultView = new DataView(memory.buffer, resultPtr, 12);
  const outputPtr = resultView.getUint32(0, true),
    outputLen = resultView.getUint32(4, true);
  free_mem(resultPtr);
  if (!success) {
    throw new Error("Unable to convert from DWARF sections");
  }
  if (!utf8Decoder) {
    utf8Decoder = new TextDecoder("utf-8");
  }
  const output = utf8Decoder.decode(
    new Uint8Array(memory.buffer, outputPtr, outputLen)
  );
  free_mem(outputPtr);
  return output;
}

async function convertToJSON(buffer: ArrayBuffer): any {
  // Note: We don't 'await' here because it could mean that multiple
  // calls to 'convertToJSON' could cause multiple fetches to be started.
  cachedWasmModule = cachedWasmModule || loadConverterModule();

  return convertDwarf(buffer, await cachedWasmModule);
}

async function loadConverterModule() {
  const wasm = await getDwarfToWasmData();
  const imports = {};
  const { instance } = await WebAssembly.instantiate(wasm, imports);
  return instance;
}

module.exports = {
  convertToJSON,
};
