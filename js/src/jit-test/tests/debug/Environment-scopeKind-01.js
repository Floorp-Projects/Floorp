// Environment.prototype.scopeKind produces expected values.

load(libdir + 'eqArrayHelper.js');

var g = newGlobal({ newCompartment: true });
var dbg = Debugger(g);

function getScopeKinds(text) {
  const kinds = [];
  dbg.onDebuggerStatement = frame => {
    let env = frame.environment;
    while (env) {
      kinds.push(env.scopeKind);
      env = env.parent;
    }
  };
  g.eval(text);
  return kinds;
}

assertEqArray(getScopeKinds("function f(x) { debugger; }; f()"),
  ["function", "global", null]);
assertEqArray(getScopeKinds("function f(x) { let y = 0; debugger; }; f()"),
  ["function lexical", "function", "global", null]);
assertEqArray(getScopeKinds("function f(x) { let y = 0; with(x) { debugger; } } f({})"),
  [null, "function lexical", "function", "global", null]);
