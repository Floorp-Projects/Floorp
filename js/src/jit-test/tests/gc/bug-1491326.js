// |jit-test| --fuzzing-safe

if (!('oomTest' in this))
    quit();

var g = newGlobal({newCompartment: true});
g.parent = this;
g.eval("new Debugger(parent).onExceptionUnwind = function() {}");
oomTest(() => l, (true));
