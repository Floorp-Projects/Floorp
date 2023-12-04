import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"
import { CodePointRangeIteratorResult } from "./CodePointRangeIteratorResult.js"

const CodePointRangeIterator_box_destroy_registry = new FinalizationRegistry(underlying => {
  wasm.CodePointRangeIterator_destroy(underlying);
});

export class CodePointRangeIterator {
  #lifetimeEdges = [];
  constructor(underlying, owned, edges) {
    this.underlying = underlying;
    this.#lifetimeEdges.push(...edges);
    if (owned) {
      CodePointRangeIterator_box_destroy_registry.register(this, underlying);
    }
  }

  next() {
    return (() => {
      const diplomat_receive_buffer = wasm.diplomat_alloc(9, 4);
      wasm.CodePointRangeIterator_next(diplomat_receive_buffer, this.underlying);
      const out = new CodePointRangeIteratorResult(diplomat_receive_buffer);
      wasm.diplomat_free(diplomat_receive_buffer, 9, 4);
      return out;
    })();
  }
}
