/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */


/*
 * Return true if both of these return true:
 * - LENIENT_PRED applied to CODE
 * - STRICT_PRED applied to CODE with a use strict directive added to the front
 *
 * Run STRICT_PRED first, for testing code that affects the global environment
 * in loose mode, but fails in strict mode.
 */
function testLenientAndStrict(code, lenient_pred, strict_pred) {
  return (strict_pred("'use strict'; " + code) && 
          lenient_pred(code));
}

/*
 * completesNormally(CODE) returns true if evaluating CODE (as eval
 * code) completes normally (rather than throwing an exception).
 */
function completesNormally(code) {
  try {
    eval(code);
    return true;
  } catch (exception) {
    return false;
  }
}

/*
 * parsesSuccessfully(CODE) returns true if CODE parses as function
 * code without an error.
 */
function parsesSuccessfully(code) {
  try {
    Function(code);
    return true;
  } catch (exception) {
    return false;
  }
};

/*
 * parseRaisesException(EXCEPTION)(CODE) returns true if parsing CODE
 * as function code raises EXCEPTION.
 */
function parseRaisesException(exception) {
  return function (code) {
    try {
      Function(code);
      return false;
    } catch (actual) {
      return exception.prototype.isPrototypeOf(actual);
    }
  };
};
