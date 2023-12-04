``iana_bcp47_mapper::ffi``
==========================

.. js:class:: ICU4XBcp47ToIanaMapper

    An object capable of mapping from a BCP-47 time zone ID to an IANA ID.

    See the `Rust documentation for IanaBcp47RoundTripMapper <https://docs.rs/icu/latest/icu/timezone/struct.IanaBcp47RoundTripMapper.html>`__ for more information.


    .. js:function:: create(provider)

        See the `Rust documentation for new <https://docs.rs/icu/latest/icu/timezone/struct.IanaBcp47RoundTripMapper.html#method.new>`__ for more information.


    .. js:method:: get(value)

        Writes out the canonical IANA time zone ID corresponding to the given BCP-47 ID.

        See the `Rust documentation for bcp47_to_iana <https://docs.rs/icu/latest/icu/datetime/time_zone/struct.IanaBcp47RoundTripMapper.html#method.bcp47_to_iana>`__ for more information.


.. js:class:: ICU4XIanaToBcp47Mapper

    An object capable of mapping from an IANA time zone ID to a BCP-47 ID.

    This can be used via ``try_set_iana_time_zone_id()`` on ```ICU4XCustomTimeZone`` <crate::timezone::ffi::ICU4XCustomTimeZone>`__.

    See the `Rust documentation for IanaToBcp47Mapper <https://docs.rs/icu/latest/icu/timezone/struct.IanaToBcp47Mapper.html>`__ for more information.


    .. js:function:: create(provider)

        See the `Rust documentation for new <https://docs.rs/icu/latest/icu/timezone/struct.IanaToBcp47Mapper.html#method.new>`__ for more information.

