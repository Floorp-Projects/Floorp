// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
pub mod ffi {
    use alloc::boxed::Box;
    use icu_locid_transform::fallback::LocaleFallbackConfig;
    use icu_locid_transform::fallback::LocaleFallbackIterator;
    use icu_locid_transform::fallback::LocaleFallbackPriority;
    use icu_locid_transform::fallback::LocaleFallbackerWithConfig;
    use icu_locid_transform::LocaleFallbacker;

    use crate::{
        errors::ffi::ICU4XError, locale::ffi::ICU4XLocale, provider::ffi::ICU4XDataProvider,
    };

    /// An object that runs the ICU4X locale fallback algorithm.
    #[diplomat::opaque]
    #[diplomat::rust_link(icu::locid_transform::fallback::LocaleFallbacker, Struct)]
    #[diplomat::rust_link(
        icu::locid_transform::fallback::LocaleFallbackerBorrowed,
        Struct,
        hidden
    )]
    pub struct ICU4XLocaleFallbacker(pub LocaleFallbacker);

    /// Priority mode for the ICU4X fallback algorithm.
    #[diplomat::enum_convert(LocaleFallbackPriority, needs_wildcard)]
    #[diplomat::rust_link(icu::locid_transform::fallback::LocaleFallbackPriority, Enum)]
    #[diplomat::rust_link(
        icu::locid_transform::fallback::LocaleFallbackPriority::const_default,
        FnInEnum,
        hidden
    )]
    pub enum ICU4XLocaleFallbackPriority {
        Language = 0,
        Region = 1,
        Collation = 2,
    }

    /// What additional data is required to load when performing fallback.
    #[diplomat::rust_link(icu::locid_transform::fallback::LocaleFallbackSupplement, Enum)]
    #[diplomat::rust_link(
        icu::locid_transform::fallback::LocaleFallbackSupplement::const_default,
        FnInEnum,
        hidden
    )]
    pub enum ICU4XLocaleFallbackSupplement {
        None = 0,
        Collation = 1,
    }

    /// Collection of configurations for the ICU4X fallback algorithm.
    #[diplomat::rust_link(icu::locid_transform::fallback::LocaleFallbackConfig, Struct)]
    #[diplomat::rust_link(
        icu::locid_transform::fallback::LocaleFallbackConfig::const_default,
        FnInStruct,
        hidden
    )]
    pub struct ICU4XLocaleFallbackConfig<'a> {
        /// Choice of priority mode.
        pub priority: ICU4XLocaleFallbackPriority,
        /// An empty string is considered `None`.
        pub extension_key: &'a str,
        /// Fallback supplement data key to customize fallback rules.
        pub fallback_supplement: ICU4XLocaleFallbackSupplement,
    }

    /// An object that runs the ICU4X locale fallback algorithm with specific configurations.
    #[diplomat::opaque]
    #[diplomat::rust_link(icu::locid_transform::fallback::LocaleFallbacker, Struct)]
    #[diplomat::rust_link(icu::locid_transform::fallback::LocaleFallbackerWithConfig, Struct)]
    pub struct ICU4XLocaleFallbackerWithConfig<'a>(pub LocaleFallbackerWithConfig<'a>);

    /// An iterator over the locale under fallback.
    #[diplomat::opaque]
    #[diplomat::rust_link(icu::locid_transform::fallback::LocaleFallbackIterator, Struct)]
    pub struct ICU4XLocaleFallbackIterator<'a>(pub LocaleFallbackIterator<'a, 'a>);

    impl ICU4XLocaleFallbacker {
        /// Creates a new `ICU4XLocaleFallbacker` from a data provider.
        #[diplomat::rust_link(icu::locid_transform::fallback::LocaleFallbacker::new, FnInStruct)]
        pub fn create(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XLocaleFallbacker>, ICU4XError> {
            Ok(Box::new(ICU4XLocaleFallbacker(call_constructor!(
                LocaleFallbacker::new [r => Ok(r.static_to_owned())],
                LocaleFallbacker::try_new_with_any_provider,
                LocaleFallbacker::try_new_with_buffer_provider,
                provider,
            )?)))
        }

        /// Creates a new `ICU4XLocaleFallbacker` without data for limited functionality.
        #[diplomat::rust_link(
            icu::locid_transform::fallback::LocaleFallbacker::new_without_data,
            FnInStruct
        )]
        pub fn create_without_data() -> Box<ICU4XLocaleFallbacker> {
            Box::new(ICU4XLocaleFallbacker(LocaleFallbacker::new_without_data()))
        }

        /// Associates this `ICU4XLocaleFallbacker` with configuration options.
        #[diplomat::rust_link(
            icu::locid_transform::fallback::LocaleFallbacker::for_config,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::locid_transform::fallback::LocaleFallbackerBorrowed::for_config,
            FnInStruct,
            hidden
        )]
        pub fn for_config<'a, 'temp>(
            &'a self,
            config: ICU4XLocaleFallbackConfig<'temp>,
        ) -> Result<Box<ICU4XLocaleFallbackerWithConfig<'a>>, ICU4XError> {
            Ok(Box::new(ICU4XLocaleFallbackerWithConfig(
                self.0.for_config(LocaleFallbackConfig::try_from(config)?),
            )))
        }
    }

    impl<'a> ICU4XLocaleFallbackerWithConfig<'a> {
        /// Creates an iterator from a locale with each step of fallback.
        #[diplomat::rust_link(
            icu::locid_transform::fallback::LocaleFallbacker::fallback_for,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::locid_transform::fallback::LocaleFallbackerBorrowed::fallback_for,
            FnInStruct,
            hidden
        )]
        #[diplomat::rust_link(
            icu::locid_transform::fallback::LocaleFallbackerWithConfig::fallback_for,
            FnInStruct,
            hidden
        )]
        pub fn fallback_for_locale<'b: 'a, 'temp>(
            &'b self,
            locale: &'temp ICU4XLocale,
        ) -> Box<ICU4XLocaleFallbackIterator<'a>> {
            Box::new(ICU4XLocaleFallbackIterator(
                self.0.fallback_for((&locale.0).into()),
            ))
        }
    }

    impl<'a> ICU4XLocaleFallbackIterator<'a> {
        /// Gets a snapshot of the current state of the locale.
        #[diplomat::rust_link(
            icu::locid_transform::fallback::LocaleFallbackIterator::get,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::locid_transform::fallback::LocaleFallbackIterator::take,
            FnInStruct,
            hidden
        )]
        pub fn get(&self) -> Box<ICU4XLocale> {
            Box::new(ICU4XLocale(self.0.get().clone().into_locale()))
        }

        /// Performs one step of the fallback algorithm, mutating the locale.
        #[diplomat::rust_link(
            icu::locid_transform::fallback::LocaleFallbackIterator::step,
            FnInStruct
        )]
        pub fn step(&mut self) {
            self.0.step();
        }
    }
}

impl TryFrom<ffi::ICU4XLocaleFallbackConfig<'_>>
    for icu_locid_transform::fallback::LocaleFallbackConfig
{
    type Error = crate::errors::ffi::ICU4XError;
    fn try_from(other: ffi::ICU4XLocaleFallbackConfig) -> Result<Self, Self::Error> {
        let mut result = Self::default();
        result.priority = other.priority.into();
        result.extension_key = match other.extension_key {
            "" => None,
            s => Some(s.parse()?),
        };
        result.fallback_supplement = match other.fallback_supplement {
            ffi::ICU4XLocaleFallbackSupplement::None => None,
            ffi::ICU4XLocaleFallbackSupplement::Collation => {
                Some(icu_locid_transform::fallback::LocaleFallbackSupplement::Collation)
            }
        };
        Ok(result)
    }
}
