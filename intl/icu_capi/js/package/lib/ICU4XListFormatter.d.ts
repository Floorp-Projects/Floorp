import { FFIError } from "./diplomat-runtime"
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XError } from "./ICU4XError";
import { ICU4XList } from "./ICU4XList";
import { ICU4XListLength } from "./ICU4XListLength";
import { ICU4XLocale } from "./ICU4XLocale";

/**

 * See the {@link https://docs.rs/icu/latest/icu/list/struct.ListFormatter.html Rust documentation for `ListFormatter`} for more information.
 */
export class ICU4XListFormatter {

  /**

   * Construct a new ICU4XListFormatter instance for And patterns

   * See the {@link https://docs.rs/icu/latest/icu/list/struct.ListFormatter.html#method.try_new_and_with_length Rust documentation for `try_new_and_with_length`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_and_with_length(provider: ICU4XDataProvider, locale: ICU4XLocale, length: ICU4XListLength): ICU4XListFormatter | never;

  /**

   * Construct a new ICU4XListFormatter instance for And patterns

   * See the {@link https://docs.rs/icu/latest/icu/list/struct.ListFormatter.html#method.try_new_or_with_length Rust documentation for `try_new_or_with_length`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_or_with_length(provider: ICU4XDataProvider, locale: ICU4XLocale, length: ICU4XListLength): ICU4XListFormatter | never;

  /**

   * Construct a new ICU4XListFormatter instance for And patterns

   * See the {@link https://docs.rs/icu/latest/icu/list/struct.ListFormatter.html#method.try_new_unit_with_length Rust documentation for `try_new_unit_with_length`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_unit_with_length(provider: ICU4XDataProvider, locale: ICU4XLocale, length: ICU4XListLength): ICU4XListFormatter | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/list/struct.ListFormatter.html#method.format Rust documentation for `format`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  format(list: ICU4XList): string | never;
}
