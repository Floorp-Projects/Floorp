import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"
import { ICU4XError_js_to_rust, ICU4XError_rust_to_js } from "./ICU4XError.js"
import { ICU4XLeadingAdjustment_js_to_rust, ICU4XLeadingAdjustment_rust_to_js } from "./ICU4XLeadingAdjustment.js"
import { ICU4XTrailingCase_js_to_rust, ICU4XTrailingCase_rust_to_js } from "./ICU4XTrailingCase.js"

const ICU4XCaseMapper_box_destroy_registry = new FinalizationRegistry(underlying => {
  wasm.ICU4XCaseMapper_destroy(underlying);
});

export class ICU4XCaseMapper {
  #lifetimeEdges = [];
  constructor(underlying, owned, edges) {
    this.underlying = underlying;
    this.#lifetimeEdges.push(...edges);
    if (owned) {
      ICU4XCaseMapper_box_destroy_registry.register(this, underlying);
    }
  }

  static create(arg_provider) {
    return (() => {
      const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
      wasm.ICU4XCaseMapper_create(diplomat_receive_buffer, arg_provider.underlying);
      const is_ok = diplomatRuntime.resultFlag(wasm, diplomat_receive_buffer, 4);
      if (is_ok) {
        const ok_value = new ICU4XCaseMapper(diplomatRuntime.ptrRead(wasm, diplomat_receive_buffer), true, []);
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        return ok_value;
      } else {
        const throw_value = ICU4XError_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, diplomat_receive_buffer)];
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        throw new diplomatRuntime.FFIError(throw_value);
      }
    })();
  }

  lowercase(arg_s, arg_locale) {
    const buf_arg_s = diplomatRuntime.DiplomatBuf.str(wasm, arg_s);
    const diplomat_out = diplomatRuntime.withWriteable(wasm, (writeable) => {
      return (() => {
        const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
        wasm.ICU4XCaseMapper_lowercase(diplomat_receive_buffer, this.underlying, buf_arg_s.ptr, buf_arg_s.size, arg_locale.underlying, writeable);
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
    buf_arg_s.free();
    return diplomat_out;
  }

  uppercase(arg_s, arg_locale) {
    const buf_arg_s = diplomatRuntime.DiplomatBuf.str(wasm, arg_s);
    const diplomat_out = diplomatRuntime.withWriteable(wasm, (writeable) => {
      return (() => {
        const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
        wasm.ICU4XCaseMapper_uppercase(diplomat_receive_buffer, this.underlying, buf_arg_s.ptr, buf_arg_s.size, arg_locale.underlying, writeable);
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
    buf_arg_s.free();
    return diplomat_out;
  }

  titlecase_segment_with_only_case_data_v1(arg_s, arg_locale, arg_options) {
    const buf_arg_s = diplomatRuntime.DiplomatBuf.str(wasm, arg_s);
    const field_leading_adjustment_arg_options = arg_options["leading_adjustment"];
    const field_trailing_case_arg_options = arg_options["trailing_case"];
    const diplomat_out = diplomatRuntime.withWriteable(wasm, (writeable) => {
      return (() => {
        const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
        wasm.ICU4XCaseMapper_titlecase_segment_with_only_case_data_v1(diplomat_receive_buffer, this.underlying, buf_arg_s.ptr, buf_arg_s.size, arg_locale.underlying, ICU4XLeadingAdjustment_js_to_rust[field_leading_adjustment_arg_options], ICU4XTrailingCase_js_to_rust[field_trailing_case_arg_options], writeable);
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
    buf_arg_s.free();
    return diplomat_out;
  }

  fold(arg_s) {
    const buf_arg_s = diplomatRuntime.DiplomatBuf.str(wasm, arg_s);
    const diplomat_out = diplomatRuntime.withWriteable(wasm, (writeable) => {
      return (() => {
        const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
        wasm.ICU4XCaseMapper_fold(diplomat_receive_buffer, this.underlying, buf_arg_s.ptr, buf_arg_s.size, writeable);
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
    buf_arg_s.free();
    return diplomat_out;
  }

  fold_turkic(arg_s) {
    const buf_arg_s = diplomatRuntime.DiplomatBuf.str(wasm, arg_s);
    const diplomat_out = diplomatRuntime.withWriteable(wasm, (writeable) => {
      return (() => {
        const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
        wasm.ICU4XCaseMapper_fold_turkic(diplomat_receive_buffer, this.underlying, buf_arg_s.ptr, buf_arg_s.size, writeable);
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
    buf_arg_s.free();
    return diplomat_out;
  }

  add_case_closure_to(arg_c, arg_builder) {
    wasm.ICU4XCaseMapper_add_case_closure_to(this.underlying, diplomatRuntime.extractCodePoint(arg_c, 'arg_c'), arg_builder.underlying);
  }

  simple_lowercase(arg_ch) {
    return wasm.ICU4XCaseMapper_simple_lowercase(this.underlying, diplomatRuntime.extractCodePoint(arg_ch, 'arg_ch'));
  }

  simple_uppercase(arg_ch) {
    return wasm.ICU4XCaseMapper_simple_uppercase(this.underlying, diplomatRuntime.extractCodePoint(arg_ch, 'arg_ch'));
  }

  simple_titlecase(arg_ch) {
    return wasm.ICU4XCaseMapper_simple_titlecase(this.underlying, diplomatRuntime.extractCodePoint(arg_ch, 'arg_ch'));
  }

  simple_fold(arg_ch) {
    return wasm.ICU4XCaseMapper_simple_fold(this.underlying, diplomatRuntime.extractCodePoint(arg_ch, 'arg_ch'));
  }

  simple_fold_turkic(arg_ch) {
    return wasm.ICU4XCaseMapper_simple_fold_turkic(this.underlying, diplomatRuntime.extractCodePoint(arg_ch, 'arg_ch'));
  }
}
