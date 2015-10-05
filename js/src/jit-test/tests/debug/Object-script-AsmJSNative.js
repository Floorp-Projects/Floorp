function test(stdlib, foreign) {
    "use asm"
    function f(y) {
        y = +y;
    }
    return f;
};
var g = newGlobal();
g.parent = this;
g.eval(`
  var dbg = new Debugger();
  var parentw = dbg.addDebuggee(parent);
  var testw = parentw.makeDebuggeeValue(parent.test);
  var scriptw = testw.script;
`);
