// |jit-test| allow-oom; skip-if: !('oomTest' in this)

function x() {
    var global = newGlobal({sameZoneAs: this});
    global.eval('function f() { debugger; }');
    var debug = new Debugger(global);
    var foo;
    debug.onDebuggerStatement = function(frame) {
        foo = frame.arguments[0];
        return null;
    };
    global.eval('f(0)');
}

oomTest(x, false);
