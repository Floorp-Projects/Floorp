// |jit-test| error: uncaught exception
g = newGlobal({newCompartment: true});
g.parent = this;
g.eval(`
    Debugger(parent).onExceptionUnwind = function(frame) { frame.older };
`);

var handler = {
    has: function(tgt, key) { if (key == 'throw') { throw null; } }
};

var proxy = new Proxy({}, handler);

for (let k of ['foo', 'throw']) {
    k in proxy;
}
