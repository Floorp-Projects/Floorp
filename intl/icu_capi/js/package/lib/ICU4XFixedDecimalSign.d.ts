
/**

 * The sign of a FixedDecimal, as shown in formatting.

 * See the {@link https://docs.rs/fixed_decimal/latest/fixed_decimal/enum.Sign.html Rust documentation for `Sign`} for more information.
 */
export enum ICU4XFixedDecimalSign {
  /**

   * No sign (implicitly positive, e.g., 1729).
   */
  None = 'None',
  /**

   * A negative sign, e.g., -1729.
   */
  Negative = 'Negative',
  /**

   * An explicit positive sign, e.g., +1729.
   */
  Positive = 'Positive',
}