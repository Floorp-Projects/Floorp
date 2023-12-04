import { u32 } from "./diplomat-runtime"
import { FFIError } from "./diplomat-runtime"
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XError } from "./ICU4XError";

/**

 * A type capable of looking up General Category mask values from a string name.

 * See the {@link https://docs.rs/icu/latest/icu/properties/struct.GeneralCategoryGroup.html#method.get_name_to_enum_mapper Rust documentation for `get_name_to_enum_mapper`} for more information.

 * See the {@link https://docs.rs/icu/latest/icu/properties/names/struct.PropertyValueNameToEnumMapper.html Rust documentation for `PropertyValueNameToEnumMapper`} for more information.
 */
export class ICU4XGeneralCategoryNameToMaskMapper {

  /**

   * Get the mask value matching the given name, using strict matching

   * Returns 0 if the name is unknown for this property
   */
  get_strict(name: string): u32;

  /**

   * Get the mask value matching the given name, using loose matching

   * Returns 0 if the name is unknown for this property
   */
  get_loose(name: string): u32;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/properties/struct.GeneralCategoryGroup.html#method.get_name_to_enum_mapper Rust documentation for `get_name_to_enum_mapper`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static load(provider: ICU4XDataProvider): ICU4XGeneralCategoryNameToMaskMapper | never;
}
