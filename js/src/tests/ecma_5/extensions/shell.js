/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

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
 * raisesException(EXCEPTION)(CODE) returns true if evaluating CODE (as eval
 * code) throws an exception object whose prototype is
 * EXCEPTION.prototype, and returns false if it throws any other error
 * or evaluates successfully. For example: raises(TypeError)("0()") ==
 * true.
 */
function raisesException(exception) {
  return function (code) {
    try {
      eval(code);
      return false;
    } catch (actual) {
      return exception.prototype.isPrototypeOf(actual);
    }
  };
};
