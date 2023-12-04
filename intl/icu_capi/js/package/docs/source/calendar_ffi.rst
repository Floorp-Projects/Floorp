``calendar::ffi``
=================

.. js:class:: ICU4XAnyCalendarKind

    The various calendar types currently supported by :js:class:`ICU4XCalendar`

    See the `Rust documentation for AnyCalendarKind <https://docs.rs/icu/latest/icu/calendar/enum.AnyCalendarKind.html>`__ for more information.


    .. js:function:: get_for_locale(locale)

        Read the calendar type off of the -u-ca- extension on a locale.

        Errors if there is no calendar on the locale or if the locale's calendar is not known or supported.

        See the `Rust documentation for get_for_locale <https://docs.rs/icu/latest/icu/calendar/enum.AnyCalendarKind.html#method.get_for_locale>`__ for more information.


    .. js:function:: get_for_bcp47(s)

        Obtain the calendar type given a BCP-47 -u-ca- extension string.

        Errors if the calendar is not known or supported.

        See the `Rust documentation for get_for_bcp47_value <https://docs.rs/icu/latest/icu/calendar/enum.AnyCalendarKind.html#method.get_for_bcp47_value>`__ for more information.


    .. js:method:: bcp47()

        Obtain the string suitable for use in the -u-ca- extension in a BCP47 locale.

        See the `Rust documentation for as_bcp47_string <https://docs.rs/icu/latest/icu/calendar/enum.AnyCalendarKind.html#method.as_bcp47_string>`__ for more information.


.. js:class:: ICU4XCalendar

    See the `Rust documentation for AnyCalendar <https://docs.rs/icu/latest/icu/calendar/enum.AnyCalendar.html>`__ for more information.


    .. js:function:: create_for_locale(provider, locale)

        Creates a new :js:class:`ICU4XCalendar` from the specified date and time.

        See the `Rust documentation for new_for_locale <https://docs.rs/icu/latest/icu/calendar/enum.AnyCalendar.html#method.new_for_locale>`__ for more information.


    .. js:function:: create_for_kind(provider, kind)

        Creates a new :js:class:`ICU4XCalendar` from the specified date and time.

        See the `Rust documentation for new <https://docs.rs/icu/latest/icu/calendar/enum.AnyCalendar.html#method.new>`__ for more information.


    .. js:method:: kind()

        Returns the kind of this calendar

        See the `Rust documentation for kind <https://docs.rs/icu/latest/icu/calendar/enum.AnyCalendar.html#method.kind>`__ for more information.

