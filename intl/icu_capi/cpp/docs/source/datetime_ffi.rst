``datetime::ffi``
=================

.. cpp:class:: ICU4XDateTime

    An ICU4X DateTime object capable of containing a date and time for any calendar.

    See the `Rust documentation for DateTime <https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XDateTime, ICU4XError> create_from_iso_in_calendar(int32_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, uint32_t nanosecond, const ICU4XCalendar& calendar)

        Creates a new :cpp:class:`ICU4XDateTime` representing the ISO date and time given but in a given calendar

        See the `Rust documentation for new_from_iso <https://docs.rs/icu/latest/icu/struct.DateTime.html#method.new_from_iso>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XDateTime, ICU4XError> create_from_codes_in_calendar(const std::string_view era_code, int32_t year, const std::string_view month_code, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, uint32_t nanosecond, const ICU4XCalendar& calendar)

        Creates a new :cpp:class:`ICU4XDateTime` from the given codes, which are interpreted in the given calendar system

        See the `Rust documentation for try_new_from_codes <https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html#method.try_new_from_codes>`__ for more information.


    .. cpp:function:: static ICU4XDateTime create_from_date_and_time(const ICU4XDate& date, const ICU4XTime& time)

        Creates a new :cpp:class:`ICU4XDateTime` from an :cpp:class:`ICU4XDate` and :cpp:class:`ICU4XTime` object

        See the `Rust documentation for new <https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html#method.new>`__ for more information.


    .. cpp:function:: ICU4XDate date() const

        Gets a copy of the date contained in this object

        See the `Rust documentation for date <https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html#structfield.date>`__ for more information.


    .. cpp:function:: ICU4XTime time() const

        Gets the time contained in this object

        See the `Rust documentation for time <https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html#structfield.time>`__ for more information.


    .. cpp:function:: ICU4XIsoDateTime to_iso() const

        Converts this date to ISO

        See the `Rust documentation for to_iso <https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html#method.to_iso>`__ for more information.


    .. cpp:function:: ICU4XDateTime to_calendar(const ICU4XCalendar& calendar) const

        Convert this datetime to one in a different calendar

        See the `Rust documentation for to_calendar <https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html#method.to_calendar>`__ for more information.


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


    .. cpp:function:: uint32_t day_of_month() const

        Returns the 1-indexed day in the month for this date

        See the `Rust documentation for day_of_month <https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.day_of_month>`__ for more information.


    .. cpp:function:: ICU4XIsoWeekday day_of_week() const

        Returns the day in the week for this day

        See the `Rust documentation for day_of_week <https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.day_of_week>`__ for more information.


    .. cpp:function:: uint32_t week_of_month(ICU4XIsoWeekday first_weekday) const

        Returns the week number in this month, 1-indexed, based on what is considered the first day of the week (often a locale preference).

        ``first_weekday`` can be obtained via ``first_weekday()`` on :cpp:class:`ICU4XWeekCalculator`

        See the `Rust documentation for week_of_month <https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.week_of_month>`__ for more information.


    .. cpp:function:: diplomat::result<ICU4XWeekOf, ICU4XError> week_of_year(const ICU4XWeekCalculator& calculator) const

        Returns the week number in this year, using week data

        See the `Rust documentation for week_of_year <https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.week_of_year>`__ for more information.


    .. cpp:function:: uint32_t ordinal_month() const

        Returns 1-indexed number of the month of this date in its year

        Note that for lunar calendars this may not lead to the same month having the same ordinal month across years; use month_code if you care about month identity.

        See the `Rust documentation for month <https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.month>`__ for more information.


    .. cpp:function:: template<typename W> diplomat::result<std::monostate, ICU4XError> month_code_to_writeable(W& write) const

        Returns the month code for this date. Typically something like "M01", "M02", but can be more complicated for lunar calendars.

        See the `Rust documentation for month <https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.month>`__ for more information.


    .. cpp:function:: diplomat::result<std::string, ICU4XError> month_code() const

        Returns the month code for this date. Typically something like "M01", "M02", but can be more complicated for lunar calendars.

        See the `Rust documentation for month <https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.month>`__ for more information.


    .. cpp:function:: int32_t year_in_era() const

        Returns the year number in the current era for this date

        See the `Rust documentation for year <https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.year>`__ for more information.


    .. cpp:function:: template<typename W> diplomat::result<std::monostate, ICU4XError> era_to_writeable(W& write) const

        Returns the era for this date,

        See the `Rust documentation for year <https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.year>`__ for more information.


    .. cpp:function:: diplomat::result<std::string, ICU4XError> era() const

        Returns the era for this date,

        See the `Rust documentation for year <https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.year>`__ for more information.


    .. cpp:function:: uint8_t months_in_year() const

        Returns the number of months in the year represented by this date

        See the `Rust documentation for months_in_year <https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.months_in_year>`__ for more information.


    .. cpp:function:: uint8_t days_in_month() const

        Returns the number of days in the month represented by this date

        See the `Rust documentation for days_in_month <https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.days_in_month>`__ for more information.


    .. cpp:function:: uint16_t days_in_year() const

        Returns the number of days in the year represented by this date

        See the `Rust documentation for days_in_year <https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.days_in_year>`__ for more information.


    .. cpp:function:: ICU4XCalendar calendar() const

        Returns the :cpp:class:`ICU4XCalendar` object backing this date

        See the `Rust documentation for calendar <https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.calendar>`__ for more information.


.. cpp:class:: ICU4XIsoDateTime

    An ICU4X DateTime object capable of containing a ISO-8601 date and time.

    See the `Rust documentation for DateTime <https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XIsoDateTime, ICU4XError> create(int32_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, uint32_t nanosecond)

        Creates a new :cpp:class:`ICU4XIsoDateTime` from the specified date and time.

        See the `Rust documentation for try_new_iso_datetime <https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html#method.try_new_iso_datetime>`__ for more information.


    .. cpp:function:: static ICU4XIsoDateTime crate_from_date_and_time(const ICU4XIsoDate& date, const ICU4XTime& time)

        Creates a new :cpp:class:`ICU4XIsoDateTime` from an :cpp:class:`ICU4XIsoDate` and :cpp:class:`ICU4XTime` object

        See the `Rust documentation for new <https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html#method.new>`__ for more information.


    .. cpp:function:: static ICU4XIsoDateTime create_from_minutes_since_local_unix_epoch(int32_t minutes)

        Construct from the minutes since the local unix epoch for this date (Jan 1 1970, 00:00)

        See the `Rust documentation for from_minutes_since_local_unix_epoch <https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html#method.from_minutes_since_local_unix_epoch>`__ for more information.


    .. cpp:function:: ICU4XIsoDate date() const

        Gets the date contained in this object

        See the `Rust documentation for date <https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html#structfield.date>`__ for more information.


    .. cpp:function:: ICU4XTime time() const

        Gets the time contained in this object

        See the `Rust documentation for time <https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html#structfield.time>`__ for more information.


    .. cpp:function:: ICU4XDateTime to_any() const

        Converts this to an :cpp:class:`ICU4XDateTime` capable of being mixed with dates of other calendars

        See the `Rust documentation for to_any <https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html#method.to_any>`__ for more information.


    .. cpp:function:: int32_t minutes_since_local_unix_epoch() const

        Gets the minutes since the local unix epoch for this date (Jan 1 1970, 00:00)

        See the `Rust documentation for minutes_since_local_unix_epoch <https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html#method.minutes_since_local_unix_epoch>`__ for more information.


    .. cpp:function:: ICU4XDateTime to_calendar(const ICU4XCalendar& calendar) const

        Convert this datetime to one in a different calendar

        See the `Rust documentation for to_calendar <https://docs.rs/icu/latest/icu/calendar/struct.DateTime.html#method.to_calendar>`__ for more information.


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


    .. cpp:function:: uint32_t day_of_month() const

        Returns the 1-indexed day in the month for this date

        See the `Rust documentation for day_of_month <https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.day_of_month>`__ for more information.


    .. cpp:function:: ICU4XIsoWeekday day_of_week() const

        Returns the day in the week for this day

        See the `Rust documentation for day_of_week <https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.day_of_week>`__ for more information.


    .. cpp:function:: uint32_t week_of_month(ICU4XIsoWeekday first_weekday) const

        Returns the week number in this month, 1-indexed, based on what is considered the first day of the week (often a locale preference).

        ``first_weekday`` can be obtained via ``first_weekday()`` on :cpp:class:`ICU4XWeekCalculator`

        See the `Rust documentation for week_of_month <https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.week_of_month>`__ for more information.


    .. cpp:function:: diplomat::result<ICU4XWeekOf, ICU4XError> week_of_year(const ICU4XWeekCalculator& calculator) const

        Returns the week number in this year, using week data

        See the `Rust documentation for week_of_year <https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.week_of_year>`__ for more information.


    .. cpp:function:: uint32_t month() const

        Returns 1-indexed number of the month of this date in its year

        See the `Rust documentation for month <https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.month>`__ for more information.


    .. cpp:function:: int32_t year() const

        Returns the year number for this date

        See the `Rust documentation for year <https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.year>`__ for more information.


    .. cpp:function:: bool is_in_leap_year() const

        Returns whether this date is in a leap year

        See the `Rust documentation for is_in_leap_year <https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.is_in_leap_year>`__ for more information.


    .. cpp:function:: uint8_t months_in_year() const

        Returns the number of months in the year represented by this date

        See the `Rust documentation for months_in_year <https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.months_in_year>`__ for more information.


    .. cpp:function:: uint8_t days_in_month() const

        Returns the number of days in the month represented by this date

        See the `Rust documentation for days_in_month <https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.days_in_month>`__ for more information.


    .. cpp:function:: uint16_t days_in_year() const

        Returns the number of days in the year represented by this date

        See the `Rust documentation for days_in_year <https://docs.rs/icu/latest/icu/calendar/struct.Date.html#method.days_in_year>`__ for more information.

