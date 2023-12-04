import { FFIError } from "./diplomat-runtime"
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XError } from "./ICU4XError";
import { ICU4XLocale } from "./ICU4XLocale";
import { ICU4XTransformResult } from "./ICU4XTransformResult";

/**

 * A locale canonicalizer.

 * See the {@link https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleCanonicalizer.html Rust documentation for `LocaleCanonicalizer`} for more information.
 */
export class ICU4XLocaleCanonicalizer {

  /**

   * Create a new {@link ICU4XLocaleCanonicalizer `ICU4XLocaleCanonicalizer`}.

   * See the {@link https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleCanonicalizer.html#method.new Rust documentation for `new`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create(provider: ICU4XDataProvider): ICU4XLocaleCanonicalizer | never;

  /**

   * Create a new {@link ICU4XLocaleCanonicalizer `ICU4XLocaleCanonicalizer`} with extended data.

   * See the {@link https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleCanonicalizer.html#method.new_with_expander Rust documentation for `new_with_expander`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_extended(provider: ICU4XDataProvider): ICU4XLocaleCanonicalizer | never;

  /**

   * FFI version of `LocaleCanonicalizer::canonicalize()`.

   * See the {@link https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleCanonicalizer.html#method.canonicalize Rust documentation for `canonicalize`} for more information.
   */
  canonicalize(locale: ICU4XLocale): ICU4XTransformResult;
}
