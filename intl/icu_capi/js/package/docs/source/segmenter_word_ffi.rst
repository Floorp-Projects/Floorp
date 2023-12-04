``segmenter_word::ffi``
=======================

.. js:class:: ICU4XSegmenterWordType

    See the `Rust documentation for WordType <https://docs.rs/icu/latest/icu/segmenter/enum.WordType.html>`__ for more information.


.. js:class:: ICU4XWordBreakIteratorLatin1

    See the `Rust documentation for WordBreakIterator <https://docs.rs/icu/latest/icu/segmenter/struct.WordBreakIterator.html>`__ for more information.


    .. js:method:: next()

        Finds the next breakpoint. Returns -1 if at the end of the string or if the index is out of range of a 32-bit signed integer.

        See the `Rust documentation for next <https://docs.rs/icu/latest/icu/segmenter/struct.WordBreakIterator.html#method.next>`__ for more information.


    .. js:method:: word_type()

        Return the status value of break boundary.

        See the `Rust documentation for word_type <https://docs.rs/icu/latest/icu/segmenter/struct.WordBreakIterator.html#method.word_type>`__ for more information.


    .. js:method:: is_word_like()

        Return true when break boundary is word-like such as letter/number/CJK

        See the `Rust documentation for is_word_like <https://docs.rs/icu/latest/icu/segmenter/struct.WordBreakIterator.html#method.is_word_like>`__ for more information.


.. js:class:: ICU4XWordBreakIteratorUtf16

    See the `Rust documentation for WordBreakIterator <https://docs.rs/icu/latest/icu/segmenter/struct.WordBreakIterator.html>`__ for more information.


    .. js:method:: next()

        Finds the next breakpoint. Returns -1 if at the end of the string or if the index is out of range of a 32-bit signed integer.

        See the `Rust documentation for next <https://docs.rs/icu/latest/icu/segmenter/struct.WordBreakIterator.html#method.next>`__ for more information.


    .. js:method:: word_type()

        Return the status value of break boundary.

        See the `Rust documentation for word_type <https://docs.rs/icu/latest/icu/segmenter/struct.WordBreakIterator.html#method.word_type>`__ for more information.


    .. js:method:: is_word_like()

        Return true when break boundary is word-like such as letter/number/CJK

        See the `Rust documentation for is_word_like <https://docs.rs/icu/latest/icu/segmenter/struct.WordBreakIterator.html#method.is_word_like>`__ for more information.


.. js:class:: ICU4XWordBreakIteratorUtf8

    See the `Rust documentation for WordBreakIterator <https://docs.rs/icu/latest/icu/segmenter/struct.WordBreakIterator.html>`__ for more information.


    .. js:method:: next()

        Finds the next breakpoint. Returns -1 if at the end of the string or if the index is out of range of a 32-bit signed integer.

        See the `Rust documentation for next <https://docs.rs/icu/latest/icu/segmenter/struct.WordBreakIterator.html#method.next>`__ for more information.


    .. js:method:: word_type()

        Return the status value of break boundary.

        See the `Rust documentation for word_type <https://docs.rs/icu/latest/icu/segmenter/struct.WordBreakIterator.html#method.word_type>`__ for more information.


    .. js:method:: is_word_like()

        Return true when break boundary is word-like such as letter/number/CJK

        See the `Rust documentation for is_word_like <https://docs.rs/icu/latest/icu/segmenter/struct.WordBreakIterator.html#method.is_word_like>`__ for more information.


.. js:class:: ICU4XWordSegmenter

    An ICU4X word-break segmenter, capable of finding word breakpoints in strings.

    See the `Rust documentation for WordSegmenter <https://docs.rs/icu/latest/icu/segmenter/struct.WordSegmenter.html>`__ for more information.


    .. js:function:: create_auto(provider)

        Construct an :js:class:`ICU4XWordSegmenter` with automatically selecting the best available LSTM or dictionary payload data.

        Note: currently, it uses dictionary for Chinese and Japanese, and LSTM for Burmese, Khmer, Lao, and Thai.

        See the `Rust documentation for new_auto <https://docs.rs/icu/latest/icu/segmenter/struct.WordSegmenter.html#method.new_auto>`__ for more information.


    .. js:function:: create_lstm(provider)

        Construct an :js:class:`ICU4XWordSegmenter` with LSTM payload data for Burmese, Khmer, Lao, and Thai.

        Warning: :js:class:`ICU4XWordSegmenter` created by this function doesn't handle Chinese or Japanese.

        See the `Rust documentation for new_lstm <https://docs.rs/icu/latest/icu/segmenter/struct.WordSegmenter.html#method.new_lstm>`__ for more information.


    .. js:function:: create_dictionary(provider)

        Construct an :js:class:`ICU4XWordSegmenter` with dictionary payload data for Chinese, Japanese, Burmese, Khmer, Lao, and Thai.

        See the `Rust documentation for new_dictionary <https://docs.rs/icu/latest/icu/segmenter/struct.WordSegmenter.html#method.new_dictionary>`__ for more information.


    .. js:method:: segment_utf8(input)

        Segments a (potentially ill-formed) UTF-8 string.

        See the `Rust documentation for segment_utf8 <https://docs.rs/icu/latest/icu/segmenter/struct.WordSegmenter.html#method.segment_utf8>`__ for more information.


    .. js:method:: segment_utf16(input)

        Segments a UTF-16 string.

        See the `Rust documentation for segment_utf16 <https://docs.rs/icu/latest/icu/segmenter/struct.WordSegmenter.html#method.segment_utf16>`__ for more information.

        - Note: ``input`` should be an ArrayBuffer or TypedArray corresponding to the slice type expected by Rust.


    .. js:method:: segment_latin1(input)

        Segments a Latin-1 string.

        See the `Rust documentation for segment_latin1 <https://docs.rs/icu/latest/icu/segmenter/struct.WordSegmenter.html#method.segment_latin1>`__ for more information.

        - Note: ``input`` should be an ArrayBuffer or TypedArray corresponding to the slice type expected by Rust.

