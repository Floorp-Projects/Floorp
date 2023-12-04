``metazone_calculator::ffi``
============================

.. cpp:class:: ICU4XMetazoneCalculator

    An object capable of computing the metazone from a timezone.

    This can be used via ``maybe_calculate_metazone()`` on ```ICU4XCustomTimeZone`` <crate::timezone::ffi::ICU4XCustomTimeZone>`__.

    See the `Rust documentation for MetazoneCalculator <https://docs.rs/icu/latest/icu/timezone/struct.MetazoneCalculator.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XMetazoneCalculator, ICU4XError> create(const ICU4XDataProvider& provider)

        See the `Rust documentation for new <https://docs.rs/icu/latest/icu/timezone/struct.MetazoneCalculator.html#method.new>`__ for more information.

