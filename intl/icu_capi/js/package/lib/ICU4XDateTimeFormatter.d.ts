import { FFIError } from "./diplomat-runtime"
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XDateLength } from "./ICU4XDateLength";
import { ICU4XDateTime } from "./ICU4XDateTime";
import { ICU4XError } from "./ICU4XError";
import { ICU4XIsoDateTime } from "./ICU4XIsoDateTime";
import { ICU4XLocale } from "./ICU4XLocale";
import { ICU4XTimeLength } from "./ICU4XTimeLength";

/**

 * An ICU4X DateFormatter object capable of formatting a {@link ICU4XDateTime `ICU4XDateTime`} as a string, using some calendar specified at runtime in the locale.

 * See the {@link https://docs.rs/icu/latest/icu/datetime/struct.DateTimeFormatter.html Rust documentation for `DateTimeFormatter`} for more information.
 */
export class ICU4XDateTimeFormatter {

  /**

   * Creates a new {@link ICU4XDateTimeFormatter `ICU4XDateTimeFormatter`} from locale data.

   * See the {@link https://docs.rs/icu/latest/icu/datetime/struct.DateTimeFormatter.html#method.try_new Rust documentation for `try_new`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_with_lengths(provider: ICU4XDataProvider, locale: ICU4XLocale, date_length: ICU4XDateLength, time_length: ICU4XTimeLength): ICU4XDateTimeFormatter | never;

  /**

   * Formats a {@link ICU4XDateTime `ICU4XDateTime`} to a string.

   * See the {@link https://docs.rs/icu/latest/icu/datetime/struct.DateTimeFormatter.html#method.format Rust documentation for `format`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  format_datetime(value: ICU4XDateTime): string | never;

  /**

   * Formats a {@link ICU4XIsoDateTime `ICU4XIsoDateTime`} to a string.

   * Will convert to this formatter's calendar first

   * See the {@link https://docs.rs/icu/latest/icu/datetime/struct.DateTimeFormatter.html#method.format Rust documentation for `format`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  format_iso_datetime(value: ICU4XIsoDateTime): string | never;
}
