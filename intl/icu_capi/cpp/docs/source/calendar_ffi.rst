``calendar::ffi``
=================

.. cpp:enum-struct:: ICU4XAnyCalendarKind

    The various calendar types currently supported by :cpp:class:`ICU4XCalendar`

    See the `Rust documentation for AnyCalendarKind <https://docs.rs/icu/latest/icu/calendar/enum.AnyCalendarKind.html>`__ for more information.


    .. cpp:enumerator:: Iso

        The kind of an Iso calendar


    .. cpp:enumerator:: Gregorian

        The kind of a Gregorian calendar


    .. cpp:enumerator:: Buddhist

        The kind of a Buddhist calendar


    .. cpp:enumerator:: Japanese

        The kind of a Japanese calendar with modern eras


    .. cpp:enumerator:: JapaneseExtended

        The kind of a Japanese calendar with modern and historic eras


    .. cpp:enumerator:: Ethiopian

        The kind of an Ethiopian calendar, with Amete Mihret era


    .. cpp:enumerator:: EthiopianAmeteAlem

        The kind of an Ethiopian calendar, with Amete Alem era


    .. cpp:enumerator:: Indian

        The kind of a Indian calendar


    .. cpp:enumerator:: Coptic

        The kind of a Coptic calendar


    .. cpp:enumerator:: Dangi

        The kind of a Dangi calendar


    .. cpp:enumerator:: Chinese

        The kind of a Chinese calendar


    .. cpp:enumerator:: Hebrew

        The kind of a Hebrew calendar


    .. cpp:enumerator:: IslamicCivil

        The kind of a Islamic civil calendar


    .. cpp:enumerator:: IslamicObservational

        The kind of a Islamic observational calendar


    .. cpp:enumerator:: IslamicTabular

        The kind of a Islamic tabular calendar


    .. cpp:enumerator:: IslamicUmmAlQura

        The kind of a Islamic Umm al-Qura calendar


    .. cpp:enumerator:: Persian

        The kind of a Persian calendar


    .. cpp:enumerator:: Roc

        The kind of a Roc calendar


    .. cpp:function:: static diplomat::result<ICU4XAnyCalendarKind, std::monostate> get_for_locale(const ICU4XLocale& locale)

        Read the calendar type off of the -u-ca- extension on a locale.

        Errors if there is no calendar on the locale or if the locale's calendar is not known or supported.

        See the `Rust documentation for get_for_locale <https://docs.rs/icu/latest/icu/calendar/enum.AnyCalendarKind.html#method.get_for_locale>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XAnyCalendarKind, std::monostate> get_for_bcp47(const std::string_view s)

        Obtain the calendar type given a BCP-47 -u-ca- extension string.

        Errors if the calendar is not known or supported.

        See the `Rust documentation for get_for_bcp47_value <https://docs.rs/icu/latest/icu/calendar/enum.AnyCalendarKind.html#method.get_for_bcp47_value>`__ for more information.


    .. cpp:function:: template<typename W> diplomat::result<std::monostate, ICU4XError> bcp47_to_writeable(W& write)

        Obtain the string suitable for use in the -u-ca- extension in a BCP47 locale.

        See the `Rust documentation for as_bcp47_string <https://docs.rs/icu/latest/icu/calendar/enum.AnyCalendarKind.html#method.as_bcp47_string>`__ for more information.


    .. cpp:function:: diplomat::result<std::string, ICU4XError> bcp47()

        Obtain the string suitable for use in the -u-ca- extension in a BCP47 locale.

        See the `Rust documentation for as_bcp47_string <https://docs.rs/icu/latest/icu/calendar/enum.AnyCalendarKind.html#method.as_bcp47_string>`__ for more information.


.. cpp:class:: ICU4XCalendar

    See the `Rust documentation for AnyCalendar <https://docs.rs/icu/latest/icu/calendar/enum.AnyCalendar.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCalendar, ICU4XError> create_for_locale(const ICU4XDataProvider& provider, const ICU4XLocale& locale)

        Creates a new :cpp:class:`ICU4XCalendar` from the specified date and time.

        See the `Rust documentation for new_for_locale <https://docs.rs/icu/latest/icu/calendar/enum.AnyCalendar.html#method.new_for_locale>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCalendar, ICU4XError> create_for_kind(const ICU4XDataProvider& provider, ICU4XAnyCalendarKind kind)

        Creates a new :cpp:class:`ICU4XCalendar` from the specified date and time.

        See the `Rust documentation for new <https://docs.rs/icu/latest/icu/calendar/enum.AnyCalendar.html#method.new>`__ for more information.


    .. cpp:function:: ICU4XAnyCalendarKind kind() const

        Returns the kind of this calendar

        See the `Rust documentation for kind <https://docs.rs/icu/latest/icu/calendar/enum.AnyCalendar.html#method.kind>`__ for more information.

