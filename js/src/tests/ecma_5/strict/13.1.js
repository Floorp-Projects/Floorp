/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*
 * In strict mode, it is a syntax error for an identifier to appear
 * more than once in a function's argument list.
 */

/*
 * The parameters of ordinary function definitions should not contain
 * duplicate identifiers.
 */
assertEq(testLenientAndStrict('function(x,y) {}',
                              parsesSuccessfully,
                              parsesSuccessfully),
         true);
assertEq(testLenientAndStrict('function(x,x) {}',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('function(x,y,z,y) {}',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

/* Exercise the hashed local name map case. */
assertEq(testLenientAndStrict('function(a,b,c,d,e,f,g,h,d) {}',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

/*
 * SpiderMonkey has always treated duplicates in destructuring
 * patterns as an error. Strict mode should not affect this.
 */
assertEq(testLenientAndStrict('function([x,y]) {}',
                              parsesSuccessfully,
                              parsesSuccessfully),
         true);
assertEq(testLenientAndStrict('function([x,x]){}',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('function(x,[x]){}',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);

/*
 * Strict rules apply to the parameters if the function's body is
 * strict.
 */
assertEq(testLenientAndStrict('function(x,x) { "use strict" };',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);

/*
 * Calls to the function constructor should not be affected by the
 * strictness of the calling code, but should be affected by the
 * strictness of the function body.
 */
assertEq(testLenientAndStrict('Function("x","x","")',
                              completesNormally,
                              completesNormally),
         true);
assertEq(testLenientAndStrict('Function("x","y","")',
                              completesNormally,
                              completesNormally),
         true);
assertEq(testLenientAndStrict('Function("x","x","\'use strict\'")',
                              raisesException(SyntaxError),
                              raisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('Function("x","y","\'use strict\'")',
                              completesNormally,
                              completesNormally),
         true);


/*
 * The parameter lists of getters and setters in object literals
 * should not contain duplicate identifiers.
 */
assertEq(testLenientAndStrict('({get x(y,y) {}})',
                               parsesSuccessfully,
                               parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('({get x(y,y) { "use strict"; }})',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('({set x(y,y) {}})',
                               parsesSuccessfully,
                               parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('({set x(y,y) { "use strict"; }})',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);

/*
 * The parameter lists of function expressions should not contain
 * duplicate identifiers.
 */
assertEq(testLenientAndStrict('(function (x,x) 2)',
                               parsesSuccessfully,
                               parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('(function (x,y) 2)',
                              parsesSuccessfully,
                              parsesSuccessfully),
         true);
