import { u16, usize } from "./diplomat-runtime"
import { FFIError } from "./diplomat-runtime"

/**

 * An object that represents the Script_Extensions property for a single character

 * See the {@link https://docs.rs/icu/latest/icu/properties/script/struct.ScriptExtensionsSet.html Rust documentation for `ScriptExtensionsSet`} for more information.
 */
export class ICU4XScriptExtensionsSet {

  /**

   * Check if the Script_Extensions property of the given code point covers the given script

   * See the {@link https://docs.rs/icu/latest/icu/properties/script/struct.ScriptExtensionsSet.html#method.contains Rust documentation for `contains`} for more information.
   */
  contains(script: u16): boolean;

  /**

   * Get the number of scripts contained in here

   * See the {@link https://docs.rs/icu/latest/icu/properties/script/struct.ScriptExtensionsSet.html#method.iter Rust documentation for `iter`} for more information.
   */
  count(): usize;

  /**

   * Get script at index, returning an error if out of bounds

   * See the {@link https://docs.rs/icu/latest/icu/properties/script/struct.ScriptExtensionsSet.html#method.iter Rust documentation for `iter`} for more information.
   * @throws {@link FFIError}<void>
   */
  script_at(index: usize): u16 | never;
}
