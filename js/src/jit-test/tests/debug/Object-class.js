// Basic tests for Debugger.Object.prototype.class.
var g = newGlobal();
var dbg = new Debugger(g);
var hits = 0;
g.eval('function f() { debugger; }');

dbg.onDebuggerStatement = function (frame) {
    var arr = frame.arguments;
    assertEq(arr[0].class, "Object");
    assertEq(arr[1].class, "Array");
    assertEq(arr[2].class, "Function");
    assertEq(arr[3].class, "Date");
    assertEq(arr[4].class, "Object");
    assertEq(arr[5].class, "Function");
    assertEq(arr[6].class, "Date");
    hits++;
};
g.f(Object.prototype, [], eval, new Date,
    Proxy.create({}), Proxy.createFunction({}, eval), new Proxy(new Date, {}));
assertEq(hits, 1);

// Debugger.Object.prototype.class should see through cross-compartment
// wrappers.
g.eval('f(Object.prototype, [], eval, new Date,\
          Proxy.create({}), Proxy.createFunction({}, f), new Proxy(new Date, {}));');
assertEq(hits, 2);
