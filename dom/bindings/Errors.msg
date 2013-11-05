/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * The format for each error message is:
 *
 * MSG_DEF(<SYMBOLIC_NAME>, <ARGUMENT_COUNT>, <FORMAT_STRING>)
 *
 * where
 *
 * <SYMBOLIC_NAME> is a legal C++ identifer that will be used in the source.
 *
 * <ARGUMENT_COUNT> is an integer literal specifying the total number of
 * replaceable arguments in the following format string.
 *
 * <FORMAT_STRING> is a string literal, containing <ARGUMENT_COUNT> sequences
 * {X} where X  is an integer representing the argument number that will
 * be replaced with a string value when the error is reported.
 */

MSG_DEF(MSG_INVALID_ENUM_VALUE, 3, "{0} '{1}' is not a valid value for enumeration {2}.")
MSG_DEF(MSG_MISSING_ARGUMENTS, 1, "Not enough arguments to {0}.")
MSG_DEF(MSG_NOT_OBJECT, 1, "{0} is not an object.")
MSG_DEF(MSG_NOT_CALLABLE, 1, "{0} is not callable.")
MSG_DEF(MSG_DOES_NOT_IMPLEMENT_INTERFACE, 2, "{0} does not implement interface {1}.")
MSG_DEF(MSG_METHOD_THIS_DOES_NOT_IMPLEMENT_INTERFACE, 2, "'{0}' called on an object that does not implement interface {1}.")
MSG_DEF(MSG_METHOD_THIS_UNWRAPPING_DENIED, 1, "Permission to call '{0}' denied.")
MSG_DEF(MSG_GETTER_THIS_DOES_NOT_IMPLEMENT_INTERFACE, 2, "'{0}' getter called on an object that does not implement interface {1}.")
MSG_DEF(MSG_GETTER_THIS_UNWRAPPING_DENIED, 1, "Permission to call '{0}' getter denied.")
MSG_DEF(MSG_SETTER_THIS_DOES_NOT_IMPLEMENT_INTERFACE, 2, "'{0}' setter called on an object that does not implement interface {1}.")
MSG_DEF(MSG_SETTER_THIS_UNWRAPPING_DENIED, 1, "Permission to call '{0}' setter denied.")
MSG_DEF(MSG_THIS_DOES_NOT_IMPLEMENT_INTERFACE, 1, "\"this\" object does not implement interface {0}.")
MSG_DEF(MSG_NOT_IN_UNION, 2, "{0} could not be converted to any of: {1}.")
MSG_DEF(MSG_ILLEGAL_CONSTRUCTOR, 0, "Illegal constructor.")
MSG_DEF(MSG_NO_INDEXED_SETTER, 1, "{0} doesn't have an indexed property setter.")
MSG_DEF(MSG_NO_NAMED_SETTER, 1, "{0} doesn't have a named property setter.")
MSG_DEF(MSG_ENFORCE_RANGE_NON_FINITE, 1, "Non-finite value is out of range for {0}.")
MSG_DEF(MSG_ENFORCE_RANGE_OUT_OF_RANGE, 1, "Value is out of range for {0}.")
MSG_DEF(MSG_NOT_SEQUENCE, 1, "{0} can't be converted to a sequence.")
MSG_DEF(MSG_NOT_DICTIONARY, 1, "{0} can't be converted to a dictionary.")
MSG_DEF(MSG_OVERLOAD_RESOLUTION_FAILED, 3, "Argument {0} is not valid for any of the {1}-argument overloads of {2}.")
MSG_DEF(MSG_GLOBAL_NOT_NATIVE, 0, "Global is not a native object.")
MSG_DEF(MSG_ENCODING_NOT_SUPPORTED, 1, "The given encoding '{0}' is not supported.")
MSG_DEF(MSG_DOM_ENCODING_NOT_UTF, 0, "The encoding must be utf-8, utf-16, or utf-16be.")
MSG_DEF(MSG_NOT_FINITE, 1, "{0} is not a finite floating-point value.")
MSG_DEF(MSG_INVALID_VERSION, 0, "0 (Zero) is not a valid database version.")
MSG_DEF(MSG_INVALID_BYTESTRING, 2, "Cannot convert string to ByteString because the character"
        " at index {0} has value {1} which is greater than 255.")
MSG_DEF(MSG_NOT_DATE, 1, "{0} is not a date.")
MSG_DEF(MSG_INVALID_ADVANCE_COUNT, 0, "0 (Zero) is not a valid advance count.")
MSG_DEF(MSG_DEFINEPROPERTY_ON_GSP, 0, "Not allowed to define a property on the named properties object.")
MSG_DEF(MSG_INVALID_URL, 1, "{0} is not a valid URL.")
