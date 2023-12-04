import { u8, usize } from "./diplomat-runtime"
import { FFIError } from "./diplomat-runtime"
import { ICU4XBidiDirection } from "./ICU4XBidiDirection";
import { ICU4XError } from "./ICU4XError";

/**

 * Bidi information for a single processed paragraph
 */
export class ICU4XBidiParagraph {

  /**

   * Given a paragraph index `n` within the surrounding text, this sets this object to the paragraph at that index. Returns `ICU4XError::OutOfBoundsError` when out of bounds.

   * This is equivalent to calling `paragraph_at()` on `ICU4XBidiInfo` but doesn't create a new object
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  set_paragraph_in_text(n: usize): void | never;

  /**

   * The primary direction of this paragraph

   * See the {@link https://docs.rs/unicode_bidi/latest/unicode_bidi/struct.Paragraph.html#method.level_at Rust documentation for `level_at`} for more information.
   */
  direction(): ICU4XBidiDirection;

  /**

   * The number of bytes in this paragraph

   * See the {@link https://docs.rs/unicode_bidi/latest/unicode_bidi/struct.ParagraphInfo.html#method.len Rust documentation for `len`} for more information.
   */
  size(): usize;

  /**

   * The start index of this paragraph within the source text
   */
  range_start(): usize;

  /**

   * The end index of this paragraph within the source text
   */
  range_end(): usize;

  /**

   * Reorder a line based on display order. The ranges are specified relative to the source text and must be contained within this paragraph's range.

   * See the {@link https://docs.rs/unicode_bidi/latest/unicode_bidi/struct.Paragraph.html#method.level_at Rust documentation for `level_at`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  reorder_line(range_start: usize, range_end: usize): string | never;

  /**

   * Get the BIDI level at a particular byte index in this paragraph. This integer is conceptually a `unicode_bidi::Level`, and can be further inspected using the static methods on ICU4XBidi.

   * Returns 0 (equivalent to `Level::ltr()`) on error

   * See the {@link https://docs.rs/unicode_bidi/latest/unicode_bidi/struct.Paragraph.html#method.level_at Rust documentation for `level_at`} for more information.
   */
  level_at(pos: usize): u8;
}
