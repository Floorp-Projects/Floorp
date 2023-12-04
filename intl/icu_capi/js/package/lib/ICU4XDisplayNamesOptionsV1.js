import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"
import { ICU4XDisplayNamesFallback_js_to_rust, ICU4XDisplayNamesFallback_rust_to_js } from "./ICU4XDisplayNamesFallback.js"
import { ICU4XDisplayNamesStyle_js_to_rust, ICU4XDisplayNamesStyle_rust_to_js } from "./ICU4XDisplayNamesStyle.js"
import { ICU4XLanguageDisplay_js_to_rust, ICU4XLanguageDisplay_rust_to_js } from "./ICU4XLanguageDisplay.js"

export class ICU4XDisplayNamesOptionsV1 {
  constructor(underlying) {
    this.style = ICU4XDisplayNamesStyle_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, underlying)];
    this.fallback = ICU4XDisplayNamesFallback_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, underlying + 4)];
    this.language_display = ICU4XLanguageDisplay_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, underlying + 8)];
  }
}
