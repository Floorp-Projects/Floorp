``properties_maps::ffi``
========================

.. cpp:class:: ICU4XCodePointMapData16

    An ICU4X Unicode Map Property object, capable of querying whether a code point (key) to obtain the Unicode property value, for a specific Unicode property.

    For properties whose values fit into 16 bits.

    See the `Rust documentation for properties <https://docs.rs/icu/latest/icu/properties/index.html>`__ for more information.

    See the `Rust documentation for CodePointMapData <https://docs.rs/icu/latest/icu/properties/maps/struct.CodePointMapData.html>`__ for more information.

    See the `Rust documentation for CodePointMapDataBorrowed <https://docs.rs/icu/latest/icu/properties/maps/struct.CodePointMapDataBorrowed.html>`__ for more information.


    .. cpp:function:: uint16_t get(char32_t cp) const

        Gets the value for a code point.

        See the `Rust documentation for get <https://docs.rs/icu/latest/icu/properties/maps/struct.CodePointMapDataBorrowed.html#method.get>`__ for more information.


    .. cpp:function:: uint16_t get32(uint32_t cp) const

        Gets the value for a code point (specified as a 32 bit integer, in UTF-32)


    .. cpp:function:: CodePointRangeIterator iter_ranges_for_value(uint16_t value) const

        Produces an iterator over ranges of code points that map to ``value``

        See the `Rust documentation for iter_ranges_for_value <https://docs.rs/icu/latest/icu/properties/maps/struct.CodePointMapDataBorrowed.html#method.iter_ranges_for_value>`__ for more information.

        Lifetimes: ``this`` must live at least as long as the output.


    .. cpp:function:: CodePointRangeIterator iter_ranges_for_value_complemented(uint16_t value) const

        Produces an iterator over ranges of code points that do not map to ``value``

        See the `Rust documentation for iter_ranges_for_value_complemented <https://docs.rs/icu/latest/icu/properties/maps/struct.CodePointMapDataBorrowed.html#method.iter_ranges_for_value_complemented>`__ for more information.

        Lifetimes: ``this`` must live at least as long as the output.


    .. cpp:function:: ICU4XCodePointSetData get_set_for_value(uint16_t value) const

        Gets a :cpp:class:`ICU4XCodePointSetData` representing all entries in this map that map to the given value

        See the `Rust documentation for get_set_for_value <https://docs.rs/icu/latest/icu/properties/maps/struct.CodePointMapDataBorrowed.html#method.get_set_for_value>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointMapData16, ICU4XError> load_script(const ICU4XDataProvider& provider)

        See the `Rust documentation for script <https://docs.rs/icu/latest/icu/properties/maps/fn.script.html>`__ for more information.


.. cpp:class:: ICU4XCodePointMapData8

    An ICU4X Unicode Map Property object, capable of querying whether a code point (key) to obtain the Unicode property value, for a specific Unicode property.

    For properties whose values fit into 8 bits.

    See the `Rust documentation for properties <https://docs.rs/icu/latest/icu/properties/index.html>`__ for more information.

    See the `Rust documentation for CodePointMapData <https://docs.rs/icu/latest/icu/properties/maps/struct.CodePointMapData.html>`__ for more information.

    See the `Rust documentation for CodePointMapDataBorrowed <https://docs.rs/icu/latest/icu/properties/maps/struct.CodePointMapDataBorrowed.html>`__ for more information.


    .. cpp:function:: uint8_t get(char32_t cp) const

        Gets the value for a code point.

        See the `Rust documentation for get <https://docs.rs/icu/latest/icu/properties/maps/struct.CodePointMapDataBorrowed.html#method.get>`__ for more information.


    .. cpp:function:: uint8_t get32(uint32_t cp) const

        Gets the value for a code point (specified as a 32 bit integer, in UTF-32)


    .. cpp:function:: static uint32_t general_category_to_mask(uint8_t gc)

        Converts a general category to its corresponding mask value

        Nonexistant general categories will map to the empty mask

        See the `Rust documentation for GeneralCategoryGroup <https://docs.rs/icu/latest/icu/properties/struct.GeneralCategoryGroup.html>`__ for more information.


    .. cpp:function:: CodePointRangeIterator iter_ranges_for_value(uint8_t value) const

        Produces an iterator over ranges of code points that map to ``value``

        See the `Rust documentation for iter_ranges_for_value <https://docs.rs/icu/latest/icu/properties/maps/struct.CodePointMapDataBorrowed.html#method.iter_ranges_for_value>`__ for more information.

        Lifetimes: ``this`` must live at least as long as the output.


    .. cpp:function:: CodePointRangeIterator iter_ranges_for_value_complemented(uint8_t value) const

        Produces an iterator over ranges of code points that do not map to ``value``

        See the `Rust documentation for iter_ranges_for_value_complemented <https://docs.rs/icu/latest/icu/properties/maps/struct.CodePointMapDataBorrowed.html#method.iter_ranges_for_value_complemented>`__ for more information.

        Lifetimes: ``this`` must live at least as long as the output.


    .. cpp:function:: CodePointRangeIterator iter_ranges_for_mask(uint32_t mask) const

        Given a mask value (the nth bit marks property value = n), produce an iterator over ranges of code points whose property values are contained in the mask.

        The main mask property supported is that for General_Category, which can be obtained via ``general_category_to_mask()`` or by using ``ICU4XGeneralCategoryNameToMaskMapper``

        Should only be used on maps for properties with values less than 32 (like Generak_Category), other maps will have unpredictable results

        See the `Rust documentation for iter_ranges_for_group <https://docs.rs/icu/latest/icu/properties/maps/struct.CodePointMapDataBorrowed.html#method.iter_ranges_for_group>`__ for more information.

        Lifetimes: ``this`` must live at least as long as the output.


    .. cpp:function:: ICU4XCodePointSetData get_set_for_value(uint8_t value) const

        Gets a :cpp:class:`ICU4XCodePointSetData` representing all entries in this map that map to the given value

        See the `Rust documentation for get_set_for_value <https://docs.rs/icu/latest/icu/properties/maps/struct.CodePointMapDataBorrowed.html#method.get_set_for_value>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointMapData8, ICU4XError> load_general_category(const ICU4XDataProvider& provider)

        See the `Rust documentation for general_category <https://docs.rs/icu/latest/icu/properties/maps/fn.general_category.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointMapData8, ICU4XError> load_bidi_class(const ICU4XDataProvider& provider)

        See the `Rust documentation for bidi_class <https://docs.rs/icu/latest/icu/properties/maps/fn.bidi_class.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointMapData8, ICU4XError> load_east_asian_width(const ICU4XDataProvider& provider)

        See the `Rust documentation for east_asian_width <https://docs.rs/icu/latest/icu/properties/maps/fn.east_asian_width.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointMapData8, ICU4XError> load_indic_syllabic_category(const ICU4XDataProvider& provider)

        See the `Rust documentation for indic_syllabic_category <https://docs.rs/icu/latest/icu/properties/maps/fn.indic_syllabic_category.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointMapData8, ICU4XError> load_line_break(const ICU4XDataProvider& provider)

        See the `Rust documentation for line_break <https://docs.rs/icu/latest/icu/properties/maps/fn.line_break.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointMapData8, ICU4XError> try_grapheme_cluster_break(const ICU4XDataProvider& provider)

        See the `Rust documentation for grapheme_cluster_break <https://docs.rs/icu/latest/icu/properties/maps/fn.grapheme_cluster_break.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointMapData8, ICU4XError> load_word_break(const ICU4XDataProvider& provider)

        See the `Rust documentation for word_break <https://docs.rs/icu/latest/icu/properties/maps/fn.word_break.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointMapData8, ICU4XError> load_sentence_break(const ICU4XDataProvider& provider)

        See the `Rust documentation for sentence_break <https://docs.rs/icu/latest/icu/properties/maps/fn.sentence_break.html>`__ for more information.

