import { FFIError } from "./diplomat-runtime"
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XDate } from "./ICU4XDate";
import { ICU4XDateLength } from "./ICU4XDateLength";
import { ICU4XDateTime } from "./ICU4XDateTime";
import { ICU4XError } from "./ICU4XError";
import { ICU4XIsoDate } from "./ICU4XIsoDate";
import { ICU4XIsoDateTime } from "./ICU4XIsoDateTime";
import { ICU4XLocale } from "./ICU4XLocale";

/**

 * An ICU4X DateFormatter object capable of formatting a {@link ICU4XDate `ICU4XDate`} as a string, using some calendar specified at runtime in the locale.

 * See the {@link https://docs.rs/icu/latest/icu/datetime/struct.DateFormatter.html Rust documentation for `DateFormatter`} for more information.
 */
export class ICU4XDateFormatter {

  /**

   * Creates a new {@link ICU4XDateFormatter `ICU4XDateFormatter`} from locale data.

   * See the {@link https://docs.rs/icu/latest/icu/datetime/struct.DateFormatter.html#method.try_new_with_length Rust documentation for `try_new_with_length`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_with_length(provider: ICU4XDataProvider, locale: ICU4XLocale, date_length: ICU4XDateLength): ICU4XDateFormatter | never;

  /**

   * Formats a {@link ICU4XDate `ICU4XDate`} to a string.

   * See the {@link https://docs.rs/icu/latest/icu/datetime/struct.DateFormatter.html#method.format Rust documentation for `format`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  format_date(value: ICU4XDate): string | never;

  /**

   * Formats a {@link ICU4XIsoDate `ICU4XIsoDate`} to a string.

   * Will convert to this formatter's calendar first

   * See the {@link https://docs.rs/icu/latest/icu/datetime/struct.DateFormatter.html#method.format Rust documentation for `format`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  format_iso_date(value: ICU4XIsoDate): string | never;

  /**

   * Formats a {@link ICU4XDateTime `ICU4XDateTime`} to a string.

   * See the {@link https://docs.rs/icu/latest/icu/datetime/struct.DateFormatter.html#method.format Rust documentation for `format`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  format_datetime(value: ICU4XDateTime): string | never;

  /**

   * Formats a {@link ICU4XIsoDateTime `ICU4XIsoDateTime`} to a string.

   * Will convert to this formatter's calendar first

   * See the {@link https://docs.rs/icu/latest/icu/datetime/struct.DateFormatter.html#method.format Rust documentation for `format`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  format_iso_datetime(value: ICU4XIsoDateTime): string | never;
}
