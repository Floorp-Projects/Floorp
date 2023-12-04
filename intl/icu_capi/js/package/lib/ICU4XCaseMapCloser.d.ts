import { char } from "./diplomat-runtime"
import { FFIError } from "./diplomat-runtime"
import { ICU4XCodePointSetBuilder } from "./ICU4XCodePointSetBuilder";
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XError } from "./ICU4XError";

/**

 * See the {@link https://docs.rs/icu/latest/icu/casemap/struct.CaseMapCloser.html Rust documentation for `CaseMapCloser`} for more information.
 */
export class ICU4XCaseMapCloser {

  /**

   * Construct a new ICU4XCaseMapper instance

   * See the {@link https://docs.rs/icu/latest/icu/casemap/struct.CaseMapCloser.html#method.new Rust documentation for `new`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create(provider: ICU4XDataProvider): ICU4XCaseMapCloser | never;

  /**

   * Adds all simple case mappings and the full case folding for `c` to `builder`. Also adds special case closure mappings.

   * See the {@link https://docs.rs/icu/latest/icu/casemap/struct.CaseMapCloser.html#method.add_case_closure_to Rust documentation for `add_case_closure_to`} for more information.
   */
  add_case_closure_to(c: char, builder: ICU4XCodePointSetBuilder): void;

  /**

   * Finds all characters and strings which may casemap to `s` as their full case folding string and adds them to the set.

   * Returns true if the string was found

   * See the {@link https://docs.rs/icu/latest/icu/casemap/struct.CaseMapCloser.html#method.add_string_case_closure_to Rust documentation for `add_string_case_closure_to`} for more information.
   */
  add_string_case_closure_to(s: string, builder: ICU4XCodePointSetBuilder): boolean;
}
