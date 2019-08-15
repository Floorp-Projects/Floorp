/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  isWasm,
  lineToWasmOffset,
  wasmOffsetToLine,
  clearWasmStates,
  renderWasmText,
} from "../wasm.js";

import { makeMockWasmSourceWithContent } from "../test-mockup";

describe("wasm", () => {
  // Compiled version of `(module (func (nop)))`
  const SIMPLE_WASM = {
    binary:
      "\x00asm\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01`\x00\x00" +
      "\x03\x82\x80\x80\x80\x00\x01\x00\x06\x81\x80\x80\x80\x00\x00" +
      "\n\x89\x80\x80\x80\x00\x01\x83\x80\x80\x80\x00\x00\x01\v",
  };
  const SIMPLE_WASM_TEXT = `(module
  (type $type0 (func))
  (func $func0
    nop
  )
)`;
  const SIMPLE_WASM_NOP_TEXT_LINE = 3;
  const SIMPLE_WASM_NOP_OFFSET = 46;

  describe("isWasm", () => {
    it("should give us the false when wasm text was not registered", () => {
      const sourceId = "source.0";
      expect(isWasm(sourceId)).toEqual(false);
    });
    it("should give us the true when wasm text was registered", () => {
      const { source, content } = makeMockWasmSourceWithContent(SIMPLE_WASM);
      renderWasmText(source.id, content.value);
      expect(isWasm(source.id)).toEqual(true);
      // clear shall remove
      clearWasmStates();
      expect(isWasm(source.id)).toEqual(false);
    });
  });

  describe("renderWasmText", () => {
    it("render simple wasm", () => {
      const { source, content } = makeMockWasmSourceWithContent(SIMPLE_WASM);
      const lines = renderWasmText(source.id, content.value);
      expect(lines.join("\n")).toEqual(SIMPLE_WASM_TEXT);
      clearWasmStates();
    });
  });

  describe("lineToWasmOffset", () => {
    // Test data sanity check: checking if 'nop' is found in the SIMPLE_WASM.
    expect(SIMPLE_WASM.binary[SIMPLE_WASM_NOP_OFFSET]).toEqual("\x01");

    it("get simple wasm nop offset", () => {
      const { source, content } = makeMockWasmSourceWithContent(SIMPLE_WASM);
      renderWasmText(source.id, content.value);
      const offset = lineToWasmOffset(source.id, SIMPLE_WASM_NOP_TEXT_LINE);
      expect(offset).toEqual(SIMPLE_WASM_NOP_OFFSET);
      clearWasmStates();
    });
  });

  describe("wasmOffsetToLine", () => {
    it("get simple wasm nop line", () => {
      const { source, content } = makeMockWasmSourceWithContent(SIMPLE_WASM);
      renderWasmText(source.id, content.value);
      const line = wasmOffsetToLine(source.id, SIMPLE_WASM_NOP_OFFSET);
      expect(line).toEqual(SIMPLE_WASM_NOP_TEXT_LINE);
      clearWasmStates();
    });
  });
});
