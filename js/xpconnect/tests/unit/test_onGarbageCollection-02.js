// Test multiple debuggers, GCs, and zones interacting with each other.
//
// Note: when observing both globals, but GC'ing in only one, we don't test that
// we *didn't* GC in the other zone because GCs are finicky and unreliable. That
// used to work when this was a jit-test, but in the process of migrating to
// xpcshell, we lost some amount of reliability and determinism.

const root1 = newGlobal();
const dbg1 = new Debugger();
dbg1.addDebuggee(root1)

const root2 = newGlobal();
const dbg2 = new Debugger();
dbg2.addDebuggee(root2)

let fired1 = false;
let fired2 = false;
dbg1.memory.onGarbageCollection = _ => fired1 = true;
dbg2.memory.onGarbageCollection = _ => fired2 = true;

function reset() {
  fired1 = false;
  fired2 = false;
}

function run_test() {
  do_test_pending();

  gc();
  executeSoon(() => {
    reset();

    // GC 1 only
    root1.eval(`gc(this)`);
    executeSoon(() => {
      equal(fired1, true);

      // GC 2 only
      reset();
      root2.eval(`gc(this)`);
      executeSoon(() => {
        equal(fired2, true);

        // Full GC
        reset();
        gc();
        executeSoon(() => {
          equal(fired1, true);
          equal(fired2, true);

          // Full GC with no debuggees
          reset();
          dbg1.removeAllDebuggees();
          dbg2.removeAllDebuggees();
          gc();
          executeSoon(() => {
            equal(fired1, false);
            equal(fired2, false);

            // One debugger with multiple debuggees in different zones.

            dbg1.addDebuggee(root1);
            dbg1.addDebuggee(root2);

            // Just debuggee 1
            reset();
            root1.eval(`gc(this)`);
            executeSoon(() => {
              equal(fired1, true);
              equal(fired2, false);

              // Just debuggee 2
              reset();
              root2.eval(`gc(this)`);
              executeSoon(() => {
                equal(fired1, true);
                equal(fired2, false);

                // All debuggees
                reset();
                gc();
                executeSoon(() => {
                  equal(fired1, true);
                  equal(fired2, false);
                  do_test_finished();
                });
              });
            });
          });
        });
      });
    });
  });
}
