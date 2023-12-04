import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"
import { ICU4XDisplayNamesFallback_js_to_rust, ICU4XDisplayNamesFallback_rust_to_js } from "./ICU4XDisplayNamesFallback.js"
import { ICU4XDisplayNamesStyle_js_to_rust, ICU4XDisplayNamesStyle_rust_to_js } from "./ICU4XDisplayNamesStyle.js"
import { ICU4XError_js_to_rust, ICU4XError_rust_to_js } from "./ICU4XError.js"
import { ICU4XLanguageDisplay_js_to_rust, ICU4XLanguageDisplay_rust_to_js } from "./ICU4XLanguageDisplay.js"

const ICU4XLocaleDisplayNamesFormatter_box_destroy_registry = new FinalizationRegistry(underlying => {
  wasm.ICU4XLocaleDisplayNamesFormatter_destroy(underlying);
});

export class ICU4XLocaleDisplayNamesFormatter {
  #lifetimeEdges = [];
  constructor(underlying, owned, edges) {
    this.underlying = underlying;
    this.#lifetimeEdges.push(...edges);
    if (owned) {
      ICU4XLocaleDisplayNamesFormatter_box_destroy_registry.register(this, underlying);
    }
  }

  static create(arg_provider, arg_locale, arg_options) {
    const field_style_arg_options = arg_options["style"];
    const field_fallback_arg_options = arg_options["fallback"];
    const field_language_display_arg_options = arg_options["language_display"];
    return (() => {
      const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
      wasm.ICU4XLocaleDisplayNamesFormatter_create(diplomat_receive_buffer, arg_provider.underlying, arg_locale.underlying, ICU4XDisplayNamesStyle_js_to_rust[field_style_arg_options], ICU4XDisplayNamesFallback_js_to_rust[field_fallback_arg_options], ICU4XLanguageDisplay_js_to_rust[field_language_display_arg_options]);
      const is_ok = diplomatRuntime.resultFlag(wasm, diplomat_receive_buffer, 4);
      if (is_ok) {
        const ok_value = new ICU4XLocaleDisplayNamesFormatter(diplomatRuntime.ptrRead(wasm, diplomat_receive_buffer), true, []);
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        return ok_value;
      } else {
        const throw_value = ICU4XError_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, diplomat_receive_buffer)];
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        throw new diplomatRuntime.FFIError(throw_value);
      }
    })();
  }

  of(arg_locale) {
    return diplomatRuntime.withWriteable(wasm, (writeable) => {
      return (() => {
        const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
        wasm.ICU4XLocaleDisplayNamesFormatter_of(diplomat_receive_buffer, this.underlying, arg_locale.underlying, writeable);
        const is_ok = diplomatRuntime.resultFlag(wasm, diplomat_receive_buffer, 4);
        if (is_ok) {
          const ok_value = {};
          wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
          return ok_value;
        } else {
          const throw_value = ICU4XError_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, diplomat_receive_buffer)];
          wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
          throw new diplomatRuntime.FFIError(throw_value);
        }
      })();
    });
  }
}
