``errors::ffi``
===============

.. cpp:enum-struct:: ICU4XError

    A common enum for errors that ICU4X may return, organized by API

    The error names are stable and can be checked against as strings in the JS API

    Additional information: `1 <https://docs.rs/fixed_decimal/latest/fixed_decimal/enum.FixedDecimalError.html>`__, `2 <https://docs.rs/icu/latest/icu/calendar/enum.CalendarError.html>`__, `3 <https://docs.rs/icu/latest/icu/collator/enum.CollatorError.html>`__, `4 <https://docs.rs/icu/latest/icu/datetime/enum.DateTimeError.html>`__, `5 <https://docs.rs/icu/latest/icu/decimal/enum.DecimalError.html>`__, `6 <https://docs.rs/icu/latest/icu/list/enum.ListError.html>`__, `7 <https://docs.rs/icu/latest/icu/locid/enum.ParserError.html>`__, `8 <https://docs.rs/icu/latest/icu/locid_transform/enum.LocaleTransformError.html>`__, `9 <https://docs.rs/icu/latest/icu/normalizer/enum.NormalizerError.html>`__, `10 <https://docs.rs/icu/latest/icu/plurals/enum.PluralsError.html>`__, `11 <https://docs.rs/icu/latest/icu/properties/enum.PropertiesError.html>`__, `12 <https://docs.rs/icu/latest/icu/provider/struct.DataError.html>`__, `13 <https://docs.rs/icu/latest/icu/provider/enum.DataErrorKind.html>`__, `14 <https://docs.rs/icu/latest/icu/segmenter/enum.SegmenterError.html>`__, `15 <https://docs.rs/icu/latest/icu/timezone/enum.TimeZoneError.html>`__


    .. cpp:enumerator:: UnknownError

        The error is not currently categorized as ICU4XError. Please file a bug


    .. cpp:enumerator:: WriteableError

        An error arising from writing to a string Typically found when not enough space is allocated Most APIs that return a string may return this error


    .. cpp:enumerator:: OutOfBoundsError

    .. cpp:enumerator:: DataMissingDataKeyError

    .. cpp:enumerator:: DataMissingVariantError

    .. cpp:enumerator:: DataMissingLocaleError

    .. cpp:enumerator:: DataNeedsVariantError

    .. cpp:enumerator:: DataNeedsLocaleError

    .. cpp:enumerator:: DataExtraneousLocaleError

    .. cpp:enumerator:: DataFilteredResourceError

    .. cpp:enumerator:: DataMismatchedTypeError

    .. cpp:enumerator:: DataMissingPayloadError

    .. cpp:enumerator:: DataInvalidStateError

    .. cpp:enumerator:: DataCustomError

    .. cpp:enumerator:: DataIoError

    .. cpp:enumerator:: DataUnavailableBufferFormatError

    .. cpp:enumerator:: DataMismatchedAnyBufferError

    .. cpp:enumerator:: LocaleUndefinedSubtagError

        The subtag being requested was not set


    .. cpp:enumerator:: LocaleParserLanguageError

        The locale or subtag string failed to parse


    .. cpp:enumerator:: LocaleParserSubtagError

    .. cpp:enumerator:: LocaleParserExtensionError

    .. cpp:enumerator:: DataStructValidityError

        Attempted to construct an invalid data struct


    .. cpp:enumerator:: PropertyUnknownScriptIdError

    .. cpp:enumerator:: PropertyUnknownGeneralCategoryGroupError

    .. cpp:enumerator:: PropertyUnexpectedPropertyNameError

    .. cpp:enumerator:: FixedDecimalLimitError

    .. cpp:enumerator:: FixedDecimalSyntaxError

    .. cpp:enumerator:: PluralsParserError

    .. cpp:enumerator:: CalendarParseError

    .. cpp:enumerator:: CalendarOverflowError

    .. cpp:enumerator:: CalendarUnderflowError

    .. cpp:enumerator:: CalendarOutOfRangeError

    .. cpp:enumerator:: CalendarUnknownEraError

    .. cpp:enumerator:: CalendarUnknownMonthCodeError

    .. cpp:enumerator:: CalendarMissingInputError

    .. cpp:enumerator:: CalendarUnknownKindError

    .. cpp:enumerator:: CalendarMissingError

    .. cpp:enumerator:: DateTimePatternError

    .. cpp:enumerator:: DateTimeMissingInputFieldError

    .. cpp:enumerator:: DateTimeSkeletonError

    .. cpp:enumerator:: DateTimeUnsupportedFieldError

    .. cpp:enumerator:: DateTimeUnsupportedOptionsError

    .. cpp:enumerator:: DateTimeMissingWeekdaySymbolError

    .. cpp:enumerator:: DateTimeMissingMonthSymbolError

    .. cpp:enumerator:: DateTimeFixedDecimalError

    .. cpp:enumerator:: DateTimeMismatchedCalendarError

    .. cpp:enumerator:: TinyStrTooLargeError

    .. cpp:enumerator:: TinyStrContainsNullError

    .. cpp:enumerator:: TinyStrNonAsciiError

    .. cpp:enumerator:: TimeZoneOffsetOutOfBoundsError

    .. cpp:enumerator:: TimeZoneInvalidOffsetError

    .. cpp:enumerator:: TimeZoneMissingInputError

    .. cpp:enumerator:: TimeZoneInvalidIdError

    .. cpp:enumerator:: NormalizerFutureExtensionError

    .. cpp:enumerator:: NormalizerValidationError
