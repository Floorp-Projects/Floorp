import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"
import { ICU4XError_js_to_rust, ICU4XError_rust_to_js } from "./ICU4XError.js"
import { ICU4XLocaleFallbackPriority_js_to_rust, ICU4XLocaleFallbackPriority_rust_to_js } from "./ICU4XLocaleFallbackPriority.js"
import { ICU4XLocaleFallbackSupplement_js_to_rust, ICU4XLocaleFallbackSupplement_rust_to_js } from "./ICU4XLocaleFallbackSupplement.js"
import { ICU4XLocaleFallbackerWithConfig } from "./ICU4XLocaleFallbackerWithConfig.js"

const ICU4XLocaleFallbacker_box_destroy_registry = new FinalizationRegistry(underlying => {
  wasm.ICU4XLocaleFallbacker_destroy(underlying);
});

export class ICU4XLocaleFallbacker {
  #lifetimeEdges = [];
  constructor(underlying, owned, edges) {
    this.underlying = underlying;
    this.#lifetimeEdges.push(...edges);
    if (owned) {
      ICU4XLocaleFallbacker_box_destroy_registry.register(this, underlying);
    }
  }

  static create(arg_provider) {
    return (() => {
      const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
      wasm.ICU4XLocaleFallbacker_create(diplomat_receive_buffer, arg_provider.underlying);
      const is_ok = diplomatRuntime.resultFlag(wasm, diplomat_receive_buffer, 4);
      if (is_ok) {
        const ok_value = new ICU4XLocaleFallbacker(diplomatRuntime.ptrRead(wasm, diplomat_receive_buffer), true, []);
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        return ok_value;
      } else {
        const throw_value = ICU4XError_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, diplomat_receive_buffer)];
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        throw new diplomatRuntime.FFIError(throw_value);
      }
    })();
  }

  static create_without_data() {
    return new ICU4XLocaleFallbacker(wasm.ICU4XLocaleFallbacker_create_without_data(), true, []);
  }

  for_config(arg_config) {
    const field_priority_arg_config = arg_config["priority"];
    const field_extension_key_arg_config = arg_config["extension_key"];
    const buf_field_extension_key_arg_config = diplomatRuntime.DiplomatBuf.str(wasm, field_extension_key_arg_config);
    const field_fallback_supplement_arg_config = arg_config["fallback_supplement"];
    const diplomat_out = (() => {
      const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
      wasm.ICU4XLocaleFallbacker_for_config(diplomat_receive_buffer, this.underlying, ICU4XLocaleFallbackPriority_js_to_rust[field_priority_arg_config], buf_field_extension_key_arg_config.ptr, buf_field_extension_key_arg_config.size, ICU4XLocaleFallbackSupplement_js_to_rust[field_fallback_supplement_arg_config]);
      const is_ok = diplomatRuntime.resultFlag(wasm, diplomat_receive_buffer, 4);
      if (is_ok) {
        const ok_value = new ICU4XLocaleFallbackerWithConfig(diplomatRuntime.ptrRead(wasm, diplomat_receive_buffer), true, [this]);
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        return ok_value;
      } else {
        const throw_value = ICU4XError_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, diplomat_receive_buffer)];
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        throw new diplomatRuntime.FFIError(throw_value);
      }
    })();
    buf_field_extension_key_arg_config.free();
    return diplomat_out;
  }
}
