// |jit-test| error: ReferenceError

var g = newGlobal();
var dbg = new g.Debugger(this);

(function() {
  const x = [][x]
})()
