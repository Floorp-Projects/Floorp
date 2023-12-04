import { FFIError } from "./diplomat-runtime"
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XError } from "./ICU4XError";
import { ICU4XLocale } from "./ICU4XLocale";
import { ICU4XTitlecaseOptionsV1 } from "./ICU4XTitlecaseOptionsV1";

/**

 * See the {@link https://docs.rs/icu/latest/icu/casemap/struct.TitlecaseMapper.html Rust documentation for `TitlecaseMapper`} for more information.
 */
export class ICU4XTitlecaseMapper {

  /**

   * Construct a new `ICU4XTitlecaseMapper` instance

   * See the {@link https://docs.rs/icu/latest/icu/casemap/struct.TitlecaseMapper.html#method.new Rust documentation for `new`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create(provider: ICU4XDataProvider): ICU4XTitlecaseMapper | never;

  /**

   * Returns the full titlecase mapping of the given string

   * The `v1` refers to the version of the options struct, which may change as we add more options

   * See the {@link https://docs.rs/icu/latest/icu/casemap/struct.TitlecaseMapper.html#method.titlecase_segment Rust documentation for `titlecase_segment`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  titlecase_segment_v1(s: string, locale: ICU4XLocale, options: ICU4XTitlecaseOptionsV1): string | never;
}
