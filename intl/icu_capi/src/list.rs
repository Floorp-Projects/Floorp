// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
pub mod ffi {
    use crate::{
        errors::ffi::ICU4XError, locale::ffi::ICU4XLocale, provider::ffi::ICU4XDataProvider,
    };
    use alloc::boxed::Box;
    use alloc::string::String;
    use alloc::vec::Vec;
    use diplomat_runtime::DiplomatWriteable;
    use icu_list::{ListFormatter, ListLength};
    use writeable::Writeable;

    /// A list of strings
    #[diplomat::opaque]
    pub struct ICU4XList(pub Vec<String>);

    impl ICU4XList {
        /// Create a new list of strings
        pub fn create() -> Box<ICU4XList> {
            Box::new(ICU4XList(Vec::new()))
        }

        /// Create a new list of strings with preallocated space to hold
        /// at least `capacity` elements
        pub fn create_with_capacity(capacity: usize) -> Box<ICU4XList> {
            Box::new(ICU4XList(Vec::with_capacity(capacity)))
        }

        /// Push a string to the list
        ///
        /// For C++ users, potentially invalid UTF8 will be handled via
        /// REPLACEMENT CHARACTERs
        pub fn push(&mut self, val: &str) {
            let val = val.as_bytes(); // #2520
            self.0.push(String::from_utf8_lossy(val).into_owned());
        }

        /// The number of elements in this list
        #[allow(clippy::len_without_is_empty)] // don't need to follow Rust conventions over FFI
        #[diplomat::attr(dart, rename = "length")]
        pub fn len(&self) -> usize {
            self.0.len()
        }
    }

    #[diplomat::rust_link(icu::list::ListLength, Enum)]
    #[diplomat::enum_convert(ListLength, needs_wildcard)]
    pub enum ICU4XListLength {
        Wide,
        Short,
        Narrow,
    }
    #[diplomat::opaque]
    #[diplomat::rust_link(icu::list::ListFormatter, Struct)]
    pub struct ICU4XListFormatter(pub ListFormatter);

    impl ICU4XListFormatter {
        /// Construct a new ICU4XListFormatter instance for And patterns
        #[diplomat::rust_link(icu::list::ListFormatter::try_new_and_with_length, FnInStruct)]
        pub fn create_and_with_length(
            provider: &ICU4XDataProvider,
            locale: &ICU4XLocale,
            length: ICU4XListLength,
        ) -> Result<Box<ICU4XListFormatter>, ICU4XError> {
            let locale = locale.to_datalocale();
            Ok(Box::new(ICU4XListFormatter(call_constructor!(
                ListFormatter::try_new_and_with_length,
                ListFormatter::try_new_and_with_length_with_any_provider,
                ListFormatter::try_new_and_with_length_with_buffer_provider,
                provider,
                &locale,
                length.into()
            )?)))
        }
        /// Construct a new ICU4XListFormatter instance for And patterns
        #[diplomat::rust_link(icu::list::ListFormatter::try_new_or_with_length, FnInStruct)]
        pub fn create_or_with_length(
            provider: &ICU4XDataProvider,
            locale: &ICU4XLocale,
            length: ICU4XListLength,
        ) -> Result<Box<ICU4XListFormatter>, ICU4XError> {
            let locale = locale.to_datalocale();
            Ok(Box::new(ICU4XListFormatter(call_constructor!(
                ListFormatter::try_new_or_with_length,
                ListFormatter::try_new_or_with_length_with_any_provider,
                ListFormatter::try_new_or_with_length_with_buffer_provider,
                provider,
                &locale,
                length.into()
            )?)))
        }
        /// Construct a new ICU4XListFormatter instance for And patterns
        #[diplomat::rust_link(icu::list::ListFormatter::try_new_unit_with_length, FnInStruct)]
        pub fn create_unit_with_length(
            provider: &ICU4XDataProvider,
            locale: &ICU4XLocale,
            length: ICU4XListLength,
        ) -> Result<Box<ICU4XListFormatter>, ICU4XError> {
            let locale = locale.to_datalocale();
            Ok(Box::new(ICU4XListFormatter(call_constructor!(
                ListFormatter::try_new_unit_with_length,
                ListFormatter::try_new_unit_with_length_with_any_provider,
                ListFormatter::try_new_unit_with_length_with_buffer_provider,
                provider,
                &locale,
                length.into()
            )?)))
        }

        #[diplomat::rust_link(icu::list::ListFormatter::format, FnInStruct)]
        #[diplomat::rust_link(icu::list::ListFormatter::format_to_string, FnInStruct, hidden)]
        pub fn format(
            &self,
            list: &ICU4XList,
            write: &mut DiplomatWriteable,
        ) -> Result<(), ICU4XError> {
            self.0.format(list.0.iter()).write_to(write)?;
            Ok(())
        }
    }
}
