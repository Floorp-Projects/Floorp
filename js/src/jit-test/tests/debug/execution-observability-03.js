// Tests that bare callVMs (in the delprop below) are patched correctly.

var o = {};
var global = this;
var p = new Proxy(o, {
  "deleteProperty": function (target, key) {
    var g = newGlobal();
    g.parent = global;
    g.eval("var dbg = new Debugger(parent); dbg.onEnterFrame = function(frame) {};");
    return true;
  }
});
function test() {
  for (var i=0; i<100; i++) {}
  assertEq(delete p.foo, true);
}
test();
