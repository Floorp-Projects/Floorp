/* -*- indent-tabs-mode: nil; js-indent-level: 2; fill-column: 80 -*- */
/* vim:set ts=2 sw=2 sts=2 et tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const wasmparser = require("devtools/client/shared/vendor/WasmParser");
const wasmdis = require("devtools/client/shared/vendor/WasmDis");

const wasmStates = new WeakMap();

function getWasmText(subject, data) {
  const parser = new wasmparser.BinaryReader();
  parser.setData(data.buffer, 0, data.length);
  const dis = new wasmdis.WasmDisassembler();
  dis.addOffsets = true;
  const done = dis.disassembleChunk(parser);
  let result = dis.getResult();
  if (result.lines.length === 0) {
    result = { lines: ["No luck with wast conversion"], offsets: [0], done, };
  }

  const offsets = result.offsets, lines = [];
  for (let i = 0; i < offsets.length; i++) {
    lines[offsets[i]] = i;
  }

  wasmStates.set(subject, { offsets, lines, });

  return { lines: result.lines, done: result.done };
}

function getWasmLineNumberFormatter(subject) {
  const codeOf0 = 48, codeOfA = 65;
  const buffer =
    [codeOf0, codeOf0, codeOf0, codeOf0, codeOf0, codeOf0, codeOf0, codeOf0];
  let last0 = 7;
  return function(number) {
    const offset = lineToWasmOffset(subject, number - 1);
    if (offset === undefined) {
      return "";
    }
    let i = 7;
    for (let n = offset | 0; n !== 0 && i >= 0; n >>= 4, i--) {
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

function isWasm(subject) {
  return wasmStates.has(subject);
}

function lineToWasmOffset(subject, number) {
  const wasmState = wasmStates.get(subject);
  if (!wasmState) {
    return undefined;
  }
  let offset = wasmState.offsets[number];
  while (offset === undefined && number > 0) {
    offset = wasmState.offsets[--number];
  }
  return offset;
}

function wasmOffsetToLine(subject, offset) {
  const wasmState = wasmStates.get(subject);
  return wasmState.lines[offset];
}

module.exports = {
  getWasmText,
  getWasmLineNumberFormatter,
  isWasm,
  lineToWasmOffset,
  wasmOffsetToLine,
};
