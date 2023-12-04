// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
pub mod ffi {
    use alloc::boxed::Box;
    use icu_collator::{Collator, CollatorOptions};

    use crate::{
        common::ffi::ICU4XOrdering, errors::ffi::ICU4XError, locale::ffi::ICU4XLocale,
        provider::ffi::ICU4XDataProvider,
    };

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::collator::Collator, Struct)]
    pub struct ICU4XCollator(pub Collator);

    #[diplomat::rust_link(icu::collator::CollatorOptions, Struct)]
    #[diplomat::rust_link(icu::collator::CollatorOptions::new, FnInStruct, hidden)]
    pub struct ICU4XCollatorOptionsV1 {
        pub strength: ICU4XCollatorStrength,
        pub alternate_handling: ICU4XCollatorAlternateHandling,
        pub case_first: ICU4XCollatorCaseFirst,
        pub max_variable: ICU4XCollatorMaxVariable,
        pub case_level: ICU4XCollatorCaseLevel,
        pub numeric: ICU4XCollatorNumeric,
        pub backward_second_level: ICU4XCollatorBackwardSecondLevel,
    }

    #[diplomat::rust_link(icu::collator::Strength, Enum)]
    pub enum ICU4XCollatorStrength {
        Auto = 0,
        Primary = 1,
        Secondary = 2,
        Tertiary = 3,
        Quaternary = 4,
        Identical = 5,
    }

    #[diplomat::rust_link(icu::collator::AlternateHandling, Enum)]
    pub enum ICU4XCollatorAlternateHandling {
        Auto = 0,
        NonIgnorable = 1,
        Shifted = 2,
    }

    #[diplomat::rust_link(icu::collator::CaseFirst, Enum)]
    pub enum ICU4XCollatorCaseFirst {
        Auto = 0,
        Off = 1,
        LowerFirst = 2,
        UpperFirst = 3,
    }

    #[diplomat::rust_link(icu::collator::MaxVariable, Enum)]
    pub enum ICU4XCollatorMaxVariable {
        Auto = 0,
        Space = 1,
        Punctuation = 2,
        Symbol = 3,
        Currency = 4,
    }

    #[diplomat::rust_link(icu::collator::CaseLevel, Enum)]
    pub enum ICU4XCollatorCaseLevel {
        Auto = 0,
        Off = 1,
        On = 2,
    }

    #[diplomat::rust_link(icu::collator::Numeric, Enum)]
    pub enum ICU4XCollatorNumeric {
        Auto = 0,
        Off = 1,
        On = 2,
    }

    #[diplomat::rust_link(icu::collator::BackwardSecondLevel, Enum)]
    pub enum ICU4XCollatorBackwardSecondLevel {
        Auto = 0,
        Off = 1,
        On = 2,
    }

    impl ICU4XCollator {
        /// Construct a new Collator instance.
        #[diplomat::rust_link(icu::collator::Collator::try_new, FnInStruct)]
        pub fn create_v1(
            provider: &ICU4XDataProvider,
            locale: &ICU4XLocale,
            options: ICU4XCollatorOptionsV1,
        ) -> Result<Box<ICU4XCollator>, ICU4XError> {
            let locale = locale.to_datalocale();
            let options = CollatorOptions::from(options);

            Ok(Box::new(ICU4XCollator(call_constructor!(
                Collator::try_new,
                Collator::try_new_with_any_provider,
                Collator::try_new_with_buffer_provider,
                provider,
                &locale,
                options,
            )?)))
        }

        /// Compare potentially ill-formed UTF-8 strings.
        ///
        /// Ill-formed input is compared
        /// as if errors had been replaced with REPLACEMENT CHARACTERs according
        /// to the WHATWG Encoding Standard.
        #[diplomat::rust_link(icu::collator::Collator::compare_utf8, FnInStruct)]
        #[diplomat::attr(dart, disable)]
        pub fn compare(&self, left: &str, right: &str) -> ICU4XOrdering {
            let left = left.as_bytes(); // #2520
            let right = right.as_bytes(); // #2520
            self.0.compare_utf8(left, right).into()
        }

        /// Compare guaranteed well-formed UTF-8 strings.
        ///
        /// Note: In C++, passing ill-formed UTF-8 strings is undefined behavior
        /// (and may be memory-unsafe to do so, too).
        #[diplomat::rust_link(icu::collator::Collator::compare, FnInStruct)]
        #[diplomat::attr(dart, rename = "compare")]
        pub fn compare_valid_utf8(&self, left: &str, right: &str) -> ICU4XOrdering {
            self.0.compare(left, right).into()
        }

        /// Compare potentially ill-formed UTF-16 strings, with unpaired surrogates
        /// compared as REPLACEMENT CHARACTER.
        #[diplomat::rust_link(icu::collator::Collator::compare_utf16, FnInStruct)]
        pub fn compare_utf16(&self, left: &[u16], right: &[u16]) -> ICU4XOrdering {
            self.0.compare_utf16(left, right).into()
        }
    }
}

use icu_collator::{
    AlternateHandling, BackwardSecondLevel, CaseFirst, CaseLevel, CollatorOptions, MaxVariable,
    Numeric, Strength,
};

impl From<ffi::ICU4XCollatorStrength> for Option<Strength> {
    fn from(strength: ffi::ICU4XCollatorStrength) -> Option<Strength> {
        match strength {
            ffi::ICU4XCollatorStrength::Auto => None,
            ffi::ICU4XCollatorStrength::Primary => Some(Strength::Primary),
            ffi::ICU4XCollatorStrength::Secondary => Some(Strength::Secondary),
            ffi::ICU4XCollatorStrength::Tertiary => Some(Strength::Tertiary),
            ffi::ICU4XCollatorStrength::Quaternary => Some(Strength::Quaternary),
            ffi::ICU4XCollatorStrength::Identical => Some(Strength::Identical),
        }
    }
}

impl From<ffi::ICU4XCollatorAlternateHandling> for Option<AlternateHandling> {
    fn from(alternate_handling: ffi::ICU4XCollatorAlternateHandling) -> Option<AlternateHandling> {
        match alternate_handling {
            ffi::ICU4XCollatorAlternateHandling::Auto => None,
            ffi::ICU4XCollatorAlternateHandling::NonIgnorable => {
                Some(AlternateHandling::NonIgnorable)
            }
            ffi::ICU4XCollatorAlternateHandling::Shifted => Some(AlternateHandling::Shifted),
        }
    }
}

impl From<ffi::ICU4XCollatorCaseFirst> for Option<CaseFirst> {
    fn from(case_first: ffi::ICU4XCollatorCaseFirst) -> Option<CaseFirst> {
        match case_first {
            ffi::ICU4XCollatorCaseFirst::Auto => None,
            ffi::ICU4XCollatorCaseFirst::Off => Some(CaseFirst::Off),
            ffi::ICU4XCollatorCaseFirst::LowerFirst => Some(CaseFirst::LowerFirst),
            ffi::ICU4XCollatorCaseFirst::UpperFirst => Some(CaseFirst::UpperFirst),
        }
    }
}

impl From<ffi::ICU4XCollatorMaxVariable> for Option<MaxVariable> {
    fn from(max_variable: ffi::ICU4XCollatorMaxVariable) -> Option<MaxVariable> {
        match max_variable {
            ffi::ICU4XCollatorMaxVariable::Auto => None,
            ffi::ICU4XCollatorMaxVariable::Space => Some(MaxVariable::Space),
            ffi::ICU4XCollatorMaxVariable::Punctuation => Some(MaxVariable::Punctuation),
            ffi::ICU4XCollatorMaxVariable::Symbol => Some(MaxVariable::Symbol),
            ffi::ICU4XCollatorMaxVariable::Currency => Some(MaxVariable::Currency),
        }
    }
}

impl From<ffi::ICU4XCollatorCaseLevel> for Option<CaseLevel> {
    fn from(case_level: ffi::ICU4XCollatorCaseLevel) -> Option<CaseLevel> {
        match case_level {
            ffi::ICU4XCollatorCaseLevel::Auto => None,
            ffi::ICU4XCollatorCaseLevel::Off => Some(CaseLevel::Off),
            ffi::ICU4XCollatorCaseLevel::On => Some(CaseLevel::On),
        }
    }
}

impl From<ffi::ICU4XCollatorNumeric> for Option<Numeric> {
    fn from(numeric: ffi::ICU4XCollatorNumeric) -> Option<Numeric> {
        match numeric {
            ffi::ICU4XCollatorNumeric::Auto => None,
            ffi::ICU4XCollatorNumeric::Off => Some(Numeric::Off),
            ffi::ICU4XCollatorNumeric::On => Some(Numeric::On),
        }
    }
}

impl From<ffi::ICU4XCollatorBackwardSecondLevel> for Option<BackwardSecondLevel> {
    fn from(
        backward_second_level: ffi::ICU4XCollatorBackwardSecondLevel,
    ) -> Option<BackwardSecondLevel> {
        match backward_second_level {
            ffi::ICU4XCollatorBackwardSecondLevel::Auto => None,
            ffi::ICU4XCollatorBackwardSecondLevel::Off => Some(BackwardSecondLevel::Off),
            ffi::ICU4XCollatorBackwardSecondLevel::On => Some(BackwardSecondLevel::On),
        }
    }
}

impl From<ffi::ICU4XCollatorOptionsV1> for CollatorOptions {
    fn from(options: ffi::ICU4XCollatorOptionsV1) -> CollatorOptions {
        let mut result = CollatorOptions::new();
        result.strength = options.strength.into();
        result.alternate_handling = options.alternate_handling.into();
        result.case_first = options.case_first.into();
        result.max_variable = options.max_variable.into();
        result.case_level = options.case_level.into();
        result.numeric = options.numeric.into();
        result.backward_second_level = options.backward_second_level.into();

        result
    }
}
