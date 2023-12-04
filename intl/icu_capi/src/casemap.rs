// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use icu_casemap::titlecase::TitlecaseOptions;

#[diplomat::bridge]
pub mod ffi {
    use crate::{
        errors::ffi::ICU4XError, locale::ffi::ICU4XLocale, provider::ffi::ICU4XDataProvider,
    };
    use alloc::boxed::Box;
    use diplomat_runtime::DiplomatWriteable;
    use icu_casemap::titlecase::{LeadingAdjustment, TrailingCase};
    use icu_casemap::{CaseMapCloser, CaseMapper, TitlecaseMapper};
    use writeable::Writeable;

    #[diplomat::enum_convert(LeadingAdjustment, needs_wildcard)]
    #[diplomat::rust_link(icu::casemap::titlecase::LeadingAdjustment, Enum)]
    pub enum ICU4XLeadingAdjustment {
        Auto,
        None,
        ToCased,
    }

    #[diplomat::enum_convert(TrailingCase, needs_wildcard)]
    #[diplomat::rust_link(icu::casemap::titlecase::TrailingCase, Enum)]
    pub enum ICU4XTrailingCase {
        Lower,
        Unchanged,
    }

    #[diplomat::rust_link(icu::casemap::titlecase::TitlecaseOptions, Struct)]
    pub struct ICU4XTitlecaseOptionsV1 {
        pub leading_adjustment: ICU4XLeadingAdjustment,
        pub trailing_case: ICU4XTrailingCase,
    }

    impl ICU4XTitlecaseOptionsV1 {
        #[diplomat::rust_link(icu::casemap::titlecase::TitlecaseOptions::default, FnInStruct)]
        pub fn default_options() -> ICU4XTitlecaseOptionsV1 {
            // named default_options to avoid keyword clashes
            Self {
                leading_adjustment: ICU4XLeadingAdjustment::Auto,
                trailing_case: ICU4XTrailingCase::Lower,
            }
        }
    }

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::casemap::CaseMapper, Struct)]
    pub struct ICU4XCaseMapper(pub CaseMapper);

    impl ICU4XCaseMapper {
        /// Construct a new ICU4XCaseMapper instance
        #[diplomat::rust_link(icu::casemap::CaseMapper::new, FnInStruct)]
        pub fn create(provider: &ICU4XDataProvider) -> Result<Box<ICU4XCaseMapper>, ICU4XError> {
            Ok(Box::new(ICU4XCaseMapper(call_constructor!(
                CaseMapper::new [r => Ok(r)],
                CaseMapper::try_new_with_any_provider,
                CaseMapper::try_new_with_buffer_provider,
                provider,
            )?)))
        }

        /// Returns the full lowercase mapping of the given string
        #[diplomat::rust_link(icu::casemap::CaseMapper::lowercase, FnInStruct)]
        #[diplomat::rust_link(icu::casemap::CaseMapper::lowercase_to_string, FnInStruct, hidden)]
        pub fn lowercase(
            &self,
            s: &str,
            locale: &ICU4XLocale,
            write: &mut DiplomatWriteable,
        ) -> Result<(), ICU4XError> {
            // #2520
            // In the future we should be able to make assumptions based on backend
            core::str::from_utf8(s.as_bytes())
                .map_err(|e| ICU4XError::DataIoError.log_original(&e))?;
            self.0.lowercase(s, &locale.0.id).write_to(write)?;

            Ok(())
        }

        /// Returns the full uppercase mapping of the given string
        #[diplomat::rust_link(icu::casemap::CaseMapper::uppercase, FnInStruct)]
        #[diplomat::rust_link(icu::casemap::CaseMapper::uppercase_to_string, FnInStruct, hidden)]
        pub fn uppercase(
            &self,
            s: &str,
            locale: &ICU4XLocale,
            write: &mut DiplomatWriteable,
        ) -> Result<(), ICU4XError> {
            // #2520
            // In the future we should be able to make assumptions based on backend
            core::str::from_utf8(s.as_bytes())
                .map_err(|e| ICU4XError::DataIoError.log_original(&e))?;
            self.0.uppercase(s, &locale.0.id).write_to(write)?;

            Ok(())
        }

        /// Returns the full titlecase mapping of the given string, performing head adjustment without
        /// loading additional data.
        /// (if head adjustment is enabled in the options)
        ///
        /// The `v1` refers to the version of the options struct, which may change as we add more options
        #[diplomat::rust_link(
            icu::casemap::CaseMapper::titlecase_segment_with_only_case_data,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::casemap::CaseMapper::titlecase_segment_with_only_case_data_to_string,
            FnInStruct,
            hidden
        )]
        pub fn titlecase_segment_with_only_case_data_v1(
            &self,
            s: &str,
            locale: &ICU4XLocale,
            options: ICU4XTitlecaseOptionsV1,
            write: &mut DiplomatWriteable,
        ) -> Result<(), ICU4XError> {
            // #2520
            // In the future we should be able to make assumptions based on backend
            core::str::from_utf8(s.as_bytes())
                .map_err(|e| ICU4XError::DataIoError.log_original(&e))?;
            self.0
                .titlecase_segment_with_only_case_data(s, &locale.0.id, options.into())
                .write_to(write)?;

            Ok(())
        }

        /// Case-folds the characters in the given string
        #[diplomat::rust_link(icu::casemap::CaseMapper::fold, FnInStruct)]
        #[diplomat::rust_link(icu::casemap::CaseMapper::fold_string, FnInStruct, hidden)]
        pub fn fold(&self, s: &str, write: &mut DiplomatWriteable) -> Result<(), ICU4XError> {
            // #2520
            // In the future we should be able to make assumptions based on backend
            core::str::from_utf8(s.as_bytes())
                .map_err(|e| ICU4XError::DataIoError.log_original(&e))?;
            self.0.fold(s).write_to(write)?;

            Ok(())
        }
        /// Case-folds the characters in the given string
        /// using Turkic (T) mappings for dotted/dotless I.
        #[diplomat::rust_link(icu::casemap::CaseMapper::fold_turkic, FnInStruct)]
        #[diplomat::rust_link(icu::casemap::CaseMapper::fold_turkic_string, FnInStruct, hidden)]
        pub fn fold_turkic(
            &self,
            s: &str,
            write: &mut DiplomatWriteable,
        ) -> Result<(), ICU4XError> {
            // #2520
            // In the future we should be able to make assumptions based on backend
            core::str::from_utf8(s.as_bytes())
                .map_err(|e| ICU4XError::DataIoError.log_original(&e))?;
            self.0.fold_turkic(s).write_to(write)?;

            Ok(())
        }

        /// Adds all simple case mappings and the full case folding for `c` to `builder`.
        /// Also adds special case closure mappings.
        ///
        /// In other words, this adds all characters that this casemaps to, as
        /// well as all characters that may casemap to this one.
        ///
        /// Note that since ICU4XCodePointSetBuilder does not contain strings, this will
        /// ignore string mappings.
        ///
        /// Identical to the similarly named method on `ICU4XCaseMapCloser`, use that if you
        /// plan on using string case closure mappings too.
        #[cfg(feature = "icu_properties")]
        #[diplomat::rust_link(icu::casemap::CaseMapper::add_case_closure_to, FnInStruct)]
        pub fn add_case_closure_to(
            &self,
            c: char,
            builder: &mut crate::collections_sets::ffi::ICU4XCodePointSetBuilder,
        ) {
            self.0.add_case_closure_to(c, &mut builder.0)
        }

        /// Returns the simple lowercase mapping of the given character.
        ///
        /// This function only implements simple and common mappings.
        /// Full mappings, which can map one char to a string, are not included.
        /// For full mappings, use `ICU4XCaseMapper::lowercase`.
        #[diplomat::rust_link(icu::casemap::CaseMapper::simple_lowercase, FnInStruct)]
        pub fn simple_lowercase(&self, ch: char) -> char {
            self.0.simple_lowercase(ch)
        }

        /// Returns the simple uppercase mapping of the given character.
        ///
        /// This function only implements simple and common mappings.
        /// Full mappings, which can map one char to a string, are not included.
        /// For full mappings, use `ICU4XCaseMapper::uppercase`.
        #[diplomat::rust_link(icu::casemap::CaseMapper::simple_uppercase, FnInStruct)]
        pub fn simple_uppercase(&self, ch: char) -> char {
            self.0.simple_uppercase(ch)
        }

        /// Returns the simple titlecase mapping of the given character.
        ///
        /// This function only implements simple and common mappings.
        /// Full mappings, which can map one char to a string, are not included.
        /// For full mappings, use `ICU4XCaseMapper::titlecase_segment`.
        #[diplomat::rust_link(icu::casemap::CaseMapper::simple_titlecase, FnInStruct)]
        pub fn simple_titlecase(&self, ch: char) -> char {
            self.0.simple_titlecase(ch)
        }

        /// Returns the simple casefolding of the given character.
        ///
        /// This function only implements simple folding.
        /// For full folding, use `ICU4XCaseMapper::fold`.
        #[diplomat::rust_link(icu::casemap::CaseMapper::simple_fold, FnInStruct)]
        pub fn simple_fold(&self, ch: char) -> char {
            self.0.simple_fold(ch)
        }
        /// Returns the simple casefolding of the given character in the Turkic locale
        ///
        /// This function only implements simple folding.
        /// For full folding, use `ICU4XCaseMapper::fold_turkic`.
        #[diplomat::rust_link(icu::casemap::CaseMapper::simple_fold_turkic, FnInStruct)]
        pub fn simple_fold_turkic(&self, ch: char) -> char {
            self.0.simple_fold_turkic(ch)
        }
    }

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::casemap::CaseMapCloser, Struct)]
    pub struct ICU4XCaseMapCloser(pub CaseMapCloser<CaseMapper>);

    impl ICU4XCaseMapCloser {
        /// Construct a new ICU4XCaseMapper instance
        #[diplomat::rust_link(icu::casemap::CaseMapCloser::new, FnInStruct)]
        #[diplomat::rust_link(icu::casemap::CaseMapCloser::new_with_mapper, FnInStruct, hidden)]
        pub fn create(provider: &ICU4XDataProvider) -> Result<Box<ICU4XCaseMapCloser>, ICU4XError> {
            Ok(Box::new(ICU4XCaseMapCloser(call_constructor!(
                CaseMapCloser::new [r => Ok(r)],
                CaseMapCloser::try_new_with_any_provider,
                CaseMapCloser::try_new_with_buffer_provider,
                provider,
            )?)))
        }

        /// Adds all simple case mappings and the full case folding for `c` to `builder`.
        /// Also adds special case closure mappings.
        #[cfg(feature = "icu_properties")]
        #[diplomat::rust_link(icu::casemap::CaseMapCloser::add_case_closure_to, FnInStruct)]
        pub fn add_case_closure_to(
            &self,
            c: char,
            builder: &mut crate::collections_sets::ffi::ICU4XCodePointSetBuilder,
        ) {
            self.0.add_case_closure_to(c, &mut builder.0)
        }

        /// Finds all characters and strings which may casemap to `s` as their full case folding string
        /// and adds them to the set.
        ///
        /// Returns true if the string was found
        #[cfg(feature = "icu_properties")]
        #[diplomat::rust_link(icu::casemap::CaseMapCloser::add_string_case_closure_to, FnInStruct)]
        pub fn add_string_case_closure_to(
            &self,
            s: &str,
            builder: &mut crate::collections_sets::ffi::ICU4XCodePointSetBuilder,
        ) -> bool {
            // #2520
            // In the future we should be able to make assumptions based on backend
            let s = core::str::from_utf8(s.as_bytes()).unwrap_or("");
            self.0.add_string_case_closure_to(s, &mut builder.0)
        }
    }

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::casemap::TitlecaseMapper, Struct)]
    pub struct ICU4XTitlecaseMapper(pub TitlecaseMapper<CaseMapper>);

    impl ICU4XTitlecaseMapper {
        /// Construct a new `ICU4XTitlecaseMapper` instance
        #[diplomat::rust_link(icu::casemap::TitlecaseMapper::new, FnInStruct)]
        #[diplomat::rust_link(icu::casemap::TitlecaseMapper::new_with_mapper, FnInStruct, hidden)]
        pub fn create(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XTitlecaseMapper>, ICU4XError> {
            Ok(Box::new(ICU4XTitlecaseMapper(call_constructor!(
                TitlecaseMapper::new [r => Ok(r)],
                TitlecaseMapper::try_new_with_any_provider,
                TitlecaseMapper::try_new_with_buffer_provider,
                provider,
            )?)))
        }

        /// Returns the full titlecase mapping of the given string
        ///
        /// The `v1` refers to the version of the options struct, which may change as we add more options
        #[diplomat::rust_link(icu::casemap::TitlecaseMapper::titlecase_segment, FnInStruct)]
        #[diplomat::rust_link(
            icu::casemap::TitlecaseMapper::titlecase_segment_to_string,
            FnInStruct,
            hidden
        )]
        pub fn titlecase_segment_v1(
            &self,
            s: &str,
            locale: &ICU4XLocale,
            options: ICU4XTitlecaseOptionsV1,
            write: &mut DiplomatWriteable,
        ) -> Result<(), ICU4XError> {
            // #2520
            // In the future we should be able to make assumptions based on backend
            core::str::from_utf8(s.as_bytes())
                .map_err(|e| ICU4XError::DataIoError.log_original(&e))?;
            self.0
                .titlecase_segment(s, &locale.0.id, options.into())
                .write_to(write)?;

            Ok(())
        }
    }
}

impl From<ffi::ICU4XTitlecaseOptionsV1> for TitlecaseOptions {
    fn from(other: ffi::ICU4XTitlecaseOptionsV1) -> Self {
        let mut ret = Self::default();

        ret.leading_adjustment = other.leading_adjustment.into();
        ret.trailing_case = other.trailing_case.into();
        ret
    }
}
