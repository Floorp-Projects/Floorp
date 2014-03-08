// The right-hand-side of a for-of is an assignment expression.

load(libdir + 'asserts.js');

function assertSyntaxError(str) {
  assertThrowsInstanceOf(function () { return Function(str); }, SyntaxError);
}

assertSyntaxError("for (var x     of 1, 2) {}");
assertSyntaxError("for (var [x]   of 1, 2) {}");
assertSyntaxError("for (var {x}   of 1, 2) {}");

assertSyntaxError("for (let x     of 1, 2) {}");
assertSyntaxError("for (let [x]   of 1, 2) {}");
assertSyntaxError("for (let {x}   of 1, 2) {}");

assertSyntaxError("for (const x   of 1, 2) {}");
assertSyntaxError("for (const [x] of 1, 2) {}");
assertSyntaxError("for (const {x} of 1, 2) {}");
