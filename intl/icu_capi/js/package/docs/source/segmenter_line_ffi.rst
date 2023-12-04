``segmenter_line::ffi``
=======================

.. js:class:: ICU4XLineBreakIteratorLatin1

    See the `Rust documentation for LineBreakIterator <https://docs.rs/icu/latest/icu/segmenter/struct.LineBreakIterator.html>`__ for more information.

    Additional information: `1 <https://docs.rs/icu/latest/icu/segmenter/type.LineBreakIteratorLatin1.html>`__


    .. js:method:: next()

        Finds the next breakpoint. Returns -1 if at the end of the string or if the index is out of range of a 32-bit signed integer.

        See the `Rust documentation for next <https://docs.rs/icu/latest/icu/segmenter/struct.LineBreakIterator.html#method.next>`__ for more information.


.. js:class:: ICU4XLineBreakIteratorUtf16

    See the `Rust documentation for LineBreakIterator <https://docs.rs/icu/latest/icu/segmenter/struct.LineBreakIterator.html>`__ for more information.

    Additional information: `1 <https://docs.rs/icu/latest/icu/segmenter/type.LineBreakIteratorUtf16.html>`__


    .. js:method:: next()

        Finds the next breakpoint. Returns -1 if at the end of the string or if the index is out of range of a 32-bit signed integer.

        See the `Rust documentation for next <https://docs.rs/icu/latest/icu/segmenter/struct.LineBreakIterator.html#method.next>`__ for more information.


.. js:class:: ICU4XLineBreakIteratorUtf8

    See the `Rust documentation for LineBreakIterator <https://docs.rs/icu/latest/icu/segmenter/struct.LineBreakIterator.html>`__ for more information.

    Additional information: `1 <https://docs.rs/icu/latest/icu/segmenter/type.LineBreakIteratorPotentiallyIllFormedUtf8.html>`__


    .. js:method:: next()

        Finds the next breakpoint. Returns -1 if at the end of the string or if the index is out of range of a 32-bit signed integer.

        See the `Rust documentation for next <https://docs.rs/icu/latest/icu/segmenter/struct.LineBreakIterator.html#method.next>`__ for more information.


.. js:class:: ICU4XLineBreakOptionsV1

    See the `Rust documentation for LineBreakOptions <https://docs.rs/icu/latest/icu/segmenter/struct.LineBreakOptions.html>`__ for more information.


    .. js:attribute:: strictness

    .. js:attribute:: word_option

    .. js:attribute:: ja_zh

.. js:class:: ICU4XLineBreakStrictness

    See the `Rust documentation for LineBreakStrictness <https://docs.rs/icu/latest/icu/segmenter/enum.LineBreakStrictness.html>`__ for more information.


.. js:class:: ICU4XLineBreakWordOption

    See the `Rust documentation for LineBreakWordOption <https://docs.rs/icu/latest/icu/segmenter/enum.LineBreakWordOption.html>`__ for more information.


.. js:class:: ICU4XLineSegmenter

    An ICU4X line-break segmenter, capable of finding breakpoints in strings.

    See the `Rust documentation for LineSegmenter <https://docs.rs/icu/latest/icu/segmenter/struct.LineSegmenter.html>`__ for more information.


    .. js:function:: create_auto(provider)

        Construct a :js:class:`ICU4XLineSegmenter` with default options. It automatically loads the best available payload data for Burmese, Khmer, Lao, and Thai.

        See the `Rust documentation for new_auto <https://docs.rs/icu/latest/icu/segmenter/struct.LineSegmenter.html#method.new_auto>`__ for more information.


    .. js:function:: create_lstm(provider)

        Construct a :js:class:`ICU4XLineSegmenter` with default options and LSTM payload data for Burmese, Khmer, Lao, and Thai.

        See the `Rust documentation for new_lstm <https://docs.rs/icu/latest/icu/segmenter/struct.LineSegmenter.html#method.new_lstm>`__ for more information.


    .. js:function:: create_dictionary(provider)

        Construct a :js:class:`ICU4XLineSegmenter` with default options and dictionary payload data for Burmese, Khmer, Lao, and Thai..

        See the `Rust documentation for new_dictionary <https://docs.rs/icu/latest/icu/segmenter/struct.LineSegmenter.html#method.new_dictionary>`__ for more information.


    .. js:function:: create_auto_with_options_v1(provider, options)

        Construct a :js:class:`ICU4XLineSegmenter` with custom options. It automatically loads the best available payload data for Burmese, Khmer, Lao, and Thai.

        See the `Rust documentation for new_auto_with_options <https://docs.rs/icu/latest/icu/segmenter/struct.LineSegmenter.html#method.new_auto_with_options>`__ for more information.


    .. js:function:: create_lstm_with_options_v1(provider, options)

        Construct a :js:class:`ICU4XLineSegmenter` with custom options and LSTM payload data for Burmese, Khmer, Lao, and Thai.

        See the `Rust documentation for new_lstm_with_options <https://docs.rs/icu/latest/icu/segmenter/struct.LineSegmenter.html#method.new_lstm_with_options>`__ for more information.


    .. js:function:: create_dictionary_with_options_v1(provider, options)

        Construct a :js:class:`ICU4XLineSegmenter` with custom options and dictionary payload data for Burmese, Khmer, Lao, and Thai.

        See the `Rust documentation for new_dictionary_with_options <https://docs.rs/icu/latest/icu/segmenter/struct.LineSegmenter.html#method.new_dictionary_with_options>`__ for more information.


    .. js:method:: segment_utf8(input)

        Segments a (potentially ill-formed) UTF-8 string.

        See the `Rust documentation for segment_utf8 <https://docs.rs/icu/latest/icu/segmenter/struct.LineSegmenter.html#method.segment_utf8>`__ for more information.


    .. js:method:: segment_utf16(input)

        Segments a UTF-16 string.

        See the `Rust documentation for segment_utf16 <https://docs.rs/icu/latest/icu/segmenter/struct.LineSegmenter.html#method.segment_utf16>`__ for more information.

        - Note: ``input`` should be an ArrayBuffer or TypedArray corresponding to the slice type expected by Rust.


    .. js:method:: segment_latin1(input)

        Segments a Latin-1 string.

        See the `Rust documentation for segment_latin1 <https://docs.rs/icu/latest/icu/segmenter/struct.LineSegmenter.html#method.segment_latin1>`__ for more information.

        - Note: ``input`` should be an ArrayBuffer or TypedArray corresponding to the slice type expected by Rust.

