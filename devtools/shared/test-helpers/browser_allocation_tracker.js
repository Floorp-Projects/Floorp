/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Load the tracker in a dedicated loader using invisibleToDebugger and freshCompartment
// so that it can inspect any other module/compartment, even DevTools, chrome,
// and this script!
const { DevToolsLoader } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
const loader = new DevToolsLoader();
loader.invisibleToDebugger = true;
loader.freshCompartment = true;
const { allocationTracker } =
  loader.require("chrome://mochitests/content/browser/devtools/shared/test-helpers/allocation-tracker");

add_task(async function() {
  // Use a sandbox to allocate test javascript object in order to avoid any
  // external noise
  const global = Cu.Sandbox("http://example.com");

  const tracker = allocationTracker({ watchGlobal: global });
  const before = tracker.stillAllocatedObjects();

  /* eslint-disable no-undef */
  Cu.evalInSandbox("let list; new " + function() {
    list = [];
    for (let i = 0; i < 1000; i++) {
      list.push({});
    }
  }, global, undefined, "test-file.js", 1);
  /* eslint-enable no-undef */

  const allocations = tracker.countAllocations();
  ok(allocations > 1000,
    `At least 1000 objects are reported as created (${allocations})`);

  // Uncomment this and comment the call to `countAllocations` to debug the allocations.
  // The call to `countAllocations` will reset the allocation record.
  // tracker.logAllocationSites();

  const afterCreation = tracker.stillAllocatedObjects();
  ok(afterCreation - before > 1000,
    `At least 1000 more objects are reported still allocated (${before}` +
    ` + ${afterCreation - before} -> ${afterCreation})`);

  Cu.evalInSandbox("list = null;", global, undefined, "test-file.js", 7);

  Cu.forceGC();
  Cu.forceCC();

  const afterGC = tracker.stillAllocatedObjects();
  ok(afterCreation - afterGC > 1000,
    `At least 1000 less objects are reported still allocated (${afterCreation}` +
    ` -(${afterGC - afterCreation})-> ${afterGC})`);

  tracker.stop();
});
