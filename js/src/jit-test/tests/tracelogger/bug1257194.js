// |jit-test| allow-oom
var du = new Debugger();
var obj = du.drainTraceLogger();
oomAfterAllocations(1);
du.drainTraceLogger().length;
