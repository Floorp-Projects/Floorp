// |jit-test| --no-ggc; allow-oom; allow-unhandlable-oom
gc();
dbg1 = new Debugger();
root2 = newGlobal();
dbg1.memory.onGarbageCollection = function(){}
dbg1.addDebuggee(root2);
for (var j = 0; j < 9999; ++j) {
    try {
        a
    } catch (e) {}
}
gcparam("maxBytes", gcparam("gcBytes") + 8000);
function g(i) {
    if (i == 0)
        return;
    var x = "";
    function f() {}
    eval('');
    g(i - 1);
}
g(100);
