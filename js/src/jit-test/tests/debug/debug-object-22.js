// |jit-test| debug
// Test .type and .generator fields of topmost stack frame passed to debuggerHandler.

var g = newGlobal('new-compartment');
g.debuggeeGlobal = this;
g.eval("var hits;");
g.eval("(" + function () {
        var dbg = Debug(debuggeeGlobal);
        dbg.hooks = {
            debuggerHandler: function (f) {
                assertEq(Object.getPrototypeOf(f), Debug.Frame.prototype);
                assertEq(f.type, ftype);
                assertEq(f.generator, fgen);
                hits++;
            }
        };
    } + ")()");

g.ftype = "global";
g.fgen = false;
g.hits = 0;
debugger;
assertEq(g.hits, 1);

g.ftype = "call";
g.hits = 0;
(function () { debugger; })();
assertEq(g.hits, 1);

g.ftype = "eval";
g.hits = 0;
eval("debugger;");
assertEq(g.hits, 1);

g.ftype = "eval";
g.hits = 0;
this.eval("debugger;");  // indirect eval
assertEq(g.hits, 1);

g.ftype = "eval";
g.hits = 0;
(function () { eval("debugger;"); })();
assertEq(g.hits, 1);

g.ftype = "call";
g.fgen = true;
g.hits = 0;
function gen() { debugger; yield 1; debugger; }
for (var x in gen()) {
}
assertEq(g.hits, 2);
