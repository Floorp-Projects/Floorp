// Test that a finalization registry target that is reachable from its global
// does not keep the global alive indefinitely.

let global = newGlobal({newCompartment: true});
global.eval(`
  this.target = {};  // Target is reachable from global.
  this.registry = new FinalizationRegistry(() => assertEq(0, 1));
  registry.register(target, 'held');
  this.finalizeObserver = makeFinalizeObserver();
`);

assertEq(finalizeCount(), 0);

global = undefined;
gc();

assertEq(finalizeCount(), 1);
