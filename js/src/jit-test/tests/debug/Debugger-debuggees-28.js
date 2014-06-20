// Test that on->off->on and off->on->off toggles don't crash.

function addRemove(dbg, g) {
  dbg.addDebuggee(g);
  var f = dbg.getNewestFrame();
  while (f)
    f = f.older;
  dbg.removeDebuggee(g);
}

function removeAdd(dbg, g) {
  dbg.removeDebuggee(g);
  dbg.addDebuggee(g);
  var f = dbg.getNewestFrame();
  while (f)
    f = f.older;
}

function newGlobalDebuggerPair(toggleSeq) {
  var g = newGlobal();
  var dbg = new Debugger;

  if (toggleSeq == removeAdd)
    dbg.addDebuggee(g);

  g.eval("" + function f() { return g(); });
  g.eval("" + function g() { return h(); });
  g.eval("line0 = Error().lineNumber;");
  g.eval("" + function h() {
    for (var i = 0; i < 100; i++)
      interruptIf(i == 95);
    debugger;
    return i;
  });

  setInterruptCallback(function () { return true; });

  return [g, dbg];
}

function testInterrupt(toggleSeq) {
  var [g, dbg] = newGlobalDebuggerPair(toggleSeq);

  setInterruptCallback(function () {
    toggleSeq(dbg, g);
    return true;
  });

  assertEq(g.f(), 100);
}

function testPrologue(toggleSeq) {
  var [g, dbg] = newGlobalDebuggerPair(toggleSeq);

  dbg.onEnterFrame = function (f) {
    if (f.callee && f.callee.name == "h")
      toggleSeq(dbg, g);
  };

  assertEq(g.f(), 100);
}

function testEpilogue(toggleSeq) {
  var [g, dbg] = newGlobalDebuggerPair(toggleSeq);

  dbg.onEnterFrame = function (f) {
    if (f.callee && f.callee.name == "h") {
      f.onPop = function () {
        toggleSeq(dbg, g);
      };
    }
  };

  assertEq(g.f(), 100);
}

function testTrap(toggleSeq) {
  var [g, dbg] = newGlobalDebuggerPair(toggleSeq);

  dbg.onEnterFrame = function (f) {
    if (f.callee && f.callee.name == "h") {
      var offs = f.script.getLineOffsets(g.line0 + 2);
      assertEq(offs.length > 0, true);
      f.script.setBreakpoint(offs[0], { hit: function () {
        toggleSeq(dbg, g);
      }});
    }
  };

  assertEq(g.f(), 100);
}

function testDebugger(toggleSeq) {
 var [g, dbg] = newGlobalDebuggerPair(toggleSeq);

  dbg.onDebuggerStatement = function () {
    toggleSeq(dbg, g);
  };

  assertEq(g.f(), 100);
}

testInterrupt(addRemove);
testInterrupt(removeAdd);

testPrologue(removeAdd);
testEpilogue(removeAdd);
testTrap(removeAdd);
testDebugger(removeAdd);
