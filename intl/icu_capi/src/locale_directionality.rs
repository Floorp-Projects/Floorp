// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
pub mod ffi {
    use crate::{
        errors::ffi::ICU4XError,
        locale::ffi::ICU4XLocale,
        locid_transform::ffi::ICU4XLocaleExpander,
        provider::{ffi::ICU4XDataProvider, ICU4XDataProviderInner},
    };
    use alloc::boxed::Box;
    use icu_locid_transform::{Direction, LocaleDirectionality};

    #[diplomat::rust_link(icu::locid_transform::Direction, Enum)]
    pub enum ICU4XLocaleDirection {
        LeftToRight,
        RightToLeft,
        Unknown,
    }

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::locid_transform::LocaleDirectionality, Struct)]
    pub struct ICU4XLocaleDirectionality(pub LocaleDirectionality);

    impl ICU4XLocaleDirectionality {
        /// Construct a new ICU4XLocaleDirectionality instance
        #[diplomat::rust_link(icu::locid_transform::LocaleDirectionality::new, FnInStruct)]
        pub fn create(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XLocaleDirectionality>, ICU4XError> {
            Ok(Box::new(ICU4XLocaleDirectionality(call_constructor!(
                LocaleDirectionality::new [r => Ok(r)],
                LocaleDirectionality::try_new_with_any_provider,
                LocaleDirectionality::try_new_with_buffer_provider,
                provider,
            )?)))
        }

        /// Construct a new ICU4XLocaleDirectionality instance with a custom expander
        #[diplomat::rust_link(
            icu::locid_transform::LocaleDirectionality::new_with_expander,
            FnInStruct
        )]
        pub fn create_with_expander(
            provider: &ICU4XDataProvider,
            expander: &ICU4XLocaleExpander,
        ) -> Result<Box<ICU4XLocaleDirectionality>, ICU4XError> {
            #[allow(unused_imports)]
            use icu_provider::prelude::*;
            Ok(Box::new(ICU4XLocaleDirectionality(match &provider.0 {
                ICU4XDataProviderInner::Destroyed => Err(icu_provider::DataError::custom(
                    "This provider has been destroyed",
                ))?,
                ICU4XDataProviderInner::Empty => {
                    LocaleDirectionality::try_new_with_expander_unstable(
                        &icu_provider_adapters::empty::EmptyDataProvider::new(),
                        expander.0.clone(),
                    )?
                }
                #[cfg(feature = "buffer_provider")]
                ICU4XDataProviderInner::Buffer(buffer_provider) => {
                    LocaleDirectionality::try_new_with_expander_unstable(
                        &buffer_provider.as_deserializing(),
                        expander.0.clone(),
                    )?
                }
                #[cfg(feature = "compiled_data")]
                ICU4XDataProviderInner::Compiled => {
                    LocaleDirectionality::new_with_expander(expander.0.clone())
                }
            })))
        }

        #[diplomat::rust_link(icu::locid_transform::LocaleDirectionality::get, FnInStruct)]
        pub fn get(&self, locale: &ICU4XLocale) -> ICU4XLocaleDirection {
            match self.0.get(&locale.0) {
                Some(Direction::LeftToRight) => ICU4XLocaleDirection::LeftToRight,
                Some(Direction::RightToLeft) => ICU4XLocaleDirection::RightToLeft,
                _ => ICU4XLocaleDirection::Unknown,
            }
        }

        #[diplomat::rust_link(
            icu::locid_transform::LocaleDirectionality::is_left_to_right,
            FnInStruct
        )]
        pub fn is_left_to_right(&self, locale: &ICU4XLocale) -> bool {
            self.0.is_left_to_right(&locale.0)
        }

        #[diplomat::rust_link(
            icu::locid_transform::LocaleDirectionality::is_right_to_left,
            FnInStruct
        )]
        pub fn is_right_to_left(&self, locale: &ICU4XLocale) -> bool {
            self.0.is_right_to_left(&locale.0)
        }
    }
}
