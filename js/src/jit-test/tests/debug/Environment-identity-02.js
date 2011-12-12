// frame.environment is different for different activations of a scope.

var g = newGlobal('new-compartment');
var dbg = Debugger(g);
g.eval("function h() { debugger; }");
var arr;
dbg.onDebuggerStatement = function (hframe) {
    var e = hframe.older.environment;
    assertEq(arr.indexOf(e), -1);
    arr.push(e);
};

function test(code, expectedHits) {
    arr = [];
    g.eval(code);
    assertEq(arr.length, expectedHits);
}

// two separate calls to a function
test("(function () { var f = function (a) { h(); return a; }; f(1); f(2); })();", 2);

// recursive calls to a function
test("(function f(n) { h(); return n < 2 ? 1 : n * f(n - 1); })(3);", 3);

// separate visits to a block in the same call frame
test("(function () { for (var i = 0; i < 3; i++) { let j = i * 4; h(); }})();", 3);

// two strict direct eval calls in the same function scope
test("(function () { 'use strict'; for (var i = 0; i < 3; i++) eval('h();'); })();", 3);
