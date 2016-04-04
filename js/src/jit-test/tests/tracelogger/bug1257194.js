// |jit-test| allow-oom

if (!('oomAfterAllocations' in this))
  quit();

var du = new Debugger();
if (typeof du.drainTraceLogger == "function") {
    var obj = du.drainTraceLogger();
    oomAfterAllocations(1);
    du.drainTraceLogger().length;
}
