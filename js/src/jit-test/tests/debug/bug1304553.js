var dbgGlobal = newGlobal({newCompartment: true});
var dbg = new dbgGlobal.Debugger();
dbg.addDebuggee(this);
function evalInFrame() {
    var frame = dbg.getNewestFrame().older.older;
    frame.eval("1");
};
var actual = '';
try {
    function f() {
        evalInFrame();
    }
} catch (e) {}
let m = parseModule(`
                    actual = '';
                    for (var i = 0; i < 1; ++i)
                        f(i);
                    actual;
                    `);
moduleLink(m);
moduleEvaluate(m);
