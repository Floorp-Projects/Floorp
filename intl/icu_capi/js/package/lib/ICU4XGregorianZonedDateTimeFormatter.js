import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"
import { ICU4XDateLength_js_to_rust, ICU4XDateLength_rust_to_js } from "./ICU4XDateLength.js"
import { ICU4XError_js_to_rust, ICU4XError_rust_to_js } from "./ICU4XError.js"
import { ICU4XIsoTimeZoneFormat_js_to_rust, ICU4XIsoTimeZoneFormat_rust_to_js } from "./ICU4XIsoTimeZoneFormat.js"
import { ICU4XIsoTimeZoneMinuteDisplay_js_to_rust, ICU4XIsoTimeZoneMinuteDisplay_rust_to_js } from "./ICU4XIsoTimeZoneMinuteDisplay.js"
import { ICU4XIsoTimeZoneSecondDisplay_js_to_rust, ICU4XIsoTimeZoneSecondDisplay_rust_to_js } from "./ICU4XIsoTimeZoneSecondDisplay.js"
import { ICU4XTimeLength_js_to_rust, ICU4XTimeLength_rust_to_js } from "./ICU4XTimeLength.js"

const ICU4XGregorianZonedDateTimeFormatter_box_destroy_registry = new FinalizationRegistry(underlying => {
  wasm.ICU4XGregorianZonedDateTimeFormatter_destroy(underlying);
});

export class ICU4XGregorianZonedDateTimeFormatter {
  #lifetimeEdges = [];
  constructor(underlying, owned, edges) {
    this.underlying = underlying;
    this.#lifetimeEdges.push(...edges);
    if (owned) {
      ICU4XGregorianZonedDateTimeFormatter_box_destroy_registry.register(this, underlying);
    }
  }

  static create_with_lengths(arg_provider, arg_locale, arg_date_length, arg_time_length) {
    return (() => {
      const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
      wasm.ICU4XGregorianZonedDateTimeFormatter_create_with_lengths(diplomat_receive_buffer, arg_provider.underlying, arg_locale.underlying, ICU4XDateLength_js_to_rust[arg_date_length], ICU4XTimeLength_js_to_rust[arg_time_length]);
      const is_ok = diplomatRuntime.resultFlag(wasm, diplomat_receive_buffer, 4);
      if (is_ok) {
        const ok_value = new ICU4XGregorianZonedDateTimeFormatter(diplomatRuntime.ptrRead(wasm, diplomat_receive_buffer), true, []);
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        return ok_value;
      } else {
        const throw_value = ICU4XError_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, diplomat_receive_buffer)];
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        throw new diplomatRuntime.FFIError(throw_value);
      }
    })();
  }

  static create_with_lengths_and_iso_8601_time_zone_fallback(arg_provider, arg_locale, arg_date_length, arg_time_length, arg_zone_options) {
    const field_format_arg_zone_options = arg_zone_options["format"];
    const field_minutes_arg_zone_options = arg_zone_options["minutes"];
    const field_seconds_arg_zone_options = arg_zone_options["seconds"];
    return (() => {
      const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
      wasm.ICU4XGregorianZonedDateTimeFormatter_create_with_lengths_and_iso_8601_time_zone_fallback(diplomat_receive_buffer, arg_provider.underlying, arg_locale.underlying, ICU4XDateLength_js_to_rust[arg_date_length], ICU4XTimeLength_js_to_rust[arg_time_length], ICU4XIsoTimeZoneFormat_js_to_rust[field_format_arg_zone_options], ICU4XIsoTimeZoneMinuteDisplay_js_to_rust[field_minutes_arg_zone_options], ICU4XIsoTimeZoneSecondDisplay_js_to_rust[field_seconds_arg_zone_options]);
      const is_ok = diplomatRuntime.resultFlag(wasm, diplomat_receive_buffer, 4);
      if (is_ok) {
        const ok_value = new ICU4XGregorianZonedDateTimeFormatter(diplomatRuntime.ptrRead(wasm, diplomat_receive_buffer), true, []);
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        return ok_value;
      } else {
        const throw_value = ICU4XError_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, diplomat_receive_buffer)];
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        throw new diplomatRuntime.FFIError(throw_value);
      }
    })();
  }

  format_iso_datetime_with_custom_time_zone(arg_datetime, arg_time_zone) {
    return diplomatRuntime.withWriteable(wasm, (writeable) => {
      return (() => {
        const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
        wasm.ICU4XGregorianZonedDateTimeFormatter_format_iso_datetime_with_custom_time_zone(diplomat_receive_buffer, this.underlying, arg_datetime.underlying, arg_time_zone.underlying, writeable);
        const is_ok = diplomatRuntime.resultFlag(wasm, diplomat_receive_buffer, 4);
        if (is_ok) {
          const ok_value = {};
          wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
          return ok_value;
        } else {
          const throw_value = ICU4XError_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, diplomat_receive_buffer)];
          wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
          throw new diplomatRuntime.FFIError(throw_value);
        }
      })();
    });
  }
}
