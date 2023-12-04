import { FFIError } from "./diplomat-runtime"
import { ICU4XError } from "./ICU4XError";
import { ICU4XLocale } from "./ICU4XLocale";

/**

 * The various calendar types currently supported by {@link ICU4XCalendar `ICU4XCalendar`}

 * See the {@link https://docs.rs/icu/latest/icu/calendar/enum.AnyCalendarKind.html Rust documentation for `AnyCalendarKind`} for more information.
 */
export enum ICU4XAnyCalendarKind {
  /**

   * The kind of an Iso calendar
   */
  Iso = 'Iso',
  /**

   * The kind of a Gregorian calendar
   */
  Gregorian = 'Gregorian',
  /**

   * The kind of a Buddhist calendar
   */
  Buddhist = 'Buddhist',
  /**

   * The kind of a Japanese calendar with modern eras
   */
  Japanese = 'Japanese',
  /**

   * The kind of a Japanese calendar with modern and historic eras
   */
  JapaneseExtended = 'JapaneseExtended',
  /**

   * The kind of an Ethiopian calendar, with Amete Mihret era
   */
  Ethiopian = 'Ethiopian',
  /**

   * The kind of an Ethiopian calendar, with Amete Alem era
   */
  EthiopianAmeteAlem = 'EthiopianAmeteAlem',
  /**

   * The kind of a Indian calendar
   */
  Indian = 'Indian',
  /**

   * The kind of a Coptic calendar
   */
  Coptic = 'Coptic',
  /**

   * The kind of a Dangi calendar
   */
  Dangi = 'Dangi',
  /**

   * The kind of a Chinese calendar
   */
  Chinese = 'Chinese',
  /**

   * The kind of a Hebrew calendar
   */
  Hebrew = 'Hebrew',
  /**

   * The kind of a Islamic civil calendar
   */
  IslamicCivil = 'IslamicCivil',
  /**

   * The kind of a Islamic observational calendar
   */
  IslamicObservational = 'IslamicObservational',
  /**

   * The kind of a Islamic tabular calendar
   */
  IslamicTabular = 'IslamicTabular',
  /**

   * The kind of a Islamic Umm al-Qura calendar
   */
  IslamicUmmAlQura = 'IslamicUmmAlQura',
  /**

   * The kind of a Persian calendar
   */
  Persian = 'Persian',
  /**

   * The kind of a Roc calendar
   */
  Roc = 'Roc',
}