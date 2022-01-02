// Bug 1250190: Shouldn't crash. |jit-test| error: yadda

var g = newGlobal({newCompartment: true});
var dbg = new Debugger(g);
dbg.onNewGlobalObject = function () {
  dbg.onNewGlobalObject = function () { throw "yadda"; };
  newGlobal({newCompartment: true});
}
newGlobal({newCompartment: true});
