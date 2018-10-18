dbgGlobal = newGlobal();
dbg = new dbgGlobal.Debugger;
dbg.addDebuggee(this);

function f() {
    dbg.getNewestFrame().older.eval("");
}

function execModule(source) {
    m = parseModule(source);
    m.declarationInstantiation();
    m.evaluation();
}

execModule("f();");
gc();

let caught;
try {
    execModule("throw 'foo'");
} catch (e) {
    caught = e;
}
assertEq(caught, 'foo');
