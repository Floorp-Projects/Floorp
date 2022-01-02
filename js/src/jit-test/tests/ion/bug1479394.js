// |jit-test| skip-if: !('stackTest' in this)
var dbgGlobal = newGlobal({newCompartment: true});
var dbg = new dbgGlobal.Debugger(this);
function f1() {
    dbg.getNewestFrame().older;
    throw new Error();
}
function f2() { f1(); }
function f3() { f2(); }
stackTest(f3);
