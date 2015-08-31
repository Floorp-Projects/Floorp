// Test that the onGarbageCollection reentrancy guard is on a per Debugger
// basis. That is if our first Debugger is observing our second Debugger's
// compartment, and this second Debugger triggers a GC inside its
// onGarbageCollection hook, the first Debugger's onGarbageCollection hook is
// still called.
//
// This is the scenario we are setting up: top level debugging the `debuggeree`
// global, which is debugging the `debuggee` global. Then, we trigger the
// following events:
//
//     debuggee gc
//         |
//         V
//     debuggeree's onGarbageCollection
//         |
//         V
//     debuggeree gc
//         |
//         V
//     top level onGarbageCollection
//
// Note that the top level's onGarbageCollection hook should be fired, at the
// same time that we are preventing reentrancy into debuggeree's
// onGarbageCollection hook.

function run_test() {
  do_test_pending();

  const debuggeree = newGlobal();
  const debuggee = debuggeree.debuggee = newGlobal();

  debuggeree.eval(
    `
    var dbg = new Debugger(this.debuggee);
    var fired = 0;
    dbg.memory.onGarbageCollection = _ => {
      fired++;
      gc(this);
    };
    `
  );

  const dbg = new Debugger(debuggeree);
  let fired = 0;
  dbg.memory.onGarbageCollection = _ => {
    fired++;
  };

  debuggee.eval(`gc(this)`);

  // Let first onGarbageCollection runnable get run.
  executeSoon(() => {

    // Let second onGarbageCollection runnable get run.
    executeSoon(() => {

      // Even though we request GC'ing a single zone, we can't rely on that
      // behavior and both zones could have been scheduled for gc for both
      // gc(this) calls.
      ok(debuggeree.fired >= 1);
      ok(fired >= 1);

      debuggeree.dbg.enabled = dbg.enabled = false;
      do_test_finished();
    });
  });
}
