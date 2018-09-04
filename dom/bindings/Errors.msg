/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * The format for each error message is:
 *
 * MSG_DEF(<SYMBOLIC_NAME>, <ARGUMENT_COUNT>, <JS_EXN_TYPE>, <FORMAT_STRING>)
 *
 * where
 *
 * <SYMBOLIC_NAME> is a legal C++ identifer that will be used in the source.
 *
 * <ARGUMENT_COUNT> is an integer literal specifying the total number of
 * replaceable arguments in the following format string.
 *
 * <JS_EXN_TYPE> is a JSExnType which specifies which kind of error the JS
 * engine should throw.
 *
 * <FORMAT_STRING> is a string literal, containing <ARGUMENT_COUNT> sequences
 * {X} where X  is an integer representing the argument number that will
 * be replaced with a string value when the error is reported.
 */

MSG_DEF(MSG_INVALID_ENUM_VALUE, 3, JSEXN_TYPEERR, "{0} '{1}' is not a valid value for enumeration {2}.")
MSG_DEF(MSG_MISSING_ARGUMENTS, 1, JSEXN_TYPEERR, "Not enough arguments to {0}.")
MSG_DEF(MSG_NOT_OBJECT, 1, JSEXN_TYPEERR, "{0} is not an object.")
MSG_DEF(MSG_NOT_CALLABLE, 1, JSEXN_TYPEERR, "{0} is not callable.")
MSG_DEF(MSG_NOT_CONSTRUCTOR, 1, JSEXN_TYPEERR, "{0} is not a constructor.")
MSG_DEF(MSG_DOES_NOT_IMPLEMENT_INTERFACE, 2, JSEXN_TYPEERR, "{0} does not implement interface {1}.")
MSG_DEF(MSG_METHOD_THIS_DOES_NOT_IMPLEMENT_INTERFACE, 2, JSEXN_TYPEERR, "'{0}' called on an object that does not implement interface {1}.")
MSG_DEF(MSG_METHOD_THIS_UNWRAPPING_DENIED, 1, JSEXN_TYPEERR, "Permission to call '{0}' denied.")
MSG_DEF(MSG_THIS_DOES_NOT_IMPLEMENT_INTERFACE, 1, JSEXN_TYPEERR, "\"this\" object does not implement interface {0}.")
MSG_DEF(MSG_NOT_IN_UNION, 2, JSEXN_TYPEERR, "{0} could not be converted to any of: {1}.")
MSG_DEF(MSG_ILLEGAL_CONSTRUCTOR, 0, JSEXN_TYPEERR, "Illegal constructor.")
MSG_DEF(MSG_ILLEGAL_TYPE_PR_CONSTRUCTOR, 1, JSEXN_TYPEERR, "TypeError:{0}")
MSG_DEF(MSG_ILLEGAL_RANGE_PR_CONSTRUCTOR, 1, JSEXN_RANGEERR, "RangeError:{0}")
MSG_DEF(MSG_CONSTRUCTOR_WITHOUT_NEW, 1, JSEXN_TYPEERR, "Constructor {0} requires 'new'")
MSG_DEF(MSG_ENFORCE_RANGE_NON_FINITE, 1, JSEXN_TYPEERR, "Non-finite value is out of range for {0}.")
MSG_DEF(MSG_ENFORCE_RANGE_OUT_OF_RANGE, 1, JSEXN_TYPEERR, "Value is out of range for {0}.")
MSG_DEF(MSG_NOT_SEQUENCE, 1, JSEXN_TYPEERR, "{0} can't be converted to a sequence.")
MSG_DEF(MSG_NOT_DICTIONARY, 1, JSEXN_TYPEERR, "{0} can't be converted to a dictionary.")
MSG_DEF(MSG_OVERLOAD_RESOLUTION_FAILED, 3, JSEXN_TYPEERR, "Argument {0} is not valid for any of the {1}-argument overloads of {2}.")
MSG_DEF(MSG_GLOBAL_NOT_NATIVE, 0, JSEXN_TYPEERR, "Global is not a native object.")
MSG_DEF(MSG_ENCODING_NOT_SUPPORTED, 1, JSEXN_RANGEERR, "The given encoding '{0}' is not supported.")
MSG_DEF(MSG_DOM_ENCODING_NOT_UTF, 0, JSEXN_RANGEERR, "The encoding must be utf-8, utf-16, or utf-16be.")
MSG_DEF(MSG_DOM_DECODING_FAILED, 0, JSEXN_TYPEERR, "Decoding failed.")
MSG_DEF(MSG_NOT_FINITE, 1, JSEXN_TYPEERR, "{0} is not a finite floating-point value.")
MSG_DEF(MSG_INVALID_VERSION, 0, JSEXN_TYPEERR, "0 (Zero) is not a valid database version.")
MSG_DEF(MSG_INVALID_BYTESTRING, 2, JSEXN_TYPEERR, "Cannot convert string to ByteString because the character"
        " at index {0} has value {1} which is greater than 255.")
MSG_DEF(MSG_NOT_DATE, 1, JSEXN_TYPEERR, "{0} is not a date.")
MSG_DEF(MSG_INVALID_ADVANCE_COUNT, 0, JSEXN_TYPEERR, "0 (Zero) is not a valid advance count.")
MSG_DEF(MSG_DEFINEPROPERTY_ON_GSP, 0, JSEXN_TYPEERR, "Not allowed to define a property on the named properties object.")
MSG_DEF(MSG_INVALID_URL, 1, JSEXN_TYPEERR, "{0} is not a valid URL.")
MSG_DEF(MSG_URL_HAS_CREDENTIALS, 1, JSEXN_TYPEERR, "{0} is an url with embedded credentials.")
MSG_DEF(MSG_METADATA_NOT_CONFIGURED, 0, JSEXN_TYPEERR, "Either size or lastModified should be true.")
MSG_DEF(MSG_INVALID_READ_SIZE, 0, JSEXN_TYPEERR, "0 (Zero) is not a valid read size.")
MSG_DEF(MSG_HEADERS_IMMUTABLE, 0, JSEXN_TYPEERR, "Headers are immutable and cannot be modified.")
MSG_DEF(MSG_INVALID_HEADER_NAME, 1, JSEXN_TYPEERR, "{0} is an invalid header name.")
MSG_DEF(MSG_INVALID_HEADER_VALUE, 1, JSEXN_TYPEERR, "{0} is an invalid header value.")
MSG_DEF(MSG_INVALID_HEADER_SEQUENCE, 0, JSEXN_TYPEERR, "Headers require name/value tuples when being initialized by a sequence.")
MSG_DEF(MSG_PERMISSION_DENIED_TO_PASS_ARG, 1, JSEXN_TYPEERR, "Permission denied to pass cross-origin object as {0}.")
MSG_DEF(MSG_MISSING_REQUIRED_DICTIONARY_MEMBER, 1, JSEXN_TYPEERR, "Missing required {0}.")
MSG_DEF(MSG_REQUEST_INTEGRITY_METADATA_NOT_EMPTY, 0, JSEXN_TYPEERR, "Request integrity metadata should be an empty string when in no-cors mode.")
MSG_DEF(MSG_INVALID_REQUEST_METHOD, 1, JSEXN_TYPEERR, "Invalid request method {0}.")
MSG_DEF(MSG_INVALID_REQUEST_MODE, 1, JSEXN_TYPEERR, "Invalid request mode {0}.")
MSG_DEF(MSG_INVALID_REFERRER_URL, 1, JSEXN_TYPEERR, "Invalid referrer URL {0}.")
MSG_DEF(MSG_CROSS_ORIGIN_REFERRER_URL, 2, JSEXN_TYPEERR, "Referrer URL {0} cannot be cross-origin to the entry settings object ({1}).")
MSG_DEF(MSG_FETCH_BODY_CONSUMED_ERROR, 0, JSEXN_TYPEERR, "Body has already been consumed.")
MSG_DEF(MSG_RESPONSE_INVALID_STATUSTEXT_ERROR, 0, JSEXN_TYPEERR, "Response statusText may not contain newline or carriage return.")
MSG_DEF(MSG_FETCH_FAILED, 0, JSEXN_TYPEERR, "NetworkError when attempting to fetch resource.")
MSG_DEF(MSG_NO_BODY_ALLOWED_FOR_GET_AND_HEAD, 0, JSEXN_TYPEERR, "HEAD or GET Request cannot have a body.")
MSG_DEF(MSG_RESPONSE_NULL_STATUS_WITH_BODY, 0, JSEXN_TYPEERR, "Response body is given with a null body status.")
MSG_DEF(MSG_DEFINE_NON_CONFIGURABLE_PROP_ON_WINDOW, 0, JSEXN_TYPEERR, "Not allowed to define a non-configurable property on the WindowProxy object")
MSG_DEF(MSG_INVALID_ZOOMANDPAN_VALUE_ERROR, 0, JSEXN_RANGEERR, "Invalid zoom and pan value.")
MSG_DEF(MSG_INVALID_TRANSFORM_ANGLE_ERROR, 0, JSEXN_RANGEERR, "Invalid transform angle.")
MSG_DEF(MSG_INVALID_RESPONSE_STATUSCODE_ERROR, 0, JSEXN_RANGEERR, "Invalid response status code.")
MSG_DEF(MSG_INVALID_REDIRECT_STATUSCODE_ERROR, 0, JSEXN_RANGEERR, "Invalid redirect status code.")
MSG_DEF(MSG_INVALID_URL_SCHEME, 2, JSEXN_TYPEERR, "{0} URL {1} must be either http:// or https://.")
MSG_DEF(MSG_RESPONSE_URL_IS_NULL, 0, JSEXN_TYPEERR, "Cannot set Response.finalURL when Response.url is null.")
MSG_DEF(MSG_RESPONSE_HAS_VARY_STAR, 0, JSEXN_TYPEERR, "Invalid Response object with a 'Vary: *' header.")
MSG_DEF(MSG_BAD_FORMDATA, 0, JSEXN_TYPEERR, "Could not parse content as FormData.")
MSG_DEF(MSG_NO_ACTIVE_WORKER, 1, JSEXN_TYPEERR, "No active worker for scope {0}.")
MSG_DEF(MSG_NOTIFICATION_PERMISSION_DENIED, 0, JSEXN_TYPEERR, "Permission to show Notification denied.")
MSG_DEF(MSG_NOTIFICATION_NO_CONSTRUCTOR_IN_SERVICEWORKER, 0, JSEXN_TYPEERR, "Notification constructor cannot be used in ServiceWorkerGlobalScope. Use registration.showNotification() instead.")
MSG_DEF(MSG_INVALID_SCOPE, 2, JSEXN_TYPEERR, "Invalid scope trying to resolve {0} with base URL {1}.")
MSG_DEF(MSG_INVALID_KEYFRAME_OFFSETS, 0, JSEXN_TYPEERR, "Keyframes with specified offsets must be in order and all be in the range [0, 1].")
MSG_DEF(MSG_IS_NOT_PROMISE, 1, JSEXN_TYPEERR, "{0} is not a Promise")
MSG_DEF(MSG_SW_INSTALL_ERROR, 2, JSEXN_TYPEERR, "ServiceWorker script at {0} for scope {1} encountered an error during installation.")
MSG_DEF(MSG_SW_SCRIPT_THREW, 2, JSEXN_TYPEERR, "ServiceWorker script at {0} for scope {1} threw an exception during script evaluation.")
MSG_DEF(MSG_TYPEDARRAY_IS_SHARED, 1, JSEXN_TYPEERR, "{0} can't be a typed array on SharedArrayBuffer")
MSG_DEF(MSG_TYPEDARRAY_IS_DETACHED, 1, JSEXN_TYPEERR, "{0} can't be a detached buffer")
MSG_DEF(MSG_CACHE_ADD_FAILED_RESPONSE, 3, JSEXN_TYPEERR, "Cache got {0} response with bad status {1} while trying to add request {2}")
MSG_DEF(MSG_SW_UPDATE_BAD_REGISTRATION, 2, JSEXN_TYPEERR, "Failed to update the ServiceWorker for scope {0} because the registration has been {1} since the update was scheduled.")
MSG_DEF(MSG_INVALID_DURATION_ERROR, 1, JSEXN_TYPEERR, "Invalid duration '{0}'.")
MSG_DEF(MSG_INVALID_EASING_ERROR, 1, JSEXN_TYPEERR, "Invalid easing '{0}'.")
MSG_DEF(MSG_USELESS_SETTIMEOUT, 1, JSEXN_TYPEERR, "Useless {0} call (missing quotes around argument?)")
MSG_DEF(MSG_TOKENLIST_NO_SUPPORTED_TOKENS, 2, JSEXN_TYPEERR, "{0} attribute of <{1}> does not define any supported tokens")
MSG_DEF(MSG_CACHE_STREAM_CLOSED, 0, JSEXN_TYPEERR, "Response body is a cache file stream that has already been closed.")
MSG_DEF(MSG_TIME_VALUE_OUT_OF_RANGE, 1, JSEXN_TYPEERR, "{0} is outside the supported range for time values.")
MSG_DEF(MSG_ONLY_IF_CACHED_WITHOUT_SAME_ORIGIN, 1, JSEXN_TYPEERR, "Request mode '{0}' was used, but request cache mode 'only-if-cached' can only be used with request mode 'same-origin'.")
MSG_DEF(MSG_THRESHOLD_RANGE_ERROR, 0, JSEXN_RANGEERR, "Threshold values must all be in the range [0, 1].")
MSG_DEF(MSG_WORKER_THREAD_SHUTTING_DOWN, 0, JSEXN_TYPEERR, "The Worker thread is shutting down.")
MSG_DEF(MSG_CACHE_OPEN_FAILED, 0, JSEXN_TYPEERR, "CacheStorage.open() failed to access the storage system.")
MSG_DEF(MSG_MATRIX_INIT_LENGTH_WRONG, 1, JSEXN_TYPEERR, "Matrix init sequence must have a length of 6 or 16 (actual value: {0})")
MSG_DEF(MSG_INVALID_MEDIA_VIDEO_CONFIGURATION, 0, JSEXN_TYPEERR, "Invalid VideoConfiguration.")
MSG_DEF(MSG_INVALID_MEDIA_AUDIO_CONFIGURATION, 0, JSEXN_TYPEERR, "Invalid AudioConfiguration.")
