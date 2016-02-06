// |jit-test| allow-oom

if (!('oomAfterAllocations' in this))
  quit();

var g = newGlobal();
var dbg = new Debugger;
g.h = function h(d) {
  if (d) {
    dbg.addDebuggee(g);
    var f = dbg.getNewestFrame().older;
    f.st_p1((oomAfterAllocations(10)) + "foo = 'string of 42'");
  }
}
g.eval("" + function f(d) {
  g(d);
});
g.eval("" + function g(d) {
  h(d);
});
g.eval("(" + function () {
  for (i = 0; i < 5; i++)
    f(false);
  assertEq(f(true), "string of 42");
} + ")();");
