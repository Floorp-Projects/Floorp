import { u8, u16, i32, u32 } from "./diplomat-runtime"
import { FFIError } from "./diplomat-runtime"
import { ICU4XCalendar } from "./ICU4XCalendar";
import { ICU4XDate } from "./ICU4XDate";
import { ICU4XError } from "./ICU4XError";
import { ICU4XIsoDateTime } from "./ICU4XIsoDateTime";
import { ICU4XIsoWeekday } from "./ICU4XIsoWeekday";
import { ICU4XTime } from "./ICU4XTime";
import { ICU4XWeekCalculator } from "./ICU4XWeekCalculator";
import { ICU4XWeekOf } from "./ICU4XWeekOf";

/**

 * An ICU4X DateTime object capable of containing a date and time for any calendar.

 * See the {@link https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html Rust documentation for `DateTime`} for more information.
 */
export class ICU4XDateTime {

  /**

   * Creates a new {@link ICU4XDateTime `ICU4XDateTime`} representing the ISO date and time given but in a given calendar

   * See the {@link https://docs.rs/icu/latest/icu/struct.DateTime.html#method.new_from_iso Rust documentation for `new_from_iso`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_from_iso_in_calendar(year: i32, month: u8, day: u8, hour: u8, minute: u8, second: u8, nanosecond: u32, calendar: ICU4XCalendar): ICU4XDateTime | never;

  /**

   * Creates a new {@link ICU4XDateTime `ICU4XDateTime`} from the given codes, which are interpreted in the given calendar system

   * See the {@link https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html#method.try_new_from_codes Rust documentation for `try_new_from_codes`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_from_codes_in_calendar(era_code: string, year: i32, month_code: string, day: u8, hour: u8, minute: u8, second: u8, nanosecond: u32, calendar: ICU4XCalendar): ICU4XDateTime | never;

  /**

   * Creates a new {@link ICU4XDateTime `ICU4XDateTime`} from an {@link ICU4XDate `ICU4XDate`} and {@link ICU4XTime `ICU4XTime`} object

   * See the {@link https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html#method.new Rust documentation for `new`} for more information.
   */
  static create_from_date_and_time(date: ICU4XDate, time: ICU4XTime): ICU4XDateTime;

  /**

   * Gets a copy of the date contained in this object

   * See the {@link https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html#structfield.date Rust documentation for `date`} for more information.
   */
  date(): ICU4XDate;

  /**

   * Gets the time contained in this object

   * See the {@link https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html#structfield.time Rust documentation for `time`} for more information.
   */
  time(): ICU4XTime;

  /**

   * Converts this date to ISO

   * See the {@link https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html#method.to_iso Rust documentation for `to_iso`} for more information.
   */
  to_iso(): ICU4XIsoDateTime;

  /**

   * Convert this datetime to one in a different calendar

   * See the {@link https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html#method.to_calendar Rust documentation for `to_calendar`} for more information.
   */
  to_calendar(calendar: ICU4XCalendar): ICU4XDateTime;

  /**

   * Returns the hour in this time

   * See the {@link https://docs.rs/icu/latest/icu/calendar/types/struct.Time.html#structfield.hour Rust documentation for `hour`} for more information.
   */
  hour(): u8;

  /**

   * Returns the minute in this time

   * See the {@link https://docs.rs/icu/latest/icu/calendar/types/struct.Time.html#structfield.minute Rust documentation for `minute`} for more information.
   */
  minute(): u8;

  /**

   * Returns the second in this time

   * See the {@link https://docs.rs/icu/latest/icu/calendar/types/struct.Time.html#structfield.second Rust documentation for `second`} for more information.
   */
  second(): u8;

  /**

   * Returns the nanosecond in this time

   * See the {@link https://docs.rs/icu/latest/icu/calendar/types/struct.Time.html#structfield.nanosecond Rust documentation for `nanosecond`} for more information.
   */
  nanosecond(): u32;

  /**

   * Returns the 1-indexed day in the month for this date

   * See the {@link https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.day_of_month Rust documentation for `day_of_month`} for more information.
   */
  day_of_month(): u32;

  /**

   * Returns the day in the week for this day

   * See the {@link https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.day_of_week Rust documentation for `day_of_week`} for more information.
   */
  day_of_week(): ICU4XIsoWeekday;

  /**

   * Returns the week number in this month, 1-indexed, based on what is considered the first day of the week (often a locale preference).

   * `first_weekday` can be obtained via `first_weekday()` on {@link ICU4XWeekCalculator `ICU4XWeekCalculator`}

   * See the {@link https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.week_of_month Rust documentation for `week_of_month`} for more information.
   */
  week_of_month(first_weekday: ICU4XIsoWeekday): u32;

  /**

   * Returns the week number in this year, using week data

   * See the {@link https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.week_of_year Rust documentation for `week_of_year`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  week_of_year(calculator: ICU4XWeekCalculator): ICU4XWeekOf | never;

  /**

   * Returns 1-indexed number of the month of this date in its year

   * Note that for lunar calendars this may not lead to the same month having the same ordinal month across years; use month_code if you care about month identity.

   * See the {@link https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.month Rust documentation for `month`} for more information.
   */
  ordinal_month(): u32;

  /**

   * Returns the month code for this date. Typically something like "M01", "M02", but can be more complicated for lunar calendars.

   * See the {@link https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.month Rust documentation for `month`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  month_code(): string | never;

  /**

   * Returns the year number in the current era for this date

   * See the {@link https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.year Rust documentation for `year`} for more information.
   */
  year_in_era(): i32;

  /**

   * Returns the era for this date,

   * See the {@link https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.year Rust documentation for `year`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  era(): string | never;

  /**

   * Returns the number of months in the year represented by this date

   * See the {@link https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.months_in_year Rust documentation for `months_in_year`} for more information.
   */
  months_in_year(): u8;

  /**

   * Returns the number of days in the month represented by this date

   * See the {@link https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.days_in_month Rust documentation for `days_in_month`} for more information.
   */
  days_in_month(): u8;

  /**

   * Returns the number of days in the year represented by this date

   * See the {@link https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.days_in_year Rust documentation for `days_in_year`} for more information.
   */
  days_in_year(): u16;

  /**

   * Returns the {@link ICU4XCalendar `ICU4XCalendar`} object backing this date

   * See the {@link https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.calendar Rust documentation for `calendar`} for more information.
   */
  calendar(): ICU4XCalendar;
}
