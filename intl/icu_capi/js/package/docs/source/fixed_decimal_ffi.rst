``fixed_decimal::ffi``
======================

.. js:class:: ICU4XFixedDecimal

    See the `Rust documentation for FixedDecimal <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html>`__ for more information.


    .. js:function:: create_from_i32(v)

        Construct an :js:class:`ICU4XFixedDecimal` from an integer.

        See the `Rust documentation for FixedDecimal <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html>`__ for more information.


    .. js:function:: create_from_u32(v)

        Construct an :js:class:`ICU4XFixedDecimal` from an integer.

        See the `Rust documentation for FixedDecimal <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html>`__ for more information.


    .. js:function:: create_from_i64(v)

        Construct an :js:class:`ICU4XFixedDecimal` from an integer.

        See the `Rust documentation for FixedDecimal <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html>`__ for more information.


    .. js:function:: create_from_u64(v)

        Construct an :js:class:`ICU4XFixedDecimal` from an integer.

        See the `Rust documentation for FixedDecimal <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html>`__ for more information.


    .. js:function:: create_from_f64_with_integer_precision(f)

        Construct an :js:class:`ICU4XFixedDecimal` from an integer-valued float

        See the `Rust documentation for try_from_f64 <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.try_from_f64>`__ for more information.

        See the `Rust documentation for FloatPrecision <https://docs.rs/fixed_decimal/latest/fixed_decimal/enum.FloatPrecision.html>`__ for more information.


    .. js:function:: create_from_f64_with_lower_magnitude(f, magnitude)

        Construct an :js:class:`ICU4XFixedDecimal` from an float, with a given power of 10 for the lower magnitude

        See the `Rust documentation for try_from_f64 <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.try_from_f64>`__ for more information.

        See the `Rust documentation for FloatPrecision <https://docs.rs/fixed_decimal/latest/fixed_decimal/enum.FloatPrecision.html>`__ for more information.


    .. js:function:: create_from_f64_with_significant_digits(f, digits)

        Construct an :js:class:`ICU4XFixedDecimal` from an float, for a given number of significant digits

        See the `Rust documentation for try_from_f64 <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.try_from_f64>`__ for more information.

        See the `Rust documentation for FloatPrecision <https://docs.rs/fixed_decimal/latest/fixed_decimal/enum.FloatPrecision.html>`__ for more information.


    .. js:function:: create_from_f64_with_floating_precision(f)

        Construct an :js:class:`ICU4XFixedDecimal` from an float, with enough digits to recover the original floating point in IEEE 754 without needing trailing zeros

        See the `Rust documentation for try_from_f64 <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.try_from_f64>`__ for more information.

        See the `Rust documentation for FloatPrecision <https://docs.rs/fixed_decimal/latest/fixed_decimal/enum.FloatPrecision.html>`__ for more information.


    .. js:function:: create_from_string(v)

        Construct an :js:class:`ICU4XFixedDecimal` from a string.

        See the `Rust documentation for from_str <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.from_str>`__ for more information.


    .. js:method:: digit_at(magnitude)

        See the `Rust documentation for digit_at <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.digit_at>`__ for more information.


    .. js:method:: magnitude_start()

        See the `Rust documentation for magnitude_range <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.magnitude_range>`__ for more information.


    .. js:method:: magnitude_end()

        See the `Rust documentation for magnitude_range <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.magnitude_range>`__ for more information.


    .. js:method:: nonzero_magnitude_start()

        See the `Rust documentation for nonzero_magnitude_start <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.nonzero_magnitude_start>`__ for more information.


    .. js:method:: nonzero_magnitude_end()

        See the `Rust documentation for nonzero_magnitude_end <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.nonzero_magnitude_end>`__ for more information.


    .. js:method:: is_zero()

        See the `Rust documentation for is_zero <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.is_zero>`__ for more information.


    .. js:method:: multiply_pow10(power)

        Multiply the :js:class:`ICU4XFixedDecimal` by a given power of ten.

        See the `Rust documentation for multiply_pow10 <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.multiply_pow10>`__ for more information.


    .. js:method:: sign()

        See the `Rust documentation for sign <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.sign>`__ for more information.


    .. js:method:: set_sign(sign)

        Set the sign of the :js:class:`ICU4XFixedDecimal`.

        See the `Rust documentation for set_sign <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.set_sign>`__ for more information.


    .. js:method:: apply_sign_display(sign_display)

        See the `Rust documentation for apply_sign_display <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.apply_sign_display>`__ for more information.


    .. js:method:: trim_start()

        See the `Rust documentation for trim_start <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.trim_start>`__ for more information.


    .. js:method:: trim_end()

        See the `Rust documentation for trim_end <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.trim_end>`__ for more information.


    .. js:method:: pad_start(position)

        Zero-pad the :js:class:`ICU4XFixedDecimal` on the left to a particular position

        See the `Rust documentation for pad_start <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.pad_start>`__ for more information.


    .. js:method:: pad_end(position)

        Zero-pad the :js:class:`ICU4XFixedDecimal` on the right to a particular position

        See the `Rust documentation for pad_end <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.pad_end>`__ for more information.


    .. js:method:: set_max_position(position)

        Truncate the :js:class:`ICU4XFixedDecimal` on the left to a particular position, deleting digits if necessary. This is useful for, e.g. abbreviating years ("2022" -> "22")

        See the `Rust documentation for set_max_position <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.set_max_position>`__ for more information.


    .. js:method:: trunc(position)

        See the `Rust documentation for trunc <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.trunc>`__ for more information.


    .. js:method:: half_trunc(position)

        See the `Rust documentation for half_trunc <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.half_trunc>`__ for more information.


    .. js:method:: expand(position)

        See the `Rust documentation for expand <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.expand>`__ for more information.


    .. js:method:: half_expand(position)

        See the `Rust documentation for half_expand <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.half_expand>`__ for more information.


    .. js:method:: ceil(position)

        See the `Rust documentation for ceil <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.ceil>`__ for more information.


    .. js:method:: half_ceil(position)

        See the `Rust documentation for half_ceil <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.half_ceil>`__ for more information.


    .. js:method:: floor(position)

        See the `Rust documentation for floor <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.floor>`__ for more information.


    .. js:method:: half_floor(position)

        See the `Rust documentation for half_floor <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.half_floor>`__ for more information.


    .. js:method:: half_even(position)

        See the `Rust documentation for half_even <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.half_even>`__ for more information.


    .. js:method:: concatenate_end(other)

        Concatenates ``other`` to the end of ``self``.

        If successful, ``other`` will be set to 0 and a successful status is returned.

        If not successful, ``other`` will be unchanged and an error is returned.

        See the `Rust documentation for concatenate_end <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.concatenate_end>`__ for more information.


    .. js:method:: to_string()

        Format the :js:class:`ICU4XFixedDecimal` as a string.

        See the `Rust documentation for write_to <https://docs.rs/fixed_decimal/latest/fixed_decimal/struct.FixedDecimal.html#method.write_to>`__ for more information.


.. js:class:: ICU4XFixedDecimalSign

    The sign of a FixedDecimal, as shown in formatting.

    See the `Rust documentation for Sign <https://docs.rs/fixed_decimal/latest/fixed_decimal/enum.Sign.html>`__ for more information.


.. js:class:: ICU4XFixedDecimalSignDisplay

    ECMA-402 compatible sign display preference.

    See the `Rust documentation for SignDisplay <https://docs.rs/fixed_decimal/latest/fixed_decimal/enum.SignDisplay.html>`__ for more information.


.. js:class:: ICU4XRoundingIncrement

    Increment used in a rounding operation.

    See the `Rust documentation for RoundingIncrement <https://docs.rs/fixed_decimal/latest/fixed_decimal/enum.RoundingIncrement.html>`__ for more information.

