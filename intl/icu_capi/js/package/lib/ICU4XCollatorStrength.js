import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"

export const ICU4XCollatorStrength_js_to_rust = {
  "Auto": 0,
  "Primary": 1,
  "Secondary": 2,
  "Tertiary": 3,
  "Quaternary": 4,
  "Identical": 5,
};

export const ICU4XCollatorStrength_rust_to_js = {
  [0]: "Auto",
  [1]: "Primary",
  [2]: "Secondary",
  [3]: "Tertiary",
  [4]: "Quaternary",
  [5]: "Identical",
};

export const ICU4XCollatorStrength = {
  "Auto": "Auto",
  "Primary": "Primary",
  "Secondary": "Secondary",
  "Tertiary": "Tertiary",
  "Quaternary": "Quaternary",
  "Identical": "Identical",
};
