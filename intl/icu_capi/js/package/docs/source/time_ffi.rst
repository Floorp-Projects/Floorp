``time::ffi``
=============

.. js:class:: ICU4XTime

    An ICU4X Time object representing a time in terms of hour, minute, second, nanosecond

    See the `Rust documentation for Time <https://docs.rs/icu/latest/icu/calendar/types/struct.Time.html>`__ for more information.


    .. js:function:: create(hour, minute, second, nanosecond)

        Creates a new :js:class:`ICU4XTime` given field values

        See the `Rust documentation for Time <https://docs.rs/icu/latest/icu/calendar/types/struct.Time.html>`__ for more information.


    .. js:function:: create_midnight()

        Creates a new :js:class:`ICU4XTime` representing midnight (00:00.000).

        See the `Rust documentation for Time <https://docs.rs/icu/latest/icu/calendar/types/struct.Time.html>`__ for more information.


    .. js:method:: hour()

        Returns the hour in this time

        See the `Rust documentation for hour <https://docs.rs/icu/latest/icu/calendar/types/struct.Time.html#structfield.hour>`__ for more information.


    .. js:method:: minute()

        Returns the minute in this time

        See the `Rust documentation for minute <https://docs.rs/icu/latest/icu/calendar/types/struct.Time.html#structfield.minute>`__ for more information.


    .. js:method:: second()

        Returns the second in this time

        See the `Rust documentation for second <https://docs.rs/icu/latest/icu/calendar/types/struct.Time.html#structfield.second>`__ for more information.


    .. js:method:: nanosecond()

        Returns the nanosecond in this time

        See the `Rust documentation for nanosecond <https://docs.rs/icu/latest/icu/calendar/types/struct.Time.html#structfield.nanosecond>`__ for more information.

