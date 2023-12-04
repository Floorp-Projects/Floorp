import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"
import { ICU4XCodePointSetData } from "./ICU4XCodePointSetData.js"
import { ICU4XScriptExtensionsSet } from "./ICU4XScriptExtensionsSet.js"

const ICU4XScriptWithExtensionsBorrowed_box_destroy_registry = new FinalizationRegistry(underlying => {
  wasm.ICU4XScriptWithExtensionsBorrowed_destroy(underlying);
});

export class ICU4XScriptWithExtensionsBorrowed {
  #lifetimeEdges = [];
  constructor(underlying, owned, edges) {
    this.underlying = underlying;
    this.#lifetimeEdges.push(...edges);
    if (owned) {
      ICU4XScriptWithExtensionsBorrowed_box_destroy_registry.register(this, underlying);
    }
  }

  get_script_val(arg_code_point) {
    return wasm.ICU4XScriptWithExtensionsBorrowed_get_script_val(this.underlying, arg_code_point);
  }

  get_script_extensions_val(arg_code_point) {
    return new ICU4XScriptExtensionsSet(wasm.ICU4XScriptWithExtensionsBorrowed_get_script_extensions_val(this.underlying, arg_code_point), true, [this]);
  }

  has_script(arg_code_point, arg_script) {
    return wasm.ICU4XScriptWithExtensionsBorrowed_has_script(this.underlying, arg_code_point, arg_script);
  }

  get_script_extensions_set(arg_script) {
    return new ICU4XCodePointSetData(wasm.ICU4XScriptWithExtensionsBorrowed_get_script_extensions_set(this.underlying, arg_script), true, []);
  }
}
