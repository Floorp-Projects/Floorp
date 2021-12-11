// |jit-test| error:123
var g = newGlobal({newCompartment: true});
var dbg = new Debugger;
dbg.addDebuggee(g);
dbg.onEnterFrame = function(frame) {
    frame.onPop = function() {
        dbg.removeDebuggee(g);
        throw 123;
    }
}
g.eval("(function() {})()");
