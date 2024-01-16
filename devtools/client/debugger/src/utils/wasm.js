/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { BinaryReader } from "devtools/client/shared/vendor/WasmParser";
import {
  WasmDisassembler,
  NameSectionReader,
} from "devtools/client/shared/vendor/WasmDis";

var wasmStates = Object.create(null);

function maybeWasmSectionNameResolver(data) {
  try {
    const parser = new BinaryReader();
    parser.setData(data.buffer, 0, data.length);
    const reader = new NameSectionReader();
    reader.read(parser);
    return reader.hasValidNames() ? reader.getNameResolver() : null;
  } catch (ex) {
    // Ignoring any errors during names section retrival.
    return null;
  }
}

/**
 * @memberof utils/wasm
 * @static
 */
export function getWasmText(sourceId, data) {
  const nameResolver = maybeWasmSectionNameResolver(data);
  const parser = new BinaryReader();
  let result;
  parser.setData(data.buffer, 0, data.length);
  const dis = new WasmDisassembler();
  if (nameResolver) {
    dis.nameResolver = nameResolver;
  }
  dis.addOffsets = true;
  try {
    const done = dis.disassembleChunk(parser);
    result = dis.getResult();
    if (result.lines.length === 0) {
      result = { lines: ["No luck with wast conversion"], offsets: [0], done };
    }
  } catch (e) {
    result = {
      lines: [`Error occured during wast conversion : ${e.message}`],
      offsets: [0],
      done: null,
    };
  }

  const { offsets } = result;
  const lines = [];
  for (let i = 0; i < offsets.length; i++) {
    lines[offsets[i]] = i;
  }

  wasmStates[sourceId] = { offsets, lines };

  return { lines: result.lines, done: result.done };
}

/**
 * @memberof utils/wasm
 * @static
 */
export function getWasmLineNumberFormatter(sourceId) {
  const codeOf0 = 48,
    codeOfA = 65;
  const buffer = [
    codeOf0,
    codeOf0,
    codeOf0,
    codeOf0,
    codeOf0,
    codeOf0,
    codeOf0,
    codeOf0,
  ];
  let last0 = 7;
  return function (number) {
    const offset = lineToWasmOffset(sourceId, number - 1);
    if (offset == undefined) {
      return "";
    }
    let i = 7;
    for (let n = offset; n !== 0 && i >= 0; n >>= 4, i--) {
      const nibble = n & 15;
      buffer[i] = nibble < 10 ? codeOf0 + nibble : codeOfA - 10 + nibble;
    }
    for (let j = i; j > last0; j--) {
      buffer[j] = codeOf0;
    }
    last0 = i;
    return String.fromCharCode.apply(null, buffer);
  };
}

/**
 * @memberof utils/wasm
 * @static
 */
export function isWasm(sourceId) {
  return sourceId in wasmStates;
}

/**
 * @memberof utils/wasm
 * @static
 */
export function lineToWasmOffset(sourceId, number) {
  const wasmState = wasmStates[sourceId];
  if (!wasmState) {
    return undefined;
  }
  let offset = wasmState.offsets[number];
  while (offset === undefined && number > 0) {
    offset = wasmState.offsets[--number];
  }
  return offset;
}

/**
 * @memberof utils/wasm
 * @static
 */
export function wasmOffsetToLine(sourceId, offset) {
  const wasmState = wasmStates[sourceId];
  if (!wasmState) {
    return undefined;
  }
  return wasmState.lines[offset];
}

/**
 * @memberof utils/wasm
 * @static
 */
export function clearWasmStates() {
  wasmStates = Object.create(null);
}

const wasmLines = new WeakMap();
export function renderWasmText(sourceId, content) {
  if (wasmLines.has(content)) {
    return wasmLines.get(content) || [];
  }

  // binary does not survive as Uint8Array, converting from string
  const { binary } = content.value;
  const data = new Uint8Array(binary.length);
  for (let i = 0; i < data.length; i++) {
    data[i] = binary.charCodeAt(i);
  }
  const { lines } = getWasmText(sourceId, data);
  const MAX_LINES = 1000000;
  if (lines.length > MAX_LINES) {
    lines.splice(MAX_LINES, lines.length - MAX_LINES);
    lines.push(";; .... text is truncated due to the size");
  }

  wasmLines.set(content, lines);
  return lines;
}
