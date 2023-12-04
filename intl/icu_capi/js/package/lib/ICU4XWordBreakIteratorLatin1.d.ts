import { i32 } from "./diplomat-runtime"
import { ICU4XSegmenterWordType } from "./ICU4XSegmenterWordType";

/**

 * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.WordBreakIterator.html Rust documentation for `WordBreakIterator`} for more information.
 */
export class ICU4XWordBreakIteratorLatin1 {

  /**

   * Finds the next breakpoint. Returns -1 if at the end of the string or if the index is out of range of a 32-bit signed integer.

   * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.WordBreakIterator.html#method.next Rust documentation for `next`} for more information.
   */
  next(): i32;

  /**

   * Return the status value of break boundary.

   * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.WordBreakIterator.html#method.word_type Rust documentation for `word_type`} for more information.
   */
  word_type(): ICU4XSegmenterWordType;

  /**

   * Return true when break boundary is word-like such as letter/number/CJK

   * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.WordBreakIterator.html#method.is_word_like Rust documentation for `is_word_like`} for more information.
   */
  is_word_like(): boolean;
}
