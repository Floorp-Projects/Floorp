// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
pub mod ffi {
    use alloc::boxed::Box;
    use alloc::sync::Arc;
    use core::fmt::Write;
    use icu_calendar::types::IsoWeekday;
    use icu_calendar::AnyCalendar;
    use icu_calendar::{Date, Iso};
    use tinystr::TinyAsciiStr;

    use crate::calendar::ffi::ICU4XCalendar;
    use crate::errors::ffi::ICU4XError;

    #[cfg(feature = "icu_calendar")]
    use crate::week::ffi::ICU4XWeekCalculator;

    #[diplomat::enum_convert(IsoWeekday)]
    pub enum ICU4XIsoWeekday {
        Monday = 1,
        Tuesday,
        Wednesday,
        Thursday,
        Friday,
        Saturday,
        Sunday,
    }
    #[diplomat::opaque]
    #[diplomat::transparent_convert]
    /// An ICU4X Date object capable of containing a ISO-8601 date
    #[diplomat::rust_link(icu::calendar::Date, Struct)]
    pub struct ICU4XIsoDate(pub Date<Iso>);

    impl ICU4XIsoDate {
        /// Creates a new [`ICU4XIsoDate`] from the specified date and time.
        #[diplomat::rust_link(icu::calendar::Date::try_new_iso_date, FnInStruct)]
        pub fn create(year: i32, month: u8, day: u8) -> Result<Box<ICU4XIsoDate>, ICU4XError> {
            Ok(Box::new(ICU4XIsoDate(Date::try_new_iso_date(
                year, month, day,
            )?)))
        }

        /// Creates a new [`ICU4XIsoDate`] representing January 1, 1970.
        #[diplomat::rust_link(icu::calendar::Date::unix_epoch, FnInStruct)]
        pub fn create_for_unix_epoch() -> Box<ICU4XIsoDate> {
            Box::new(ICU4XIsoDate(Date::unix_epoch()))
        }

        /// Convert this date to one in a different calendar
        #[diplomat::rust_link(icu::calendar::Date::to_calendar, FnInStruct)]
        pub fn to_calendar(&self, calendar: &ICU4XCalendar) -> Box<ICU4XDate> {
            Box::new(ICU4XDate(self.0.to_calendar(calendar.0.clone())))
        }

        #[diplomat::rust_link(icu::calendar::Date::to_any, FnInStruct)]
        pub fn to_any(&self) -> Box<ICU4XDate> {
            Box::new(ICU4XDate(self.0.to_any().wrap_calendar_in_arc()))
        }

        /// Returns the 1-indexed day in the month for this date
        #[diplomat::rust_link(icu::calendar::Date::day_of_month, FnInStruct)]
        pub fn day_of_month(&self) -> u32 {
            self.0.day_of_month().0
        }

        /// Returns the day in the week for this day
        #[diplomat::rust_link(icu::calendar::Date::day_of_week, FnInStruct)]
        pub fn day_of_week(&self) -> ICU4XIsoWeekday {
            self.0.day_of_week().into()
        }

        /// Returns the week number in this month, 1-indexed, based on what
        /// is considered the first day of the week (often a locale preference).
        ///
        /// `first_weekday` can be obtained via `first_weekday()` on [`ICU4XWeekCalculator`]
        #[diplomat::rust_link(icu::calendar::Date::week_of_month, FnInStruct)]
        #[diplomat::rust_link(
            icu::calendar::week::WeekCalculator::week_of_month,
            FnInStruct,
            hidden
        )]
        pub fn week_of_month(&self, first_weekday: ICU4XIsoWeekday) -> u32 {
            self.0.week_of_month(first_weekday.into()).0
        }

        /// Returns the week number in this year, using week data
        #[diplomat::rust_link(icu::calendar::Date::week_of_year, FnInStruct)]
        #[diplomat::rust_link(
            icu::calendar::week::WeekCalculator::week_of_year,
            FnInStruct,
            hidden
        )]
        #[cfg(feature = "icu_calendar")]
        pub fn week_of_year(
            &self,
            calculator: &ICU4XWeekCalculator,
        ) -> Result<crate::week::ffi::ICU4XWeekOf, ICU4XError> {
            Ok(self.0.week_of_year(&calculator.0)?.into())
        }

        /// Returns 1-indexed number of the month of this date in its year
        #[diplomat::rust_link(icu::calendar::Date::month, FnInStruct)]
        pub fn month(&self) -> u32 {
            self.0.month().ordinal
        }

        /// Returns the year number for this date
        #[diplomat::rust_link(icu::calendar::Date::year, FnInStruct)]
        pub fn year(&self) -> i32 {
            self.0.year().number
        }

        /// Returns if the year is a leap year for this date
        #[diplomat::rust_link(icu::calendar::Date::is_in_leap_year, FnInStruct)]
        pub fn is_in_leap_year(&self) -> bool {
            self.0.is_in_leap_year()
        }

        /// Returns the number of months in the year represented by this date
        #[diplomat::rust_link(icu::calendar::Date::months_in_year, FnInStruct)]
        pub fn months_in_year(&self) -> u8 {
            self.0.months_in_year()
        }

        /// Returns the number of days in the month represented by this date
        #[diplomat::rust_link(icu::calendar::Date::days_in_month, FnInStruct)]
        pub fn days_in_month(&self) -> u8 {
            self.0.days_in_month()
        }

        /// Returns the number of days in the year represented by this date
        #[diplomat::rust_link(icu::calendar::Date::days_in_year, FnInStruct)]
        pub fn days_in_year(&self) -> u16 {
            self.0.days_in_year()
        }
    }

    #[diplomat::opaque]
    #[diplomat::transparent_convert]
    /// An ICU4X Date object capable of containing a date and time for any calendar.
    #[diplomat::rust_link(icu::calendar::Date, Struct)]
    pub struct ICU4XDate(pub Date<Arc<AnyCalendar>>);

    impl ICU4XDate {
        /// Creates a new [`ICU4XDate`] representing the ISO date and time
        /// given but in a given calendar
        #[diplomat::rust_link(icu::calendar::Date::new_from_iso, FnInStruct)]
        pub fn create_from_iso_in_calendar(
            year: i32,
            month: u8,
            day: u8,
            calendar: &ICU4XCalendar,
        ) -> Result<Box<ICU4XDate>, ICU4XError> {
            let cal = calendar.0.clone();
            Ok(Box::new(ICU4XDate(
                Date::try_new_iso_date(year, month, day)?.to_calendar(cal),
            )))
        }

        /// Creates a new [`ICU4XDate`] from the given codes, which are interpreted in the given calendar system
        #[diplomat::rust_link(icu::calendar::Date::try_new_from_codes, FnInStruct)]
        pub fn create_from_codes_in_calendar(
            era_code: &str,
            year: i32,
            month_code: &str,
            day: u8,
            calendar: &ICU4XCalendar,
        ) -> Result<Box<ICU4XDate>, ICU4XError> {
            let era_code = era_code.as_bytes(); // #2520
            let month_code = month_code.as_bytes(); // #2520
            let era = TinyAsciiStr::from_bytes(era_code)?.into();
            let month = TinyAsciiStr::from_bytes(month_code)?.into();
            let cal = calendar.0.clone();
            Ok(Box::new(ICU4XDate(Date::try_new_from_codes(
                era, year, month, day, cal,
            )?)))
        }

        /// Convert this date to one in a different calendar
        #[diplomat::rust_link(icu::calendar::Date::to_calendar, FnInStruct)]
        pub fn to_calendar(&self, calendar: &ICU4XCalendar) -> Box<ICU4XDate> {
            Box::new(ICU4XDate(self.0.to_calendar(calendar.0.clone())))
        }

        /// Converts this date to ISO
        #[diplomat::rust_link(icu::calendar::Date::to_iso, FnInStruct)]
        pub fn to_iso(&self) -> Box<ICU4XIsoDate> {
            Box::new(ICU4XIsoDate(self.0.to_iso()))
        }

        /// Returns the 1-indexed day in the month for this date
        #[diplomat::rust_link(icu::calendar::Date::day_of_month, FnInStruct)]
        pub fn day_of_month(&self) -> u32 {
            self.0.day_of_month().0
        }

        /// Returns the day in the week for this day
        #[diplomat::rust_link(icu::calendar::Date::day_of_week, FnInStruct)]
        pub fn day_of_week(&self) -> ICU4XIsoWeekday {
            self.0.day_of_week().into()
        }

        /// Returns the week number in this month, 1-indexed, based on what
        /// is considered the first day of the week (often a locale preference).
        ///
        /// `first_weekday` can be obtained via `first_weekday()` on [`ICU4XWeekCalculator`]
        #[diplomat::rust_link(icu::calendar::Date::week_of_month, FnInStruct)]
        #[diplomat::rust_link(
            icu::calendar::week::WeekCalculator::week_of_month,
            FnInStruct,
            hidden
        )]
        pub fn week_of_month(&self, first_weekday: ICU4XIsoWeekday) -> u32 {
            self.0.week_of_month(first_weekday.into()).0
        }

        /// Returns the week number in this year, using week data
        #[diplomat::rust_link(icu::calendar::Date::week_of_year, FnInStruct)]
        #[diplomat::rust_link(
            icu::calendar::week::WeekCalculator::week_of_year,
            FnInStruct,
            hidden
        )]
        #[cfg(feature = "icu_calendar")]
        pub fn week_of_year(
            &self,
            calculator: &ICU4XWeekCalculator,
        ) -> Result<crate::week::ffi::ICU4XWeekOf, ICU4XError> {
            Ok(self.0.week_of_year(&calculator.0)?.into())
        }

        /// Returns 1-indexed number of the month of this date in its year
        ///
        /// Note that for lunar calendars this may not lead to the same month
        /// having the same ordinal month across years; use month_code if you care
        /// about month identity.
        #[diplomat::rust_link(icu::calendar::Date::month, FnInStruct)]
        pub fn ordinal_month(&self) -> u32 {
            self.0.month().ordinal
        }

        /// Returns the month code for this date. Typically something
        /// like "M01", "M02", but can be more complicated for lunar calendars.
        #[diplomat::rust_link(icu::calendar::Date::month, FnInStruct)]
        pub fn month_code(
            &self,
            write: &mut diplomat_runtime::DiplomatWriteable,
        ) -> Result<(), ICU4XError> {
            let code = self.0.month().code;
            write.write_str(&code.0)?;
            Ok(())
        }

        /// Returns the year number in the current era for this date
        #[diplomat::rust_link(icu::calendar::Date::year, FnInStruct)]
        pub fn year_in_era(&self) -> i32 {
            self.0.year().number
        }

        /// Returns the era for this date,
        #[diplomat::rust_link(icu::Date::year, FnInStruct)]
        #[diplomat::rust_link(icu::types::Era, Struct, compact)]
        pub fn era(
            &self,
            write: &mut diplomat_runtime::DiplomatWriteable,
        ) -> Result<(), ICU4XError> {
            let era = self.0.year().era;
            write.write_str(&era.0)?;
            Ok(())
        }

        /// Returns the number of months in the year represented by this date
        #[diplomat::rust_link(icu::calendar::Date::months_in_year, FnInStruct)]
        pub fn months_in_year(&self) -> u8 {
            self.0.months_in_year()
        }

        /// Returns the number of days in the month represented by this date
        #[diplomat::rust_link(icu::calendar::Date::days_in_month, FnInStruct)]
        pub fn days_in_month(&self) -> u8 {
            self.0.days_in_month()
        }

        /// Returns the number of days in the year represented by this date
        #[diplomat::rust_link(icu::calendar::Date::days_in_year, FnInStruct)]
        pub fn days_in_year(&self) -> u16 {
            self.0.days_in_year()
        }

        /// Returns the [`ICU4XCalendar`] object backing this date
        #[diplomat::rust_link(icu::calendar::Date::calendar, FnInStruct)]
        #[diplomat::rust_link(icu::calendar::Date::calendar_wrapper, FnInStruct, hidden)]
        pub fn calendar(&self) -> Box<ICU4XCalendar> {
            Box::new(ICU4XCalendar(self.0.calendar_wrapper().clone()))
        }
    }
}
