
/**

 * Priority mode for the ICU4X fallback algorithm.

 * See the {@link https://docs.rs/icu/latest/icu/locid_transform/fallback/enum.LocaleFallbackPriority.html Rust documentation for `LocaleFallbackPriority`} for more information.
 */
export enum ICU4XLocaleFallbackPriority {
  /**
   */
  Language = 'Language',
  /**
   */
  Region = 'Region',
  /**
   */
  Collation = 'Collation',
}