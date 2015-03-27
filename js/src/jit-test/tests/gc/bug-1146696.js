// |jit-test| error: out of memory
dbg1 = new Debugger();
root2 = newGlobal();
dbg1.memory.onGarbageCollection = function(){}
dbg1.addDebuggee(root2);
for (var j = 0; j < 9999; ++j) {
    try {
        a
    } catch (e) {}
}
gcparam("maxBytes", gcparam("gcBytes") + 1);
g();
function g() {
    var x = "";
    function f() {}
    eval('');
    g();
}
