import { FFIError } from "./diplomat-runtime"
import { ICU4XCustomTimeZone } from "./ICU4XCustomTimeZone";
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XError } from "./ICU4XError";
import { ICU4XIsoTimeZoneOptions } from "./ICU4XIsoTimeZoneOptions";
import { ICU4XLocale } from "./ICU4XLocale";

/**

 * An ICU4X TimeZoneFormatter object capable of formatting an {@link ICU4XCustomTimeZone `ICU4XCustomTimeZone`} type (and others) as a string

 * See the {@link https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html Rust documentation for `TimeZoneFormatter`} for more information.
 */
export class ICU4XTimeZoneFormatter {

  /**

   * Creates a new {@link ICU4XTimeZoneFormatter `ICU4XTimeZoneFormatter`} from locale data.

   * Uses localized GMT as the fallback format.

   * See the {@link https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.try_new Rust documentation for `try_new`} for more information.

   * Additional information: {@link https://docs.rs/icu/latest/icu/datetime/time_zone/enum.FallbackFormat.html 1}
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_with_localized_gmt_fallback(provider: ICU4XDataProvider, locale: ICU4XLocale): ICU4XTimeZoneFormatter | never;

  /**

   * Creates a new {@link ICU4XTimeZoneFormatter `ICU4XTimeZoneFormatter`} from locale data.

   * Uses ISO-8601 as the fallback format.

   * See the {@link https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.try_new Rust documentation for `try_new`} for more information.

   * Additional information: {@link https://docs.rs/icu/latest/icu/datetime/time_zone/enum.FallbackFormat.html 1}
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_with_iso_8601_fallback(provider: ICU4XDataProvider, locale: ICU4XLocale, options: ICU4XIsoTimeZoneOptions): ICU4XTimeZoneFormatter | never;

  /**

   * Loads generic non-location long format. Example: "Pacific Time"

   * See the {@link https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.include_generic_non_location_long Rust documentation for `include_generic_non_location_long`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  load_generic_non_location_long(provider: ICU4XDataProvider): void | never;

  /**

   * Loads generic non-location short format. Example: "PT"

   * See the {@link https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.include_generic_non_location_short Rust documentation for `include_generic_non_location_short`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  load_generic_non_location_short(provider: ICU4XDataProvider): void | never;

  /**

   * Loads specific non-location long format. Example: "Pacific Standard Time"

   * See the {@link https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.include_specific_non_location_long Rust documentation for `include_specific_non_location_long`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  load_specific_non_location_long(provider: ICU4XDataProvider): void | never;

  /**

   * Loads specific non-location short format. Example: "PST"

   * See the {@link https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.include_specific_non_location_short Rust documentation for `include_specific_non_location_short`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  load_specific_non_location_short(provider: ICU4XDataProvider): void | never;

  /**

   * Loads generic location format. Example: "Los Angeles Time"

   * See the {@link https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.include_generic_location_format Rust documentation for `include_generic_location_format`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  load_generic_location_format(provider: ICU4XDataProvider): void | never;

  /**

   * Loads localized GMT format. Example: "GMT-07:00"

   * See the {@link https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.include_localized_gmt_format Rust documentation for `include_localized_gmt_format`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  include_localized_gmt_format(): void | never;

  /**

   * Loads ISO-8601 format. Example: "-07:00"

   * See the {@link https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.include_iso_8601_format Rust documentation for `include_iso_8601_format`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  load_iso_8601_format(options: ICU4XIsoTimeZoneOptions): void | never;

  /**

   * Formats a {@link ICU4XCustomTimeZone `ICU4XCustomTimeZone`} to a string.

   * See the {@link https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.format Rust documentation for `format`} for more information.

   * See the {@link https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.format_to_string Rust documentation for `format_to_string`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  format_custom_time_zone(value: ICU4XCustomTimeZone): string | never;
}
