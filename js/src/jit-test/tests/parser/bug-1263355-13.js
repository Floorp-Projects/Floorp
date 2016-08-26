// |jit-test| error: ReferenceError

// jsfunfuzz-generated
gczeal(9);
for (var i in function(){});
s = newGlobal();
aa = f();
function f(x) {
    evalcx(x, s)
}
function h(x) {
    f(x)
}
// Adapted from randomly chosen test: js/src/jit-test/tests/debug/resumption-05.js
h("\
    var g = newGlobal();\
    g.debuggeeGlobal = this;\
    g.eval(\"(\" + function() {\
        var dbg = Debugger(debuggeeGlobal);\
        dbg.onDebuggerStatement = function(frame) {\
            frame.eval(\"f\")\
        }\
    } + \")()\");\
    debugger;\
");
z;
