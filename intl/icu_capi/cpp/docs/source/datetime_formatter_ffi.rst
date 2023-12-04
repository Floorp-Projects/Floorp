``datetime_formatter::ffi``
===========================

.. cpp:class:: ICU4XDateFormatter

    An ICU4X DateFormatter object capable of formatting a :cpp:class:`ICU4XDate` as a string, using some calendar specified at runtime in the locale.

    See the `Rust documentation for DateFormatter <https://docs.rs/icu/latest/icu/datetime/struct.DateFormatter.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XDateFormatter, ICU4XError> create_with_length(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XDateLength date_length)

        Creates a new :cpp:class:`ICU4XDateFormatter` from locale data.

        See the `Rust documentation for try_new_with_length <https://docs.rs/icu/latest/icu/datetime/struct.DateFormatter.html#method.try_new_with_length>`__ for more information.


    .. cpp:function:: template<typename W> diplomat::result<std::monostate, ICU4XError> format_date_to_writeable(const ICU4XDate& value, W& write) const

        Formats a :cpp:class:`ICU4XDate` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.DateFormatter.html#method.format>`__ for more information.


    .. cpp:function:: diplomat::result<std::string, ICU4XError> format_date(const ICU4XDate& value) const

        Formats a :cpp:class:`ICU4XDate` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.DateFormatter.html#method.format>`__ for more information.


    .. cpp:function:: template<typename W> diplomat::result<std::monostate, ICU4XError> format_iso_date_to_writeable(const ICU4XIsoDate& value, W& write) const

        Formats a :cpp:class:`ICU4XIsoDate` to a string.

        Will convert to this formatter's calendar first

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.DateFormatter.html#method.format>`__ for more information.


    .. cpp:function:: diplomat::result<std::string, ICU4XError> format_iso_date(const ICU4XIsoDate& value) const

        Formats a :cpp:class:`ICU4XIsoDate` to a string.

        Will convert to this formatter's calendar first

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.DateFormatter.html#method.format>`__ for more information.


    .. cpp:function:: template<typename W> diplomat::result<std::monostate, ICU4XError> format_datetime_to_writeable(const ICU4XDateTime& value, W& write) const

        Formats a :cpp:class:`ICU4XDateTime` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.DateFormatter.html#method.format>`__ for more information.


    .. cpp:function:: diplomat::result<std::string, ICU4XError> format_datetime(const ICU4XDateTime& value) const

        Formats a :cpp:class:`ICU4XDateTime` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.DateFormatter.html#method.format>`__ for more information.


    .. cpp:function:: template<typename W> diplomat::result<std::monostate, ICU4XError> format_iso_datetime_to_writeable(const ICU4XIsoDateTime& value, W& write) const

        Formats a :cpp:class:`ICU4XIsoDateTime` to a string.

        Will convert to this formatter's calendar first

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.DateFormatter.html#method.format>`__ for more information.


    .. cpp:function:: diplomat::result<std::string, ICU4XError> format_iso_datetime(const ICU4XIsoDateTime& value) const

        Formats a :cpp:class:`ICU4XIsoDateTime` to a string.

        Will convert to this formatter's calendar first

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.DateFormatter.html#method.format>`__ for more information.


.. cpp:enum-struct:: ICU4XDateLength

    See the `Rust documentation for Date <https://docs.rs/icu/latest/icu/datetime/options/length/enum.Date.html>`__ for more information.


    .. cpp:enumerator:: Full

    .. cpp:enumerator:: Long

    .. cpp:enumerator:: Medium

    .. cpp:enumerator:: Short

.. cpp:class:: ICU4XDateTimeFormatter

    An ICU4X DateFormatter object capable of formatting a :cpp:class:`ICU4XDateTime` as a string, using some calendar specified at runtime in the locale.

    See the `Rust documentation for DateTimeFormatter <https://docs.rs/icu/latest/icu/datetime/struct.DateTimeFormatter.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XDateTimeFormatter, ICU4XError> create_with_lengths(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XDateLength date_length, ICU4XTimeLength time_length)

        Creates a new :cpp:class:`ICU4XDateTimeFormatter` from locale data.

        See the `Rust documentation for try_new <https://docs.rs/icu/latest/icu/datetime/struct.DateTimeFormatter.html#method.try_new>`__ for more information.


    .. cpp:function:: template<typename W> diplomat::result<std::monostate, ICU4XError> format_datetime_to_writeable(const ICU4XDateTime& value, W& write) const

        Formats a :cpp:class:`ICU4XDateTime` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.DateTimeFormatter.html#method.format>`__ for more information.


    .. cpp:function:: diplomat::result<std::string, ICU4XError> format_datetime(const ICU4XDateTime& value) const

        Formats a :cpp:class:`ICU4XDateTime` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.DateTimeFormatter.html#method.format>`__ for more information.


    .. cpp:function:: template<typename W> diplomat::result<std::monostate, ICU4XError> format_iso_datetime_to_writeable(const ICU4XIsoDateTime& value, W& write) const

        Formats a :cpp:class:`ICU4XIsoDateTime` to a string.

        Will convert to this formatter's calendar first

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.DateTimeFormatter.html#method.format>`__ for more information.


    .. cpp:function:: diplomat::result<std::string, ICU4XError> format_iso_datetime(const ICU4XIsoDateTime& value) const

        Formats a :cpp:class:`ICU4XIsoDateTime` to a string.

        Will convert to this formatter's calendar first

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.DateTimeFormatter.html#method.format>`__ for more information.


.. cpp:class:: ICU4XGregorianDateFormatter

    An ICU4X TypedDateFormatter object capable of formatting a :cpp:class:`ICU4XIsoDateTime` as a string, using the Gregorian Calendar.

    See the `Rust documentation for TypedDateFormatter <https://docs.rs/icu/latest/icu/datetime/struct.TypedDateFormatter.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XGregorianDateFormatter, ICU4XError> create_with_length(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XDateLength length)

        Creates a new :cpp:class:`ICU4XGregorianDateFormatter` from locale data.

        See the `Rust documentation for try_new_with_length <https://docs.rs/icu/latest/icu/datetime/struct.TypedDateFormatter.html#method.try_new_with_length>`__ for more information.


    .. cpp:function:: template<typename W> diplomat::result<std::monostate, ICU4XError> format_iso_date_to_writeable(const ICU4XIsoDate& value, W& write) const

        Formats a :cpp:class:`ICU4XIsoDate` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.TypedDateFormatter.html#method.format>`__ for more information.


    .. cpp:function:: diplomat::result<std::string, ICU4XError> format_iso_date(const ICU4XIsoDate& value) const

        Formats a :cpp:class:`ICU4XIsoDate` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.TypedDateFormatter.html#method.format>`__ for more information.


    .. cpp:function:: template<typename W> diplomat::result<std::monostate, ICU4XError> format_iso_datetime_to_writeable(const ICU4XIsoDateTime& value, W& write) const

        Formats a :cpp:class:`ICU4XIsoDateTime` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.TypedDateFormatter.html#method.format>`__ for more information.


    .. cpp:function:: diplomat::result<std::string, ICU4XError> format_iso_datetime(const ICU4XIsoDateTime& value) const

        Formats a :cpp:class:`ICU4XIsoDateTime` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.TypedDateFormatter.html#method.format>`__ for more information.


.. cpp:class:: ICU4XGregorianDateTimeFormatter

    An ICU4X TypedDateTimeFormatter object capable of formatting a :cpp:class:`ICU4XIsoDateTime` as a string, using the Gregorian Calendar.

    See the `Rust documentation for TypedDateTimeFormatter <https://docs.rs/icu/latest/icu/datetime/struct.TypedDateTimeFormatter.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XGregorianDateTimeFormatter, ICU4XError> create_with_lengths(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XDateLength date_length, ICU4XTimeLength time_length)

        Creates a new :cpp:class:`ICU4XGregorianDateFormatter` from locale data.

        See the `Rust documentation for try_new <https://docs.rs/icu/latest/icu/datetime/struct.TypedDateTimeFormatter.html#method.try_new>`__ for more information.


    .. cpp:function:: template<typename W> diplomat::result<std::monostate, ICU4XError> format_iso_datetime_to_writeable(const ICU4XIsoDateTime& value, W& write) const

        Formats a :cpp:class:`ICU4XIsoDateTime` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.TypedDateTimeFormatter.html#method.format>`__ for more information.


    .. cpp:function:: diplomat::result<std::string, ICU4XError> format_iso_datetime(const ICU4XIsoDateTime& value) const

        Formats a :cpp:class:`ICU4XIsoDateTime` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.TypedDateTimeFormatter.html#method.format>`__ for more information.


.. cpp:class:: ICU4XTimeFormatter

    An ICU4X TimeFormatter object capable of formatting an :cpp:class:`ICU4XTime` type (and others) as a string

    See the `Rust documentation for TimeFormatter <https://docs.rs/icu/latest/icu/datetime/struct.TimeFormatter.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XTimeFormatter, ICU4XError> create_with_length(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XTimeLength length)

        Creates a new :cpp:class:`ICU4XTimeFormatter` from locale data.

        See the `Rust documentation for try_new_with_length <https://docs.rs/icu/latest/icu/datetime/struct.TimeFormatter.html#method.try_new_with_length>`__ for more information.


    .. cpp:function:: template<typename W> diplomat::result<std::monostate, ICU4XError> format_time_to_writeable(const ICU4XTime& value, W& write) const

        Formats a :cpp:class:`ICU4XTime` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.TimeFormatter.html#method.format>`__ for more information.


    .. cpp:function:: diplomat::result<std::string, ICU4XError> format_time(const ICU4XTime& value) const

        Formats a :cpp:class:`ICU4XTime` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.TimeFormatter.html#method.format>`__ for more information.


    .. cpp:function:: template<typename W> diplomat::result<std::monostate, ICU4XError> format_datetime_to_writeable(const ICU4XDateTime& value, W& write) const

        Formats a :cpp:class:`ICU4XDateTime` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.TimeFormatter.html#method.format>`__ for more information.


    .. cpp:function:: diplomat::result<std::string, ICU4XError> format_datetime(const ICU4XDateTime& value) const

        Formats a :cpp:class:`ICU4XDateTime` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.TimeFormatter.html#method.format>`__ for more information.


    .. cpp:function:: template<typename W> diplomat::result<std::monostate, ICU4XError> format_iso_datetime_to_writeable(const ICU4XIsoDateTime& value, W& write) const

        Formats a :cpp:class:`ICU4XIsoDateTime` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.TimeFormatter.html#method.format>`__ for more information.


    .. cpp:function:: diplomat::result<std::string, ICU4XError> format_iso_datetime(const ICU4XIsoDateTime& value) const

        Formats a :cpp:class:`ICU4XIsoDateTime` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.TimeFormatter.html#method.format>`__ for more information.


.. cpp:enum-struct:: ICU4XTimeLength

    See the `Rust documentation for Time <https://docs.rs/icu/latest/icu/datetime/options/length/enum.Time.html>`__ for more information.


    .. cpp:enumerator:: Full

    .. cpp:enumerator:: Long

    .. cpp:enumerator:: Medium

    .. cpp:enumerator:: Short
