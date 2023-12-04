import { char } from "./diplomat-runtime"
import { FFIError } from "./diplomat-runtime"
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XError } from "./ICU4XError";

/**

 * The raw canonical composition operation.

 * Callers should generally use ICU4XComposingNormalizer unless they specifically need raw composition operations

 * See the {@link https://docs.rs/icu/latest/icu/normalizer/properties/struct.CanonicalComposition.html Rust documentation for `CanonicalComposition`} for more information.
 */
export class ICU4XCanonicalComposition {

  /**

   * Construct a new ICU4XCanonicalComposition instance for NFC

   * See the {@link https://docs.rs/icu/latest/icu/normalizer/properties/struct.CanonicalComposition.html#method.new Rust documentation for `new`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create(provider: ICU4XDataProvider): ICU4XCanonicalComposition | never;

  /**

   * Performs canonical composition (including Hangul) on a pair of characters or returns NUL if these characters don’t compose. Composition exclusions are taken into account.

   * See the {@link https://docs.rs/icu/latest/icu/normalizer/properties/struct.CanonicalComposition.html#method.compose Rust documentation for `compose`} for more information.
   */
  compose(starter: char, second: char): char;
}
