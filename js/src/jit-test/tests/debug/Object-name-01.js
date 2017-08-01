// Debugger.Object.prototype.name

load(libdir + 'nightly-only.js');

var g = newGlobal();
var dbg = Debugger(g);
var name;
dbg.onDebuggerStatement = function (frame) { name = frame.callee.name; };

g.eval("(function f() { debugger; })();");
assertEq(name, "f");
g.eval("(function () { debugger; })();");
assertEq(name, undefined);
g.eval("Function('debugger;')();");
assertEq(name, "anonymous");
g.eval("(async function grondo() { debugger; })();");
assertEq(name, "grondo");
nightlyOnly(g.SyntaxError, () => {
  g.eval("(async function* estux() { debugger; })().next();");
  assertEq(name, "estux");
});
