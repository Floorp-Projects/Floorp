import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"
import { ICU4XError_js_to_rust, ICU4XError_rust_to_js } from "./ICU4XError.js"

const ICU4XGeneralCategoryNameToMaskMapper_box_destroy_registry = new FinalizationRegistry(underlying => {
  wasm.ICU4XGeneralCategoryNameToMaskMapper_destroy(underlying);
});

export class ICU4XGeneralCategoryNameToMaskMapper {
  #lifetimeEdges = [];
  constructor(underlying, owned, edges) {
    this.underlying = underlying;
    this.#lifetimeEdges.push(...edges);
    if (owned) {
      ICU4XGeneralCategoryNameToMaskMapper_box_destroy_registry.register(this, underlying);
    }
  }

  get_strict(arg_name) {
    const buf_arg_name = diplomatRuntime.DiplomatBuf.str(wasm, arg_name);
    const diplomat_out = wasm.ICU4XGeneralCategoryNameToMaskMapper_get_strict(this.underlying, buf_arg_name.ptr, buf_arg_name.size);
    buf_arg_name.free();
    return diplomat_out;
  }

  get_loose(arg_name) {
    const buf_arg_name = diplomatRuntime.DiplomatBuf.str(wasm, arg_name);
    const diplomat_out = wasm.ICU4XGeneralCategoryNameToMaskMapper_get_loose(this.underlying, buf_arg_name.ptr, buf_arg_name.size);
    buf_arg_name.free();
    return diplomat_out;
  }

  static load(arg_provider) {
    return (() => {
      const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
      wasm.ICU4XGeneralCategoryNameToMaskMapper_load(diplomat_receive_buffer, arg_provider.underlying);
      const is_ok = diplomatRuntime.resultFlag(wasm, diplomat_receive_buffer, 4);
      if (is_ok) {
        const ok_value = new ICU4XGeneralCategoryNameToMaskMapper(diplomatRuntime.ptrRead(wasm, diplomat_receive_buffer), true, []);
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        return ok_value;
      } else {
        const throw_value = ICU4XError_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, diplomat_receive_buffer)];
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        throw new diplomatRuntime.FFIError(throw_value);
      }
    })();
  }
}
