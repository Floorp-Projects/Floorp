``segmenter_sentence::ffi``
===========================

.. js:class:: ICU4XSentenceBreakIteratorLatin1

    See the `Rust documentation for SentenceBreakIterator <https://docs.rs/icu/latest/icu/segmenter/struct.SentenceBreakIterator.html>`__ for more information.


    .. js:method:: next()

        Finds the next breakpoint. Returns -1 if at the end of the string or if the index is out of range of a 32-bit signed integer.

        See the `Rust documentation for next <https://docs.rs/icu/latest/icu/segmenter/struct.SentenceBreakIterator.html#method.next>`__ for more information.


.. js:class:: ICU4XSentenceBreakIteratorUtf16

    See the `Rust documentation for SentenceBreakIterator <https://docs.rs/icu/latest/icu/segmenter/struct.SentenceBreakIterator.html>`__ for more information.


    .. js:method:: next()

        Finds the next breakpoint. Returns -1 if at the end of the string or if the index is out of range of a 32-bit signed integer.

        See the `Rust documentation for next <https://docs.rs/icu/latest/icu/segmenter/struct.SentenceBreakIterator.html#method.next>`__ for more information.


.. js:class:: ICU4XSentenceBreakIteratorUtf8

    See the `Rust documentation for SentenceBreakIterator <https://docs.rs/icu/latest/icu/segmenter/struct.SentenceBreakIterator.html>`__ for more information.


    .. js:method:: next()

        Finds the next breakpoint. Returns -1 if at the end of the string or if the index is out of range of a 32-bit signed integer.

        See the `Rust documentation for next <https://docs.rs/icu/latest/icu/segmenter/struct.SentenceBreakIterator.html#method.next>`__ for more information.


.. js:class:: ICU4XSentenceSegmenter

    An ICU4X sentence-break segmenter, capable of finding sentence breakpoints in strings.

    See the `Rust documentation for SentenceSegmenter <https://docs.rs/icu/latest/icu/segmenter/struct.SentenceSegmenter.html>`__ for more information.


    .. js:function:: create(provider)

        Construct an :js:class:`ICU4XSentenceSegmenter`.

        See the `Rust documentation for new <https://docs.rs/icu/latest/icu/segmenter/struct.SentenceSegmenter.html#method.new>`__ for more information.


    .. js:method:: segment_utf8(input)

        Segments a (potentially ill-formed) UTF-8 string.

        See the `Rust documentation for segment_utf8 <https://docs.rs/icu/latest/icu/segmenter/struct.SentenceSegmenter.html#method.segment_utf8>`__ for more information.


    .. js:method:: segment_utf16(input)

        Segments a UTF-16 string.

        See the `Rust documentation for segment_utf16 <https://docs.rs/icu/latest/icu/segmenter/struct.SentenceSegmenter.html#method.segment_utf16>`__ for more information.

        - Note: ``input`` should be an ArrayBuffer or TypedArray corresponding to the slice type expected by Rust.


    .. js:method:: segment_latin1(input)

        Segments a Latin-1 string.

        See the `Rust documentation for segment_latin1 <https://docs.rs/icu/latest/icu/segmenter/struct.SentenceSegmenter.html#method.segment_latin1>`__ for more information.

        - Note: ``input`` should be an ArrayBuffer or TypedArray corresponding to the slice type expected by Rust.

