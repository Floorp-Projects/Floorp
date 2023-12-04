// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
pub mod ffi {
    use crate::errors::ffi::ICU4XError;
    use alloc::boxed::Box;
    use core::str;
    use diplomat_runtime::DiplomatWriteable;
    use icu_locid::extensions::unicode::Key;
    use icu_locid::subtags::{Language, Region, Script};
    use icu_locid::Locale;
    use writeable::Writeable;

    use crate::common::ffi::ICU4XOrdering;

    #[diplomat::opaque]
    /// An ICU4X Locale, capable of representing strings like `"en-US"`.
    #[diplomat::rust_link(icu::locid::Locale, Struct)]
    pub struct ICU4XLocale(pub Locale);

    impl ICU4XLocale {
        /// Construct an [`ICU4XLocale`] from an locale identifier.
        ///
        /// This will run the complete locale parsing algorithm. If code size and
        /// performance are critical and the locale is of a known shape (such as
        /// `aa-BB`) use `create_und`, `set_language`, `set_script`, and `set_region`.
        #[diplomat::rust_link(icu::locid::Locale::try_from_bytes, FnInStruct)]
        #[diplomat::rust_link(icu::locid::Locale::from_str, FnInStruct, hidden)]
        pub fn create_from_string(name: &str) -> Result<Box<ICU4XLocale>, ICU4XError> {
            let name = name.as_bytes(); // #2520
            Ok(Box::new(ICU4XLocale(Locale::try_from_bytes(name)?)))
        }

        /// Construct a default undefined [`ICU4XLocale`] "und".
        #[diplomat::rust_link(icu::locid::Locale::UND, AssociatedConstantInStruct)]
        pub fn create_und() -> Box<ICU4XLocale> {
            Box::new(ICU4XLocale(Locale::UND))
        }

        /// Clones the [`ICU4XLocale`].
        #[diplomat::rust_link(icu::locid::Locale, Struct)]
        #[allow(clippy::should_implement_trait)]
        pub fn clone(&self) -> Box<ICU4XLocale> {
            Box::new(ICU4XLocale(self.0.clone()))
        }

        /// Write a string representation of the `LanguageIdentifier` part of
        /// [`ICU4XLocale`] to `write`.
        #[diplomat::rust_link(icu::locid::Locale::id, StructField)]
        pub fn basename(
            &self,
            write: &mut diplomat_runtime::DiplomatWriteable,
        ) -> Result<(), ICU4XError> {
            self.0.id.write_to(write)?;
            Ok(())
        }

        /// Write a string representation of the unicode extension to `write`
        #[diplomat::rust_link(icu::locid::Locale::extensions, StructField)]
        pub fn get_unicode_extension(
            &self,
            bytes: &str,
            write: &mut diplomat_runtime::DiplomatWriteable,
        ) -> Result<(), ICU4XError> {
            let bytes = bytes.as_bytes(); // #2520
            self.0
                .extensions
                .unicode
                .keywords
                .get(&Key::try_from_bytes(bytes)?)
                .ok_or(ICU4XError::LocaleUndefinedSubtagError)?
                .write_to(write)?;
            Ok(())
        }

        /// Write a string representation of [`ICU4XLocale`] language to `write`
        #[diplomat::rust_link(icu::locid::Locale::id, StructField)]
        pub fn language(
            &self,
            write: &mut diplomat_runtime::DiplomatWriteable,
        ) -> Result<(), ICU4XError> {
            self.0.id.language.write_to(write)?;
            Ok(())
        }

        /// Set the language part of the [`ICU4XLocale`].
        #[diplomat::rust_link(icu::locid::Locale::try_from_bytes, FnInStruct)]
        pub fn set_language(&mut self, bytes: &str) -> Result<(), ICU4XError> {
            let bytes = bytes.as_bytes(); // #2520
            self.0.id.language = if bytes.is_empty() {
                Language::UND
            } else {
                Language::try_from_bytes(bytes)?
            };
            Ok(())
        }

        /// Write a string representation of [`ICU4XLocale`] region to `write`
        #[diplomat::rust_link(icu::locid::Locale::id, StructField)]
        pub fn region(
            &self,
            write: &mut diplomat_runtime::DiplomatWriteable,
        ) -> Result<(), ICU4XError> {
            if let Some(region) = self.0.id.region {
                region.write_to(write)?;
                Ok(())
            } else {
                Err(ICU4XError::LocaleUndefinedSubtagError)
            }
        }

        /// Set the region part of the [`ICU4XLocale`].
        #[diplomat::rust_link(icu::locid::Locale::try_from_bytes, FnInStruct)]
        pub fn set_region(&mut self, bytes: &str) -> Result<(), ICU4XError> {
            let bytes = bytes.as_bytes(); // #2520
            self.0.id.region = if bytes.is_empty() {
                None
            } else {
                Some(Region::try_from_bytes(bytes)?)
            };
            Ok(())
        }

        /// Write a string representation of [`ICU4XLocale`] script to `write`
        #[diplomat::rust_link(icu::locid::Locale::id, StructField)]
        pub fn script(
            &self,
            write: &mut diplomat_runtime::DiplomatWriteable,
        ) -> Result<(), ICU4XError> {
            if let Some(script) = self.0.id.script {
                script.write_to(write)?;
                Ok(())
            } else {
                Err(ICU4XError::LocaleUndefinedSubtagError)
            }
        }

        /// Set the script part of the [`ICU4XLocale`]. Pass an empty string to remove the script.
        #[diplomat::rust_link(icu::locid::Locale::try_from_bytes, FnInStruct)]
        pub fn set_script(&mut self, bytes: &str) -> Result<(), ICU4XError> {
            let bytes = bytes.as_bytes(); // #2520
            self.0.id.script = if bytes.is_empty() {
                None
            } else {
                Some(Script::try_from_bytes(bytes)?)
            };
            Ok(())
        }

        /// Best effort locale canonicalizer that doesn't need any data
        ///
        /// Use ICU4XLocaleCanonicalizer for better control and functionality
        #[diplomat::rust_link(icu::locid::Locale::canonicalize, FnInStruct)]
        pub fn canonicalize(bytes: &str, write: &mut DiplomatWriteable) -> Result<(), ICU4XError> {
            let bytes = bytes.as_bytes(); // #2520
            Locale::canonicalize(bytes)?.write_to(write)?;
            Ok(())
        }
        /// Write a string representation of [`ICU4XLocale`] to `write`
        #[diplomat::rust_link(icu::locid::Locale::write_to, FnInStruct)]
        pub fn to_string(
            &self,
            write: &mut diplomat_runtime::DiplomatWriteable,
        ) -> Result<(), ICU4XError> {
            self.0.write_to(write)?;
            Ok(())
        }

        #[diplomat::rust_link(icu::locid::Locale::normalizing_eq, FnInStruct)]
        pub fn normalizing_eq(&self, other: &str) -> bool {
            let other = other.as_bytes(); // #2520
            if let Ok(other) = str::from_utf8(other) {
                self.0.normalizing_eq(other)
            } else {
                // invalid UTF8 won't be allowed in locales anyway
                false
            }
        }

        #[diplomat::rust_link(icu::locid::Locale::strict_cmp, FnInStruct)]
        pub fn strict_cmp(&self, other: &str) -> ICU4XOrdering {
            let other = other.as_bytes(); // #2520
            self.0.strict_cmp(other).into()
        }

        /// Deprecated
        ///
        /// Use `create_from_string("en").
        #[cfg(feature = "provider_test")]
        #[diplomat::attr(dart, disable)]
        pub fn create_en() -> Box<ICU4XLocale> {
            Box::new(ICU4XLocale(icu_locid::locale!("en")))
        }

        /// Deprecated
        ///
        /// Use `create_from_string("bn").
        #[cfg(feature = "provider_test")]
        #[diplomat::attr(dart, disable)]
        pub fn create_bn() -> Box<ICU4XLocale> {
            Box::new(ICU4XLocale(icu_locid::locale!("bn")))
        }
    }
}

impl ffi::ICU4XLocale {
    pub fn to_datalocale(&self) -> icu_provider::DataLocale {
        (&self.0).into()
    }
}
