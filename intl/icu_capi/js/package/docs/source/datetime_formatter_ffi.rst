``datetime_formatter::ffi``
===========================

.. js:class:: ICU4XDateFormatter

    An ICU4X DateFormatter object capable of formatting a :js:class:`ICU4XDate` as a string, using some calendar specified at runtime in the locale.

    See the `Rust documentation for DateFormatter <https://docs.rs/icu/latest/icu/datetime/struct.DateFormatter.html>`__ for more information.


    .. js:function:: create_with_length(provider, locale, date_length)

        Creates a new :js:class:`ICU4XDateFormatter` from locale data.

        See the `Rust documentation for try_new_with_length <https://docs.rs/icu/latest/icu/datetime/struct.DateFormatter.html#method.try_new_with_length>`__ for more information.


    .. js:method:: format_date(value)

        Formats a :js:class:`ICU4XDate` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.DateFormatter.html#method.format>`__ for more information.


    .. js:method:: format_iso_date(value)

        Formats a :js:class:`ICU4XIsoDate` to a string.

        Will convert to this formatter's calendar first

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.DateFormatter.html#method.format>`__ for more information.


    .. js:method:: format_datetime(value)

        Formats a :js:class:`ICU4XDateTime` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.DateFormatter.html#method.format>`__ for more information.


    .. js:method:: format_iso_datetime(value)

        Formats a :js:class:`ICU4XIsoDateTime` to a string.

        Will convert to this formatter's calendar first

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.DateFormatter.html#method.format>`__ for more information.


.. js:class:: ICU4XDateLength

    See the `Rust documentation for Date <https://docs.rs/icu/latest/icu/datetime/options/length/enum.Date.html>`__ for more information.


.. js:class:: ICU4XDateTimeFormatter

    An ICU4X DateFormatter object capable of formatting a :js:class:`ICU4XDateTime` as a string, using some calendar specified at runtime in the locale.

    See the `Rust documentation for DateTimeFormatter <https://docs.rs/icu/latest/icu/datetime/struct.DateTimeFormatter.html>`__ for more information.


    .. js:function:: create_with_lengths(provider, locale, date_length, time_length)

        Creates a new :js:class:`ICU4XDateTimeFormatter` from locale data.

        See the `Rust documentation for try_new <https://docs.rs/icu/latest/icu/datetime/struct.DateTimeFormatter.html#method.try_new>`__ for more information.


    .. js:method:: format_datetime(value)

        Formats a :js:class:`ICU4XDateTime` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.DateTimeFormatter.html#method.format>`__ for more information.


    .. js:method:: format_iso_datetime(value)

        Formats a :js:class:`ICU4XIsoDateTime` to a string.

        Will convert to this formatter's calendar first

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.DateTimeFormatter.html#method.format>`__ for more information.


.. js:class:: ICU4XGregorianDateFormatter

    An ICU4X TypedDateFormatter object capable of formatting a :js:class:`ICU4XIsoDateTime` as a string, using the Gregorian Calendar.

    See the `Rust documentation for TypedDateFormatter <https://docs.rs/icu/latest/icu/datetime/struct.TypedDateFormatter.html>`__ for more information.


    .. js:function:: create_with_length(provider, locale, length)

        Creates a new :js:class:`ICU4XGregorianDateFormatter` from locale data.

        See the `Rust documentation for try_new_with_length <https://docs.rs/icu/latest/icu/datetime/struct.TypedDateFormatter.html#method.try_new_with_length>`__ for more information.


    .. js:method:: format_iso_date(value)

        Formats a :js:class:`ICU4XIsoDate` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.TypedDateFormatter.html#method.format>`__ for more information.


    .. js:method:: format_iso_datetime(value)

        Formats a :js:class:`ICU4XIsoDateTime` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.TypedDateFormatter.html#method.format>`__ for more information.


.. js:class:: ICU4XGregorianDateTimeFormatter

    An ICU4X TypedDateTimeFormatter object capable of formatting a :js:class:`ICU4XIsoDateTime` as a string, using the Gregorian Calendar.

    See the `Rust documentation for TypedDateTimeFormatter <https://docs.rs/icu/latest/icu/datetime/struct.TypedDateTimeFormatter.html>`__ for more information.


    .. js:function:: create_with_lengths(provider, locale, date_length, time_length)

        Creates a new :js:class:`ICU4XGregorianDateFormatter` from locale data.

        See the `Rust documentation for try_new <https://docs.rs/icu/latest/icu/datetime/struct.TypedDateTimeFormatter.html#method.try_new>`__ for more information.


    .. js:method:: format_iso_datetime(value)

        Formats a :js:class:`ICU4XIsoDateTime` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.TypedDateTimeFormatter.html#method.format>`__ for more information.


.. js:class:: ICU4XTimeFormatter

    An ICU4X TimeFormatter object capable of formatting an :js:class:`ICU4XTime` type (and others) as a string

    See the `Rust documentation for TimeFormatter <https://docs.rs/icu/latest/icu/datetime/struct.TimeFormatter.html>`__ for more information.


    .. js:function:: create_with_length(provider, locale, length)

        Creates a new :js:class:`ICU4XTimeFormatter` from locale data.

        See the `Rust documentation for try_new_with_length <https://docs.rs/icu/latest/icu/datetime/struct.TimeFormatter.html#method.try_new_with_length>`__ for more information.


    .. js:method:: format_time(value)

        Formats a :js:class:`ICU4XTime` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.TimeFormatter.html#method.format>`__ for more information.


    .. js:method:: format_datetime(value)

        Formats a :js:class:`ICU4XDateTime` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.TimeFormatter.html#method.format>`__ for more information.


    .. js:method:: format_iso_datetime(value)

        Formats a :js:class:`ICU4XIsoDateTime` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.TimeFormatter.html#method.format>`__ for more information.


.. js:class:: ICU4XTimeLength

    See the `Rust documentation for Time <https://docs.rs/icu/latest/icu/datetime/options/length/enum.Time.html>`__ for more information.

