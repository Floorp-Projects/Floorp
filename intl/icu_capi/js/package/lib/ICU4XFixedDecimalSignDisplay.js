import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"

export const ICU4XFixedDecimalSignDisplay_js_to_rust = {
  "Auto": 0,
  "Never": 1,
  "Always": 2,
  "ExceptZero": 3,
  "Negative": 4,
};

export const ICU4XFixedDecimalSignDisplay_rust_to_js = {
  [0]: "Auto",
  [1]: "Never",
  [2]: "Always",
  [3]: "ExceptZero",
  [4]: "Negative",
};

export const ICU4XFixedDecimalSignDisplay = {
  "Auto": "Auto",
  "Never": "Never",
  "Always": "Always",
  "ExceptZero": "ExceptZero",
  "Negative": "Negative",
};
