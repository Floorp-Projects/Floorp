``data_struct::ffi``
====================

.. cpp:class:: ICU4XDataStruct

    A generic data struct to be used by ICU4X

    This can be used to construct a StructDataProvider.


    .. cpp:function:: static diplomat::result<ICU4XDataStruct, ICU4XError> create_decimal_symbols_v1(const std::string_view plus_sign_prefix, const std::string_view plus_sign_suffix, const std::string_view minus_sign_prefix, const std::string_view minus_sign_suffix, const std::string_view decimal_separator, const std::string_view grouping_separator, uint8_t primary_group_size, uint8_t secondary_group_size, uint8_t min_group_size, const diplomat::span<const char32_t> digits)

        Construct a new DecimalSymbolsV1 data struct.

        C++ users: All string arguments must be valid UTF8

        See the `Rust documentation for DecimalSymbolsV1 <https://docs.rs/icu/latest/icu/decimal/provider/struct.DecimalSymbolsV1.html>`__ for more information.

