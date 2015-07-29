// |jit-test| allow-unhandlable-oom; allow-oom

if (typeof oomAfterAllocations == "function") {
  Debugger()
  oomAfterAllocations(6)
  Debugger()
}
