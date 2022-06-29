/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that we can save a core dump with very deep allocation stacks and read
// it back into a HeapSnapshot.

function stackDepth(stack) {
  return stack ? 1 + stackDepth(stack.parent) : 0;
}

function run_test() {
  Services.prefs.setBoolPref(
    "security.allow_parent_unrestricted_js_loads",
    true
  );
  Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
  Services.prefs.setBoolPref("security.allow_eval_in_parent_process", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("security.allow_parent_unrestricted_js_loads");
    Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
    Services.prefs.clearUserPref("security.allow_eval_in_parent_process");
  });

  // Create a Debugger observing a debuggee's allocations.
  const debuggee = new Cu.Sandbox(null);
  const dbg = new Debugger(debuggee);
  dbg.memory.trackingAllocationSites = true;

  // Allocate some objects in the debuggee that will have their allocation
  // stacks recorded by the Debugger.

  debuggee.eval("this.objects = []");
  debuggee.eval(
    function recursiveAllocate(n) {
      if (n <= 0) {
        return;
      }

      // Make sure to recurse before pushing the object so that when TCO is
      // implemented sometime in the future, it doesn't invalidate this test.
      recursiveAllocate(n - 1);
      this.objects.push({});
    }.toString()
  );
  debuggee.eval("recursiveAllocate = recursiveAllocate.bind(this);");
  debuggee.eval("recursiveAllocate(200);");

  // Now save a snapshot that will include the allocation stacks and read it
  // back again.

  const filePath = ChromeUtils.saveHeapSnapshot({ runtime: true });
  ok(true, "Should be able to save a snapshot.");

  const snapshot = ChromeUtils.readHeapSnapshot(filePath);
  ok(snapshot, "Should be able to read a heap snapshot");
  ok(HeapSnapshot.isInstance(snapshot), "Should be an instanceof HeapSnapshot");

  const report = snapshot.takeCensus({
    breakdown: {
      by: "allocationStack",
      then: { by: "count", bytes: true, count: true },
      noStack: { by: "count", bytes: true, count: true },
    },
  });

  // Keep this synchronized with `HeapSnapshot::MAX_STACK_DEPTH`!
  const MAX_STACK_DEPTH = 60;

  let foundStacks = false;
  report.forEach((v, k) => {
    if (k === "noStack") {
      return;
    }

    foundStacks = true;
    const depth = stackDepth(k);
    dumpn("Stack depth is " + depth);
    ok(
      depth <= MAX_STACK_DEPTH,
      "Every stack should have depth less than or equal to the maximum stack depth"
    );
  });
  ok(foundStacks);

  do_test_finished();
}
