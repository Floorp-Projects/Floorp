import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"

const ICU4XLogger_box_destroy_registry = new FinalizationRegistry(underlying => {
  wasm.ICU4XLogger_destroy(underlying);
});

export class ICU4XLogger {
  #lifetimeEdges = [];
  constructor(underlying, owned, edges) {
    this.underlying = underlying;
    this.#lifetimeEdges.push(...edges);
    if (owned) {
      ICU4XLogger_box_destroy_registry.register(this, underlying);
    }
  }

  static init_simple_logger() {
    return wasm.ICU4XLogger_init_simple_logger();
  }

  static init_console_logger() {
    return wasm.ICU4XLogger_init_console_logger();
  }
}
