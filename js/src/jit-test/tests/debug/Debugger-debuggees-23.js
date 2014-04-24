// Adding a debuggee allowed with scripts on stack from stranger places.

// Test CCW.
(function testCCW() {
  var g = newGlobal();
  var dbg = new Debugger;
  g.dbg = dbg;
  g.GLOBAL = g;

  g.turnOnDebugger = function () {
    dbg.addDebuggee(g);
  };

  g.eval("" + function f(d) {
    turnOnDebugger();
    assertEq(dbg.hasDebuggee(GLOBAL), true);
  });

  g.eval("(" + function test() {
    f(false);
    f(false);
    f(true);
    f(true);
  } + ")();");
})();

// Test getter.
(function testGetter() {
  var g = newGlobal();
  g.dbg = new Debugger;
  g.GLOBAL = g;

  g.eval("" + function f(obj) {
    obj.foo;
    assertEq(dbg.hasDebuggee(GLOBAL), true);
  });

  g.eval("(" + function test() {
    f({ get foo() { dbg.addDebuggee(GLOBAL); } });
  } + ")();");
})();

// Test setter.
(function testSetter() {
  var g = newGlobal();
  g.dbg = new Debugger;
  g.GLOBAL = g;

  g.eval("" + function f(obj) {
    obj.foo = 42;
    assertEq(dbg.hasDebuggee(GLOBAL), true);
  });

  g.eval("(" + function test() {
    f({ set foo(v) { dbg.addDebuggee(GLOBAL); } });
  } + ")();");
})();

// Test toString.
(function testToString() {
  var g = newGlobal();
  g.dbg = new Debugger;
  g.GLOBAL = g;

  g.eval("" + function f(obj) {
    obj + "";
    assertEq(dbg.hasDebuggee(GLOBAL), true);
  });

  g.eval("(" + function test() {
    f({ toString: function () { dbg.addDebuggee(GLOBAL); }});
  } + ")();");
})();

// Test valueOf.
(function testValueOf() {
  var g = newGlobal();
  g.dbg = new Debugger;
  g.GLOBAL = g;

  g.eval("" + function f(obj) {
    obj + "";
    assertEq(dbg.hasDebuggee(GLOBAL), true);
  });

  g.eval("(" + function test() {
    f({ valueOf: function () { dbg.addDebuggee(GLOBAL); }});
  } + ")();");
})();

// Test proxy trap.
(function testProxyTrap() {
  var g = newGlobal();
  g.dbg = new Debugger;
  g.GLOBAL = g;

  g.eval("" + function f(proxy) {
    proxy["foo"];
    assertEq(dbg.hasDebuggee(GLOBAL), true);
  });

  g.eval("(" + function test() {
    var handler = { get: function () { dbg.addDebuggee(GLOBAL); } };
    var proxy = new Proxy({}, handler);
    f(proxy);
  } + ")();");
})();
