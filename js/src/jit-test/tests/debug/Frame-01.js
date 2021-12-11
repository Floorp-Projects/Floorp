// Test .type fields of topmost stack frame passed to onDebuggerStatement.

var g = newGlobal({newCompartment: true});
var dbg = Debugger(g);
var expected, hits;
dbg.onDebuggerStatement = function (f) {
    assertEq(Object.getPrototypeOf(f), Debugger.Frame.prototype);
    assertEq(f.type, expected.type);
    assertEq(f.script.isGeneratorFunction, expected.generator);
    assertEq(f.constructing, expected.constructing);
    hits++;
};

function test(code, expectobj, expectedHits) {
    expected = expectobj;
    hits = 0;
    g.evaluate(code);
    assertEq(hits, arguments.length < 3 ? 1 : expectedHits);
}

test("debugger;", {type: "global", generator: false, constructing: false});
test("(function () { debugger; })();", {type: "call", generator: false, constructing: false});
test("new function() { debugger; };", {type: "call", generator: false, constructing: true});
test("new function () { (function() { debugger; })(); }", {type: "call", generator: false, constructing: false});
test("eval('debugger;');", {type: "eval", generator: false, constructing: false});
test("this.eval('debugger;');  // indirect eval", {type: "eval", generator: false, constructing: false});
test("(function () { eval('debugger;'); })();", {type: "eval", generator: false, constructing: false});
test("new function () { eval('debugger'); }", {type: "eval", generator: false, constructing: false});
test("function* gen() { debugger; yield 1; debugger; }\n" +
     "for (var x of gen()) {}\n",
     {type: "call", generator: true, constructing: false}, 2);
test("var iter = (function* stargen() { debugger; yield 1; debugger; })();\n" +
     "iter.next(); iter.next();",
     {type: "call", generator: true, constructing: false}, 2);
