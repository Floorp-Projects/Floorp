var g = newGlobal();
var dbg = new g.Debugger(this);

try {
    function f() {}
    (1 for (x in []))
} catch (e) {}
gc()
