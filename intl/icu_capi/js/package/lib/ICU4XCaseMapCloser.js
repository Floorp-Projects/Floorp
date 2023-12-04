import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"
import { ICU4XError_js_to_rust, ICU4XError_rust_to_js } from "./ICU4XError.js"

const ICU4XCaseMapCloser_box_destroy_registry = new FinalizationRegistry(underlying => {
  wasm.ICU4XCaseMapCloser_destroy(underlying);
});

export class ICU4XCaseMapCloser {
  #lifetimeEdges = [];
  constructor(underlying, owned, edges) {
    this.underlying = underlying;
    this.#lifetimeEdges.push(...edges);
    if (owned) {
      ICU4XCaseMapCloser_box_destroy_registry.register(this, underlying);
    }
  }

  static create(arg_provider) {
    return (() => {
      const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
      wasm.ICU4XCaseMapCloser_create(diplomat_receive_buffer, arg_provider.underlying);
      const is_ok = diplomatRuntime.resultFlag(wasm, diplomat_receive_buffer, 4);
      if (is_ok) {
        const ok_value = new ICU4XCaseMapCloser(diplomatRuntime.ptrRead(wasm, diplomat_receive_buffer), true, []);
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        return ok_value;
      } else {
        const throw_value = ICU4XError_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, diplomat_receive_buffer)];
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        throw new diplomatRuntime.FFIError(throw_value);
      }
    })();
  }

  add_case_closure_to(arg_c, arg_builder) {
    wasm.ICU4XCaseMapCloser_add_case_closure_to(this.underlying, diplomatRuntime.extractCodePoint(arg_c, 'arg_c'), arg_builder.underlying);
  }

  add_string_case_closure_to(arg_s, arg_builder) {
    const buf_arg_s = diplomatRuntime.DiplomatBuf.str(wasm, arg_s);
    const diplomat_out = wasm.ICU4XCaseMapCloser_add_string_case_closure_to(this.underlying, buf_arg_s.ptr, buf_arg_s.size, arg_builder.underlying);
    buf_arg_s.free();
    return diplomat_out;
  }
}
