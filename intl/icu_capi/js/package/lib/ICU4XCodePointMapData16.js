import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"
import { CodePointRangeIterator } from "./CodePointRangeIterator.js"
import { ICU4XCodePointSetData } from "./ICU4XCodePointSetData.js"
import { ICU4XError_js_to_rust, ICU4XError_rust_to_js } from "./ICU4XError.js"

const ICU4XCodePointMapData16_box_destroy_registry = new FinalizationRegistry(underlying => {
  wasm.ICU4XCodePointMapData16_destroy(underlying);
});

export class ICU4XCodePointMapData16 {
  #lifetimeEdges = [];
  constructor(underlying, owned, edges) {
    this.underlying = underlying;
    this.#lifetimeEdges.push(...edges);
    if (owned) {
      ICU4XCodePointMapData16_box_destroy_registry.register(this, underlying);
    }
  }

  get(arg_cp) {
    return wasm.ICU4XCodePointMapData16_get(this.underlying, diplomatRuntime.extractCodePoint(arg_cp, 'arg_cp'));
  }

  get32(arg_cp) {
    return wasm.ICU4XCodePointMapData16_get32(this.underlying, arg_cp);
  }

  iter_ranges_for_value(arg_value) {
    return new CodePointRangeIterator(wasm.ICU4XCodePointMapData16_iter_ranges_for_value(this.underlying, arg_value), true, [this]);
  }

  iter_ranges_for_value_complemented(arg_value) {
    return new CodePointRangeIterator(wasm.ICU4XCodePointMapData16_iter_ranges_for_value_complemented(this.underlying, arg_value), true, [this]);
  }

  get_set_for_value(arg_value) {
    return new ICU4XCodePointSetData(wasm.ICU4XCodePointMapData16_get_set_for_value(this.underlying, arg_value), true, []);
  }

  static load_script(arg_provider) {
    return (() => {
      const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
      wasm.ICU4XCodePointMapData16_load_script(diplomat_receive_buffer, arg_provider.underlying);
      const is_ok = diplomatRuntime.resultFlag(wasm, diplomat_receive_buffer, 4);
      if (is_ok) {
        const ok_value = new ICU4XCodePointMapData16(diplomatRuntime.ptrRead(wasm, diplomat_receive_buffer), true, []);
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
