Object.defineProperty(this, "fuzzutils", {
    value: {
        evaluate: function() {},
    }
});
var g = newGlobal({
    newCompartment: true
});
g.parent = this;
g.eval("(" + function() {
    var dbg = Debugger(parent);
    dbg.onEnterFrame = function(frame) {
        frame.onStep = function() {}
    }
} + ")()");
fuzzutils.evaluate();
oomTest(new Function(), {expectExceptionOnFailure: false});
