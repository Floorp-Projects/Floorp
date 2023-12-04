import { FFIError } from "./diplomat-runtime"
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XDisplayNamesOptionsV1 } from "./ICU4XDisplayNamesOptionsV1";
import { ICU4XError } from "./ICU4XError";
import { ICU4XLocale } from "./ICU4XLocale";

/**

 * See the {@link https://docs.rs/icu/latest/icu/displaynames/struct.LocaleDisplayNamesFormatter.html Rust documentation for `LocaleDisplayNamesFormatter`} for more information.
 */
export class ICU4XLocaleDisplayNamesFormatter {

  /**

   * Creates a new `LocaleDisplayNamesFormatter` from locale data and an options bag.

   * See the {@link https://docs.rs/icu/latest/icu/displaynames/struct.LocaleDisplayNamesFormatter.html#method.try_new Rust documentation for `try_new`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create(provider: ICU4XDataProvider, locale: ICU4XLocale, options: ICU4XDisplayNamesOptionsV1): ICU4XLocaleDisplayNamesFormatter | never;

  /**

   * Returns the locale-specific display name of a locale.

   * See the {@link https://docs.rs/icu/latest/icu/displaynames/struct.LocaleDisplayNamesFormatter.html#method.of Rust documentation for `of`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  of(locale: ICU4XLocale): string | never;
}
