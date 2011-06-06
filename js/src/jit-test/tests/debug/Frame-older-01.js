// |jit-test| debug
// Basic call chain.

var g = newGlobal('new-compartment');
g.debuggeeGlobal = this;
g.result = null;
g.eval("(" + function () {
        var dbg = new Debug(debuggeeGlobal);
        dbg.hooks = {
            debuggerHandler: function (frame) {
                var a = [];
                assertEq(frame === frame.older, false);
                for (; frame; frame = frame.older)
                    a.push(frame.type === 'call' ? frame.callee.name : frame.type);
                a.reverse();
                result = a.join(", ");
            }
        };
    } + ")();");

function first() { return second(); }
function second() { return eval("third()"); }
function third() { debugger; }
first();
assertEq(g.result, "global, first, second, eval, third");
