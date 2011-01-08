/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

// Ordinary function definitions should be unaffected.
assertEq(testLenientAndStrict("function f() { }",
                              parsesSuccessfully,
                              parsesSuccessfully),
         true);

// Function statements within blocks are forbidden in strict mode code.
assertEq(testLenientAndStrict("{ function f() { } }",
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

// Lambdas are always permitted within blocks.
assertEq(testLenientAndStrict("{ (function f() { }) }",
                              parsesSuccessfully,
                              parsesSuccessfully),
         true);

// Function statements within any sort of statement are forbidden in strict mode code.
assertEq(testLenientAndStrict("if (true) function f() { }",
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict("while (true) function f() { }",
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict("do function f() { } while (true);",
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict("for(;;) function f() { }",
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict("for(x in []) function f() { }",
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict("with(o) function f() { }",
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict("switch(1) { case 1: function f() { } }",
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict("x: function f() { }",
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict("try { function f() { } } catch (x) { }",
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

// Lambdas are always permitted within any sort of statement.
assertEq(testLenientAndStrict("if (true) (function f() { })",
                              parsesSuccessfully,
                              parsesSuccessfully),
         true);

// Function statements are permitted in blocks within lenient functions.
assertEq(parsesSuccessfully("function f() { function g() { } }"),
         true);

// Function statements are permitted in any statement within lenient functions.
assertEq(parsesSuccessfully("function f() { if (true) function g() { } }"),
         true);

assertEq(parseRaisesException(SyntaxError)
         ("function f() { 'use strict'; if (true) function g() { } }"),
         true);

assertEq(parseRaisesException(SyntaxError)
         ("function f() { 'use strict'; { function g() { } } }"),
         true);

assertEq(parsesSuccessfully("function f() { 'use strict'; if (true) (function g() { }) }"),
         true);

assertEq(parsesSuccessfully("function f() { 'use strict'; { (function g() { }) } }"),
         true);

// Eval should behave the same way. (The parse-only tests use the Function constructor.)
assertEq(testLenientAndStrict("function f() { }",
                              completesNormally,
                              completesNormally),
         true);
assertEq(testLenientAndStrict("{ function f() { } }",
                              completesNormally,
                              raisesException(SyntaxError)),
         true);

reportCompare(true, true);
