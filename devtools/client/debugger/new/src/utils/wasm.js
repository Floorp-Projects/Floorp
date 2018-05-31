"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getWasmText = getWasmText;
exports.getWasmLineNumberFormatter = getWasmLineNumberFormatter;
exports.isWasm = isWasm;
exports.lineToWasmOffset = lineToWasmOffset;
exports.wasmOffsetToLine = wasmOffsetToLine;
exports.clearWasmStates = clearWasmStates;
exports.renderWasmText = renderWasmText;

var _WasmParser = require("devtools/client/shared/vendor/WasmParser");

var _WasmDis = require("devtools/client/shared/vendor/WasmDis");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
var wasmStates = Object.create(null);

function maybeWasmSectionNameResolver(data) {
  try {
    const parser = new _WasmParser.BinaryReader();
    parser.setData(data.buffer, 0, data.length);
    const reader = new _WasmDis.NameSectionReader();
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


function getWasmText(sourceId, data) {
  const nameResolver = maybeWasmSectionNameResolver(data);
  const parser = new _WasmParser.BinaryReader();
  parser.setData(data.buffer, 0, data.length);
  const dis = new _WasmDis.WasmDisassembler();

  if (nameResolver) {
    dis.nameResolver = nameResolver;
  }

  dis.addOffsets = true;
  const done = dis.disassembleChunk(parser);
  let result = dis.getResult();

  if (result.lines.length === 0) {
    result = {
      lines: ["No luck with wast conversion"],
      offsets: [0],
      done
    };
  }

  const offsets = result.offsets;
  const lines = [];

  for (let i = 0; i < offsets.length; i++) {
    lines[offsets[i]] = i;
  }

  wasmStates[sourceId] = {
    offsets,
    lines
  };
  return {
    lines: result.lines,
    done: result.done
  };
}
/**
 * @memberof utils/wasm
 * @static
 */


function getWasmLineNumberFormatter(sourceId) {
  const codeOf0 = 48,
        codeOfA = 65;
  const buffer = [codeOf0, codeOf0, codeOf0, codeOf0, codeOf0, codeOf0, codeOf0, codeOf0];
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


function isWasm(sourceId) {
  return sourceId in wasmStates;
}
/**
 * @memberof utils/wasm
 * @static
 */


function lineToWasmOffset(sourceId, number) {
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


function wasmOffsetToLine(sourceId, offset) {
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


function clearWasmStates() {
  wasmStates = Object.create(null);
}

function renderWasmText(sourceId, {
  binary
}) {
  // binary does not survive as Uint8Array, converting from string
  const data = new Uint8Array(binary.length);

  for (let i = 0; i < data.length; i++) {
    data[i] = binary.charCodeAt(i);
  }

  const {
    lines
  } = getWasmText(sourceId, data);
  const MAX_LINES = 1000000;

  if (lines.length > MAX_LINES) {
    lines.splice(MAX_LINES, lines.length - MAX_LINES);
    lines.push(";; .... text is truncated due to the size");
  }

  return lines;
}