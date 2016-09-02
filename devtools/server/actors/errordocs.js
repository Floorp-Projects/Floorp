/* this source code form is subject to the terms of the mozilla public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A mapping of error message names to external documentation. Any error message
 * included here will be displayed alongside its link in the web console.
 */

"use strict";

const baseURL = "https://developer.mozilla.org/docs/Web/JavaScript/Reference/Errors/";
const params = "?utm_source=mozilla&utm_medium=firefox-console-errors&utm_campaign=default";
const ErrorDocs = {
  JSMSG_READ_ONLY: "Read-only",
  JSMSG_BAD_ARRAY_LENGTH: "Invalid_array_length",
  JSMSG_NEGATIVE_REPETITION_COUNT: "Negative_repetition_count",
  JSMSG_RESULTING_STRING_TOO_LARGE: "Resulting_string_too_large",
  JSMSG_BAD_RADIX: "Bad_radix",
  JSMSG_PRECISION_RANGE: "Precision_range",
  JSMSG_BAD_FORMAL: "Malformed_formal_parameter",
  JSMSG_STMT_AFTER_RETURN: "Stmt_after_return",
  JSMSG_NOT_A_CODEPOINT: "Not_a_codepoint",
  JSMSG_BAD_SORT_ARG: "Array_sort_argument",
  JSMSG_UNEXPECTED_TYPE: "Unexpected_type",
  JSMSG_NOT_DEFINED: "Not_defined",
  JSMSG_NOT_FUNCTION: "Not_a_function",
  JSMSG_EQUAL_AS_ASSIGN: "Equal_as_assign",
  JSMSG_UNDEFINED_PROP: "Undefined_prop",
  JSMSG_DEPRECATED_PRAGMA: "Deprecated_source_map_pragma",
  JSMSG_DEPRECATED_USAGE: "Deprecated_caller_or_arguments_usage",
  JSMSG_CANT_DELETE: "Cant_delete",
  JSMSG_VAR_HIDES_ARG: "Var_hides_argument",
  JSMSG_JSON_BAD_PARSE: "JSON_bad_parse",
  JSMSG_UNDECLARED_VAR: "Undeclared_var",
  JSMSG_UNEXPECTED_TOKEN: "Unexpected_token",
  JSMSG_BAD_OCTAL: "Bad_octal",
  JSMSG_PROPERTY_ACCESS_DENIED: "Property_access_denied",
  JSMSG_NO_PROPERTIES: "No_properties",
  JSMSG_ALREADY_HAS_PRAGMA: "Already_has_pragma",
  JSMSG_BAD_RETURN_OR_YIELD: "Bad_return_or_yield",
  JSMSG_SEMI_BEFORE_STMNT: "Missing_semicolon_before_statement",
  JSMSG_OVER_RECURSED: "Too_much_recursion",
  JSMSG_BRACKET_AFTER_LIST: "Missing_bracket_after_list",
  JSMSG_PAREN_AFTER_ARGS: "Missing_parenthesis_after_argument_list",
  JSMSG_MORE_ARGS_NEEDED: "More_arguments_needed",
  JSMSG_BAD_LEFTSIDE_OF_ASS: "Invalid_assignment_left-hand_side",
  JSMSG_UNTERMINATED_STRING: "Unterminated_string_literal",
  JSMSG_NOT_CONSTRUCTOR: "Not_a_constructor",
  JSMSG_CURLY_AFTER_LIST: "Missing_curly_after_property_list",
  JSMSG_DEPRECATED_FOR_EACH: "For-each-in_loops_are_deprecated",
};

const MIXED_CONTENT_LEARN_MORE = "https://developer.mozilla.org/docs/Web/Security/Mixed_content";
const TRACKING_PROTECTION_LEARN_MORE = "https://developer.mozilla.org/Firefox/Privacy/Tracking_Protection";
const INSECURE_PASSWORDS_LEARN_MORE = "https://developer.mozilla.org/docs/Web/Security/Insecure_passwords";
const PUBLIC_KEY_PINS_LEARN_MORE = "https://developer.mozilla.org/docs/Web/Security/Public_Key_Pinning";
const STRICT_TRANSPORT_SECURITY_LEARN_MORE = "https://developer.mozilla.org/docs/Web/Security/HTTP_strict_transport_security";
const WEAK_SIGNATURE_ALGORITHM_LEARN_MORE = "https://developer.mozilla.org/docs/Web/Security/Weak_Signature_Algorithm";
const ErrorCategories = {
  "Insecure Password Field": INSECURE_PASSWORDS_LEARN_MORE,
  "Mixed Content Message": MIXED_CONTENT_LEARN_MORE,
  "Mixed Content Blocker": MIXED_CONTENT_LEARN_MORE,
  "Invalid HPKP Headers": PUBLIC_KEY_PINS_LEARN_MORE,
  "Invalid HSTS Headers": STRICT_TRANSPORT_SECURITY_LEARN_MORE,
  "SHA-1 Signature": WEAK_SIGNATURE_ALGORITHM_LEARN_MORE,
  "Tracking Protection": TRACKING_PROTECTION_LEARN_MORE,
};

exports.GetURL = (error) => {
  if (!error) {
    return;
  }

  let doc = ErrorDocs[error.errorMessageName];
  if (doc) {
    return baseURL + doc + params;
  }

  let categoryURL = ErrorCategories[error.category];
  if (categoryURL) {
    return categoryURL + params;
  }
};
