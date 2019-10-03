// |jit-test| skip-if: !('gczeal' in this)
// Bug 1584195: Debugger.Frame finalizer should't try to apply
// IsAboutToBeFinalized to cells of other alloc kinds, whose arenas may have
// been turned over to fresh allocations.

try {
  evaluate(`
    var g9 = newGlobal({newCompartment: true});
    g9.parent = this;
    g9.eval(\`
      var dbg = new Debugger(parent);
      dbg.onEnterFrame = frame => {};
    \`);
    function lfPromise(x) {
        return new Promise(resolve => {});
    }
    async function lfAsync() {
        await lfPromise();
    } lfAsync();
  `);
  gczeal(20, 2);
  evaluate(`
    async function lfAsync() {} lfAsync();
  `);
  gczeal(12, 7);
  drainJobQueue();
  evaluate(`
    new { ...  v22 => { 
  `);
} catch (exc) {}
evaluate("");
