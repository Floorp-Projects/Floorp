// A closure's .environment keeps the lexical environment alive even if the closure is destroyed.

var N = 4;
var g = newGlobal('new-compartment');
g.eval("function add(a) { return function (b) { return eval('a + b'); }; }");
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
var aw = gw.getOwnPropertyDescriptor("add").value;

// Create N closures and collect environments.
var arr = [];
for (var i = 0; i < N; i++)
    arr[i] = aw.call(null, i).return.environment;

// Test that they work now.
function check() {
    for (var i = 0; i < N; i++) {
        assertEq(arr[i].find("b"), null);
        assertEq(arr[i].find("a"), arr[i]);
    }
}
check();

// Test that they work after gc.
gc();
gc({});
g.gc(g);
check();
