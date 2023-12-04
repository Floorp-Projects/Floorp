``locale_directionality::ffi``
==============================

.. cpp:enum-struct:: ICU4XLocaleDirection

    See the `Rust documentation for Direction <https://docs.rs/icu/latest/icu/locid_transform/enum.Direction.html>`__ for more information.


    .. cpp:enumerator:: LeftToRight

    .. cpp:enumerator:: RightToLeft

    .. cpp:enumerator:: Unknown

.. cpp:class:: ICU4XLocaleDirectionality

    See the `Rust documentation for LocaleDirectionality <https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleDirectionality.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XLocaleDirectionality, ICU4XError> create(const ICU4XDataProvider& provider)

        Construct a new ICU4XLocaleDirectionality instance

        See the `Rust documentation for new <https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleDirectionality.html#method.new>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XLocaleDirectionality, ICU4XError> create_with_expander(const ICU4XDataProvider& provider, const ICU4XLocaleExpander& expander)

        Construct a new ICU4XLocaleDirectionality instance with a custom expander

        See the `Rust documentation for new_with_expander <https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleDirectionality.html#method.new_with_expander>`__ for more information.


    .. cpp:function:: ICU4XLocaleDirection get(const ICU4XLocale& locale) const

        See the `Rust documentation for get <https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleDirectionality.html#method.get>`__ for more information.


    .. cpp:function:: bool is_left_to_right(const ICU4XLocale& locale) const

        See the `Rust documentation for is_left_to_right <https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleDirectionality.html#method.is_left_to_right>`__ for more information.


    .. cpp:function:: bool is_right_to_left(const ICU4XLocale& locale) const

        See the `Rust documentation for is_right_to_left <https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleDirectionality.html#method.is_right_to_left>`__ for more information.

