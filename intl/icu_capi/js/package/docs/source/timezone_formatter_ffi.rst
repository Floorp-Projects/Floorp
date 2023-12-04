``timezone_formatter::ffi``
===========================

.. js:class:: ICU4XIsoTimeZoneFormat

    See the `Rust documentation for IsoFormat <https://docs.rs/icu/latest/icu/datetime/time_zone/enum.IsoFormat.html>`__ for more information.


.. js:class:: ICU4XIsoTimeZoneMinuteDisplay

    See the `Rust documentation for IsoMinutes <https://docs.rs/icu/latest/icu/datetime/time_zone/enum.IsoMinutes.html>`__ for more information.


.. js:class:: ICU4XIsoTimeZoneOptions

    .. js:attribute:: format

    .. js:attribute:: minutes

    .. js:attribute:: seconds

.. js:class:: ICU4XIsoTimeZoneSecondDisplay

    See the `Rust documentation for IsoSeconds <https://docs.rs/icu/latest/icu/datetime/time_zone/enum.IsoSeconds.html>`__ for more information.


.. js:class:: ICU4XTimeZoneFormatter

    An ICU4X TimeZoneFormatter object capable of formatting an :js:class:`ICU4XCustomTimeZone` type (and others) as a string

    See the `Rust documentation for TimeZoneFormatter <https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html>`__ for more information.


    .. js:function:: create_with_localized_gmt_fallback(provider, locale)

        Creates a new :js:class:`ICU4XTimeZoneFormatter` from locale data.

        Uses localized GMT as the fallback format.

        See the `Rust documentation for try_new <https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.try_new>`__ for more information.

        Additional information: `1 <https://docs.rs/icu/latest/icu/datetime/time_zone/enum.FallbackFormat.html>`__


    .. js:function:: create_with_iso_8601_fallback(provider, locale, options)

        Creates a new :js:class:`ICU4XTimeZoneFormatter` from locale data.

        Uses ISO-8601 as the fallback format.

        See the `Rust documentation for try_new <https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.try_new>`__ for more information.

        Additional information: `1 <https://docs.rs/icu/latest/icu/datetime/time_zone/enum.FallbackFormat.html>`__


    .. js:method:: load_generic_non_location_long(provider)

        Loads generic non-location long format. Example: "Pacific Time"

        See the `Rust documentation for include_generic_non_location_long <https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.include_generic_non_location_long>`__ for more information.


    .. js:method:: load_generic_non_location_short(provider)

        Loads generic non-location short format. Example: "PT"

        See the `Rust documentation for include_generic_non_location_short <https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.include_generic_non_location_short>`__ for more information.


    .. js:method:: load_specific_non_location_long(provider)

        Loads specific non-location long format. Example: "Pacific Standard Time"

        See the `Rust documentation for include_specific_non_location_long <https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.include_specific_non_location_long>`__ for more information.


    .. js:method:: load_specific_non_location_short(provider)

        Loads specific non-location short format. Example: "PST"

        See the `Rust documentation for include_specific_non_location_short <https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.include_specific_non_location_short>`__ for more information.


    .. js:method:: load_generic_location_format(provider)

        Loads generic location format. Example: "Los Angeles Time"

        See the `Rust documentation for include_generic_location_format <https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.include_generic_location_format>`__ for more information.


    .. js:method:: include_localized_gmt_format()

        Loads localized GMT format. Example: "GMT-07:00"

        See the `Rust documentation for include_localized_gmt_format <https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.include_localized_gmt_format>`__ for more information.


    .. js:method:: load_iso_8601_format(options)

        Loads ISO-8601 format. Example: "-07:00"

        See the `Rust documentation for include_iso_8601_format <https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.include_iso_8601_format>`__ for more information.


    .. js:method:: format_custom_time_zone(value)

        Formats a :js:class:`ICU4XCustomTimeZone` to a string.

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.format>`__ for more information.

        See the `Rust documentation for format_to_string <https://docs.rs/icu/latest/icu/datetime/time_zone/struct.TimeZoneFormatter.html#method.format_to_string>`__ for more information.

