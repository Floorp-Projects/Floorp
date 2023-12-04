import { FFIError } from "./diplomat-runtime"
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XError } from "./ICU4XError";
import { ICU4XGraphemeClusterBreakIteratorLatin1 } from "./ICU4XGraphemeClusterBreakIteratorLatin1";
import { ICU4XGraphemeClusterBreakIteratorUtf16 } from "./ICU4XGraphemeClusterBreakIteratorUtf16";
import { ICU4XGraphemeClusterBreakIteratorUtf8 } from "./ICU4XGraphemeClusterBreakIteratorUtf8";

/**

 * An ICU4X grapheme-cluster-break segmenter, capable of finding grapheme cluster breakpoints in strings.

 * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterSegmenter.html Rust documentation for `GraphemeClusterSegmenter`} for more information.
 */
export class ICU4XGraphemeClusterSegmenter {

  /**

   * Construct an {@link ICU4XGraphemeClusterSegmenter `ICU4XGraphemeClusterSegmenter`}.

   * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterSegmenter.html#method.new Rust documentation for `new`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create(provider: ICU4XDataProvider): ICU4XGraphemeClusterSegmenter | never;

  /**

   * Segments a (potentially ill-formed) UTF-8 string.

   * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterSegmenter.html#method.segment_utf8 Rust documentation for `segment_utf8`} for more information.
   */
  segment_utf8(input: string): ICU4XGraphemeClusterBreakIteratorUtf8;

  /**

   * Segments a UTF-16 string.

   * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterSegmenter.html#method.segment_utf16 Rust documentation for `segment_utf16`} for more information.
   */
  segment_utf16(input: Uint16Array): ICU4XGraphemeClusterBreakIteratorUtf16;

  /**

   * Segments a Latin-1 string.

   * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterSegmenter.html#method.segment_latin1 Rust documentation for `segment_latin1`} for more information.
   */
  segment_latin1(input: Uint8Array): ICU4XGraphemeClusterBreakIteratorLatin1;
}
