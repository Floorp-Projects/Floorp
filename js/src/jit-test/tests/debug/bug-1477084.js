// Don't assert trying to force return before the initial yield of an async function.

var g = newGlobal({newCompartment: true});
g.parent = this;
g.parentExc = new Error("pants");
g.eval(`
  var dbg = new Debugger;
  var pw = dbg.addDebuggee(parent);
  var hits = 0;
  dbg.onExceptionUnwind = function (frame) {
    dbg.onExceptionUnwind = undefined;
    return {return: undefined};
  };
  dbg.uncaughtExceptionHook = exc => {
    hits++;
    assertEq(exc instanceof TypeError, true);
    assertEq(/force return.*before the initial yield/.test(exc.message), true);
    return {throw: pw.makeDebuggeeValue(parentExc)};
  };
`);

async function* method({ x: callbackfn = unresolvableReference }) {}
try {
  method();
} catch (exc) {
  g.dbg.enabled = false;
  assertEq(exc, g.parentExc);
}

assertEq(g.hits, 1);
