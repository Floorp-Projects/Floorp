// |jit-test| error: out of memory
var y = {
    then: function(m) {
        throw 0;
    }
};
Promise.resolve(y);

g = newGlobal();
g.parent = this;
g.eval("Debugger(parent).onExceptionUnwind = function(fr, e) {return {throw:e}};");
gcparam("maxBytes", gcparam("gcBytes"));
