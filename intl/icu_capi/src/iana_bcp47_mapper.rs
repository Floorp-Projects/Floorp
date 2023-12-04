// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
pub mod ffi {
    use crate::errors::ffi::ICU4XError;
    use crate::provider::ffi::ICU4XDataProvider;
    use alloc::boxed::Box;
    use icu_timezone::IanaBcp47RoundTripMapper;
    use icu_timezone::IanaToBcp47Mapper;
    use icu_timezone::TimeZoneBcp47Id;

    /// An object capable of mapping from an IANA time zone ID to a BCP-47 ID.
    ///
    /// This can be used via `try_set_iana_time_zone_id()` on [`ICU4XCustomTimeZone`].
    ///
    /// [`ICU4XCustomTimeZone`]: crate::timezone::ffi::ICU4XCustomTimeZone
    #[diplomat::opaque]
    #[diplomat::rust_link(icu::timezone::IanaToBcp47Mapper, Struct)]
    #[diplomat::rust_link(icu::timezone::IanaToBcp47Mapper::as_borrowed, FnInStruct, hidden)]
    pub struct ICU4XIanaToBcp47Mapper(pub IanaToBcp47Mapper);

    impl ICU4XIanaToBcp47Mapper {
        #[diplomat::rust_link(icu::timezone::IanaToBcp47Mapper::new, FnInStruct)]
        pub fn create(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XIanaToBcp47Mapper>, ICU4XError> {
            Ok(Box::new(ICU4XIanaToBcp47Mapper(call_constructor!(
                IanaToBcp47Mapper::new [r => Ok(r)],
                IanaToBcp47Mapper::try_new_with_any_provider,
                IanaToBcp47Mapper::try_new_with_buffer_provider,
                provider,
            )?)))
        }
    }

    /// An object capable of mapping from a BCP-47 time zone ID to an IANA ID.
    #[diplomat::opaque]
    #[diplomat::rust_link(icu::timezone::IanaBcp47RoundTripMapper, Struct)]
    #[diplomat::rust_link(
        icu::timezone::IanaBcp47RoundTripMapper::as_borrowed,
        FnInStruct,
        hidden
    )]
    pub struct ICU4XBcp47ToIanaMapper(pub IanaBcp47RoundTripMapper);

    impl ICU4XBcp47ToIanaMapper {
        #[diplomat::rust_link(icu::timezone::IanaBcp47RoundTripMapper::new, FnInStruct)]
        pub fn create(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XBcp47ToIanaMapper>, ICU4XError> {
            Ok(Box::new(ICU4XBcp47ToIanaMapper(call_constructor!(
                IanaBcp47RoundTripMapper::new [r => Ok(r)],
                IanaBcp47RoundTripMapper::try_new_with_any_provider,
                IanaBcp47RoundTripMapper::try_new_with_buffer_provider,
                provider,
            )?)))
        }

        /// Writes out the canonical IANA time zone ID corresponding to the given BCP-47 ID.
        #[diplomat::rust_link(
            icu::datetime::time_zone::IanaBcp47RoundTripMapper::bcp47_to_iana,
            FnInStruct
        )]
        pub fn get(
            &self,
            value: &str,
            write: &mut diplomat_runtime::DiplomatWriteable,
        ) -> Result<(), ICU4XError> {
            use core::str::FromStr;
            use writeable::Writeable;
            let handle = self.0.as_borrowed();
            TimeZoneBcp47Id::from_str(value)
                .ok()
                .and_then(|bcp47_id| handle.bcp47_to_iana(bcp47_id))
                .ok_or(ICU4XError::TimeZoneInvalidIdError)?
                .write_to(write)?;
            Ok(())
        }
    }
}
