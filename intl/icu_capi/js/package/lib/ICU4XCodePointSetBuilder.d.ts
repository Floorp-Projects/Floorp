import { u32, char } from "./diplomat-runtime"
import { ICU4XCodePointSetData } from "./ICU4XCodePointSetData";

/**

 * See the {@link https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html Rust documentation for `CodePointInversionListBuilder`} for more information.
 */
export class ICU4XCodePointSetBuilder {

  /**

   * Make a new set builder containing nothing

   * See the {@link https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.new Rust documentation for `new`} for more information.
   */
  static create(): ICU4XCodePointSetBuilder;

  /**

   * Build this into a set

   * This object is repopulated with an empty builder

   * See the {@link https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.build Rust documentation for `build`} for more information.
   */
  build(): ICU4XCodePointSetData;

  /**

   * Complements this set

   * (Elements in this set are removed and vice versa)

   * See the {@link https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.complement Rust documentation for `complement`} for more information.
   */
  complement(): void;

  /**

   * Returns whether this set is empty

   * See the {@link https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.is_empty Rust documentation for `is_empty`} for more information.
   */
  is_empty(): boolean;

  /**

   * Add a single character to the set

   * See the {@link https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.add_char Rust documentation for `add_char`} for more information.
   */
  add_char(ch: char): void;

  /**

   * Add a single u32 value to the set

   * See the {@link https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.add_u32 Rust documentation for `add_u32`} for more information.
   */
  add_u32(ch: u32): void;

  /**

   * Add an inclusive range of characters to the set

   * See the {@link https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.add_range Rust documentation for `add_range`} for more information.
   */
  add_inclusive_range(start: char, end: char): void;

  /**

   * Add an inclusive range of u32s to the set

   * See the {@link https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.add_range_u32 Rust documentation for `add_range_u32`} for more information.
   */
  add_inclusive_range_u32(start: u32, end: u32): void;

  /**

   * Add all elements that belong to the provided set to the set

   * See the {@link https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.add_set Rust documentation for `add_set`} for more information.
   */
  add_set(data: ICU4XCodePointSetData): void;

  /**

   * Remove a single character to the set

   * See the {@link https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.remove_char Rust documentation for `remove_char`} for more information.
   */
  remove_char(ch: char): void;

  /**

   * Remove an inclusive range of characters from the set

   * See the {@link https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.remove_range Rust documentation for `remove_range`} for more information.
   */
  remove_inclusive_range(start: char, end: char): void;

  /**

   * Remove all elements that belong to the provided set from the set

   * See the {@link https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.remove_set Rust documentation for `remove_set`} for more information.
   */
  remove_set(data: ICU4XCodePointSetData): void;

  /**

   * Removes all elements from the set except a single character

   * See the {@link https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.retain_char Rust documentation for `retain_char`} for more information.
   */
  retain_char(ch: char): void;

  /**

   * Removes all elements from the set except an inclusive range of characters f

   * See the {@link https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.retain_range Rust documentation for `retain_range`} for more information.
   */
  retain_inclusive_range(start: char, end: char): void;

  /**

   * Removes all elements from the set except all elements in the provided set

   * See the {@link https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.retain_set Rust documentation for `retain_set`} for more information.
   */
  retain_set(data: ICU4XCodePointSetData): void;

  /**

   * Complement a single character to the set

   * (Characters which are in this set are removed and vice versa)

   * See the {@link https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.complement_char Rust documentation for `complement_char`} for more information.
   */
  complement_char(ch: char): void;

  /**

   * Complement an inclusive range of characters from the set

   * (Characters which are in this set are removed and vice versa)

   * See the {@link https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.complement_range Rust documentation for `complement_range`} for more information.
   */
  complement_inclusive_range(start: char, end: char): void;

  /**

   * Complement all elements that belong to the provided set from the set

   * (Characters which are in this set are removed and vice versa)

   * See the {@link https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.complement_set Rust documentation for `complement_set`} for more information.
   */
  complement_set(data: ICU4XCodePointSetData): void;
}
