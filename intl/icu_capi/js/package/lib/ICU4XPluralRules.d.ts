import { FFIError } from "./diplomat-runtime"
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XError } from "./ICU4XError";
import { ICU4XLocale } from "./ICU4XLocale";
import { ICU4XPluralCategories } from "./ICU4XPluralCategories";
import { ICU4XPluralCategory } from "./ICU4XPluralCategory";
import { ICU4XPluralOperands } from "./ICU4XPluralOperands";

/**

 * FFI version of `PluralRules`.

 * See the {@link https://docs.rs/icu/latest/icu/plurals/struct.PluralRules.html Rust documentation for `PluralRules`} for more information.
 */
export class ICU4XPluralRules {

  /**

   * Construct an {@link ICU4XPluralRules `ICU4XPluralRules`} for the given locale, for cardinal numbers

   * See the {@link https://docs.rs/icu/latest/icu/plurals/struct.PluralRules.html#method.try_new_cardinal Rust documentation for `try_new_cardinal`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_cardinal(provider: ICU4XDataProvider, locale: ICU4XLocale): ICU4XPluralRules | never;

  /**

   * Construct an {@link ICU4XPluralRules `ICU4XPluralRules`} for the given locale, for ordinal numbers

   * See the {@link https://docs.rs/icu/latest/icu/plurals/struct.PluralRules.html#method.try_new_ordinal Rust documentation for `try_new_ordinal`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_ordinal(provider: ICU4XDataProvider, locale: ICU4XLocale): ICU4XPluralRules | never;

  /**

   * Get the category for a given number represented as operands

   * See the {@link https://docs.rs/icu/latest/icu/plurals/struct.PluralRules.html#method.category_for Rust documentation for `category_for`} for more information.
   */
  category_for(op: ICU4XPluralOperands): ICU4XPluralCategory;

  /**

   * Get all of the categories needed in the current locale

   * See the {@link https://docs.rs/icu/latest/icu/plurals/struct.PluralRules.html#method.categories Rust documentation for `categories`} for more information.
   */
  categories(): ICU4XPluralCategories;
}
