import { FFIError } from "./diplomat-runtime"
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XError } from "./ICU4XError";
import { ICU4XLocale } from "./ICU4XLocale";
import { ICU4XLocaleDirection } from "./ICU4XLocaleDirection";
import { ICU4XLocaleExpander } from "./ICU4XLocaleExpander";

/**

 * See the {@link https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleDirectionality.html Rust documentation for `LocaleDirectionality`} for more information.
 */
export class ICU4XLocaleDirectionality {

  /**

   * Construct a new ICU4XLocaleDirectionality instance

   * See the {@link https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleDirectionality.html#method.new Rust documentation for `new`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create(provider: ICU4XDataProvider): ICU4XLocaleDirectionality | never;

  /**

   * Construct a new ICU4XLocaleDirectionality instance with a custom expander

   * See the {@link https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleDirectionality.html#method.new_with_expander Rust documentation for `new_with_expander`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_with_expander(provider: ICU4XDataProvider, expander: ICU4XLocaleExpander): ICU4XLocaleDirectionality | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleDirectionality.html#method.get Rust documentation for `get`} for more information.
   */
  get(locale: ICU4XLocale): ICU4XLocaleDirection;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleDirectionality.html#method.is_left_to_right Rust documentation for `is_left_to_right`} for more information.
   */
  is_left_to_right(locale: ICU4XLocale): boolean;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleDirectionality.html#method.is_right_to_left Rust documentation for `is_right_to_left`} for more information.
   */
  is_right_to_left(locale: ICU4XLocale): boolean;
}
