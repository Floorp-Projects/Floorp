// |jit-test| debug
// When the debugger is triggered from different stack frames that happen to
// occupy the same memory, it must deliver different Debug.Frame objects.

var g = newGlobal('new-compartment');
g.debuggeeGlobal = this;
g.eval("var hits;");
g.eval("(" + function () {
        var a = [];
        var dbg = Debug(debuggeeGlobal);
        dbg.hooks = {
            debuggerHandler: function (frame) {
                for (var i = 0; i < a.length; i++) 
                    assertEq(a[i] === frame, false);
                a.push(frame);
                hits++;
            }
        };
    } + ")()");

function f() { debugger; }
function h() { debugger; f(); }
g.hits = 0;
for (var i = 0; i < 4; i++)
    h();
assertEq(g.hits, 8);
