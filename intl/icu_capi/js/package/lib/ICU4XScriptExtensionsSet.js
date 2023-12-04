import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"

const ICU4XScriptExtensionsSet_box_destroy_registry = new FinalizationRegistry(underlying => {
  wasm.ICU4XScriptExtensionsSet_destroy(underlying);
});

export class ICU4XScriptExtensionsSet {
  #lifetimeEdges = [];
  constructor(underlying, owned, edges) {
    this.underlying = underlying;
    this.#lifetimeEdges.push(...edges);
    if (owned) {
      ICU4XScriptExtensionsSet_box_destroy_registry.register(this, underlying);
    }
  }

  contains(arg_script) {
    return wasm.ICU4XScriptExtensionsSet_contains(this.underlying, arg_script);
  }

  count() {
    return wasm.ICU4XScriptExtensionsSet_count(this.underlying);
  }

  script_at(arg_index) {
    return (() => {
      const diplomat_receive_buffer = wasm.diplomat_alloc(3, 2);
      wasm.ICU4XScriptExtensionsSet_script_at(diplomat_receive_buffer, this.underlying, arg_index);
      const is_ok = diplomatRuntime.resultFlag(wasm, diplomat_receive_buffer, 2);
      if (is_ok) {
        const ok_value = (new Uint16Array(wasm.memory.buffer, diplomat_receive_buffer, 1))[0];
        wasm.diplomat_free(diplomat_receive_buffer, 3, 2);
        return ok_value;
      } else {
        const throw_value = {};
        wasm.diplomat_free(diplomat_receive_buffer, 3, 2);
        throw new diplomatRuntime.FFIError(throw_value);
      }
    })();
  }
}
