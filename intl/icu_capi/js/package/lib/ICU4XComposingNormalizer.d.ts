import { FFIError } from "./diplomat-runtime"
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XError } from "./ICU4XError";

/**

 * See the {@link https://docs.rs/icu/latest/icu/normalizer/struct.ComposingNormalizer.html Rust documentation for `ComposingNormalizer`} for more information.
 */
export class ICU4XComposingNormalizer {

  /**

   * Construct a new ICU4XComposingNormalizer instance for NFC

   * See the {@link https://docs.rs/icu/latest/icu/normalizer/struct.ComposingNormalizer.html#method.new_nfc Rust documentation for `new_nfc`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_nfc(provider: ICU4XDataProvider): ICU4XComposingNormalizer | never;

  /**

   * Construct a new ICU4XComposingNormalizer instance for NFKC

   * See the {@link https://docs.rs/icu/latest/icu/normalizer/struct.ComposingNormalizer.html#method.new_nfkc Rust documentation for `new_nfkc`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_nfkc(provider: ICU4XDataProvider): ICU4XComposingNormalizer | never;

  /**

   * Normalize a (potentially ill-formed) UTF8 string

   * Errors are mapped to REPLACEMENT CHARACTER

   * See the {@link https://docs.rs/icu/latest/icu/normalizer/struct.ComposingNormalizer.html#method.normalize_utf8 Rust documentation for `normalize_utf8`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  normalize(s: string): string | never;

  /**

   * Check if a (potentially ill-formed) UTF8 string is normalized

   * Errors are mapped to REPLACEMENT CHARACTER

   * See the {@link https://docs.rs/icu/latest/icu/normalizer/struct.ComposingNormalizer.html#method.is_normalized_utf8 Rust documentation for `is_normalized_utf8`} for more information.
   */
  is_normalized(s: string): boolean;
}
