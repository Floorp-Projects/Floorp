``bidi::ffi``
=============

.. cpp:class:: ICU4XBidi

    An ICU4X Bidi object, containing loaded bidi data

    See the `Rust documentation for BidiClassAdapter <https://docs.rs/icu/latest/icu/properties/bidi/struct.BidiClassAdapter.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XBidi, ICU4XError> create(const ICU4XDataProvider& provider)

        Creates a new :cpp:class:`ICU4XBidi` from locale data.

        See the `Rust documentation for new <https://docs.rs/icu/latest/icu/properties/bidi/struct.BidiClassAdapter.html#method.new>`__ for more information.


    .. cpp:function:: ICU4XBidiInfo for_text(const std::string_view text, uint8_t default_level) const

        Use the data loaded in this object to process a string and calculate bidi information

        Takes in a Level for the default level, if it is an invalid value it will default to LTR

        See the `Rust documentation for new_with_data_source <https://docs.rs/unicode_bidi/latest/unicode_bidi/struct.BidiInfo.html#method.new_with_data_source>`__ for more information.

        Lifetimes: ``text`` must live at least as long as the output.


    .. cpp:function:: ICU4XReorderedIndexMap reorder_visual(const diplomat::span<const uint8_t> levels) const

        Utility function for producing reorderings given a list of levels

        Produces a map saying which visual index maps to which source index.

        The levels array must not have values greater than 126 (this is the Bidi maximum explicit depth plus one). Failure to follow this invariant may lead to incorrect results, but is still safe.

        See the `Rust documentation for reorder_visual <https://docs.rs/unicode_bidi/latest/unicode_bidi/struct.BidiInfo.html#method.reorder_visual>`__ for more information.


    .. cpp:function:: static bool level_is_rtl(uint8_t level)

        Check if a Level returned by level_at is an RTL level.

        Invalid levels (numbers greater than 125) will be assumed LTR

        See the `Rust documentation for is_rtl <https://docs.rs/unicode_bidi/latest/unicode_bidi/struct.Level.html#method.is_rtl>`__ for more information.


    .. cpp:function:: static bool level_is_ltr(uint8_t level)

        Check if a Level returned by level_at is an LTR level.

        Invalid levels (numbers greater than 125) will be assumed LTR

        See the `Rust documentation for is_ltr <https://docs.rs/unicode_bidi/latest/unicode_bidi/struct.Level.html#method.is_ltr>`__ for more information.


    .. cpp:function:: static uint8_t level_rtl()

        Get a basic RTL Level value

        See the `Rust documentation for rtl <https://docs.rs/unicode_bidi/latest/unicode_bidi/struct.Level.html#method.rtl>`__ for more information.


    .. cpp:function:: static uint8_t level_ltr()

        Get a simple LTR Level value

        See the `Rust documentation for ltr <https://docs.rs/unicode_bidi/latest/unicode_bidi/struct.Level.html#method.ltr>`__ for more information.


.. cpp:enum-struct:: ICU4XBidiDirection

    .. cpp:enumerator:: Ltr

    .. cpp:enumerator:: Rtl

    .. cpp:enumerator:: Mixed

.. cpp:class:: ICU4XBidiInfo

    An object containing bidi information for a given string, produced by ``for_text()`` on ``ICU4XBidi``

    See the `Rust documentation for BidiInfo <https://docs.rs/unicode_bidi/latest/unicode_bidi/struct.BidiInfo.html>`__ for more information.


    .. cpp:function:: size_t paragraph_count() const

        The number of paragraphs contained here


    .. cpp:function:: std::optional<ICU4XBidiParagraph> paragraph_at(size_t n) const

        Get the nth paragraph, returning ``None`` if out of bounds

        Lifetimes: ``this`` must live at least as long as the output.


    .. cpp:function:: size_t size() const

        The number of bytes in this full text


    .. cpp:function:: uint8_t level_at(size_t pos) const

        Get the BIDI level at a particular byte index in the full text. This integer is conceptually a ``unicode_bidi::Level``, and can be further inspected using the static methods on ICU4XBidi.

        Returns 0 (equivalent to ``Level::ltr()``) on error


.. cpp:class:: ICU4XBidiParagraph

    Bidi information for a single processed paragraph


    .. cpp:function:: diplomat::result<std::monostate, ICU4XError> set_paragraph_in_text(size_t n)

        Given a paragraph index ``n`` within the surrounding text, this sets this object to the paragraph at that index. Returns ``ICU4XError::OutOfBoundsError`` when out of bounds.

        This is equivalent to calling ``paragraph_at()`` on ``ICU4XBidiInfo`` but doesn't create a new object


    .. cpp:function:: ICU4XBidiDirection direction() const

        The primary direction of this paragraph

        See the `Rust documentation for level_at <https://docs.rs/unicode_bidi/latest/unicode_bidi/struct.Paragraph.html#method.level_at>`__ for more information.


    .. cpp:function:: size_t size() const

        The number of bytes in this paragraph

        See the `Rust documentation for len <https://docs.rs/unicode_bidi/latest/unicode_bidi/struct.ParagraphInfo.html#method.len>`__ for more information.


    .. cpp:function:: size_t range_start() const

        The start index of this paragraph within the source text


    .. cpp:function:: size_t range_end() const

        The end index of this paragraph within the source text


    .. cpp:function:: template<typename W> diplomat::result<std::monostate, ICU4XError> reorder_line_to_writeable(size_t range_start, size_t range_end, W& out) const

        Reorder a line based on display order. The ranges are specified relative to the source text and must be contained within this paragraph's range.

        See the `Rust documentation for level_at <https://docs.rs/unicode_bidi/latest/unicode_bidi/struct.Paragraph.html#method.level_at>`__ for more information.


    .. cpp:function:: diplomat::result<std::string, ICU4XError> reorder_line(size_t range_start, size_t range_end) const

        Reorder a line based on display order. The ranges are specified relative to the source text and must be contained within this paragraph's range.

        See the `Rust documentation for level_at <https://docs.rs/unicode_bidi/latest/unicode_bidi/struct.Paragraph.html#method.level_at>`__ for more information.


    .. cpp:function:: uint8_t level_at(size_t pos) const

        Get the BIDI level at a particular byte index in this paragraph. This integer is conceptually a ``unicode_bidi::Level``, and can be further inspected using the static methods on ICU4XBidi.

        Returns 0 (equivalent to ``Level::ltr()``) on error

        See the `Rust documentation for level_at <https://docs.rs/unicode_bidi/latest/unicode_bidi/struct.Paragraph.html#method.level_at>`__ for more information.


.. cpp:class:: ICU4XReorderedIndexMap

    Thin wrapper around a vector that maps visual indices to source indices

    ``map[visualIndex] = sourceIndex``

    Produced by ``reorder_visual()`` on :cpp:class:`ICU4XBidi`.


    .. cpp:function:: const diplomat::span<const size_t> as_slice() const

        Get this as a slice/array of indices

        Lifetimes: ``this`` must live at least as long as the output.


    .. cpp:function:: size_t len() const

        The length of this map


    .. cpp:function:: size_t get(size_t index) const

        Get element at ``index``. Returns 0 when out of bounds (note that 0 is also a valid in-bounds value, please use ``len()`` to avoid out-of-bounds)

