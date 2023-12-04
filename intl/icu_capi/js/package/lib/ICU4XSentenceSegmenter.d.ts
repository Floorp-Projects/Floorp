import { FFIError } from "./diplomat-runtime"
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XError } from "./ICU4XError";
import { ICU4XSentenceBreakIteratorLatin1 } from "./ICU4XSentenceBreakIteratorLatin1";
import { ICU4XSentenceBreakIteratorUtf16 } from "./ICU4XSentenceBreakIteratorUtf16";
import { ICU4XSentenceBreakIteratorUtf8 } from "./ICU4XSentenceBreakIteratorUtf8";

/**

 * An ICU4X sentence-break segmenter, capable of finding sentence breakpoints in strings.

 * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.SentenceSegmenter.html Rust documentation for `SentenceSegmenter`} for more information.
 */
export class ICU4XSentenceSegmenter {

  /**

   * Construct an {@link ICU4XSentenceSegmenter `ICU4XSentenceSegmenter`}.

   * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.SentenceSegmenter.html#method.new Rust documentation for `new`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create(provider: ICU4XDataProvider): ICU4XSentenceSegmenter | never;

  /**

   * Segments a (potentially ill-formed) UTF-8 string.

   * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.SentenceSegmenter.html#method.segment_utf8 Rust documentation for `segment_utf8`} for more information.
   */
  segment_utf8(input: string): ICU4XSentenceBreakIteratorUtf8;

  /**

   * Segments a UTF-16 string.

   * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.SentenceSegmenter.html#method.segment_utf16 Rust documentation for `segment_utf16`} for more information.
   */
  segment_utf16(input: Uint16Array): ICU4XSentenceBreakIteratorUtf16;

  /**

   * Segments a Latin-1 string.

   * See the {@link https://docs.rs/icu/latest/icu/segmenter/struct.SentenceSegmenter.html#method.segment_latin1 Rust documentation for `segment_latin1`} for more information.
   */
  segment_latin1(input: Uint8Array): ICU4XSentenceBreakIteratorLatin1;
}
