// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
pub mod ffi {
    use alloc::boxed::Box;
    use alloc::sync::Arc;

    use core::fmt::Write;
    use icu_calendar::{AnyCalendar, AnyCalendarKind};

    use crate::errors::ffi::ICU4XError;
    use crate::locale::ffi::ICU4XLocale;
    use crate::provider::ffi::ICU4XDataProvider;

    /// The various calendar types currently supported by [`ICU4XCalendar`]
    #[diplomat::enum_convert(AnyCalendarKind, needs_wildcard)]
    #[diplomat::rust_link(icu::calendar::AnyCalendarKind, Enum)]
    pub enum ICU4XAnyCalendarKind {
        /// The kind of an Iso calendar
        Iso = 0,
        /// The kind of a Gregorian calendar
        Gregorian = 1,
        /// The kind of a Buddhist calendar
        Buddhist = 2,
        /// The kind of a Japanese calendar with modern eras
        Japanese = 3,
        /// The kind of a Japanese calendar with modern and historic eras
        JapaneseExtended = 4,
        /// The kind of an Ethiopian calendar, with Amete Mihret era
        Ethiopian = 5,
        /// The kind of an Ethiopian calendar, with Amete Alem era
        EthiopianAmeteAlem = 6,
        /// The kind of a Indian calendar
        Indian = 7,
        /// The kind of a Coptic calendar
        Coptic = 8,
        /// The kind of a Dangi calendar
        Dangi = 9,
        /// The kind of a Chinese calendar
        Chinese = 10,
        /// The kind of a Hebrew calendar
        Hebrew = 11,
        /// The kind of a Islamic civil calendar
        IslamicCivil = 12,
        /// The kind of a Islamic observational calendar
        IslamicObservational = 13,
        /// The kind of a Islamic tabular calendar
        IslamicTabular = 14,
        /// The kind of a Islamic Umm al-Qura calendar
        IslamicUmmAlQura = 15,
        /// The kind of a Persian calendar
        Persian = 16,
        /// The kind of a Roc calendar
        Roc = 17,
    }

    impl ICU4XAnyCalendarKind {
        /// Read the calendar type off of the -u-ca- extension on a locale.
        ///
        /// Errors if there is no calendar on the locale or if the locale's calendar
        /// is not known or supported.
        #[diplomat::rust_link(icu::calendar::AnyCalendarKind::get_for_locale, FnInEnum)]
        pub fn get_for_locale(locale: &ICU4XLocale) -> Result<ICU4XAnyCalendarKind, ()> {
            AnyCalendarKind::get_for_locale(&locale.0)
                .map(Into::into)
                .ok_or(())
        }

        /// Obtain the calendar type given a BCP-47 -u-ca- extension string.
        ///
        /// Errors if the calendar is not known or supported.
        #[diplomat::rust_link(icu::calendar::AnyCalendarKind::get_for_bcp47_value, FnInEnum)]
        #[diplomat::rust_link(
            icu::calendar::AnyCalendarKind::get_for_bcp47_string,
            FnInEnum,
            hidden
        )]
        #[diplomat::rust_link(
            icu::calendar::AnyCalendarKind::get_for_bcp47_bytes,
            FnInEnum,
            hidden
        )]
        pub fn get_for_bcp47(s: &str) -> Result<ICU4XAnyCalendarKind, ()> {
            let s = s.as_bytes(); // #2520
            AnyCalendarKind::get_for_bcp47_bytes(s)
                .map(Into::into)
                .ok_or(())
        }

        /// Obtain the string suitable for use in the -u-ca- extension in a BCP47 locale.
        #[diplomat::rust_link(icu::calendar::AnyCalendarKind::as_bcp47_string, FnInEnum)]
        #[diplomat::rust_link(icu::calendar::AnyCalendarKind::as_bcp47_value, FnInEnum, hidden)]
        pub fn bcp47(
            self,
            write: &mut diplomat_runtime::DiplomatWriteable,
        ) -> Result<(), ICU4XError> {
            let kind = AnyCalendarKind::from(self);
            Ok(write.write_str(kind.as_bcp47_string())?)
        }
    }

    #[diplomat::opaque]
    #[diplomat::transparent_convert]
    #[diplomat::rust_link(icu::calendar::AnyCalendar, Enum)]
    pub struct ICU4XCalendar(pub Arc<AnyCalendar>);

    impl ICU4XCalendar {
        /// Creates a new [`ICU4XCalendar`] from the specified date and time.
        #[diplomat::rust_link(icu::calendar::AnyCalendar::new_for_locale, FnInEnum)]
        pub fn create_for_locale(
            provider: &ICU4XDataProvider,
            locale: &ICU4XLocale,
        ) -> Result<Box<ICU4XCalendar>, ICU4XError> {
            let locale = locale.to_datalocale();

            Ok(Box::new(ICU4XCalendar(Arc::new(call_constructor!(
                AnyCalendar::new_for_locale [r => Ok(r)],
                AnyCalendar::try_new_for_locale_with_any_provider,
                AnyCalendar::try_new_for_locale_with_buffer_provider,
                provider,
                &locale
            )?))))
        }

        /// Creates a new [`ICU4XCalendar`] from the specified date and time.
        #[diplomat::rust_link(icu::calendar::AnyCalendar::new, FnInEnum)]
        pub fn create_for_kind(
            provider: &ICU4XDataProvider,
            kind: ICU4XAnyCalendarKind,
        ) -> Result<Box<ICU4XCalendar>, ICU4XError> {
            Ok(Box::new(ICU4XCalendar(Arc::new(call_constructor!(
                AnyCalendar::new [r => Ok(r)],
                AnyCalendar::try_new_with_any_provider,
                AnyCalendar::try_new_with_buffer_provider,
                provider,
                kind.into()
            )?))))
        }

        /// Returns the kind of this calendar
        #[diplomat::rust_link(icu::calendar::AnyCalendar::kind, FnInEnum)]
        pub fn kind(&self) -> ICU4XAnyCalendarKind {
            self.0.kind().into()
        }
    }
}
