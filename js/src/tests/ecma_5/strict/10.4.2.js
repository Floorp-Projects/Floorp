/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/* Direct calls to eval should inherit the strictness of the calling code. */
assertEq(testLenientAndStrict("eval('010')",
                              completesNormally,
                              raisesException(SyntaxError)),
         true);

/*
 * Directives in the eval code itself should always override a direct
 * caller's strictness.
 */
assertEq(testLenientAndStrict("eval('\"use strict\"; 010')",
                              raisesException(SyntaxError),
                              raisesException(SyntaxError)),
         true);

/* Code passed to indirect calls to eval should never be strict. */
assertEq(testLenientAndStrict("var evil=eval; evil('010')",
                              completesNormally,
                              completesNormally),
         true);

/*
 * Code passed to the Function constructor never inherits the caller's
 * strictness.
 */
assertEq(completesNormally("Function('010')"),
         true);
assertEq(raisesException(SyntaxError)("Function('\"use strict\"; 010')"),
         true);

reportCompare(true, true);
