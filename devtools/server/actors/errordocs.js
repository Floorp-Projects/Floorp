/* this source code form is subject to the terms of the mozilla public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A mapping of error message names to external documentation. Any error message
 * included here will be displayed alongside its link in the web console.
 */

"use strict";

const ErrorDocs = {
  JSMSG_READ_ONLY: "https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Errors/Read-only",
  JSMSG_BAD_ARRAY_LENGTH: "https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Errors/Invalid_array_length",
  JSMSG_NEGATIVE_REPETITION_COUNT: "https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Errors/Negative_repetition_count",
  JSMSG_RESULTING_STRING_TOO_LARGE: "https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Errors/Resulting_string_too_large",
  JSMSG_BAD_RADIX: "https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Errors/Bad_radix",
  JSMSG_PRECISION_RANGE: "https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Errors/Precision_range",
  JSMSG_BAD_FORMAL: "https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Errors/Malformed_formal_parameter",
  JSMSG_STMT_AFTER_RETURN: "https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Errors/Stmt_after_return",
};

exports.GetURL = (errorName) => ErrorDocs[errorName];
