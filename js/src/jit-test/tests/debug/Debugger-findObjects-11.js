// This shouldn't segfault.

var g = newGlobal();
g.eval(`function f() { return function() {
  function g() {}
}; }`);
new Debugger(g).findObjects();
