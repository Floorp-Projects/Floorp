import { FFIError } from "./diplomat-runtime"
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XError } from "./ICU4XError";

/**

 * An object capable of mapping from a BCP-47 time zone ID to an IANA ID.

 * See the {@link https://docs.rs/icu/latest/icu/timezone/struct.IanaBcp47RoundTripMapper.html Rust documentation for `IanaBcp47RoundTripMapper`} for more information.
 */
export class ICU4XBcp47ToIanaMapper {

  /**

   * See the {@link https://docs.rs/icu/latest/icu/timezone/struct.IanaBcp47RoundTripMapper.html#method.new Rust documentation for `new`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create(provider: ICU4XDataProvider): ICU4XBcp47ToIanaMapper | never;

  /**

   * Writes out the canonical IANA time zone ID corresponding to the given BCP-47 ID.

   * See the {@link https://docs.rs/icu/latest/icu/datetime/time_zone/struct.IanaBcp47RoundTripMapper.html#method.bcp47_to_iana Rust documentation for `bcp47_to_iana`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  get(value: string): string | never;
}
