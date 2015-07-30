// Debugger.prototype.findObjects should not expose internal JSFunction objects.

var g = newGlobal();
g.eval(`function f() { return function() {}; }`);
new Debugger(g).findObjects();
