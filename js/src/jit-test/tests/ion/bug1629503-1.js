// |jit-test| error: TypeError: invalid assignment to const
function f() {
    const g = newGlobal({newCompartment: true});
    const dbg = new Debugger(g);
    g.eval(`function f() {}`);
    dbg.onEnterFrame = function(frame) {
        [x, dbg] = [];
    }
    for (var i = 0; i < 5; i++) {
        g.eval("f()");
    }
}
f();
