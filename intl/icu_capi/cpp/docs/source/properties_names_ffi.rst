``properties_names::ffi``
=========================

.. cpp:class:: ICU4XGeneralCategoryNameToMaskMapper

    A type capable of looking up General Category mask values from a string name.

    See the `Rust documentation for get_name_to_enum_mapper <https://docs.rs/icu/latest/icu/properties/struct.GeneralCategoryGroup.html#method.get_name_to_enum_mapper>`__ for more information.

    See the `Rust documentation for PropertyValueNameToEnumMapper <https://docs.rs/icu/latest/icu/properties/names/struct.PropertyValueNameToEnumMapper.html>`__ for more information.


    .. cpp:function:: uint32_t get_strict(const std::string_view name) const

        Get the mask value matching the given name, using strict matching

        Returns 0 if the name is unknown for this property


    .. cpp:function:: uint32_t get_loose(const std::string_view name) const

        Get the mask value matching the given name, using loose matching

        Returns 0 if the name is unknown for this property


    .. cpp:function:: static diplomat::result<ICU4XGeneralCategoryNameToMaskMapper, ICU4XError> load(const ICU4XDataProvider& provider)

        See the `Rust documentation for get_name_to_enum_mapper <https://docs.rs/icu/latest/icu/properties/struct.GeneralCategoryGroup.html#method.get_name_to_enum_mapper>`__ for more information.


.. cpp:class:: ICU4XPropertyValueNameToEnumMapper

    A type capable of looking up a property value from a string name.

    See the `Rust documentation for PropertyValueNameToEnumMapper <https://docs.rs/icu/latest/icu/properties/names/struct.PropertyValueNameToEnumMapper.html>`__ for more information.

    See the `Rust documentation for PropertyValueNameToEnumMapperBorrowed <https://docs.rs/icu/latest/icu/properties/names/struct.PropertyValueNameToEnumMapperBorrowed.html>`__ for more information.


    .. cpp:function:: int16_t get_strict(const std::string_view name) const

        Get the property value matching the given name, using strict matching

        Returns -1 if the name is unknown for this property

        See the `Rust documentation for get_strict <https://docs.rs/icu/latest/icu/properties/names/struct.PropertyValueNameToEnumMapperBorrowed.html#method.get_strict>`__ for more information.


    .. cpp:function:: int16_t get_loose(const std::string_view name) const

        Get the property value matching the given name, using loose matching

        Returns -1 if the name is unknown for this property

        See the `Rust documentation for get_loose <https://docs.rs/icu/latest/icu/properties/names/struct.PropertyValueNameToEnumMapperBorrowed.html#method.get_loose>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XPropertyValueNameToEnumMapper, ICU4XError> load_general_category(const ICU4XDataProvider& provider)

        See the `Rust documentation for get_name_to_enum_mapper <https://docs.rs/icu/latest/icu/properties/struct.GeneralCategory.html#method.get_name_to_enum_mapper>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XPropertyValueNameToEnumMapper, ICU4XError> load_bidi_class(const ICU4XDataProvider& provider)

        See the `Rust documentation for name_to_enum_mapper <https://docs.rs/icu/latest/icu/properties/struct.BidiClass.html#method.name_to_enum_mapper>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XPropertyValueNameToEnumMapper, ICU4XError> load_east_asian_width(const ICU4XDataProvider& provider)

        See the `Rust documentation for name_to_enum_mapper <https://docs.rs/icu/latest/icu/properties/struct.EastAsianWidth.html#method.name_to_enum_mapper>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XPropertyValueNameToEnumMapper, ICU4XError> load_indic_syllabic_category(const ICU4XDataProvider& provider)

        See the `Rust documentation for name_to_enum_mapper <https://docs.rs/icu/latest/icu/properties/struct.IndicSyllabicCategory.html#method.name_to_enum_mapper>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XPropertyValueNameToEnumMapper, ICU4XError> load_line_break(const ICU4XDataProvider& provider)

        See the `Rust documentation for name_to_enum_mapper <https://docs.rs/icu/latest/icu/properties/struct.LineBreak.html#method.name_to_enum_mapper>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XPropertyValueNameToEnumMapper, ICU4XError> load_grapheme_cluster_break(const ICU4XDataProvider& provider)

        See the `Rust documentation for get_name_to_enum_mapper <https://docs.rs/icu/latest/icu/properties/struct.GraphemeClusterBreak.html#method.get_name_to_enum_mapper>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XPropertyValueNameToEnumMapper, ICU4XError> load_word_break(const ICU4XDataProvider& provider)

        See the `Rust documentation for name_to_enum_mapper <https://docs.rs/icu/latest/icu/properties/struct.WordBreak.html#method.name_to_enum_mapper>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XPropertyValueNameToEnumMapper, ICU4XError> load_sentence_break(const ICU4XDataProvider& provider)

        See the `Rust documentation for name_to_enum_mapper <https://docs.rs/icu/latest/icu/properties/struct.SentenceBreak.html#method.name_to_enum_mapper>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XPropertyValueNameToEnumMapper, ICU4XError> load_script(const ICU4XDataProvider& provider)

        See the `Rust documentation for name_to_enum_mapper <https://docs.rs/icu/latest/icu/properties/struct.Script.html#method.name_to_enum_mapper>`__ for more information.

