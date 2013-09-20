// Creating a new script with any number of subscripts triggers the newScript hook exactly once.

var g = newGlobal();
var dbg = Debugger(g);
var seen = WeakMap();
var hits;
dbg.onNewScript = function (s) {
    assertEq(s instanceof Debugger.Script, true);
    assertEq(!seen.has(s), true);
    seen.set(s, true);
    hits++;
};

dbg.uncaughtExceptionHook = function () { hits = -999; };

function test(f) {
    hits = 0;
    f();
    assertEq(hits, 1);
}

// eval declaring a function
test(function () { g.eval("function A(m, n) { return m===0?n+1:n===0?A(m-1,1):A(m-1,A(m,n-1)); }"); });

// evaluate declaring a function
test(function () { g.eval("function g(a, b) { return b===0?a:g(b,a%b); }"); });

// eval declaring multiple functions
test(function () {
    g.eval("function e(i) { return i===0||o(i-1); }\n" +
           "function o(i) { return i!==0&&e(i-1); }\n");
});

// eval declaring nested functions
test(function () { g.eval("function plus(x) { return function plusx(y) { return x + y; }; }"); });

// eval with a function-expression
test(function () { g.eval("[3].map(function (i) { return -i; });"); });

// eval with getters and setters
test(function () { g.eval("var obj = {get x() { return 1; }, set x(v) { print(v); }};"); });

// Function with nested functions
test(function () { return g.Function("a", "b", "return b - a;"); });

// cloning a function with nested functions
test(function () { g.clone(evaluate("(function(x) { return x + 1; })", {compileAndGo: false})); });

// eval declaring a generator
test(function () { g.eval("function r(n) { for (var i=0;i<n;i++) yield i; }"); });

// eval declaring a star generator
test(function () { g.eval("function* sg(n) { for (var i=0;i<n;i++) yield i; }"); });

// eval with a generator-expression
test(function () { g.eval("var it = (obj[p] for (p in obj));"); });

// eval creating several instances of a closure
test(function () { g.eval("for (var i = 0; i < 7; i++)\n" +
                          "    obj = function () { return obj; };\n"); });

// non-strict-mode direct eval
g.eval("function e(s) { eval(s); }");
test(function () { g.e("function f(x) { return -x; }"); });

// strict-mode direct eval
g.eval("function E(s) { 'use strict'; eval(s); }");
test(function () { g.E("function g(x) { return -x; }"); });
