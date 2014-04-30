// Test that lazy inner functions inside eval are tagged properly so we don't
// incorrectly do NAME -> GNAME optimization.

var g = newGlobal();
var dbg = new Debugger(g);
dbg.onNewScript = function delazify(script, global) {
  // Force delazification of inner functions.
  script.getChildScripts();
};

g.eval("" + function f() {
  var $;
  eval('var obj={foo:1}; $=function() { assertEq(obj.foo, 1); }');
  return $;
});
g.eval("f()();");
