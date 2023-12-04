import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"

export const ICU4XListLength_js_to_rust = {
  "Wide": 0,
  "Short": 1,
  "Narrow": 2,
};

export const ICU4XListLength_rust_to_js = {
  [0]: "Wide",
  [1]: "Short",
  [2]: "Narrow",
};

export const ICU4XListLength = {
  "Wide": "Wide",
  "Short": "Short",
  "Narrow": "Narrow",
};
