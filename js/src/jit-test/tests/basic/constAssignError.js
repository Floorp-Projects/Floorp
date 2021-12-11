// Can't assign to const is a runtime TypeError

load(libdir + 'asserts.js');

function assertTypeError(str) {
  assertThrowsInstanceOf(function () { eval(str); }, TypeError);
}

assertTypeError("(function() { const x = 3; (() => x++)(); return x })()");
assertTypeError("(function() { const x = 3; (() => x++)(); return x++ })()");
assertTypeError("(function() { const x = 2; (() => x++)(); return ++x })()");
assertTypeError("(function() { const x = 2; (() => x++)(); return x += 1 })()");

assertTypeError("(function() { const x = 3; x = 1; return (function() { return x})() })()");
assertTypeError("(function() { const x = 3; x = 1; return (function() { return x})() })()");
assertTypeError("(function() { const x = 3; x++; return (function() { return x})() })()");
assertTypeError("(function() { const x = 3; ++x; return (function() { return x})() })()");
assertTypeError("(function() { const x = 3; x--; return (function() { return x})() })()");
assertTypeError("(function() { const x = 3; --x; return (function() { return x})() })()");
assertTypeError("(function() { const x = 3; x += 1; return (function() { return x})() })()");
assertTypeError("(function() { const x = 3; x -= 1; return (function() { return x})() })()");

assertTypeError("(function() { const x = 3; [x] = [1]; })()");
