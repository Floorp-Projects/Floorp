
/**

 * Increment used in a rounding operation.

 * See the {@link https://docs.rs/fixed_decimal/latest/fixed_decimal/enum.RoundingIncrement.html Rust documentation for `RoundingIncrement`} for more information.
 */
export enum ICU4XRoundingIncrement {
  /**
   */
  MultiplesOf1 = 'MultiplesOf1',
  /**
   */
  MultiplesOf2 = 'MultiplesOf2',
  /**
   */
  MultiplesOf5 = 'MultiplesOf5',
  /**
   */
  MultiplesOf25 = 'MultiplesOf25',
}