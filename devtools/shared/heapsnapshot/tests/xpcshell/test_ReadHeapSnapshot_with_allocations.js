/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that we can save a core dump with allocation stacks and read it back
// into a HeapSnapshot.

if (typeof Debugger != "function") {
  const { addDebuggerToGlobal } = ChromeUtils.import(
    "resource://gre/modules/jsdebugger.jsm"
  );
  addDebuggerToGlobal(this);
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
  for (let i = 0; i < 100; i++) {
    debuggee.eval("this.objects.push({})");
  }

  // Now save a snapshot that will include the allocation stacks and read it
  // back again.

  const filePath = ChromeUtils.saveHeapSnapshot({ runtime: true });
  ok(true, "Should be able to save a snapshot.");

  const snapshot = ChromeUtils.readHeapSnapshot(filePath);
  ok(snapshot, "Should be able to read a heap snapshot");
  ok(HeapSnapshot.isInstance(snapshot), "Should be an instanceof HeapSnapshot");

  do_test_finished();
}
