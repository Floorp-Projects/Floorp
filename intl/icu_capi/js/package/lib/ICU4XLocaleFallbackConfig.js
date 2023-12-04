import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"
import { ICU4XLocaleFallbackPriority_js_to_rust, ICU4XLocaleFallbackPriority_rust_to_js } from "./ICU4XLocaleFallbackPriority.js"
import { ICU4XLocaleFallbackSupplement_js_to_rust, ICU4XLocaleFallbackSupplement_rust_to_js } from "./ICU4XLocaleFallbackSupplement.js"

export class ICU4XLocaleFallbackConfig {
  constructor(underlying, edges_a) {
    this.priority = ICU4XLocaleFallbackPriority_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, underlying)];
    this.extension_key = (() => {
      const [ptr, size] = new Uint32Array(wasm.memory.buffer, underlying + 4, 2);
      return diplomatRuntime.readString(wasm, ptr, size);
    })();
    this.fallback_supplement = ICU4XLocaleFallbackSupplement_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, underlying + 12)];
  }
}
