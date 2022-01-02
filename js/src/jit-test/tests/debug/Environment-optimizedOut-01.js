// Optimized out scopes should be considered optimizedOut.

var g = newGlobal({newCompartment: true});
var dbg = new Debugger;
dbg.addDebuggee(g);

g.eval("" + function f() {
  var x = 42;
  {
    let y = 43;
    (function () { })();
  }
});

dbg.onEnterFrame = function (f) {
  if (f.callee && (f.callee.name === undefined)) {
    blockenv = f.environment.parent;
    assertEq(blockenv.optimizedOut, true);
    assertEq(blockenv.inspectable, true);
    assertEq(blockenv.type, "declarative");
    assertEq(blockenv.calleeScript, null);
    assertEq(blockenv.names().indexOf("y") !== -1, true);

    funenv = blockenv.parent;
    assertEq(funenv.optimizedOut, true);
    assertEq(funenv.inspectable, true);
    assertEq(funenv.type, "declarative");
    assertEq(funenv.calleeScript, f.older.script);
    assertEq(funenv.names().indexOf("x") !== -1, true);

    globalenv = funenv.parent.parent;
    assertEq(globalenv.optimizedOut, false);
    assertEq(globalenv.inspectable, true);
    assertEq(globalenv.type, "object");
    assertEq(globalenv.calleeScript, null);

    dbg.removeDebuggee(g);

    assertEq(blockenv.inspectable, false);
    assertEq(funenv.inspectable, false);
  }
}

g.f();
