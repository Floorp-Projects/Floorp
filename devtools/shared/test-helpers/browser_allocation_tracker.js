/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Load the tracker in a dedicated loader using invisibleToDebugger and freshCompartment
// so that it can inspect any other module/compartment, even DevTools, chrome,
// and this script!
const { DevToolsLoader } = ChromeUtils.import(
  "resource://devtools/shared/Loader.jsm"
);
const loader = new DevToolsLoader({
  invisibleToDebugger: true,
  freshCompartment: true,
});
const { allocationTracker } = loader.require(
  "chrome://mochitests/content/browser/devtools/shared/test-helpers/allocation-tracker"
);

add_task(async function() {
  // Use a sandbox to allocate test javascript object in order to avoid any
  // external noise
  const global = Cu.Sandbox("http://example.com");

  const tracker = allocationTracker({ watchGlobal: global });
  const before = tracker.stillAllocatedObjects();

  /* eslint-disable no-undef */
  // This will allocation 1001 objects. The array and 1000 elements in it.
  Cu.evalInSandbox(
    "let list; new " +
      function() {
        list = [];
        for (let i = 0; i < 1000; i++) {
          list.push({});
        }
      },
    global,
    undefined,
    "test-file.js",
    1,
    /* enforceFilenameRestrictions */ false
  );
  /* eslint-enable no-undef */

  const allocations = tracker.countAllocations();
  ok(
    allocations >= 1001,
    `At least 1001 objects are reported as created (${allocations})`
  );

  // Uncomment this and comment the call to `countAllocations` to debug the allocations.
  // The call to `countAllocations` will reset the allocation record.
  // tracker.logAllocationSites();

  const afterCreation = tracker.stillAllocatedObjects();
  is(
    afterCreation.objectsWithStack - before.objectsWithStack,
    1001,
    "We got exactly the expected number of objects recorded with an allocation site"
  );
  ok(
    afterCreation.objectsWithStack > before.objectsWithStack,
    "We got some random number of objects without an allocation site"
  );

  Cu.evalInSandbox(
    "list = null;",
    global,
    undefined,
    "test-file.js",
    7,
    /* enforceFilenameRestrictions */ false
  );

  Cu.forceGC();
  Cu.forceCC();

  const afterGC = tracker.stillAllocatedObjects();
  is(
    afterCreation.objectsWithStack - afterGC.objectsWithStack,
    1001,
    "All the expected objects were reported freed in the count with allocation sites"
  );
  ok(
    afterGC.objectsWithoutStack < afterCreation.objectsWithoutStack,
    "And we released some random number of objects without an allocation site"
  );

  tracker.stop();
});
