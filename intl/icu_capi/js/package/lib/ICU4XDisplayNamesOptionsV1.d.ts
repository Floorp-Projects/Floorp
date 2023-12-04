import { ICU4XDisplayNamesFallback } from "./ICU4XDisplayNamesFallback";
import { ICU4XDisplayNamesStyle } from "./ICU4XDisplayNamesStyle";
import { ICU4XLanguageDisplay } from "./ICU4XLanguageDisplay";

/**

 * See the {@link https://docs.rs/icu/latest/icu/displaynames/options/struct.DisplayNamesOptions.html Rust documentation for `DisplayNamesOptions`} for more information.
 */
export class ICU4XDisplayNamesOptionsV1 {
  style: ICU4XDisplayNamesStyle;
  fallback: ICU4XDisplayNamesFallback;
  language_display: ICU4XLanguageDisplay;
}
