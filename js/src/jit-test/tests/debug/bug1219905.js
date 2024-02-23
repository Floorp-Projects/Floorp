// |jit-test| allow-oom

// We need allow-oom here because the debugger reports an uncaught exception if
// it hits OOM calling the exception unwind hook. This causes the shell to exit
// with the OOM reason.

var g = newGlobal({newCompartment: true});
g.parent = this;
g.eval("new Debugger(parent).onExceptionUnwind = function() {}");
let finished = false;
oomTest(() => l, false);
