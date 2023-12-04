``bidi::ffi``
=============

.. js:class:: ICU4XBidi

    An ICU4X Bidi object, containing loaded bidi data

    See the `Rust documentation for BidiClassAdapter <https://docs.rs/icu/latest/icu/properties/bidi/struct.BidiClassAdapter.html>`__ for more information.


    .. js:function:: create(provider)

        Creates a new :js:class:`ICU4XBidi` from locale data.

        See the `Rust documentation for new <https://docs.rs/icu/latest/icu/properties/bidi/struct.BidiClassAdapter.html#method.new>`__ for more information.


    .. js:method:: for_text(text, default_level)

        Use the data loaded in this object to process a string and calculate bidi information

        Takes in a Level for the default level, if it is an invalid value it will default to LTR

        See the `Rust documentation for new_with_data_source <https://docs.rs/unicode_bidi/latest/unicode_bidi/struct.BidiInfo.html#method.new_with_data_source>`__ for more information.


    .. js:method:: reorder_visual(levels)

        Utility function for producing reorderings given a list of levels

        Produces a map saying which visual index maps to which source index.

        The levels array must not have values greater than 126 (this is the Bidi maximum explicit depth plus one). Failure to follow this invariant may lead to incorrect results, but is still safe.

        See the `Rust documentation for reorder_visual <https://docs.rs/unicode_bidi/latest/unicode_bidi/struct.BidiInfo.html#method.reorder_visual>`__ for more information.

        - Note: ``levels`` should be an ArrayBuffer or TypedArray corresponding to the slice type expected by Rust.


    .. js:function:: level_is_rtl(level)

        Check if a Level returned by level_at is an RTL level.

        Invalid levels (numbers greater than 125) will be assumed LTR

        See the `Rust documentation for is_rtl <https://docs.rs/unicode_bidi/latest/unicode_bidi/struct.Level.html#method.is_rtl>`__ for more information.


    .. js:function:: level_is_ltr(level)

        Check if a Level returned by level_at is an LTR level.

        Invalid levels (numbers greater than 125) will be assumed LTR

        See the `Rust documentation for is_ltr <https://docs.rs/unicode_bidi/latest/unicode_bidi/struct.Level.html#method.is_ltr>`__ for more information.


    .. js:function:: level_rtl()

        Get a basic RTL Level value

        See the `Rust documentation for rtl <https://docs.rs/unicode_bidi/latest/unicode_bidi/struct.Level.html#method.rtl>`__ for more information.


    .. js:function:: level_ltr()

        Get a simple LTR Level value

        See the `Rust documentation for ltr <https://docs.rs/unicode_bidi/latest/unicode_bidi/struct.Level.html#method.ltr>`__ for more information.


.. js:class:: ICU4XBidiDirection

.. js:class:: ICU4XBidiInfo

    An object containing bidi information for a given string, produced by ``for_text()`` on ``ICU4XBidi``

    See the `Rust documentation for BidiInfo <https://docs.rs/unicode_bidi/latest/unicode_bidi/struct.BidiInfo.html>`__ for more information.


    .. js:method:: paragraph_count()

        The number of paragraphs contained here


    .. js:method:: paragraph_at(n)

        Get the nth paragraph, returning ``None`` if out of bounds


    .. js:method:: size()

        The number of bytes in this full text


    .. js:method:: level_at(pos)

        Get the BIDI level at a particular byte index in the full text. This integer is conceptually a ``unicode_bidi::Level``, and can be further inspected using the static methods on ICU4XBidi.

        Returns 0 (equivalent to ``Level::ltr()``) on error


.. js:class:: ICU4XBidiParagraph

    Bidi information for a single processed paragraph


    .. js:method:: set_paragraph_in_text(n)

        Given a paragraph index ``n`` within the surrounding text, this sets this object to the paragraph at that index. Returns ``ICU4XError::OutOfBoundsError`` when out of bounds.

        This is equivalent to calling ``paragraph_at()`` on ``ICU4XBidiInfo`` but doesn't create a new object


    .. js:method:: direction()

        The primary direction of this paragraph

        See the `Rust documentation for level_at <https://docs.rs/unicode_bidi/latest/unicode_bidi/struct.Paragraph.html#method.level_at>`__ for more information.


    .. js:method:: size()

        The number of bytes in this paragraph

        See the `Rust documentation for len <https://docs.rs/unicode_bidi/latest/unicode_bidi/struct.ParagraphInfo.html#method.len>`__ for more information.


    .. js:method:: range_start()

        The start index of this paragraph within the source text


    .. js:method:: range_end()

        The end index of this paragraph within the source text


    .. js:method:: reorder_line(range_start, range_end)

        Reorder a line based on display order. The ranges are specified relative to the source text and must be contained within this paragraph's range.

        See the `Rust documentation for level_at <https://docs.rs/unicode_bidi/latest/unicode_bidi/struct.Paragraph.html#method.level_at>`__ for more information.


    .. js:method:: level_at(pos)

        Get the BIDI level at a particular byte index in this paragraph. This integer is conceptually a ``unicode_bidi::Level``, and can be further inspected using the static methods on ICU4XBidi.

        Returns 0 (equivalent to ``Level::ltr()``) on error

        See the `Rust documentation for level_at <https://docs.rs/unicode_bidi/latest/unicode_bidi/struct.Paragraph.html#method.level_at>`__ for more information.


.. js:class:: ICU4XReorderedIndexMap

    Thin wrapper around a vector that maps visual indices to source indices

    ``map[visualIndex] = sourceIndex``

    Produced by ``reorder_visual()`` on :js:class:`ICU4XBidi`.


    .. js:method:: as_slice()

        Get this as a slice/array of indices


    .. js:method:: len()

        The length of this map


    .. js:method:: get(index)

        Get element at ``index``. Returns 0 when out of bounds (note that 0 is also a valid in-bounds value, please use ``len()`` to avoid out-of-bounds)

