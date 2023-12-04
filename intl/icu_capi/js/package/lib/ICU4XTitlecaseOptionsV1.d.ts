import { ICU4XLeadingAdjustment } from "./ICU4XLeadingAdjustment";
import { ICU4XTrailingCase } from "./ICU4XTrailingCase";

/**

 * See the {@link https://docs.rs/icu/latest/icu/casemap/titlecase/struct.TitlecaseOptions.html Rust documentation for `TitlecaseOptions`} for more information.
 */
export class ICU4XTitlecaseOptionsV1 {
  leading_adjustment: ICU4XLeadingAdjustment;
  trailing_case: ICU4XTrailingCase;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/casemap/titlecase/struct.TitlecaseOptions.html#method.default Rust documentation for `default`} for more information.
   */
  static default_options(): ICU4XTitlecaseOptionsV1;
}
