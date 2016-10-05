setJitCompilerOption('ion.warmup.trigger', 0);
gczeal(7, 1);
var dbgGlobal = newGlobal();
var dbg = new dbgGlobal.Debugger();
dbg.addDebuggee(this);
function f(x, await = () => Array.isArray(revocable.proxy), ...get) {
    dbg.getNewestFrame().older.eval("print(a)");
}
function a() {}
for (var i = 0; i < 10; i++) f();
