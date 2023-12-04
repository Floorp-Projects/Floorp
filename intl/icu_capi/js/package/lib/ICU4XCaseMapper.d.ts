import { char } from "./diplomat-runtime"
import { FFIError } from "./diplomat-runtime"
import { ICU4XCodePointSetBuilder } from "./ICU4XCodePointSetBuilder";
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XError } from "./ICU4XError";
import { ICU4XLocale } from "./ICU4XLocale";
import { ICU4XTitlecaseOptionsV1 } from "./ICU4XTitlecaseOptionsV1";

/**

 * See the {@link https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html Rust documentation for `CaseMapper`} for more information.
 */
export class ICU4XCaseMapper {

  /**

   * Construct a new ICU4XCaseMapper instance

   * See the {@link https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.new Rust documentation for `new`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create(provider: ICU4XDataProvider): ICU4XCaseMapper | never;

  /**

   * Returns the full lowercase mapping of the given string

   * See the {@link https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.lowercase Rust documentation for `lowercase`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  lowercase(s: string, locale: ICU4XLocale): string | never;

  /**

   * Returns the full uppercase mapping of the given string

   * See the {@link https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.uppercase Rust documentation for `uppercase`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  uppercase(s: string, locale: ICU4XLocale): string | never;

  /**

   * Returns the full titlecase mapping of the given string, performing head adjustment without loading additional data. (if head adjustment is enabled in the options)

   * The `v1` refers to the version of the options struct, which may change as we add more options

   * See the {@link https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.titlecase_segment_with_only_case_data Rust documentation for `titlecase_segment_with_only_case_data`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  titlecase_segment_with_only_case_data_v1(s: string, locale: ICU4XLocale, options: ICU4XTitlecaseOptionsV1): string | never;

  /**

   * Case-folds the characters in the given string

   * See the {@link https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.fold Rust documentation for `fold`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  fold(s: string): string | never;

  /**

   * Case-folds the characters in the given string using Turkic (T) mappings for dotted/dotless I.

   * See the {@link https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.fold_turkic Rust documentation for `fold_turkic`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  fold_turkic(s: string): string | never;

  /**

   * Adds all simple case mappings and the full case folding for `c` to `builder`. Also adds special case closure mappings.

   * In other words, this adds all characters that this casemaps to, as well as all characters that may casemap to this one.

   * Note that since ICU4XCodePointSetBuilder does not contain strings, this will ignore string mappings.

   * Identical to the similarly named method on `ICU4XCaseMapCloser`, use that if you plan on using string case closure mappings too.

   * See the {@link https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.add_case_closure_to Rust documentation for `add_case_closure_to`} for more information.
   */
  add_case_closure_to(c: char, builder: ICU4XCodePointSetBuilder): void;

  /**

   * Returns the simple lowercase mapping of the given character.

   * This function only implements simple and common mappings. Full mappings, which can map one char to a string, are not included. For full mappings, use `ICU4XCaseMapper::lowercase`.

   * See the {@link https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.simple_lowercase Rust documentation for `simple_lowercase`} for more information.
   */
  simple_lowercase(ch: char): char;

  /**

   * Returns the simple uppercase mapping of the given character.

   * This function only implements simple and common mappings. Full mappings, which can map one char to a string, are not included. For full mappings, use `ICU4XCaseMapper::uppercase`.

   * See the {@link https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.simple_uppercase Rust documentation for `simple_uppercase`} for more information.
   */
  simple_uppercase(ch: char): char;

  /**

   * Returns the simple titlecase mapping of the given character.

   * This function only implements simple and common mappings. Full mappings, which can map one char to a string, are not included. For full mappings, use `ICU4XCaseMapper::titlecase_segment`.

   * See the {@link https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.simple_titlecase Rust documentation for `simple_titlecase`} for more information.
   */
  simple_titlecase(ch: char): char;

  /**

   * Returns the simple casefolding of the given character.

   * This function only implements simple folding. For full folding, use `ICU4XCaseMapper::fold`.

   * See the {@link https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.simple_fold Rust documentation for `simple_fold`} for more information.
   */
  simple_fold(ch: char): char;

  /**

   * Returns the simple casefolding of the given character in the Turkic locale

   * This function only implements simple folding. For full folding, use `ICU4XCaseMapper::fold_turkic`.

   * See the {@link https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.simple_fold_turkic Rust documentation for `simple_fold_turkic`} for more information.
   */
  simple_fold_turkic(ch: char): char;
}
