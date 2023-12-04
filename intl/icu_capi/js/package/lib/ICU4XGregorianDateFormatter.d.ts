import { FFIError } from "./diplomat-runtime"
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XDateLength } from "./ICU4XDateLength";
import { ICU4XError } from "./ICU4XError";
import { ICU4XIsoDate } from "./ICU4XIsoDate";
import { ICU4XIsoDateTime } from "./ICU4XIsoDateTime";
import { ICU4XLocale } from "./ICU4XLocale";

/**

 * An ICU4X TypedDateFormatter object capable of formatting a {@link ICU4XIsoDateTime `ICU4XIsoDateTime`} as a string, using the Gregorian Calendar.

 * See the {@link https://docs.rs/icu/latest/icu/datetime/struct.TypedDateFormatter.html Rust documentation for `TypedDateFormatter`} for more information.
 */
export class ICU4XGregorianDateFormatter {

  /**

   * Creates a new {@link ICU4XGregorianDateFormatter `ICU4XGregorianDateFormatter`} from locale data.

   * See the {@link https://docs.rs/icu/latest/icu/datetime/struct.TypedDateFormatter.html#method.try_new_with_length Rust documentation for `try_new_with_length`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_with_length(provider: ICU4XDataProvider, locale: ICU4XLocale, length: ICU4XDateLength): ICU4XGregorianDateFormatter | never;

  /**

   * Formats a {@link ICU4XIsoDate `ICU4XIsoDate`} to a string.

   * See the {@link https://docs.rs/icu/latest/icu/datetime/struct.TypedDateFormatter.html#method.format Rust documentation for `format`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  format_iso_date(value: ICU4XIsoDate): string | never;

  /**

   * Formats a {@link ICU4XIsoDateTime `ICU4XIsoDateTime`} to a string.

   * See the {@link https://docs.rs/icu/latest/icu/datetime/struct.TypedDateFormatter.html#method.format Rust documentation for `format`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  format_iso_datetime(value: ICU4XIsoDateTime): string | never;
}
