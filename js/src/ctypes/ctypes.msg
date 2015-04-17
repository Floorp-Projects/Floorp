/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This is the jsctypes error message file.
 *
 * For syntax details, see js/src/js.msg.
 */

MSG_DEF(CTYPESMSG_PLACEHOLDER_0, 0, JSEXN_NONE, NULL)

/* type conversion */
MSG_DEF(CTYPESMSG_CONV_ERROR_ARG,3, JSEXN_TYPEERR, "can't pass {0} to argument {1} of {2}")
MSG_DEF(CTYPESMSG_CONV_ERROR_ARRAY,3, JSEXN_TYPEERR, "can't convert {0} to element {1} of the type {2}")
MSG_DEF(CTYPESMSG_CONV_ERROR_FIN,2, JSEXN_TYPEERR, "can't convert {0} to the type of argument 1 of {1}")
MSG_DEF(CTYPESMSG_CONV_ERROR_RET,2, JSEXN_TYPEERR, "can't convert {0} to the return type of {1}")
MSG_DEF(CTYPESMSG_CONV_ERROR_SET,2, JSEXN_TYPEERR, "can't convert {0} to the type {1}")
MSG_DEF(CTYPESMSG_CONV_ERROR_STRUCT,5, JSEXN_TYPEERR, "can't convert {0} to the '{1}' field ({2}) of {3}{4}")
MSG_DEF(CTYPESMSG_NON_PRIMITIVE, 1, JSEXN_TYPEERR, ".value only works on character and numeric types, not `{0}`")
MSG_DEF(CTYPESMSG_TYPE_ERROR,    2, JSEXN_TYPEERR, "expected {0}, got {1}")

/* array */
MSG_DEF(CTYPESMSG_ARRAY_MISMATCH,4, JSEXN_TYPEERR, "length of {0} does not match to the length of the type {1} (expected {2}, got {3})")
MSG_DEF(CTYPESMSG_ARRAY_OVERFLOW,4, JSEXN_TYPEERR, "length of {0} does not fit to the length of the type {1} (expected {2} or lower, got {3})")

/* struct */
MSG_DEF(CTYPESMSG_FIELD_MISMATCH,5, JSEXN_TYPEERR, "property count of {0} does not match to field count of the type {1} (expected {2}, got {3}){4}")
MSG_DEF(CTYPESMSG_PROP_NONSTRING,3, JSEXN_TYPEERR, "property name {0} of {1} is not a string{2}")

/* data finalizer */
MSG_DEF(CTYPESMSG_EMPTY_FIN,     1, JSEXN_TYPEERR, "attempting to convert an empty CDataFinalizer{0}")
MSG_DEF(CTYPESMSG_FIN_SIZE_ERROR,2, JSEXN_TYPEERR, "expected an object with the same size as argument 1 of {0}, got {1}")
