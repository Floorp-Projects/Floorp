// |jit-test| debug
// Test .type and .generator fields of topmost stack frame passed to debuggerHandler.

var g = newGlobal('new-compartment');
g.debuggeeGlobal = this;
g.eval("var hits;");
g.eval("(" + function () {
        var dbg = Debug(debuggeeGlobal);
        dbg.hooks = {
            debuggerHandler: function (f) {
                // print(uneval(expected));
                assertEq(Object.getPrototypeOf(f), Debug.Frame.prototype);
                assertEq(f.type, expected.type);
                assertEq(f.generator, expected.generator);
                assertEq(f.constructing, expected.constructing);
                hits++;
            }
        };
    } + ")()");

g.expected = { type:"global", generator:false, constructing:false };
g.hits = 0;
debugger;
assertEq(g.hits, 1);

g.expected = { type:"call", generator:false, constructing:false };
g.hits = 0;
(function () { debugger; })();
assertEq(g.hits, 1);

g.expected = { type:"call", generator:false, constructing:true };
g.hits = 0;
new function() { debugger; };
assertEq(g.hits, 1);

g.expected = { type:"call", generator:false, constructing:false };
g.hits = 0;
new function () {
    (function() { debugger; })();
    assertEq(g.hits, 1);
}

g.expected = { type:"eval", generator:false, constructing:false };
g.hits = 0;
eval("debugger;");
assertEq(g.hits, 1);

g.expected = { type:"eval", generator:false, constructing:false };
g.hits = 0;
this.eval("debugger;");  // indirect eval
assertEq(g.hits, 1);

g.expected = { type:"eval", generator:false, constructing:false };
g.hits = 0;
(function () { eval("debugger;"); })();
assertEq(g.hits, 1);

g.expected = { type:"eval", generator:false, constructing:false };
g.hits = 0;
new function () {
    eval("debugger");
    assertEq(g.hits, 1);
}

g.expected = { type:"call", generator:true, constructing:false };
g.hits = 0;
function gen() { debugger; yield 1; debugger; }
for (var x in gen()) {
}
assertEq(g.hits, 2);
