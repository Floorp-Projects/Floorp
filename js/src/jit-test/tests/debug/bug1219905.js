// |jit-test| allow-oom

// We need allow-oom here because the debugger reports an uncaught exception if
// it hits OOM calling the exception unwind hook. This causes the shell to exit
// with the OOM reason.

if (!('oomTest' in this))
    quit();

var g = newGlobal();
g.parent = this;
g.eval("new Debugger(parent).onExceptionUnwind = function() {}");
let finished = false;
oomTest(() => l, false);
