// Can't assign to const

load(libdir + 'asserts.js');

function assertSyntaxError(str) {
  assertThrowsInstanceOf(function () { eval(str); }, SyntaxError);
}

assertSyntaxError("(function() { const x = 3; (function() x++)(); return x })");
assertSyntaxError("(function() { const x = 3; (function() x++)(); return x++ })");
assertSyntaxError("(function() { const x = 2; (function() x++)(); return ++x })");
assertSyntaxError("(function() { const x = 2; (function() x++)(); return x += 1 })");

assertSyntaxError("(function() { const x = 3; x = 1; return (function() { return x})() })");
assertSyntaxError("(function() { const x = 3; x = 1; return (function() { return x})() })");
assertSyntaxError("(function() { const x = 3; x++; return (function() { return x})() })");
assertSyntaxError("(function() { const x = 3; ++x; return (function() { return x})() })");
assertSyntaxError("(function() { const x = 3; x--; return (function() { return x})() })");
assertSyntaxError("(function() { const x = 3; --x; return (function() { return x})() })");
assertSyntaxError("(function() { const x = 3; x += 1; return (function() { return x})() })");
assertSyntaxError("(function() { const x = 3; x -= 1; return (function() { return x})() })");

assertSyntaxError("(function() { const x = 3; [x] = [1]; })");
