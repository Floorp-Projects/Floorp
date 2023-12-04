``time::ffi``
=============

.. cpp:class:: ICU4XTime

    An ICU4X Time object representing a time in terms of hour, minute, second, nanosecond

    See the `Rust documentation for Time <https://docs.rs/icu/latest/icu/calendar/types/struct.Time.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XTime, ICU4XError> create(uint8_t hour, uint8_t minute, uint8_t second, uint32_t nanosecond)

        Creates a new :cpp:class:`ICU4XTime` given field values

        See the `Rust documentation for Time <https://docs.rs/icu/latest/icu/calendar/types/struct.Time.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XTime, ICU4XError> create_midnight()

        Creates a new :cpp:class:`ICU4XTime` representing midnight (00:00.000).

        See the `Rust documentation for Time <https://docs.rs/icu/latest/icu/calendar/types/struct.Time.html>`__ for more information.


    .. cpp:function:: uint8_t hour() const

        Returns the hour in this time

        See the `Rust documentation for hour <https://docs.rs/icu/latest/icu/calendar/types/struct.Time.html#structfield.hour>`__ for more information.


    .. cpp:function:: uint8_t minute() const

        Returns the minute in this time

        See the `Rust documentation for minute <https://docs.rs/icu/latest/icu/calendar/types/struct.Time.html#structfield.minute>`__ for more information.


    .. cpp:function:: uint8_t second() const

        Returns the second in this time

        See the `Rust documentation for second <https://docs.rs/icu/latest/icu/calendar/types/struct.Time.html#structfield.second>`__ for more information.


    .. cpp:function:: uint32_t nanosecond() const

        Returns the nanosecond in this time

        See the `Rust documentation for nanosecond <https://docs.rs/icu/latest/icu/calendar/types/struct.Time.html#structfield.nanosecond>`__ for more information.

