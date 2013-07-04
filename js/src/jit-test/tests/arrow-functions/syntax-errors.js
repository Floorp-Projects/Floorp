// Check that we correctly throw SyntaxErrors for various syntactic near-misses.

load(libdir + "asserts.js");

var mistakes = [
    "((a)) => expr",
    "a + b => a",
    "'' + a => a",
    "...x",
    "[x] => x",
    "([x] => x)",
    "{p: p} => p",
    "({p: p} => p)",
    "{p} => p",
    "(...x => expr)",
    "1 || a => a",
    "'use strict' => {}",
    "package => {'use strict';}",    // tricky: FutureReservedWord in strict mode code only
    "'use strict'; arguments => 0",  // names banned in strict mode code
    "'use strict'; eval => 0",
    "a => {'use strict'; with (a) return x; }",
    "a => yield a",
    "a => { yield a; }",
    "a => { { let x; yield a; } }",
    "(a = yield 0) => a",
    "for (;;) a => { break; };",
    "for (;;) a => { continue; };",
    "...rest) =>",
    "2 + ...rest) =>"
];

for (var s of mistakes)
    assertThrowsInstanceOf(function () { Function(s); }, SyntaxError);

// Check that the tricky case is not an error in non-strict-mode code.
var f = package => 0;
assertEq(f(1), 0);

