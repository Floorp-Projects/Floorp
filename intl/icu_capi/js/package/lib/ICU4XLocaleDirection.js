import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"

export const ICU4XLocaleDirection_js_to_rust = {
  "LeftToRight": 0,
  "RightToLeft": 1,
  "Unknown": 2,
};

export const ICU4XLocaleDirection_rust_to_js = {
  [0]: "LeftToRight",
  [1]: "RightToLeft",
  [2]: "Unknown",
};

export const ICU4XLocaleDirection = {
  "LeftToRight": "LeftToRight",
  "RightToLeft": "RightToLeft",
  "Unknown": "Unknown",
};
