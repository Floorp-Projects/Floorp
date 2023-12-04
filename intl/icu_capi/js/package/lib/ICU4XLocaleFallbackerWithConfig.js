import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"
import { ICU4XLocaleFallbackIterator } from "./ICU4XLocaleFallbackIterator.js"

const ICU4XLocaleFallbackerWithConfig_box_destroy_registry = new FinalizationRegistry(underlying => {
  wasm.ICU4XLocaleFallbackerWithConfig_destroy(underlying);
});

export class ICU4XLocaleFallbackerWithConfig {
  #lifetimeEdges = [];
  constructor(underlying, owned, edges) {
    this.underlying = underlying;
    this.#lifetimeEdges.push(...edges);
    if (owned) {
      ICU4XLocaleFallbackerWithConfig_box_destroy_registry.register(this, underlying);
    }
  }

  fallback_for_locale(arg_locale) {
    return new ICU4XLocaleFallbackIterator(wasm.ICU4XLocaleFallbackerWithConfig_fallback_for_locale(this.underlying, arg_locale.underlying), true, [this]);
  }
}
