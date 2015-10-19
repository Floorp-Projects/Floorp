// Tests D.O.asEnvironment() returning the global lexical scope.

var g = newGlobal();
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);

g.evaluate(`
  var x = 42;
  let y = "foo"
`);

var globalLexical = gw.asEnvironment();
assertEq(globalLexical.names().length, 1);
assertEq(globalLexical.getVariable("y"), "foo");
assertEq(globalLexical.parent.getVariable("x"), 42);
