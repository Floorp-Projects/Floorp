``segmenter_grapheme::ffi``
===========================

.. js:class:: ICU4XGraphemeClusterBreakIteratorLatin1

    See the `Rust documentation for GraphemeClusterBreakIterator <https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterBreakIterator.html>`__ for more information.


    .. js:method:: next()

        Finds the next breakpoint. Returns -1 if at the end of the string or if the index is out of range of a 32-bit signed integer.

        See the `Rust documentation for next <https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterBreakIterator.html#method.next>`__ for more information.


.. js:class:: ICU4XGraphemeClusterBreakIteratorUtf16

    See the `Rust documentation for GraphemeClusterBreakIterator <https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterBreakIterator.html>`__ for more information.


    .. js:method:: next()

        Finds the next breakpoint. Returns -1 if at the end of the string or if the index is out of range of a 32-bit signed integer.

        See the `Rust documentation for next <https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterBreakIterator.html#method.next>`__ for more information.


.. js:class:: ICU4XGraphemeClusterBreakIteratorUtf8

    See the `Rust documentation for GraphemeClusterBreakIterator <https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterBreakIterator.html>`__ for more information.


    .. js:method:: next()

        Finds the next breakpoint. Returns -1 if at the end of the string or if the index is out of range of a 32-bit signed integer.

        See the `Rust documentation for next <https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterBreakIterator.html#method.next>`__ for more information.


.. js:class:: ICU4XGraphemeClusterSegmenter

    An ICU4X grapheme-cluster-break segmenter, capable of finding grapheme cluster breakpoints in strings.

    See the `Rust documentation for GraphemeClusterSegmenter <https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterSegmenter.html>`__ for more information.


    .. js:function:: create(provider)

        Construct an :js:class:`ICU4XGraphemeClusterSegmenter`.

        See the `Rust documentation for new <https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterSegmenter.html#method.new>`__ for more information.


    .. js:method:: segment_utf8(input)

        Segments a (potentially ill-formed) UTF-8 string.

        See the `Rust documentation for segment_utf8 <https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterSegmenter.html#method.segment_utf8>`__ for more information.


    .. js:method:: segment_utf16(input)

        Segments a UTF-16 string.

        See the `Rust documentation for segment_utf16 <https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterSegmenter.html#method.segment_utf16>`__ for more information.

        - Note: ``input`` should be an ArrayBuffer or TypedArray corresponding to the slice type expected by Rust.


    .. js:method:: segment_latin1(input)

        Segments a Latin-1 string.

        See the `Rust documentation for segment_latin1 <https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterSegmenter.html#method.segment_latin1>`__ for more information.

        - Note: ``input`` should be an ArrayBuffer or TypedArray corresponding to the slice type expected by Rust.

