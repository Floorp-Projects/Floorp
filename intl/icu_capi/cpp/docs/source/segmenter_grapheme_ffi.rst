``segmenter_grapheme::ffi``
===========================

.. cpp:class:: ICU4XGraphemeClusterBreakIteratorLatin1

    See the `Rust documentation for GraphemeClusterBreakIterator <https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterBreakIterator.html>`__ for more information.


    .. cpp:function:: int32_t next()

        Finds the next breakpoint. Returns -1 if at the end of the string or if the index is out of range of a 32-bit signed integer.

        See the `Rust documentation for next <https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterBreakIterator.html#method.next>`__ for more information.


.. cpp:class:: ICU4XGraphemeClusterBreakIteratorUtf16

    See the `Rust documentation for GraphemeClusterBreakIterator <https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterBreakIterator.html>`__ for more information.


    .. cpp:function:: int32_t next()

        Finds the next breakpoint. Returns -1 if at the end of the string or if the index is out of range of a 32-bit signed integer.

        See the `Rust documentation for next <https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterBreakIterator.html#method.next>`__ for more information.


.. cpp:class:: ICU4XGraphemeClusterBreakIteratorUtf8

    See the `Rust documentation for GraphemeClusterBreakIterator <https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterBreakIterator.html>`__ for more information.


    .. cpp:function:: int32_t next()

        Finds the next breakpoint. Returns -1 if at the end of the string or if the index is out of range of a 32-bit signed integer.

        See the `Rust documentation for next <https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterBreakIterator.html#method.next>`__ for more information.


.. cpp:class:: ICU4XGraphemeClusterSegmenter

    An ICU4X grapheme-cluster-break segmenter, capable of finding grapheme cluster breakpoints in strings.

    See the `Rust documentation for GraphemeClusterSegmenter <https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterSegmenter.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XGraphemeClusterSegmenter, ICU4XError> create(const ICU4XDataProvider& provider)

        Construct an :cpp:class:`ICU4XGraphemeClusterSegmenter`.

        See the `Rust documentation for new <https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterSegmenter.html#method.new>`__ for more information.


    .. cpp:function:: ICU4XGraphemeClusterBreakIteratorUtf8 segment_utf8(const std::string_view input) const

        Segments a (potentially ill-formed) UTF-8 string.

        See the `Rust documentation for segment_utf8 <https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterSegmenter.html#method.segment_utf8>`__ for more information.

        Lifetimes: ``this``, ``input`` must live at least as long as the output.


    .. cpp:function:: ICU4XGraphemeClusterBreakIteratorUtf16 segment_utf16(const diplomat::span<const uint16_t> input) const

        Segments a UTF-16 string.

        See the `Rust documentation for segment_utf16 <https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterSegmenter.html#method.segment_utf16>`__ for more information.

        Lifetimes: ``this``, ``input`` must live at least as long as the output.


    .. cpp:function:: ICU4XGraphemeClusterBreakIteratorLatin1 segment_latin1(const diplomat::span<const uint8_t> input) const

        Segments a Latin-1 string.

        See the `Rust documentation for segment_latin1 <https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterSegmenter.html#method.segment_latin1>`__ for more information.

        Lifetimes: ``this``, ``input`` must live at least as long as the output.

