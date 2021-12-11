/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

// Ordinary function definitions should be unaffected.
assertEq(testLenientAndStrict("function f() { }",
                              parsesSuccessfully,
                              parsesSuccessfully),
         true);

// Lambdas are always permitted within blocks.
assertEq(testLenientAndStrict("{ (function f() { }) }",
                              parsesSuccessfully,
                              parsesSuccessfully),
         true);

// Function statements within unbraced blocks are forbidden in strict mode code.
// They are allowed only under if statements in sloppy mode.
assertEq(testLenientAndStrict("if (true) function f() { }",
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict("while (true) function f() { }",
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict("do function f() { } while (true);",
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict("for(;;) function f() { }",
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict("for(x in []) function f() { }",
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict("with(o) function f() { }",
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict("switch(1) { case 1: function f() { } }",
                              parsesSuccessfully,
                              parsesSuccessfully),
         true);
assertEq(testLenientAndStrict("x: function f() { }",
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict("try { function f() { } } catch (x) { }",
                              parsesSuccessfully,
                              parsesSuccessfully),
         true);

// Lambdas are always permitted within any sort of statement.
assertEq(testLenientAndStrict("if (true) (function f() { })",
                              parsesSuccessfully,
                              parsesSuccessfully),
         true);

// Function statements are permitted in blocks within lenient functions.
assertEq(parsesSuccessfully("function f() { function g() { } }"),
         true);

// Function statements are permitted in if statement within lenient functions.
assertEq(parsesSuccessfully("function f() { if (true) function g() { } }"),
         true);

assertEq(parseRaisesException(SyntaxError)
         ("function f() { 'use strict'; if (true) function g() { } }"),
         true);

assertEq(parsesSuccessfully("function f() { 'use strict'; { function g() { } } }"),
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
                              completesNormally),
         true);

reportCompare(true, true);
