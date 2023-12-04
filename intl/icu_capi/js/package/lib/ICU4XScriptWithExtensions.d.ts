import { u16, u32 } from "./diplomat-runtime"
import { FFIError } from "./diplomat-runtime"
import { CodePointRangeIterator } from "./CodePointRangeIterator";
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XError } from "./ICU4XError";
import { ICU4XScriptWithExtensionsBorrowed } from "./ICU4XScriptWithExtensionsBorrowed";

/**

 * An ICU4X ScriptWithExtensions map object, capable of holding a map of codepoints to scriptextensions values

 * See the {@link https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensions.html Rust documentation for `ScriptWithExtensions`} for more information.
 */
export class ICU4XScriptWithExtensions {

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/script/fn.script_with_extensions.html Rust documentation for `script_with_extensions`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create(provider: ICU4XDataProvider): ICU4XScriptWithExtensions | never;

  /**

   * Get the Script property value for a code point

   * See the {@link https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html#method.get_script_val Rust documentation for `get_script_val`} for more information.
   */
  get_script_val(code_point: u32): u16;

  /**

   * Check if the Script_Extensions property of the given code point covers the given script

   * See the {@link https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html#method.has_script Rust documentation for `has_script`} for more information.
   */
  has_script(code_point: u32, script: u16): boolean;

  /**

   * Borrow this object for a slightly faster variant with more operations

   * See the {@link https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensions.html#method.as_borrowed Rust documentation for `as_borrowed`} for more information.
   */
  as_borrowed(): ICU4XScriptWithExtensionsBorrowed;

  /**

   * Get a list of ranges of code points that contain this script in their Script_Extensions values

   * See the {@link https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html#method.get_script_extensions_ranges Rust documentation for `get_script_extensions_ranges`} for more information.
   */
  iter_ranges_for_script(script: u16): CodePointRangeIterator;
}
