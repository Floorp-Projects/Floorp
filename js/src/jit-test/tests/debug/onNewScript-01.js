// Basic newScript hook tests.

var g = newGlobal('new-compartment');
var dbg = Debugger(g);
var seen = WeakMap();
var hits = 0;
dbg.onNewScript = function (s) {
    // Exceptions thrown from onNewScript are swept under the rug, but they
    // will at least prevent hits from being the expected number.
    assertEq(s instanceof Debugger.Script, true);
    assertEq(!seen.has(s), true);
    seen.set(s, true);
    hits++;
};

dbg.uncaughtExceptionHook = function () { hits = -999; };

// eval code
hits = 0;
assertEq(g.eval("2 + 2"), 4);
assertEq(hits, 1);

hits = 0;
assertEq(g.eval("eval('2 + 3')"), 5);
assertEq(hits, 2);

// global code
hits = 0;
g.evaluate("3 + 4");
assertEq(hits, 1);

// function code
hits = 0;
var fn = g.Function("a", "return 5 + a;");
assertEq(hits, 1);
assertEq(fn(8), 13);
assertEq(hits, 1);

// cloning functions across compartments
fn = g.evaluate("(function(a) { return 5 + a; })", {compileAndGo: false});
var g2 = newGlobal('new-compartment');
dbg.addDebuggee(g2, dbg);
hits = 0;
g2.clone(fn);
assertEq(hits, 1);
