``data_struct::ffi``
====================

.. js:class:: ICU4XDataStruct

    A generic data struct to be used by ICU4X

    This can be used to construct a StructDataProvider.


    .. js:function:: create_decimal_symbols_v1(plus_sign_prefix, plus_sign_suffix, minus_sign_prefix, minus_sign_suffix, decimal_separator, grouping_separator, primary_group_size, secondary_group_size, min_group_size, digits)

        Construct a new DecimalSymbolsV1 data struct.

        C++ users: All string arguments must be valid UTF8

        See the `Rust documentation for DecimalSymbolsV1 <https://docs.rs/icu/latest/icu/decimal/provider/struct.DecimalSymbolsV1.html>`__ for more information.

        - Note: ``digits`` should be an ArrayBuffer or TypedArray corresponding to the slice type expected by Rust.

