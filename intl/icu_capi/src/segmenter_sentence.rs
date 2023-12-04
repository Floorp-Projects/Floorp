// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
pub mod ffi {
    use crate::errors::ffi::ICU4XError;
    use crate::provider::ffi::ICU4XDataProvider;
    use alloc::boxed::Box;
    use core::convert::TryFrom;
    use icu_segmenter::{
        SentenceBreakIteratorLatin1, SentenceBreakIteratorPotentiallyIllFormedUtf8,
        SentenceBreakIteratorUtf16, SentenceSegmenter,
    };

    #[diplomat::opaque]
    /// An ICU4X sentence-break segmenter, capable of finding sentence breakpoints in strings.
    #[diplomat::rust_link(icu::segmenter::SentenceSegmenter, Struct)]
    pub struct ICU4XSentenceSegmenter(SentenceSegmenter);

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::segmenter::SentenceBreakIterator, Struct)]
    #[diplomat::rust_link(
        icu::segmenter::SentenceBreakIteratorPotentiallyIllFormedUtf8,
        Typedef,
        hidden
    )]
    pub struct ICU4XSentenceBreakIteratorUtf8<'a>(
        SentenceBreakIteratorPotentiallyIllFormedUtf8<'a, 'a>,
    );

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::segmenter::SentenceBreakIterator, Struct)]
    #[diplomat::rust_link(icu::segmenter::SentenceBreakIteratorUtf16, Typedef, hidden)]
    pub struct ICU4XSentenceBreakIteratorUtf16<'a>(SentenceBreakIteratorUtf16<'a, 'a>);

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::segmenter::SentenceBreakIterator, Struct)]
    #[diplomat::rust_link(icu::segmenter::SentenceBreakIteratorLatin1, Typedef, hidden)]
    pub struct ICU4XSentenceBreakIteratorLatin1<'a>(SentenceBreakIteratorLatin1<'a, 'a>);

    impl ICU4XSentenceSegmenter {
        /// Construct an [`ICU4XSentenceSegmenter`].
        #[diplomat::rust_link(icu::segmenter::SentenceSegmenter::new, FnInStruct)]
        pub fn create(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XSentenceSegmenter>, ICU4XError> {
            Ok(Box::new(ICU4XSentenceSegmenter(call_constructor!(
                SentenceSegmenter::new [r => Ok(r)],
                SentenceSegmenter::try_new_with_any_provider,
                SentenceSegmenter::try_new_with_buffer_provider,
                provider,
            )?)))
        }

        /// Segments a (potentially ill-formed) UTF-8 string.
        #[diplomat::rust_link(icu::segmenter::SentenceSegmenter::segment_utf8, FnInStruct)]
        #[diplomat::rust_link(icu::segmenter::SentenceSegmenter::segment_str, FnInStruct, hidden)]
        pub fn segment_utf8<'a>(
            &'a self,
            input: &'a str,
        ) -> Box<ICU4XSentenceBreakIteratorUtf8<'a>> {
            let input = input.as_bytes(); // #2520
            Box::new(ICU4XSentenceBreakIteratorUtf8(self.0.segment_utf8(input)))
        }

        /// Segments a UTF-16 string.
        #[diplomat::rust_link(icu::segmenter::SentenceSegmenter::segment_utf16, FnInStruct)]
        pub fn segment_utf16<'a>(
            &'a self,
            input: &'a [u16],
        ) -> Box<ICU4XSentenceBreakIteratorUtf16<'a>> {
            Box::new(ICU4XSentenceBreakIteratorUtf16(self.0.segment_utf16(input)))
        }

        /// Segments a Latin-1 string.
        #[diplomat::rust_link(icu::segmenter::SentenceSegmenter::segment_latin1, FnInStruct)]
        pub fn segment_latin1<'a>(
            &'a self,
            input: &'a [u8],
        ) -> Box<ICU4XSentenceBreakIteratorLatin1<'a>> {
            Box::new(ICU4XSentenceBreakIteratorLatin1(
                self.0.segment_latin1(input),
            ))
        }
    }

    impl<'a> ICU4XSentenceBreakIteratorUtf8<'a> {
        /// Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
        /// out of range of a 32-bit signed integer.
        #[allow(clippy::should_implement_trait)]
        #[diplomat::rust_link(icu::segmenter::SentenceBreakIterator::next, FnInStruct)]
        #[diplomat::rust_link(
            icu::segmenter::SentenceBreakIterator::Item,
            AssociatedTypeInStruct,
            hidden
        )]
        pub fn next(&mut self) -> i32 {
            self.0
                .next()
                .and_then(|u| i32::try_from(u).ok())
                .unwrap_or(-1)
        }
    }

    impl<'a> ICU4XSentenceBreakIteratorUtf16<'a> {
        /// Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
        /// out of range of a 32-bit signed integer.
        #[allow(clippy::should_implement_trait)]
        #[diplomat::rust_link(icu::segmenter::SentenceBreakIterator::next, FnInStruct)]
        #[diplomat::rust_link(
            icu::segmenter::SentenceBreakIterator::Item,
            AssociatedTypeInStruct,
            hidden
        )]
        pub fn next(&mut self) -> i32 {
            self.0
                .next()
                .and_then(|u| i32::try_from(u).ok())
                .unwrap_or(-1)
        }
    }

    impl<'a> ICU4XSentenceBreakIteratorLatin1<'a> {
        /// Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
        /// out of range of a 32-bit signed integer.
        #[allow(clippy::should_implement_trait)]
        #[diplomat::rust_link(icu::segmenter::SentenceBreakIterator::next, FnInStruct)]
        #[diplomat::rust_link(
            icu::segmenter::SentenceBreakIterator::Item,
            AssociatedTypeInStruct,
            hidden
        )]
        pub fn next(&mut self) -> i32 {
            self.0
                .next()
                .and_then(|u| i32::try_from(u).ok())
                .unwrap_or(-1)
        }
    }
}
