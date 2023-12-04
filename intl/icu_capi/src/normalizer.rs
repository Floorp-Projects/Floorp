// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
pub mod ffi {
    use crate::{errors::ffi::ICU4XError, provider::ffi::ICU4XDataProvider};
    use alloc::boxed::Box;
    use diplomat_runtime::DiplomatWriteable;
    use icu_normalizer::{ComposingNormalizer, DecomposingNormalizer};

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::normalizer::ComposingNormalizer, Struct)]
    pub struct ICU4XComposingNormalizer(pub ComposingNormalizer);

    impl ICU4XComposingNormalizer {
        /// Construct a new ICU4XComposingNormalizer instance for NFC
        #[diplomat::rust_link(icu::normalizer::ComposingNormalizer::new_nfc, FnInStruct)]
        pub fn create_nfc(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XComposingNormalizer>, ICU4XError> {
            Ok(Box::new(ICU4XComposingNormalizer(call_constructor!(
                ComposingNormalizer::new_nfc [r => Ok(r)],
                ComposingNormalizer::try_new_nfc_with_any_provider,
                ComposingNormalizer::try_new_nfc_with_buffer_provider,
                provider,
            )?)))
        }

        /// Construct a new ICU4XComposingNormalizer instance for NFKC
        #[diplomat::rust_link(icu::normalizer::ComposingNormalizer::new_nfkc, FnInStruct)]
        pub fn create_nfkc(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XComposingNormalizer>, ICU4XError> {
            Ok(Box::new(ICU4XComposingNormalizer(call_constructor!(
                ComposingNormalizer::new_nfkc [r => Ok(r)],
                ComposingNormalizer::try_new_nfkc_with_any_provider,
                ComposingNormalizer::try_new_nfkc_with_buffer_provider,
                provider,
            )?)))
        }

        /// Normalize a (potentially ill-formed) UTF8 string
        ///
        /// Errors are mapped to REPLACEMENT CHARACTER
        #[diplomat::rust_link(icu::normalizer::ComposingNormalizer::normalize_utf8, FnInStruct)]
        #[diplomat::rust_link(icu::normalizer::ComposingNormalizer::normalize, FnInStruct, hidden)]
        #[diplomat::rust_link(
            icu::normalizer::ComposingNormalizer::normalize_to,
            FnInStruct,
            hidden
        )]
        #[diplomat::rust_link(
            icu::normalizer::ComposingNormalizer::normalize_utf8_to,
            FnInStruct,
            hidden
        )]
        pub fn normalize(&self, s: &str, write: &mut DiplomatWriteable) -> Result<(), ICU4XError> {
            let s = s.as_bytes(); // #2520
            self.0.normalize_utf8_to(s, write)?;
            Ok(())
        }

        /// Check if a (potentially ill-formed) UTF8 string is normalized
        ///
        /// Errors are mapped to REPLACEMENT CHARACTER
        #[diplomat::rust_link(icu::normalizer::ComposingNormalizer::is_normalized_utf8, FnInStruct)]
        #[diplomat::rust_link(
            icu::normalizer::ComposingNormalizer::is_normalized,
            FnInStruct,
            hidden
        )]
        pub fn is_normalized(&self, s: &str) -> bool {
            let s = s.as_bytes(); // #2520
            self.0.is_normalized_utf8(s)
        }
    }

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::normalizer::DecomposingNormalizer, Struct)]
    pub struct ICU4XDecomposingNormalizer(pub DecomposingNormalizer);

    impl ICU4XDecomposingNormalizer {
        /// Construct a new ICU4XDecomposingNormalizer instance for NFC
        #[diplomat::rust_link(icu::normalizer::DecomposingNormalizer::new_nfd, FnInStruct)]
        pub fn create_nfd(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XDecomposingNormalizer>, ICU4XError> {
            Ok(Box::new(ICU4XDecomposingNormalizer(call_constructor!(
                DecomposingNormalizer::new_nfd [r => Ok(r)],
                DecomposingNormalizer::try_new_nfd_with_any_provider,
                DecomposingNormalizer::try_new_nfd_with_buffer_provider,
                provider,
            )?)))
        }

        /// Construct a new ICU4XDecomposingNormalizer instance for NFKC
        #[diplomat::rust_link(icu::normalizer::DecomposingNormalizer::new_nfkd, FnInStruct)]
        pub fn create_nfkd(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XDecomposingNormalizer>, ICU4XError> {
            Ok(Box::new(ICU4XDecomposingNormalizer(call_constructor!(
                DecomposingNormalizer::new_nfkd [r => Ok(r)],
                DecomposingNormalizer::try_new_nfkd_with_any_provider,
                DecomposingNormalizer::try_new_nfkd_with_buffer_provider,
                provider,
            )?)))
        }

        /// Normalize a (potentially ill-formed) UTF8 string
        ///
        /// Errors are mapped to REPLACEMENT CHARACTER
        #[diplomat::rust_link(icu::normalizer::DecomposingNormalizer::normalize_utf8, FnInStruct)]
        #[diplomat::rust_link(
            icu::normalizer::DecomposingNormalizer::normalize,
            FnInStruct,
            hidden
        )]
        #[diplomat::rust_link(
            icu::normalizer::DecomposingNormalizer::normalize_to,
            FnInStruct,
            hidden
        )]
        #[diplomat::rust_link(
            icu::normalizer::DecomposingNormalizer::normalize_utf8_to,
            FnInStruct,
            hidden
        )]
        pub fn normalize(&self, s: &str, write: &mut DiplomatWriteable) -> Result<(), ICU4XError> {
            let s = s.as_bytes(); // #2520
            self.0.normalize_utf8_to(s, write)?;
            Ok(())
        }

        /// Check if a (potentially ill-formed) UTF8 string is normalized
        ///
        /// Errors are mapped to REPLACEMENT CHARACTER
        #[diplomat::rust_link(
            icu::normalizer::DecomposingNormalizer::is_normalized_utf8,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::normalizer::DecomposingNormalizer::is_normalized,
            FnInStruct,
            hidden
        )]
        pub fn is_normalized(&self, s: &str) -> bool {
            let s = s.as_bytes(); // #2520
            self.0.is_normalized_utf8(s)
        }
    }
}
