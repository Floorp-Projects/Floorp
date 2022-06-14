/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * The format for each error message is:
 *
 * MSG_DEF(<SYMBOLIC_NAME>, <ARGUMENT_COUNT>, <CONTEXT_ARG>, <JS_EXN_TYPE>, <FORMAT_STRING>)
 *
 * where
 *
 * <SYMBOLIC_NAME> is a legal C++ identifer that will be used in the source.
 *
 * <ARGUMENT_COUNT> is an integer literal specifying the total number of
 * replaceable arguments in the following format string.
 *
 * <CONTEXT_ARG> is a boolean indicating whether the first arg should be
 * replaced with the context string describing what's throwing the exception,
 * if there is such a context.  If false, there should be no such replacement.
 * If true and there is no context, the first arg will be the empty string.
 * That means that a true value here implies the string should begin with {0}
 * with no space after it.
 *
 * <JS_EXN_TYPE> is a JSExnType which specifies which kind of error the JS
 * engine should throw.
 *
 * <FORMAT_STRING> is a string literal, containing <ARGUMENT_COUNT> sequences
 * {X} where X  is an integer representing the argument number that will
 * be replaced with a string value when the error is reported.
 */

MSG_DEF(MSG_INVALID_ENUM_VALUE, 4, true, JSEXN_TYPEERR, "{0}'{2}' (value of {1}) is not a valid value for enumeration {3}.")
MSG_DEF(MSG_INVALID_OVERLOAD_ARGCOUNT, 2, true, JSEXN_TYPEERR, "{0}{1} is not a valid argument count for any overload.")
MSG_DEF(MSG_NOT_OBJECT, 2, true, JSEXN_TYPEERR, "{0}{1} is not an object.")
MSG_DEF(MSG_NOT_CALLABLE, 2, true, JSEXN_TYPEERR, "{0}{1} is not callable.")
MSG_DEF(MSG_NOT_CONSTRUCTOR, 2, true, JSEXN_TYPEERR, "{0}{1} is not a constructor.")
MSG_DEF(MSG_DOES_NOT_IMPLEMENT_INTERFACE, 3, true, JSEXN_TYPEERR, "{0}{1} does not implement interface {2}.")
MSG_DEF(MSG_METHOD_THIS_DOES_NOT_IMPLEMENT_INTERFACE, 2, false, JSEXN_TYPEERR, "'{0}' called on an object that does not implement interface {1}.")
MSG_DEF(MSG_NOT_IN_UNION, 3, true, JSEXN_TYPEERR, "{0}{1} could not be converted to any of: {2}.")
MSG_DEF(MSG_ILLEGAL_CONSTRUCTOR, 1, true, JSEXN_TYPEERR, "{0}Illegal constructor.")
MSG_DEF(MSG_CONSTRUCTOR_WITHOUT_NEW, 1, false, JSEXN_TYPEERR, "{0} constructor: 'new' is required")
MSG_DEF(MSG_ENFORCE_RANGE_NON_FINITE, 3, true, JSEXN_TYPEERR, "{0}{1} is not a finite value, so is out of range for {2}.")
MSG_DEF(MSG_ENFORCE_RANGE_OUT_OF_RANGE, 3, true, JSEXN_TYPEERR, "{0}{1} is out of range for {2}.")
MSG_DEF(MSG_CONVERSION_ERROR, 3, true, JSEXN_TYPEERR, "{0}{1} can't be converted to a {2}.")
MSG_DEF(MSG_OVERLOAD_RESOLUTION_FAILED, 3, true, JSEXN_TYPEERR, "{0}Argument {1} is not valid for any of the {2}-argument overloads.")
MSG_DEF(MSG_ENCODING_NOT_SUPPORTED, 2, true, JSEXN_RANGEERR, "{0}The given encoding '{1}' is not supported.")
MSG_DEF(MSG_DOM_DECODING_FAILED, 1, true, JSEXN_TYPEERR, "{0}Decoding failed.")
MSG_DEF(MSG_NOT_FINITE, 2, true, JSEXN_TYPEERR, "{0}{1} is not a finite floating-point value.")
MSG_DEF(MSG_INVALID_BYTESTRING, 4, true, JSEXN_TYPEERR, "{0}Cannot convert {1} to ByteString because the character"
        " at index {2} has value {3} which is greater than 255.")
MSG_DEF(MSG_INVALID_URL, 2, true, JSEXN_TYPEERR, "{0}{1} is not a valid URL.")
MSG_DEF(MSG_URL_HAS_CREDENTIALS, 2, true, JSEXN_TYPEERR, "{0}{1} is an url with embedded credentials.")
MSG_DEF(MSG_INVALID_HEADER_NAME, 2, true, JSEXN_TYPEERR, "{0}{1} is an invalid header name.")
MSG_DEF(MSG_INVALID_HEADER_VALUE, 2, true, JSEXN_TYPEERR, "{0}{1} is an invalid header value.")
MSG_DEF(MSG_PERMISSION_DENIED_TO_PASS_ARG, 2, true, JSEXN_TYPEERR, "{0}Permission denied to pass cross-origin object as {1}.")
MSG_DEF(MSG_MISSING_REQUIRED_DICTIONARY_MEMBER, 2, true, JSEXN_TYPEERR, "{0}Missing required {1}.")
MSG_DEF(MSG_INVALID_REQUEST_METHOD, 2, true, JSEXN_TYPEERR, "{0}Invalid request method {1}.")
MSG_DEF(MSG_INVALID_REQUEST_MODE, 2, true, JSEXN_TYPEERR, "{0}Invalid request mode {1}.")
MSG_DEF(MSG_INVALID_REFERRER_URL, 2, true, JSEXN_TYPEERR, "{0}Invalid referrer URL {1}.")
MSG_DEF(MSG_FETCH_BODY_CONSUMED_ERROR, 1, true, JSEXN_TYPEERR, "{0}Body has already been consumed.")
MSG_DEF(MSG_RESPONSE_INVALID_STATUSTEXT_ERROR, 1, true, JSEXN_TYPEERR, "{0}Response statusText may not contain newline or carriage return.")
MSG_DEF(MSG_FETCH_FAILED, 1, true, JSEXN_TYPEERR, "{0}NetworkError when attempting to fetch resource.")
MSG_DEF(MSG_FETCH_BODY_WRONG_TYPE, 1, true, JSEXN_TYPEERR, "{0}Can't convert value to Uint8Array while consuming Body")
MSG_DEF(MSG_INVALID_ZOOMANDPAN_VALUE_ERROR, 1, true, JSEXN_RANGEERR, "{0}Invalid zoom and pan value.")
MSG_DEF(MSG_INVALID_TRANSFORM_ANGLE_ERROR, 1, true, JSEXN_RANGEERR, "{0}Invalid transform angle.")
MSG_DEF(MSG_INVALID_URL_SCHEME, 3, true, JSEXN_TYPEERR, "{0}{1} URL {2} must be either http:// or https://.")
MSG_DEF(MSG_BAD_FORMDATA, 1, true, JSEXN_TYPEERR, "{0}Could not parse content as FormData.")
MSG_DEF(MSG_NO_ACTIVE_WORKER, 2, true, JSEXN_TYPEERR, "{0}No active worker for scope {1}.")
MSG_DEF(MSG_INVALID_SCOPE, 3, true, JSEXN_TYPEERR, "{0}Invalid scope trying to resolve {1} with base URL {2}.")
MSG_DEF(MSG_INVALID_KEYFRAME_OFFSETS, 1, true, JSEXN_TYPEERR, "{0}Keyframes with specified offsets must be in order and all be in the range [0, 1].")
MSG_DEF(MSG_IS_NOT_PROMISE, 1, true, JSEXN_TYPEERR, "{0}Argument is not a Promise")
MSG_DEF(MSG_SW_INSTALL_ERROR, 3, true, JSEXN_TYPEERR, "{0}ServiceWorker script at {1} for scope {2} encountered an error during installation.")
MSG_DEF(MSG_SW_SCRIPT_THREW, 3, true, JSEXN_TYPEERR, "{0}ServiceWorker script at {1} for scope {2} threw an exception during script evaluation.")
MSG_DEF(MSG_TYPEDARRAY_IS_SHARED, 2, true, JSEXN_TYPEERR, "{0}{1} can't be a SharedArrayBuffer or an ArrayBufferView backed by a SharedArrayBuffer")
MSG_DEF(MSG_TYPEDARRAY_IS_LARGE, 2, true, JSEXN_TYPEERR, "{0}{1} can't be an ArrayBuffer or an ArrayBufferView larger than 2 GB")
MSG_DEF(MSG_CACHE_ADD_FAILED_RESPONSE, 4, true, JSEXN_TYPEERR, "{0}Cache got {1} response with bad status {2} while trying to add request {3}")
MSG_DEF(MSG_SW_UPDATE_BAD_REGISTRATION, 3, true, JSEXN_TYPEERR, "{0}Failed to update the ServiceWorker for scope {1} because the registration has been {2} since the update was scheduled.")
MSG_DEF(MSG_INVALID_DURATION_ERROR, 2, true, JSEXN_TYPEERR, "{0}Invalid duration '{1}'.")
MSG_DEF(MSG_INVALID_EASING_ERROR, 2, true, JSEXN_TYPEERR, "{0}Invalid easing '{1}'.")
MSG_DEF(MSG_TOKENLIST_NO_SUPPORTED_TOKENS, 3, true, JSEXN_TYPEERR, "{0}{1} attribute of <{2}> does not define any supported tokens")
MSG_DEF(MSG_TIME_VALUE_OUT_OF_RANGE, 2, true, JSEXN_TYPEERR, "{0}{1} is outside the supported range for time values.")
MSG_DEF(MSG_ONLY_IF_CACHED_WITHOUT_SAME_ORIGIN, 2, true, JSEXN_TYPEERR, "{0}Request mode '{1}' was used, but request cache mode 'only-if-cached' can only be used with request mode 'same-origin'.")
MSG_DEF(MSG_THRESHOLD_RANGE_ERROR, 1, true, JSEXN_RANGEERR, "{0}Threshold values must all be in the range [0, 1].")
MSG_DEF(MSG_MATRIX_INIT_CONFLICTING_VALUE, 3, true, JSEXN_TYPEERR, "{0}Matrix init unexpectedly got different values for '{1}' and '{2}'.")
MSG_DEF(MSG_MATRIX_INIT_EXCEEDS_2D, 2, true, JSEXN_TYPEERR, "{0}Matrix init has an unexpected 3D element '{1}' which cannot coexist with 'is2D: true'.")
MSG_DEF(MSG_MATRIX_INIT_LENGTH_WRONG, 2, true, JSEXN_TYPEERR, "{0}Matrix init sequence must have a length of 6 or 16 (actual value: {1})")
MSG_DEF(MSG_INVALID_MEDIA_VIDEO_CONFIGURATION, 1, true, JSEXN_TYPEERR, "{0}Invalid VideoConfiguration.")
MSG_DEF(MSG_INVALID_MEDIA_AUDIO_CONFIGURATION, 1, true, JSEXN_TYPEERR, "{0}Invalid AudioConfiguration.")
MSG_DEF(MSG_INVALID_AUDIOPARAM_METHOD_START_TIME_ERROR, 1, true, JSEXN_RANGEERR, "{0}The start time for an AudioParam method must be non-negative.")
MSG_DEF(MSG_INVALID_AUDIOPARAM_METHOD_END_TIME_ERROR, 1, true, JSEXN_RANGEERR, "{0}The end time for an AudioParam method must be non-negative.")
MSG_DEF(MSG_VALUE_OUT_OF_RANGE, 2, true, JSEXN_RANGEERR, "{0}The value for the {1} is outside the valid range.")
MSG_DEF(MSG_NOT_ARRAY_NOR_UNDEFINED, 2, true, JSEXN_TYPEERR, "{0}{1} is neither an array nor undefined.")
MSG_DEF(MSG_URL_NOT_LOADABLE, 2, true, JSEXN_TYPEERR, "{0}Access to '{1}' from script denied.")
MSG_DEF(MSG_ONE_OFF_TYPEERR, 2, true, JSEXN_TYPEERR, "{0}{1}")
MSG_DEF(MSG_ONE_OFF_RANGEERR, 2, true, JSEXN_RANGEERR, "{0}{1}")
MSG_DEF(MSG_NO_CODECS_PARAMETER, 2, true, JSEXN_TYPEERR, "{0}The provided type '{1}' does not have a 'codecs' parameter.")
