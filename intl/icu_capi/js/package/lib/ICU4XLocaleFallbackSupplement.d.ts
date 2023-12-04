
/**

 * What additional data is required to load when performing fallback.

 * See the {@link https://docs.rs/icu/latest/icu/locid_transform/fallback/enum.LocaleFallbackSupplement.html Rust documentation for `LocaleFallbackSupplement`} for more information.
 */
export enum ICU4XLocaleFallbackSupplement {
  /**
   */
  None = 'None',
  /**
   */
  Collation = 'Collation',
}