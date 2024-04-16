// OOM during SharedArrayBuffer initialization can expose partially initialized
// object to metadata builder.
// It shouldn't crash.

newGlobal({ newCompartment: true }).Debugger(this).memory.trackingAllocationSites = true;
for (let i = 0; i < 9; i++) {
  oomTest(function () {
    class C extends WebAssembly.Memory {}
    new C({
      initial: 0,
      maximum: 1,
      shared: 1,
    });
  });
}
