// |jit-test| debug
// tests calling script functions via Debug.Object.prototype.apply

load(libdir + "asserts.js");

var g = newGlobal("new-compartment");
g.eval("function f() { debugger; }");
var dbg = new Debug(g);
dbg.hooks = {debuggerHandler: function () {}};

dbg.hooks.debuggerHandler = function (frame) {
    var fn = frame.arguments[0];
    var cv = fn.apply(null, [9, 16]);
    assertEq(Object.keys(cv).join(","), "return");
    assertEq(Object.getPrototypeOf(cv), Object.prototype);
    assertEq(cv.return, 25);

    cv = fn.apply(null, ["hello ", "world"]);
    assertEq(Object.keys(cv).join(","), "return");
    assertEq(cv.return, "hello world");

    // Handle more or less arguments.
    assertEq(fn.apply(null, [1, 5, 100]).return, 6);
    assertEq(fn.apply(null, []).return, NaN);
    assertEq(fn.apply().return, NaN);

    // Throw if a this-value or argument is an object but not a Debug.Object.
    assertThrowsInstanceOf(function () { fn.apply({}, []); }, TypeError);
    assertThrowsInstanceOf(function () { fn.apply(null, [{}]); }, TypeError);
};
g.eval("f(function (a, b) { return a + b; });");

// The callee receives the right arguments even if more arguments are provided
// than the callee's .length.
dbg.hooks.debuggerHandler = function (frame) {
    assertEq(frame.arguments[0].apply(null, ['one', 'two']).return, 2);
};
g.eval("f(function () { return arguments.length; });");

// Exceptions are reported as {throw:} completion values.
dbg.hooks.debuggerHandler = function (frame) {
    var lose = frame.arguments[0];
    var cv = lose.apply(null, []);
    assertEq(Object.keys(cv).join(","), "throw");
    assertEq(cv.throw, frame.callee);
};
g.eval("f(function lose() { throw f; });");
