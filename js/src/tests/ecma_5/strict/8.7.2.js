/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*
 * These tests depend on the fact that testLenientAndStrict tries the
 * strict case first; otherwise, the non-strict evaluation creates the
 * variable. Ugh. Ideally, we would use evalcx, but that's not
 * available in the browser.
 */

/* Assigning to an undeclared variable should fail in strict mode. */
assertEq(testLenientAndStrict('undeclared=1',
                              completesNormally,
                              raisesException(ReferenceError)),
         true);

/*
 * Assigning to a var-declared variable should be okay in strict and
 * lenient modes.
 */
assertEq(testLenientAndStrict('var var_declared; var_declared=1',
                              completesNormally,
                              completesNormally),
         true);

/*
 * We mustn't report errors until the code is actually run; the variable
 * could be created in the mean time.
 */
assertEq(testLenientAndStrict('undeclared_at_compiletime=1',
                              parsesSuccessfully,
                              parsesSuccessfully),
         true);

reportCompare(true, true);
