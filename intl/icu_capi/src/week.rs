// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use icu_calendar::week::WeekOf;

#[diplomat::bridge]
pub mod ffi {
    use crate::date::ffi::ICU4XIsoWeekday;
    use crate::errors::ffi::ICU4XError;
    use crate::locale::ffi::ICU4XLocale;
    use crate::provider::ffi::ICU4XDataProvider;
    use alloc::boxed::Box;
    use icu_calendar::week::{RelativeUnit, WeekCalculator};

    #[diplomat::rust_link(icu::calendar::week::RelativeUnit, Enum)]
    #[diplomat::enum_convert(RelativeUnit)]
    pub enum ICU4XWeekRelativeUnit {
        Previous,
        Current,
        Next,
    }

    #[diplomat::rust_link(icu::calendar::week::WeekOf, Struct)]
    pub struct ICU4XWeekOf {
        pub week: u16,
        pub unit: ICU4XWeekRelativeUnit,
    }
    /// A Week calculator, useful to be passed in to `week_of_year()` on Date and DateTime types
    #[diplomat::opaque]
    #[diplomat::rust_link(icu::calendar::week::WeekCalculator, Struct)]
    pub struct ICU4XWeekCalculator(pub WeekCalculator);

    impl ICU4XWeekCalculator {
        /// Creates a new [`ICU4XWeekCalculator`] from locale data.
        #[diplomat::rust_link(icu::calendar::week::WeekCalculator::try_new, FnInStruct)]
        pub fn create(
            provider: &ICU4XDataProvider,
            locale: &ICU4XLocale,
        ) -> Result<Box<ICU4XWeekCalculator>, ICU4XError> {
            let locale = locale.to_datalocale();

            Ok(Box::new(ICU4XWeekCalculator(call_constructor!(
                WeekCalculator::try_new,
                WeekCalculator::try_new_with_any_provider,
                WeekCalculator::try_new_with_buffer_provider,
                provider,
                &locale,
            )?)))
        }

        #[diplomat::rust_link(
            icu::calendar::week::WeekCalculator::first_weekday,
            StructField,
            compact
        )]
        #[diplomat::rust_link(
            icu::calendar::week::WeekCalculator::min_week_days,
            StructField,
            compact
        )]
        pub fn create_from_first_day_of_week_and_min_week_days(
            first_weekday: ICU4XIsoWeekday,
            min_week_days: u8,
        ) -> Box<ICU4XWeekCalculator> {
            let mut calculator = WeekCalculator::default();
            calculator.first_weekday = first_weekday.into();
            calculator.min_week_days = min_week_days;
            Box::new(ICU4XWeekCalculator(calculator))
        }

        /// Returns the weekday that starts the week for this object's locale
        #[diplomat::rust_link(icu::calendar::week::WeekCalculator::first_weekday, StructField)]
        pub fn first_weekday(&self) -> ICU4XIsoWeekday {
            self.0.first_weekday.into()
        }
        /// The minimum number of days overlapping a year required for a week to be
        /// considered part of that year
        #[diplomat::rust_link(icu::calendar::week::WeekCalculator::min_week_days, StructField)]
        pub fn min_week_days(&self) -> u8 {
            self.0.min_week_days
        }
    }
}

impl From<WeekOf> for ffi::ICU4XWeekOf {
    fn from(other: WeekOf) -> Self {
        ffi::ICU4XWeekOf {
            week: other.week,
            unit: other.unit.into(),
        }
    }
}
