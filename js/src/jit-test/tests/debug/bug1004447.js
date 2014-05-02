// Tests that earlier try notes don't interfere with later exception handling.

var g = newGlobal();
g.debuggeeGlobal = this;
g.eval("(" + function () {
  dbg = new Debugger(debuggeeGlobal);
} + ")();");
var myObj = { p1: 'a', }
try {
  with(myObj) {
    do {
      throw value;
    } while(false);
  }
} catch(e) {
  // The above is expected to throw.
}

try {
  if(!(p1 === 1)) { }
} catch (e) {
  // The above is expected to throw.
}
