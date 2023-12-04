import { FFIError } from "./diplomat-runtime"
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XError } from "./ICU4XError";

/**

 * An object capable of mapping from an IANA time zone ID to a BCP-47 ID.

 * This can be used via `try_set_iana_time_zone_id()` on {@link crate::timezone::ffi::ICU4XCustomTimeZone `ICU4XCustomTimeZone`}.

 * See the {@link https://docs.rs/icu/latest/icu/timezone/struct.IanaToBcp47Mapper.html Rust documentation for `IanaToBcp47Mapper`} for more information.
 */
export class ICU4XIanaToBcp47Mapper {

  /**

   * See the {@link https://docs.rs/icu/latest/icu/timezone/struct.IanaToBcp47Mapper.html#method.new Rust documentation for `new`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create(provider: ICU4XDataProvider): ICU4XIanaToBcp47Mapper | never;
}
