import { ICU4XLocale } from "./ICU4XLocale";

/**

 * An iterator over the locale under fallback.

 * See the {@link https://docs.rs/icu/latest/icu/locid_transform/fallback/struct.LocaleFallbackIterator.html Rust documentation for `LocaleFallbackIterator`} for more information.
 */
export class ICU4XLocaleFallbackIterator {

  /**

   * Gets a snapshot of the current state of the locale.

   * See the {@link https://docs.rs/icu/latest/icu/locid_transform/fallback/struct.LocaleFallbackIterator.html#method.get Rust documentation for `get`} for more information.
   */
  get(): ICU4XLocale;

  /**

   * Performs one step of the fallback algorithm, mutating the locale.

   * See the {@link https://docs.rs/icu/latest/icu/locid_transform/fallback/struct.LocaleFallbackIterator.html#method.step Rust documentation for `step`} for more information.
   */
  step(): void;
}
