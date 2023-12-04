import { ICU4XLineBreakStrictness } from "./ICU4XLineBreakStrictness";
import { ICU4XLineBreakWordOption } from "./ICU4XLineBreakWordOption";

/**

 * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.LineBreakOptions.html Rust documentation for `LineBreakOptions`} for more information.
 */
export class ICU4XLineBreakOptionsV1 {
  strictness: ICU4XLineBreakStrictness;
  word_option: ICU4XLineBreakWordOption;
  ja_zh: boolean;
}
