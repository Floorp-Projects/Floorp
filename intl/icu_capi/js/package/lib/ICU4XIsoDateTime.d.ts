import { u8, u16, i32, u32 } from "./diplomat-runtime"
import { FFIError } from "./diplomat-runtime"
import { ICU4XCalendar } from "./ICU4XCalendar";
import { ICU4XDateTime } from "./ICU4XDateTime";
import { ICU4XError } from "./ICU4XError";
import { ICU4XIsoDate } from "./ICU4XIsoDate";
import { ICU4XIsoWeekday } from "./ICU4XIsoWeekday";
import { ICU4XTime } from "./ICU4XTime";
import { ICU4XWeekCalculator } from "./ICU4XWeekCalculator";
import { ICU4XWeekOf } from "./ICU4XWeekOf";

/**

 * An ICU4X DateTime object capable of containing a ISO-8601 date and time.

 * See the {@link https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html Rust documentation for `DateTime`} for more information.
 */
export class ICU4XIsoDateTime {

  /**

   * Creates a new {@link ICU4XIsoDateTime `ICU4XIsoDateTime`} from the specified date and time.

   * See the {@link https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html#method.try_new_iso_datetime Rust documentation for `try_new_iso_datetime`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create(year: i32, month: u8, day: u8, hour: u8, minute: u8, second: u8, nanosecond: u32): ICU4XIsoDateTime | never;

  /**

   * Creates a new {@link ICU4XIsoDateTime `ICU4XIsoDateTime`} from an {@link ICU4XIsoDate `ICU4XIsoDate`} and {@link ICU4XTime `ICU4XTime`} object

   * See the {@link https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html#method.new Rust documentation for `new`} for more information.
   */
  static crate_from_date_and_time(date: ICU4XIsoDate, time: ICU4XTime): ICU4XIsoDateTime;

  /**

   * Construct from the minutes since the local unix epoch for this date (Jan 1 1970, 00:00)

   * See the {@link https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html#method.from_minutes_since_local_unix_epoch Rust documentation for `from_minutes_since_local_unix_epoch`} for more information.
   */
  static create_from_minutes_since_local_unix_epoch(minutes: i32): ICU4XIsoDateTime;

  /**

   * Gets the date contained in this object

   * See the {@link https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html#structfield.date Rust documentation for `date`} for more information.
   */
  date(): ICU4XIsoDate;

  /**

   * Gets the time contained in this object

   * See the {@link https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html#structfield.time Rust documentation for `time`} for more information.
   */
  time(): ICU4XTime;

  /**

   * Converts this to an {@link ICU4XDateTime `ICU4XDateTime`} capable of being mixed with dates of other calendars

   * See the {@link https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html#method.to_any Rust documentation for `to_any`} for more information.
   */
  to_any(): ICU4XDateTime;

  /**

   * Gets the minutes since the local unix epoch for this date (Jan 1 1970, 00:00)

   * See the {@link https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html#method.minutes_since_local_unix_epoch Rust documentation for `minutes_since_local_unix_epoch`} for more information.
   */
  minutes_since_local_unix_epoch(): i32;

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

   * See the {@link https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.month Rust documentation for `month`} for more information.
   */
  month(): u32;

  /**

   * Returns the year number for this date

   * See the {@link https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.year Rust documentation for `year`} for more information.
   */
  year(): i32;

  /**

   * Returns whether this date is in a leap year

   * See the {@link https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.is_in_leap_year Rust documentation for `is_in_leap_year`} for more information.
   */
  is_in_leap_year(): boolean;

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
}
