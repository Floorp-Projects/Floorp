// |jit-test| error: ReferenceError

var g = newGlobal({newCompartment: true});
var dbg = new g.Debugger(this);

(function() {
  const x = [][x]
})()
