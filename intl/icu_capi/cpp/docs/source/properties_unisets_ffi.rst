``properties_unisets::ffi``
===========================

.. cpp:class:: ICU4XUnicodeSetData

    An ICU4X Unicode Set Property object, capable of querying whether a code point is contained in a set based on a Unicode property.

    See the `Rust documentation for properties <https://docs.rs/icu/latest/icu/properties/index.html>`__ for more information.

    See the `Rust documentation for UnicodeSetData <https://docs.rs/icu/latest/icu/properties/sets/struct.UnicodeSetData.html>`__ for more information.

    See the `Rust documentation for UnicodeSetDataBorrowed <https://docs.rs/icu/latest/icu/properties/sets/struct.UnicodeSetDataBorrowed.html>`__ for more information.


    .. cpp:function:: bool contains(const std::string_view s) const

        Checks whether the string is in the set.

        See the `Rust documentation for contains <https://docs.rs/icu/latest/icu/properties/sets/struct.UnicodeSetDataBorrowed.html#method.contains>`__ for more information.


    .. cpp:function:: bool contains_char(char32_t cp) const

        Checks whether the code point is in the set.

        See the `Rust documentation for contains_char <https://docs.rs/icu/latest/icu/properties/sets/struct.UnicodeSetDataBorrowed.html#method.contains_char>`__ for more information.


    .. cpp:function:: bool contains32(uint32_t cp) const

        Checks whether the code point (specified as a 32 bit integer, in UTF-32) is in the set.


    .. cpp:function:: static diplomat::result<ICU4XUnicodeSetData, ICU4XError> load_basic_emoji(const ICU4XDataProvider& provider)

        See the `Rust documentation for basic_emoji <https://docs.rs/icu/latest/icu/properties/sets/fn.basic_emoji.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XUnicodeSetData, ICU4XError> load_exemplars_main(const ICU4XDataProvider& provider, const ICU4XLocale& locale)

        See the `Rust documentation for exemplars_main <https://docs.rs/icu/latest/icu/properties/exemplar_chars/fn.exemplars_main.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XUnicodeSetData, ICU4XError> load_exemplars_auxiliary(const ICU4XDataProvider& provider, const ICU4XLocale& locale)

        See the `Rust documentation for exemplars_auxiliary <https://docs.rs/icu/latest/icu/properties/exemplar_chars/fn.exemplars_auxiliary.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XUnicodeSetData, ICU4XError> load_exemplars_punctuation(const ICU4XDataProvider& provider, const ICU4XLocale& locale)

        See the `Rust documentation for exemplars_punctuation <https://docs.rs/icu/latest/icu/properties/exemplar_chars/fn.exemplars_punctuation.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XUnicodeSetData, ICU4XError> load_exemplars_numbers(const ICU4XDataProvider& provider, const ICU4XLocale& locale)

        See the `Rust documentation for exemplars_numbers <https://docs.rs/icu/latest/icu/properties/exemplar_chars/fn.exemplars_numbers.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XUnicodeSetData, ICU4XError> load_exemplars_index(const ICU4XDataProvider& provider, const ICU4XLocale& locale)

        See the `Rust documentation for exemplars_index <https://docs.rs/icu/latest/icu/properties/exemplar_chars/fn.exemplars_index.html>`__ for more information.

