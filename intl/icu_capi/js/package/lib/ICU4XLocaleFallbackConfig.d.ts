import { ICU4XLocaleFallbackPriority } from "./ICU4XLocaleFallbackPriority";
import { ICU4XLocaleFallbackSupplement } from "./ICU4XLocaleFallbackSupplement";

/**

 * Collection of configurations for the ICU4X fallback algorithm.

 * See the {@link https://docs.rs/icu/latest/icu/locid_transform/fallback/struct.LocaleFallbackConfig.html Rust documentation for `LocaleFallbackConfig`} for more information.
 */
export class ICU4XLocaleFallbackConfig {
  priority: ICU4XLocaleFallbackPriority;
  extension_key: string;
  fallback_supplement: ICU4XLocaleFallbackSupplement;
}
