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
    "package => package.name",  // tricky: FutureReservedWord in strict mode code only
    "arguments => 0",  // names banned in strict mode code
    "eval => 0",
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
