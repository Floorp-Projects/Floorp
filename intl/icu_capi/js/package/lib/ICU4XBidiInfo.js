import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"
import { ICU4XBidiParagraph } from "./ICU4XBidiParagraph.js"

const ICU4XBidiInfo_box_destroy_registry = new FinalizationRegistry(underlying => {
  wasm.ICU4XBidiInfo_destroy(underlying);
});

export class ICU4XBidiInfo {
  #lifetimeEdges = [];
  constructor(underlying, owned, edges) {
    this.underlying = underlying;
    this.#lifetimeEdges.push(...edges);
    if (owned) {
      ICU4XBidiInfo_box_destroy_registry.register(this, underlying);
    }
  }

  paragraph_count() {
    return wasm.ICU4XBidiInfo_paragraph_count(this.underlying);
  }

  paragraph_at(arg_n) {
    return (() => {
      const option_ptr = wasm.ICU4XBidiInfo_paragraph_at(this.underlying, arg_n);
      return (option_ptr == 0) ? null : new ICU4XBidiParagraph(option_ptr, true, [this]);
    })();
  }

  size() {
    return wasm.ICU4XBidiInfo_size(this.underlying);
  }

  level_at(arg_pos) {
    return wasm.ICU4XBidiInfo_level_at(this.underlying, arg_pos);
  }
}
