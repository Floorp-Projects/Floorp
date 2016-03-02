// Bug 1250190: Shouldn't crash. |jit-test| error: yadda

var g = newGlobal();
var dbg = new Debugger(g);
dbg.onNewGlobalObject = function () {
  dbg.onNewGlobalObject = function () { throw "yadda"; };
  newGlobal();
}
newGlobal();
