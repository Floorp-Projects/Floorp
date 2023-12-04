import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"
import { ICU4XLeadingAdjustment_js_to_rust, ICU4XLeadingAdjustment_rust_to_js } from "./ICU4XLeadingAdjustment.js"
import { ICU4XTrailingCase_js_to_rust, ICU4XTrailingCase_rust_to_js } from "./ICU4XTrailingCase.js"

export class ICU4XTitlecaseOptionsV1 {
  constructor(underlying) {
    this.leading_adjustment = ICU4XLeadingAdjustment_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, underlying)];
    this.trailing_case = ICU4XTrailingCase_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, underlying + 4)];
  }

  static default_options() {
    return (() => {
      const diplomat_receive_buffer = wasm.diplomat_alloc(8, 4);
      wasm.ICU4XTitlecaseOptionsV1_default_options(diplomat_receive_buffer);
      const out = new ICU4XTitlecaseOptionsV1(diplomat_receive_buffer);
      wasm.diplomat_free(diplomat_receive_buffer, 8, 4);
      return out;
    })();
  }
}
