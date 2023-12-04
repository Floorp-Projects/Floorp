import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"
import { ICU4XError_js_to_rust, ICU4XError_rust_to_js } from "./ICU4XError.js"
import { ICU4XSentenceBreakIteratorLatin1 } from "./ICU4XSentenceBreakIteratorLatin1.js"
import { ICU4XSentenceBreakIteratorUtf16 } from "./ICU4XSentenceBreakIteratorUtf16.js"
import { ICU4XSentenceBreakIteratorUtf8 } from "./ICU4XSentenceBreakIteratorUtf8.js"

const ICU4XSentenceSegmenter_box_destroy_registry = new FinalizationRegistry(underlying => {
  wasm.ICU4XSentenceSegmenter_destroy(underlying);
});

export class ICU4XSentenceSegmenter {
  #lifetimeEdges = [];
  constructor(underlying, owned, edges) {
    this.underlying = underlying;
    this.#lifetimeEdges.push(...edges);
    if (owned) {
      ICU4XSentenceSegmenter_box_destroy_registry.register(this, underlying);
    }
  }

  static create(arg_provider) {
    return (() => {
      const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
      wasm.ICU4XSentenceSegmenter_create(diplomat_receive_buffer, arg_provider.underlying);
      const is_ok = diplomatRuntime.resultFlag(wasm, diplomat_receive_buffer, 4);
      if (is_ok) {
        const ok_value = new ICU4XSentenceSegmenter(diplomatRuntime.ptrRead(wasm, diplomat_receive_buffer), true, []);
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        return ok_value;
      } else {
        const throw_value = ICU4XError_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, diplomat_receive_buffer)];
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        throw new diplomatRuntime.FFIError(throw_value);
      }
    })();
  }

  segment_utf8(arg_input) {
    const buf_arg_input = diplomatRuntime.DiplomatBuf.str(wasm, arg_input);
    return new ICU4XSentenceBreakIteratorUtf8(wasm.ICU4XSentenceSegmenter_segment_utf8(this.underlying, buf_arg_input.ptr, buf_arg_input.size), true, [this, buf_arg_input]);
  }

  segment_utf16(arg_input) {
    const buf_arg_input = diplomatRuntime.DiplomatBuf.slice(wasm, arg_input, 2);
    return new ICU4XSentenceBreakIteratorUtf16(wasm.ICU4XSentenceSegmenter_segment_utf16(this.underlying, buf_arg_input.ptr, buf_arg_input.size), true, [this, buf_arg_input]);
  }

  segment_latin1(arg_input) {
    const buf_arg_input = diplomatRuntime.DiplomatBuf.slice(wasm, arg_input, 1);
    return new ICU4XSentenceBreakIteratorLatin1(wasm.ICU4XSentenceSegmenter_segment_latin1(this.underlying, buf_arg_input.ptr, buf_arg_input.size), true, [this, buf_arg_input]);
  }
}
