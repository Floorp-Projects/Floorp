import { i32 } from "./diplomat-runtime"

/**

 * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.SentenceBreakIterator.html Rust documentation for `SentenceBreakIterator`} for more information.
 */
export class ICU4XSentenceBreakIteratorUtf8 {

  /**

   * Finds the next breakpoint. Returns -1 if at the end of the string or if the index is out of range of a 32-bit signed integer.

   * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.SentenceBreakIterator.html#method.next Rust documentation for `next`} for more information.
   */
  next(): i32;
}
