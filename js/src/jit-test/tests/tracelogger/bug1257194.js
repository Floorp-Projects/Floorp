// |jit-test| allow-oom

if (!('oomAfterAllocations' in this))
  quit();

var du = new Debugger();
var obj = du.drainTraceLogger();
oomAfterAllocations(1);
du.drainTraceLogger().length;
