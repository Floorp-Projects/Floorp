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
        GraphemeClusterBreakIteratorLatin1, GraphemeClusterBreakIteratorPotentiallyIllFormedUtf8,
        GraphemeClusterBreakIteratorUtf16, GraphemeClusterSegmenter,
    };

    #[diplomat::opaque]
    /// An ICU4X grapheme-cluster-break segmenter, capable of finding grapheme cluster breakpoints
    /// in strings.
    #[diplomat::rust_link(icu::segmenter::GraphemeClusterSegmenter, Struct)]
    pub struct ICU4XGraphemeClusterSegmenter(GraphemeClusterSegmenter);

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::segmenter::GraphemeClusterBreakIterator, Struct)]
    #[diplomat::rust_link(
        icu::segmenter::GraphemeClusterBreakIteratorPotentiallyIllFormedUtf8,
        Typedef,
        hidden
    )]
    pub struct ICU4XGraphemeClusterBreakIteratorUtf8<'a>(
        GraphemeClusterBreakIteratorPotentiallyIllFormedUtf8<'a, 'a>,
    );

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::segmenter::GraphemeClusterBreakIterator, Struct)]
    #[diplomat::rust_link(icu::segmenter::GraphemeClusterBreakIteratorUtf16, Typedef, hidden)]
    pub struct ICU4XGraphemeClusterBreakIteratorUtf16<'a>(
        GraphemeClusterBreakIteratorUtf16<'a, 'a>,
    );

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::segmenter::GraphemeClusterBreakIterator, Struct)]
    #[diplomat::rust_link(icu::segmenter::GraphemeClusterBreakIteratorLatin1, Typedef, hidden)]
    pub struct ICU4XGraphemeClusterBreakIteratorLatin1<'a>(
        GraphemeClusterBreakIteratorLatin1<'a, 'a>,
    );

    impl ICU4XGraphemeClusterSegmenter {
        /// Construct an [`ICU4XGraphemeClusterSegmenter`].
        #[diplomat::rust_link(icu::segmenter::GraphemeClusterSegmenter::new, FnInStruct)]
        pub fn create(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XGraphemeClusterSegmenter>, ICU4XError> {
            Ok(Box::new(ICU4XGraphemeClusterSegmenter(call_constructor!(
                GraphemeClusterSegmenter::new [r => Ok(r)],
                GraphemeClusterSegmenter::try_new_with_any_provider,
                GraphemeClusterSegmenter::try_new_with_buffer_provider,
                provider,
            )?)))
        }

        /// Segments a (potentially ill-formed) UTF-8 string.
        #[diplomat::rust_link(
            icu::segmenter::GraphemeClusterSegmenter::segment_str,
            FnInStruct,
            hidden
        )]
        #[diplomat::rust_link(icu::segmenter::GraphemeClusterSegmenter::segment_utf8, FnInStruct)]
        pub fn segment_utf8<'a>(
            &'a self,
            input: &'a str,
        ) -> Box<ICU4XGraphemeClusterBreakIteratorUtf8<'a>> {
            let input = input.as_bytes(); // #2520
            Box::new(ICU4XGraphemeClusterBreakIteratorUtf8(
                self.0.segment_utf8(input),
            ))
        }

        /// Segments a UTF-16 string.
        #[diplomat::rust_link(icu::segmenter::GraphemeClusterSegmenter::segment_utf16, FnInStruct)]
        pub fn segment_utf16<'a>(
            &'a self,
            input: &'a [u16],
        ) -> Box<ICU4XGraphemeClusterBreakIteratorUtf16<'a>> {
            Box::new(ICU4XGraphemeClusterBreakIteratorUtf16(
                self.0.segment_utf16(input),
            ))
        }

        /// Segments a Latin-1 string.
        #[diplomat::rust_link(icu::segmenter::GraphemeClusterSegmenter::segment_latin1, FnInStruct)]
        pub fn segment_latin1<'a>(
            &'a self,
            input: &'a [u8],
        ) -> Box<ICU4XGraphemeClusterBreakIteratorLatin1<'a>> {
            Box::new(ICU4XGraphemeClusterBreakIteratorLatin1(
                self.0.segment_latin1(input),
            ))
        }
    }

    impl<'a> ICU4XGraphemeClusterBreakIteratorUtf8<'a> {
        /// Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
        /// out of range of a 32-bit signed integer.
        #[allow(clippy::should_implement_trait)]
        #[diplomat::rust_link(icu::segmenter::GraphemeClusterBreakIterator::next, FnInStruct)]
        #[diplomat::rust_link(
            icu::segmenter::GraphemeClusterBreakIterator::Item,
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

    impl<'a> ICU4XGraphemeClusterBreakIteratorUtf16<'a> {
        /// Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
        /// out of range of a 32-bit signed integer.
        #[allow(clippy::should_implement_trait)]
        #[diplomat::rust_link(icu::segmenter::GraphemeClusterBreakIterator::next, FnInStruct)]
        #[diplomat::rust_link(
            icu::segmenter::GraphemeClusterBreakIterator::Item,
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

    impl<'a> ICU4XGraphemeClusterBreakIteratorLatin1<'a> {
        /// Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
        /// out of range of a 32-bit signed integer.
        #[allow(clippy::should_implement_trait)]
        #[diplomat::rust_link(icu::segmenter::GraphemeClusterBreakIterator::next, FnInStruct)]
        #[diplomat::rust_link(
            icu::segmenter::GraphemeClusterBreakIterator::Item,
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
