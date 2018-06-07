assertEq(scriptedCallerGlobal(), this);

var g = newGlobal();
assertEq(g.evaluate("scriptedCallerGlobal()"), g);
assertEq(g.scriptedCallerGlobal(), this);
