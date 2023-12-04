import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"
import { ICU4XError_js_to_rust, ICU4XError_rust_to_js } from "./ICU4XError.js"
import { ICU4XIsoWeekday_js_to_rust, ICU4XIsoWeekday_rust_to_js } from "./ICU4XIsoWeekday.js"

const ICU4XWeekCalculator_box_destroy_registry = new FinalizationRegistry(underlying => {
  wasm.ICU4XWeekCalculator_destroy(underlying);
});

export class ICU4XWeekCalculator {
  #lifetimeEdges = [];
  constructor(underlying, owned, edges) {
    this.underlying = underlying;
    this.#lifetimeEdges.push(...edges);
    if (owned) {
      ICU4XWeekCalculator_box_destroy_registry.register(this, underlying);
    }
  }

  static create(arg_provider, arg_locale) {
    return (() => {
      const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
      wasm.ICU4XWeekCalculator_create(diplomat_receive_buffer, arg_provider.underlying, arg_locale.underlying);
      const is_ok = diplomatRuntime.resultFlag(wasm, diplomat_receive_buffer, 4);
      if (is_ok) {
        const ok_value = new ICU4XWeekCalculator(diplomatRuntime.ptrRead(wasm, diplomat_receive_buffer), true, []);
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        return ok_value;
      } else {
        const throw_value = ICU4XError_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, diplomat_receive_buffer)];
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        throw new diplomatRuntime.FFIError(throw_value);
      }
    })();
  }

  static create_from_first_day_of_week_and_min_week_days(arg_first_weekday, arg_min_week_days) {
    return new ICU4XWeekCalculator(wasm.ICU4XWeekCalculator_create_from_first_day_of_week_and_min_week_days(ICU4XIsoWeekday_js_to_rust[arg_first_weekday], arg_min_week_days), true, []);
  }

  first_weekday() {
    return ICU4XIsoWeekday_rust_to_js[wasm.ICU4XWeekCalculator_first_weekday(this.underlying)];
  }

  min_week_days() {
    return wasm.ICU4XWeekCalculator_min_week_days(this.underlying);
  }
}
