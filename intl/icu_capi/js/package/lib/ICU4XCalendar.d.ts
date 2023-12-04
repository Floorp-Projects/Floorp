import { FFIError } from "./diplomat-runtime"
import { ICU4XAnyCalendarKind } from "./ICU4XAnyCalendarKind";
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XError } from "./ICU4XError";
import { ICU4XLocale } from "./ICU4XLocale";

/**

 * See the {@link https://docs.rs/icu/latest/icu/calendar/enum.AnyCalendar.html Rust documentation for `AnyCalendar`} for more information.
 */
export class ICU4XCalendar {

  /**

   * Creates a new {@link ICU4XCalendar `ICU4XCalendar`} from the specified date and time.

   * See the {@link https://docs.rs/icu/latest/icu/calendar/enum.AnyCalendar.html#method.new_for_locale Rust documentation for `new_for_locale`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_for_locale(provider: ICU4XDataProvider, locale: ICU4XLocale): ICU4XCalendar | never;

  /**

   * Creates a new {@link ICU4XCalendar `ICU4XCalendar`} from the specified date and time.

   * See the {@link https://docs.rs/icu/latest/icu/calendar/enum.AnyCalendar.html#method.new Rust documentation for `new`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_for_kind(provider: ICU4XDataProvider, kind: ICU4XAnyCalendarKind): ICU4XCalendar | never;

  /**

   * Returns the kind of this calendar

   * See the {@link https://docs.rs/icu/latest/icu/calendar/enum.AnyCalendar.html#method.kind Rust documentation for `kind`} for more information.
   */
  kind(): ICU4XAnyCalendarKind;
}
