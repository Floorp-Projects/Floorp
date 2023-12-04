``segmenter_sentence::ffi``
===========================

.. cpp:class:: ICU4XSentenceBreakIteratorLatin1

    See the `Rust documentation for SentenceBreakIterator <https://docs.rs/icu/latest/icu/segmenter/struct.SentenceBreakIterator.html>`__ for more information.


    .. cpp:function:: int32_t next()

        Finds the next breakpoint. Returns -1 if at the end of the string or if the index is out of range of a 32-bit signed integer.

        See the `Rust documentation for next <https://docs.rs/icu/latest/icu/segmenter/struct.SentenceBreakIterator.html#method.next>`__ for more information.


.. cpp:class:: ICU4XSentenceBreakIteratorUtf16

    See the `Rust documentation for SentenceBreakIterator <https://docs.rs/icu/latest/icu/segmenter/struct.SentenceBreakIterator.html>`__ for more information.


    .. cpp:function:: int32_t next()

        Finds the next breakpoint. Returns -1 if at the end of the string or if the index is out of range of a 32-bit signed integer.

        See the `Rust documentation for next <https://docs.rs/icu/latest/icu/segmenter/struct.SentenceBreakIterator.html#method.next>`__ for more information.


.. cpp:class:: ICU4XSentenceBreakIteratorUtf8

    See the `Rust documentation for SentenceBreakIterator <https://docs.rs/icu/latest/icu/segmenter/struct.SentenceBreakIterator.html>`__ for more information.


    .. cpp:function:: int32_t next()

        Finds the next breakpoint. Returns -1 if at the end of the string or if the index is out of range of a 32-bit signed integer.

        See the `Rust documentation for next <https://docs.rs/icu/latest/icu/segmenter/struct.SentenceBreakIterator.html#method.next>`__ for more information.


.. cpp:class:: ICU4XSentenceSegmenter

    An ICU4X sentence-break segmenter, capable of finding sentence breakpoints in strings.

    See the `Rust documentation for SentenceSegmenter <https://docs.rs/icu/latest/icu/segmenter/struct.SentenceSegmenter.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XSentenceSegmenter, ICU4XError> create(const ICU4XDataProvider& provider)

        Construct an :cpp:class:`ICU4XSentenceSegmenter`.

        See the `Rust documentation for new <https://docs.rs/icu/latest/icu/segmenter/struct.SentenceSegmenter.html#method.new>`__ for more information.


    .. cpp:function:: ICU4XSentenceBreakIteratorUtf8 segment_utf8(const std::string_view input) const

        Segments a (potentially ill-formed) UTF-8 string.

        See the `Rust documentation for segment_utf8 <https://docs.rs/icu/latest/icu/segmenter/struct.SentenceSegmenter.html#method.segment_utf8>`__ for more information.

        Lifetimes: ``this``, ``input`` must live at least as long as the output.


    .. cpp:function:: ICU4XSentenceBreakIteratorUtf16 segment_utf16(const diplomat::span<const uint16_t> input) const

        Segments a UTF-16 string.

        See the `Rust documentation for segment_utf16 <https://docs.rs/icu/latest/icu/segmenter/struct.SentenceSegmenter.html#method.segment_utf16>`__ for more information.

        Lifetimes: ``this``, ``input`` must live at least as long as the output.


    .. cpp:function:: ICU4XSentenceBreakIteratorLatin1 segment_latin1(const diplomat::span<const uint8_t> input) const

        Segments a Latin-1 string.

        See the `Rust documentation for segment_latin1 <https://docs.rs/icu/latest/icu/segmenter/struct.SentenceSegmenter.html#method.segment_latin1>`__ for more information.

        Lifetimes: ``this``, ``input`` must live at least as long as the output.

