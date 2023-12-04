import { u16, u32 } from "./diplomat-runtime"
import { ICU4XCodePointSetData } from "./ICU4XCodePointSetData";
import { ICU4XScriptExtensionsSet } from "./ICU4XScriptExtensionsSet";

/**

 * A slightly faster ICU4XScriptWithExtensions object

 * See the {@link https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html Rust documentation for `ScriptWithExtensionsBorrowed`} for more information.
 */
export class ICU4XScriptWithExtensionsBorrowed {

  /**

   * Get the Script property value for a code point

   * See the {@link https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html#method.get_script_val Rust documentation for `get_script_val`} for more information.
   */
  get_script_val(code_point: u32): u16;

  /**

   * Get the Script property value for a code point

   * See the {@link https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html#method.get_script_extensions_val Rust documentation for `get_script_extensions_val`} for more information.
   */
  get_script_extensions_val(code_point: u32): ICU4XScriptExtensionsSet;

  /**

   * Check if the Script_Extensions property of the given code point covers the given script

   * See the {@link https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html#method.has_script Rust documentation for `has_script`} for more information.
   */
  has_script(code_point: u32, script: u16): boolean;

  /**

   * Build the CodePointSetData corresponding to a codepoints matching a particular script in their Script_Extensions

   * See the {@link https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html#method.get_script_extensions_set Rust documentation for `get_script_extensions_set`} for more information.
   */
  get_script_extensions_set(script: u16): ICU4XCodePointSetData;
}
