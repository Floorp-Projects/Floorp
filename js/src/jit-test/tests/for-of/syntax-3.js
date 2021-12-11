// For-of can't have initializers.

load(libdir + 'asserts.js');

function assertSyntaxError(str) {
  assertThrowsInstanceOf(function () { return Function(str); }, SyntaxError);
}

assertSyntaxError("for (var x = 1 of []) {}");
assertSyntaxError("for (var [x] = 1 of []) {}");
assertSyntaxError("for (var {x} = 1 of []) {}");

assertSyntaxError("for (let x = 1 of []) {}");
assertSyntaxError("for (let [x] = 1 of []) {}");
assertSyntaxError("for (let {x} = 1 of []) {}");

assertSyntaxError("for (const x = 1 of []) {}");
assertSyntaxError("for (const [x] = 1 of []) {}");
assertSyntaxError("for (const {x} = 1 of []) {}");
