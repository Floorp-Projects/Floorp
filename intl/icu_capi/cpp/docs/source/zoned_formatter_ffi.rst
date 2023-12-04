``zoned_formatter::ffi``
========================

.. cpp:class:: ICU4XGregorianZonedDateTimeFormatter

    An object capable of formatting a date time with time zone to a string.

    See the `Rust documentation for TypedZonedDateTimeFormatter <https://docs.rs/icu/latest/icu/datetime/struct.TypedZonedDateTimeFormatter.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XGregorianZonedDateTimeFormatter, ICU4XError> create_with_lengths(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XDateLength date_length, ICU4XTimeLength time_length)

        Creates a new :cpp:class:`ICU4XGregorianZonedDateTimeFormatter` from locale data.

        This function has ``date_length`` and ``time_length`` arguments and uses default options for the time zone.

        See the `Rust documentation for try_new <https://docs.rs/icu/latest/icu/datetime/struct.TypedZonedDateTimeFormatter.html#method.try_new>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XGregorianZonedDateTimeFormatter, ICU4XError> create_with_lengths_and_iso_8601_time_zone_fallback(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XDateLength date_length, ICU4XTimeLength time_length, ICU4XIsoTimeZoneOptions zone_options)

        Creates a new :cpp:class:`ICU4XGregorianZonedDateTimeFormatter` from locale data.

        This function has ``date_length`` and ``time_length`` arguments and uses an ISO-8601 style fallback for the time zone with the given configurations.

        See the `Rust documentation for try_new <https://docs.rs/icu/latest/icu/datetime/struct.TypedZonedDateTimeFormatter.html#method.try_new>`__ for more information.


    .. cpp:function:: template<typename W> diplomat::result<std::monostate, ICU4XError> format_iso_datetime_with_custom_time_zone_to_writeable(const ICU4XIsoDateTime& datetime, const ICU4XCustomTimeZone& time_zone, W& write) const

        Formats a :cpp:class:`ICU4XIsoDateTime` and :cpp:class:`ICU4XCustomTimeZone` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.TypedZonedDateTimeFormatter.html#method.format>`__ for more information.


    .. cpp:function:: diplomat::result<std::string, ICU4XError> format_iso_datetime_with_custom_time_zone(const ICU4XIsoDateTime& datetime, const ICU4XCustomTimeZone& time_zone) const

        Formats a :cpp:class:`ICU4XIsoDateTime` and :cpp:class:`ICU4XCustomTimeZone` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.TypedZonedDateTimeFormatter.html#method.format>`__ for more information.


.. cpp:class:: ICU4XZonedDateTimeFormatter

    An object capable of formatting a date time with time zone to a string.

    See the `Rust documentation for ZonedDateTimeFormatter <https://docs.rs/icu/latest/icu/datetime/struct.ZonedDateTimeFormatter.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XZonedDateTimeFormatter, ICU4XError> create_with_lengths(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XDateLength date_length, ICU4XTimeLength time_length)

        Creates a new :cpp:class:`ICU4XZonedDateTimeFormatter` from locale data.

        This function has ``date_length`` and ``time_length`` arguments and uses default options for the time zone.

        See the `Rust documentation for try_new <https://docs.rs/icu/latest/icu/datetime/struct.ZonedDateTimeFormatter.html#method.try_new>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XZonedDateTimeFormatter, ICU4XError> create_with_lengths_and_iso_8601_time_zone_fallback(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XDateLength date_length, ICU4XTimeLength time_length, ICU4XIsoTimeZoneOptions zone_options)

        Creates a new :cpp:class:`ICU4XZonedDateTimeFormatter` from locale data.

        This function has ``date_length`` and ``time_length`` arguments and uses an ISO-8601 style fallback for the time zone with the given configurations.

        See the `Rust documentation for try_new <https://docs.rs/icu/latest/icu/datetime/struct.ZonedDateTimeFormatter.html#method.try_new>`__ for more information.


    .. cpp:function:: template<typename W> diplomat::result<std::monostate, ICU4XError> format_datetime_with_custom_time_zone_to_writeable(const ICU4XDateTime& datetime, const ICU4XCustomTimeZone& time_zone, W& write) const

        Formats a :cpp:class:`ICU4XDateTime` and :cpp:class:`ICU4XCustomTimeZone` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.ZonedDateTimeFormatter.html#method.format>`__ for more information.


    .. cpp:function:: diplomat::result<std::string, ICU4XError> format_datetime_with_custom_time_zone(const ICU4XDateTime& datetime, const ICU4XCustomTimeZone& time_zone) const

        Formats a :cpp:class:`ICU4XDateTime` and :cpp:class:`ICU4XCustomTimeZone` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.ZonedDateTimeFormatter.html#method.format>`__ for more information.


    .. cpp:function:: template<typename W> diplomat::result<std::monostate, ICU4XError> format_iso_datetime_with_custom_time_zone_to_writeable(const ICU4XIsoDateTime& datetime, const ICU4XCustomTimeZone& time_zone, W& write) const

        Formats a :cpp:class:`ICU4XIsoDateTime` and :cpp:class:`ICU4XCustomTimeZone` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.ZonedDateTimeFormatter.html#method.format>`__ for more information.


    .. cpp:function:: diplomat::result<std::string, ICU4XError> format_iso_datetime_with_custom_time_zone(const ICU4XIsoDateTime& datetime, const ICU4XCustomTimeZone& time_zone) const

        Formats a :cpp:class:`ICU4XIsoDateTime` and :cpp:class:`ICU4XCustomTimeZone` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/struct.ZonedDateTimeFormatter.html#method.format>`__ for more information.

