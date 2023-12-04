``pluralrules::ffi``
====================

.. cpp:struct:: ICU4XPluralCategories

    FFI version of ``PluralRules::categories()`` data.


    .. cpp:member:: bool zero

    .. cpp:member:: bool one

    .. cpp:member:: bool two

    .. cpp:member:: bool few

    .. cpp:member:: bool many

    .. cpp:member:: bool other

.. cpp:enum-struct:: ICU4XPluralCategory

    FFI version of ``PluralCategory``.

    See the `Rust documentation for PluralCategory <https://docs.rs/icu/latest/icu/plurals/enum.PluralCategory.html>`__ for more information.


    .. cpp:enumerator:: Zero

    .. cpp:enumerator:: One

    .. cpp:enumerator:: Two

    .. cpp:enumerator:: Few

    .. cpp:enumerator:: Many

    .. cpp:enumerator:: Other

    .. cpp:function:: static diplomat::result<ICU4XPluralCategory, std::monostate> get_for_cldr_string(const std::string_view s)

        Construct from a string in the format `specified in TR35 <https://unicode.org/reports/tr35/tr35-numbers.html#Language_Plural_Rules>`__

        See the `Rust documentation for get_for_cldr_string <https://docs.rs/icu/latest/icu/plurals/enum.PluralCategory.html#method.get_for_cldr_string>`__ for more information.

        See the `Rust documentation for get_for_cldr_bytes <https://docs.rs/icu/latest/icu/plurals/enum.PluralCategory.html#method.get_for_cldr_bytes>`__ for more information.


.. cpp:class:: ICU4XPluralOperands

    FFI version of ``PluralOperands``.

    See the `Rust documentation for PluralOperands <https://docs.rs/icu/latest/icu/plurals/struct.PluralOperands.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XPluralOperands, ICU4XError> create_from_string(const std::string_view s)

        Construct for a given string representing a number

        See the `Rust documentation for from_str <https://docs.rs/icu/latest/icu/plurals/struct.PluralOperands.html#method.from_str>`__ for more information.


.. cpp:class:: ICU4XPluralRules

    FFI version of ``PluralRules``.

    See the `Rust documentation for PluralRules <https://docs.rs/icu/latest/icu/plurals/struct.PluralRules.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XPluralRules, ICU4XError> create_cardinal(const ICU4XDataProvider& provider, const ICU4XLocale& locale)

        Construct an :cpp:class:`ICU4XPluralRules` for the given locale, for cardinal numbers

        See the `Rust documentation for try_new_cardinal <https://docs.rs/icu/latest/icu/plurals/struct.PluralRules.html#method.try_new_cardinal>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XPluralRules, ICU4XError> create_ordinal(const ICU4XDataProvider& provider, const ICU4XLocale& locale)

        Construct an :cpp:class:`ICU4XPluralRules` for the given locale, for ordinal numbers

        See the `Rust documentation for try_new_ordinal <https://docs.rs/icu/latest/icu/plurals/struct.PluralRules.html#method.try_new_ordinal>`__ for more information.


    .. cpp:function:: ICU4XPluralCategory category_for(const ICU4XPluralOperands& op) const

        Get the category for a given number represented as operands

        See the `Rust documentation for category_for <https://docs.rs/icu/latest/icu/plurals/struct.PluralRules.html#method.category_for>`__ for more information.


    .. cpp:function:: ICU4XPluralCategories categories() const

        Get all of the categories needed in the current locale

        See the `Rust documentation for categories <https://docs.rs/icu/latest/icu/plurals/struct.PluralRules.html#method.categories>`__ for more information.

