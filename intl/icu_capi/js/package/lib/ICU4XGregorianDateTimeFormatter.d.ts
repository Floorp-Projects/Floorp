import { FFIError } from "./diplomat-runtime"
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XDateLength } from "./ICU4XDateLength";
import { ICU4XError } from "./ICU4XError";
import { ICU4XIsoDateTime } from "./ICU4XIsoDateTime";
import { ICU4XLocale } from "./ICU4XLocale";
import { ICU4XTimeLength } from "./ICU4XTimeLength";

/**

 * An ICU4X TypedDateTimeFormatter object capable of formatting a {@link ICU4XIsoDateTime `ICU4XIsoDateTime`} as a string, using the Gregorian Calendar.

 * See the {@link https://docs.rs/icu/latest/icu/datetime/struct.TypedDateTimeFormatter.html Rust documentation for `TypedDateTimeFormatter`} for more information.
 */
export class ICU4XGregorianDateTimeFormatter {

  /**

   * Creates a new {@link ICU4XGregorianDateFormatter `ICU4XGregorianDateFormatter`} from locale data.

   * See the {@link https://docs.rs/icu/latest/icu/datetime/struct.TypedDateTimeFormatter.html#method.try_new Rust documentation for `try_new`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_with_lengths(provider: ICU4XDataProvider, locale: ICU4XLocale, date_length: ICU4XDateLength, time_length: ICU4XTimeLength): ICU4XGregorianDateTimeFormatter | never;

  /**

   * Formats a {@link ICU4XIsoDateTime `ICU4XIsoDateTime`} to a string.

   * See the {@link https://docs.rs/icu/latest/icu/datetime/struct.TypedDateTimeFormatter.html#method.format Rust documentation for `format`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  format_iso_datetime(value: ICU4XIsoDateTime): string | never;
}
