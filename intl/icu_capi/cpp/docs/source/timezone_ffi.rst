``timezone::ffi``
=================

.. cpp:class:: ICU4XCustomTimeZone

    See the `Rust documentation for CustomTimeZone <https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCustomTimeZone, ICU4XError> create_from_string(const std::string_view s)

        Creates a time zone from an offset string.

        See the `Rust documentation for from_str <https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#method.from_str>`__ for more information.


    .. cpp:function:: static ICU4XCustomTimeZone create_empty()

        Creates a time zone with no information.

        See the `Rust documentation for new_empty <https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#method.new_empty>`__ for more information.


    .. cpp:function:: static ICU4XCustomTimeZone create_utc()

        Creates a time zone for UTC.

        See the `Rust documentation for utc <https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#method.utc>`__ for more information.


    .. cpp:function:: diplomat::result<std::monostate, ICU4XError> try_set_gmt_offset_seconds(int32_t offset_seconds)

        Sets the ``gmt_offset`` field from offset seconds.

        Errors if the offset seconds are out of range.

        See the `Rust documentation for try_from_offset_seconds <https://docs.rs/icu/latest/icu/timezone/struct.GmtOffset.html#method.try_from_offset_seconds>`__ for more information.

        Additional information: `1 <https://docs.rs/icu/latest/icu/timezone/struct.GmtOffset.html>`__


    .. cpp:function:: void clear_gmt_offset()

        Clears the ``gmt_offset`` field.

        See the `Rust documentation for offset_seconds <https://docs.rs/icu/latest/icu/timezone/struct.GmtOffset.html#method.offset_seconds>`__ for more information.

        Additional information: `1 <https://docs.rs/icu/latest/icu/timezone/struct.GmtOffset.html>`__


    .. cpp:function:: diplomat::result<int32_t, ICU4XError> gmt_offset_seconds() const

        Returns the value of the ``gmt_offset`` field as offset seconds.

        Errors if the ``gmt_offset`` field is empty.

        See the `Rust documentation for offset_seconds <https://docs.rs/icu/latest/icu/timezone/struct.GmtOffset.html#method.offset_seconds>`__ for more information.

        Additional information: `1 <https://docs.rs/icu/latest/icu/timezone/struct.GmtOffset.html>`__


    .. cpp:function:: diplomat::result<bool, ICU4XError> is_gmt_offset_positive() const

        Returns whether the ``gmt_offset`` field is positive.

        Errors if the ``gmt_offset`` field is empty.

        See the `Rust documentation for is_positive <https://docs.rs/icu/latest/icu/timezone/struct.GmtOffset.html#method.is_positive>`__ for more information.


    .. cpp:function:: diplomat::result<bool, ICU4XError> is_gmt_offset_zero() const

        Returns whether the ``gmt_offset`` field is zero.

        Errors if the ``gmt_offset`` field is empty (which is not the same as zero).

        See the `Rust documentation for is_zero <https://docs.rs/icu/latest/icu/timezone/struct.GmtOffset.html#method.is_zero>`__ for more information.


    .. cpp:function:: diplomat::result<bool, ICU4XError> gmt_offset_has_minutes() const

        Returns whether the ``gmt_offset`` field has nonzero minutes.

        Errors if the ``gmt_offset`` field is empty.

        See the `Rust documentation for has_minutes <https://docs.rs/icu/latest/icu/timezone/struct.GmtOffset.html#method.has_minutes>`__ for more information.


    .. cpp:function:: diplomat::result<bool, ICU4XError> gmt_offset_has_seconds() const

        Returns whether the ``gmt_offset`` field has nonzero seconds.

        Errors if the ``gmt_offset`` field is empty.

        See the `Rust documentation for has_seconds <https://docs.rs/icu/latest/icu/timezone/struct.GmtOffset.html#method.has_seconds>`__ for more information.


    .. cpp:function:: diplomat::result<std::monostate, ICU4XError> try_set_time_zone_id(const std::string_view id)

        Sets the ``time_zone_id`` field from a BCP-47 string.

        Errors if the string is not a valid BCP-47 time zone ID.

        See the `Rust documentation for time_zone_id <https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#structfield.time_zone_id>`__ for more information.

        Additional information: `1 <https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneBcp47Id.html>`__


    .. cpp:function:: diplomat::result<std::monostate, ICU4XError> try_set_iana_time_zone_id(const ICU4XIanaToBcp47Mapper& mapper, const std::string_view id)

        Sets the ``time_zone_id`` field from an IANA string by looking up the corresponding BCP-47 string.

        Errors if the string is not a valid BCP-47 time zone ID.

        See the `Rust documentation for get <https://docs.rs/icu/latest/icu/timezone/struct.IanaToBcp47MapperBorrowed.html#method.get>`__ for more information.


    .. cpp:function:: void clear_time_zone_id()

        Clears the ``time_zone_id`` field.

        See the `Rust documentation for time_zone_id <https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#structfield.time_zone_id>`__ for more information.

        Additional information: `1 <https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneBcp47Id.html>`__


    .. cpp:function:: template<typename W> diplomat::result<std::monostate, ICU4XError> time_zone_id_to_writeable(W& write) const

        Writes the value of the ``time_zone_id`` field as a string.

        Errors if the ``time_zone_id`` field is empty.

        See the `Rust documentation for time_zone_id <https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#structfield.time_zone_id>`__ for more information.

        Additional information: `1 <https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneBcp47Id.html>`__


    .. cpp:function:: diplomat::result<std::string, ICU4XError> time_zone_id() const

        Writes the value of the ``time_zone_id`` field as a string.

        Errors if the ``time_zone_id`` field is empty.

        See the `Rust documentation for time_zone_id <https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#structfield.time_zone_id>`__ for more information.

        Additional information: `1 <https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneBcp47Id.html>`__


    .. cpp:function:: diplomat::result<std::monostate, ICU4XError> try_set_metazone_id(const std::string_view id)

        Sets the ``metazone_id`` field from a string.

        Errors if the string is not a valid BCP-47 metazone ID.

        See the `Rust documentation for metazone_id <https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#structfield.metazone_id>`__ for more information.

        Additional information: `1 <https://docs.rs/icu/latest/icu/timezone/struct.MetazoneId.html>`__


    .. cpp:function:: void clear_metazone_id()

        Clears the ``metazone_id`` field.

        See the `Rust documentation for metazone_id <https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#structfield.metazone_id>`__ for more information.

        Additional information: `1 <https://docs.rs/icu/latest/icu/timezone/struct.MetazoneId.html>`__


    .. cpp:function:: template<typename W> diplomat::result<std::monostate, ICU4XError> metazone_id_to_writeable(W& write) const

        Writes the value of the ``metazone_id`` field as a string.

        Errors if the ``metazone_id`` field is empty.

        See the `Rust documentation for metazone_id <https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#structfield.metazone_id>`__ for more information.

        Additional information: `1 <https://docs.rs/icu/latest/icu/timezone/struct.MetazoneId.html>`__


    .. cpp:function:: diplomat::result<std::string, ICU4XError> metazone_id() const

        Writes the value of the ``metazone_id`` field as a string.

        Errors if the ``metazone_id`` field is empty.

        See the `Rust documentation for metazone_id <https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#structfield.metazone_id>`__ for more information.

        Additional information: `1 <https://docs.rs/icu/latest/icu/timezone/struct.MetazoneId.html>`__


    .. cpp:function:: diplomat::result<std::monostate, ICU4XError> try_set_zone_variant(const std::string_view id)

        Sets the ``zone_variant`` field from a string.

        Errors if the string is not a valid zone variant.

        See the `Rust documentation for zone_variant <https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#structfield.zone_variant>`__ for more information.

        Additional information: `1 <https://docs.rs/icu/latest/icu/timezone/struct.ZoneVariant.html>`__


    .. cpp:function:: void clear_zone_variant()

        Clears the ``zone_variant`` field.

        See the `Rust documentation for zone_variant <https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#structfield.zone_variant>`__ for more information.

        Additional information: `1 <https://docs.rs/icu/latest/icu/timezone/struct.ZoneVariant.html>`__


    .. cpp:function:: template<typename W> diplomat::result<std::monostate, ICU4XError> zone_variant_to_writeable(W& write) const

        Writes the value of the ``zone_variant`` field as a string.

        Errors if the ``zone_variant`` field is empty.

        See the `Rust documentation for zone_variant <https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#structfield.zone_variant>`__ for more information.

        Additional information: `1 <https://docs.rs/icu/latest/icu/timezone/struct.ZoneVariant.html>`__


    .. cpp:function:: diplomat::result<std::string, ICU4XError> zone_variant() const

        Writes the value of the ``zone_variant`` field as a string.

        Errors if the ``zone_variant`` field is empty.

        See the `Rust documentation for zone_variant <https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#structfield.zone_variant>`__ for more information.

        Additional information: `1 <https://docs.rs/icu/latest/icu/timezone/struct.ZoneVariant.html>`__


    .. cpp:function:: void set_standard_time()

        Sets the ``zone_variant`` field to standard time.

        See the `Rust documentation for standard <https://docs.rs/icu/latest/icu/timezone/struct.ZoneVariant.html#method.standard>`__ for more information.

        Additional information: `1 <https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#structfield.zone_variant>`__


    .. cpp:function:: void set_daylight_time()

        Sets the ``zone_variant`` field to daylight time.

        See the `Rust documentation for daylight <https://docs.rs/icu/latest/icu/timezone/struct.ZoneVariant.html#method.daylight>`__ for more information.

        Additional information: `1 <https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#structfield.zone_variant>`__


    .. cpp:function:: diplomat::result<bool, ICU4XError> is_standard_time() const

        Returns whether the ``zone_variant`` field is standard time.

        Errors if the ``zone_variant`` field is empty.

        See the `Rust documentation for standard <https://docs.rs/icu/latest/icu/timezone/struct.ZoneVariant.html#method.standard>`__ for more information.

        Additional information: `1 <https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#structfield.zone_variant>`__


    .. cpp:function:: diplomat::result<bool, ICU4XError> is_daylight_time() const

        Returns whether the ``zone_variant`` field is daylight time.

        Errors if the ``zone_variant`` field is empty.

        See the `Rust documentation for daylight <https://docs.rs/icu/latest/icu/timezone/struct.ZoneVariant.html#method.daylight>`__ for more information.

        Additional information: `1 <https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#structfield.zone_variant>`__


    .. cpp:function:: void maybe_calculate_metazone(const ICU4XMetazoneCalculator& metazone_calculator, const ICU4XIsoDateTime& local_datetime)

        Sets the metazone based on the time zone and the local timestamp.

        See the `Rust documentation for maybe_calculate_metazone <https://docs.rs/icu/latest/icu/timezone/struct.CustomTimeZone.html#method.maybe_calculate_metazone>`__ for more information.

        Additional information: `1 <https://docs.rs/icu/latest/icu/timezone/struct.MetazoneCalculator.html#method.compute_metazone_from_time_zone>`__

