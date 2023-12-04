import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"
import { ICU4XError_js_to_rust, ICU4XError_rust_to_js } from "./ICU4XError.js"
import { ICU4XLocaleDirection_js_to_rust, ICU4XLocaleDirection_rust_to_js } from "./ICU4XLocaleDirection.js"

const ICU4XLocaleDirectionality_box_destroy_registry = new FinalizationRegistry(underlying => {
  wasm.ICU4XLocaleDirectionality_destroy(underlying);
});

export class ICU4XLocaleDirectionality {
  #lifetimeEdges = [];
  constructor(underlying, owned, edges) {
    this.underlying = underlying;
    this.#lifetimeEdges.push(...edges);
    if (owned) {
      ICU4XLocaleDirectionality_box_destroy_registry.register(this, underlying);
    }
  }

  static create(arg_provider) {
    return (() => {
      const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
      wasm.ICU4XLocaleDirectionality_create(diplomat_receive_buffer, arg_provider.underlying);
      const is_ok = diplomatRuntime.resultFlag(wasm, diplomat_receive_buffer, 4);
      if (is_ok) {
        const ok_value = new ICU4XLocaleDirectionality(diplomatRuntime.ptrRead(wasm, diplomat_receive_buffer), true, []);
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        return ok_value;
      } else {
        const throw_value = ICU4XError_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, diplomat_receive_buffer)];
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        throw new diplomatRuntime.FFIError(throw_value);
      }
    })();
  }

  static create_with_expander(arg_provider, arg_expander) {
    return (() => {
      const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
      wasm.ICU4XLocaleDirectionality_create_with_expander(diplomat_receive_buffer, arg_provider.underlying, arg_expander.underlying);
      const is_ok = diplomatRuntime.resultFlag(wasm, diplomat_receive_buffer, 4);
      if (is_ok) {
        const ok_value = new ICU4XLocaleDirectionality(diplomatRuntime.ptrRead(wasm, diplomat_receive_buffer), true, []);
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        return ok_value;
      } else {
        const throw_value = ICU4XError_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, diplomat_receive_buffer)];
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        throw new diplomatRuntime.FFIError(throw_value);
      }
    })();
  }

  get(arg_locale) {
    return ICU4XLocaleDirection_rust_to_js[wasm.ICU4XLocaleDirectionality_get(this.underlying, arg_locale.underlying)];
  }

  is_left_to_right(arg_locale) {
    return wasm.ICU4XLocaleDirectionality_is_left_to_right(this.underlying, arg_locale.underlying);
  }

  is_right_to_left(arg_locale) {
    return wasm.ICU4XLocaleDirectionality_is_right_to_left(this.underlying, arg_locale.underlying);
  }
}
