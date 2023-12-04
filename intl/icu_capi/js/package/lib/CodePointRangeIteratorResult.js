import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"

export class CodePointRangeIteratorResult {
  constructor(underlying) {
    this.start = (new Uint32Array(wasm.memory.buffer, underlying, 1))[0];
    this.end = (new Uint32Array(wasm.memory.buffer, underlying + 4, 1))[0];
    this.done = (new Uint8Array(wasm.memory.buffer, underlying + 8, 1))[0] == 1;
  }
}
