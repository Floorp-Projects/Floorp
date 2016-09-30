/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This is the jsctypes error message file.
 *
 * For syntax details, see js/src/js.msg.
 */

MSG_DEF(CTYPESMSG_PLACEHOLDER_0, 0, JSEXN_ERR, NULL)

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
MSG_DEF(CTYPESMSG_ARRAY_OVERFLOW,4, JSEXN_TYPEERR, "length of {0} does not fit in the length of the type {1} (expected {2} or lower, got {3})")
MSG_DEF(CTYPESMSG_INVALID_INDEX, 1, JSEXN_TYPEERR, "{0} is not a valid array index")
MSG_DEF(CTYPESMSG_INVALID_RANGE, 2, JSEXN_RANGEERR, "array index {0} is out of bounds for array of length {1}")

/* struct */
MSG_DEF(CTYPESMSG_DUPLICATE_FIELD, 1, JSEXN_TYPEERR, "struct fields must have unique names, '{0}' field appears twice")
MSG_DEF(CTYPESMSG_FIELD_DESC_COUNT,2, JSEXN_TYPEERR, "struct field descriptors must contain one property (got {0} with {1} properties)")
MSG_DEF(CTYPESMSG_FIELD_DESC_NAME,1, JSEXN_TYPEERR, "{0} is not a valid name of struct field descriptors")
MSG_DEF(CTYPESMSG_FIELD_DESC_SIZE,2, JSEXN_TYPEERR, "struct field type must have defined and nonzero size (got {0} for '{1}' field)")
MSG_DEF(CTYPESMSG_FIELD_DESC_NAMETYPE,1, JSEXN_TYPEERR, "struct field descriptors require a valid name and type (got {0})")
MSG_DEF(CTYPESMSG_FIELD_DESC_TYPE,2, JSEXN_TYPEERR, "{0} is not a valid type of struct field descriptors for '{1}' field")
MSG_DEF(CTYPESMSG_FIELD_MISMATCH,5, JSEXN_TYPEERR, "property count of {0} does not match to field count of the type {1} (expected {2}, got {3}){4}")
MSG_DEF(CTYPESMSG_FIELD_MISSING, 2, JSEXN_TYPEERR, "{0} does not have a field named '{1}'")
MSG_DEF(CTYPESMSG_PROP_NONSTRING,3, JSEXN_TYPEERR, "property name {0} of {1} is not a string{2}")

/* data finalizer */
MSG_DEF(CTYPESMSG_EMPTY_FIN,     1, JSEXN_TYPEERR, "attempting to convert an empty CDataFinalizer{0}")
MSG_DEF(CTYPESMSG_EMPTY_FIN_CALL,1, JSEXN_TYPEERR, "{0} called on empty CDataFinalizer")
MSG_DEF(CTYPESMSG_FIN_SIZE_ERROR,2, JSEXN_TYPEERR, "expected an object with the same size as argument 1 of {0}, got {1}")

/* native function */
MSG_DEF(CTYPESMSG_ARG_RANGE_MISMATCH,2, JSEXN_RANGEERR, "argument of {0} must be {1}")
MSG_DEF(CTYPESMSG_ARG_TYPE_MISMATCH,3, JSEXN_TYPEERR, "{0}argument of {1} must be {2}")
MSG_DEF(CTYPESMSG_INCOMPATIBLE_CALLEE,2, JSEXN_TYPEERR, "callee is not correct in {0}, got {1}")
MSG_DEF(CTYPESMSG_INCOMPATIBLE_THIS,2, JSEXN_TYPEERR, "{0} called on incompatible {1}")
MSG_DEF(CTYPESMSG_INCOMPATIBLE_THIS_TYPE,2, JSEXN_TYPEERR, "{0} called on {1}")
MSG_DEF(CTYPESMSG_INCOMPATIBLE_THIS_VAL,3, JSEXN_TYPEERR, "{0} called on {1}, got {2}")
MSG_DEF(CTYPESMSG_WRONG_ARG_LENGTH,3, JSEXN_TYPEERR, "{0} takes {1} argument{2}")

/* overflow */
MSG_DEF(CTYPESMSG_SIZE_OVERFLOW, 2, JSEXN_RANGEERR, "{0} does not fit in {1}")
MSG_DEF(CTYPESMSG_TYPE_OVERFLOW, 2, JSEXN_RANGEERR, "{0} does not fit in the type {1}")

/* function */
MSG_DEF(CTYPESMSG_ARG_COUNT_MISMATCH,4, JSEXN_RANGEERR, "number of arguments does not match declaration of {0} (expected {1}{2}, got {3})")
MSG_DEF(CTYPESMSG_ARG_TYPE_ERROR,3, JSEXN_TYPEERR, "the type of argument {0} {1} (got {2})")
MSG_DEF(CTYPESMSG_FUNCTION_CONSTRUCT,0, JSEXN_TYPEERR, "cannot construct from FunctionType; use FunctionType.ptr instead")
MSG_DEF(CTYPESMSG_RET_TYPE_ERROR,2, JSEXN_TYPEERR, "return type {0} (got {1})")
MSG_DEF(CTYPESMSG_VARG_TYPE_ERROR,2, JSEXN_TYPEERR, "variadic argument {0} must be a CData object (got {1})")

/* ctype */
MSG_DEF(CTYPESMSG_CANNOT_CONSTRUCT,1, JSEXN_TYPEERR, "cannot construct from {0}")

/* pointer */
MSG_DEF(CTYPESMSG_UNDEFINED_SIZE,2, JSEXN_TYPEERR, "cannot {0} pointer of undefined size {1}")
MSG_DEF(CTYPESMSG_NULL_POINTER,  2, JSEXN_TYPEERR, "cannot {0} null pointer {1}")
MSG_DEF(CTYPESMSG_NON_STRING_BASE,1, JSEXN_TYPEERR, "base type {0} is not an 8-bit or 16-bit integer or character type")

/* cast */
MSG_DEF(CTYPESMSG_UNDEFINED_SIZE_CAST,1, JSEXN_TYPEERR, "target type {0} has undefined size")
MSG_DEF(CTYPESMSG_SIZE_MISMATCH_CAST,4, JSEXN_TYPEERR, "target type {0} has larger size than source type {1} ({2} > {3})")
