/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Load the tracker in a dedicated loader using invisibleToDebugger and freshCompartment
// so that it can inspect any other module/compartment, even DevTools, chrome,
// and this script!
const { DevToolsLoader } = ChromeUtils.importESModule(
  "resource://devtools/shared/loader/Loader.sys.mjs"
);
const loader = new DevToolsLoader({
  invisibleToDebugger: true,
  freshCompartment: true,
});
const { allocationTracker } = loader.require(
  "chrome://mochitests/content/browser/devtools/shared/test-helpers/allocation-tracker"
);
const TrackedObjects = loader.require(
  "resource://devtools/shared/test-helpers/tracked-objects.sys.mjs"
);

// This test record multiple times complete heap snapshot,
// so that it can take a little bit to complete.
requestLongerTimeout(2);

add_task(async function () {
  // Use a sandbox to allocate test javascript object in order to avoid any
  // external noise
  const global = Cu.Sandbox("http://example.com");

  const tracker = allocationTracker({ watchGlobal: global });
  const before = tracker.stillAllocatedObjects();

  /* eslint-disable no-undef */
  // This will allocation 1001 objects. The array and 1000 elements in it.
  Cu.evalInSandbox(
    "let list; new " +
      function () {
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

add_task(async function () {
  const leaked = {};
  TrackedObjects.track(leaked);
  let transient = {};
  TrackedObjects.track(transient);

  is(TrackedObjects.getAllNodeIds().length, 2, "The two objects are reported");

  info("Free the transient object");
  transient = null;
  Cu.forceGC();

  is(
    TrackedObjects.getAllNodeIds().length,
    1,
    "We now only have the leaked object"
  );
  TrackedObjects.clear();
});

add_task(async function () {
  info("Test start and stop recording without any debug mode");
  const tracker = allocationTracker({ watchDevToolsGlobals: true });
  await tracker.startRecordingAllocations();
  await tracker.stopRecordingAllocations();
  tracker.stop();
});

add_task(async function () {
  info("Test start and stop recording with 'allocations' debug mode");
  const tracker = allocationTracker({ watchDevToolsGlobals: true });
  await tracker.startRecordingAllocations("allocations");
  await tracker.stopRecordingAllocations("allocations");
  tracker.stop();
});

add_task(async function () {
  info("Test start and stop recording with 'leaks' debug mode");
  const tracker = allocationTracker({ watchDevToolsGlobals: true });
  await tracker.startRecordingAllocations("leaks");
  await tracker.stopRecordingAllocations("leaks");
  tracker.stop();
});

add_task(async function () {
  info("Test start and stop recording with tracked objects");

  const leaked = {};
  TrackedObjects.track(leaked);

  const tracker = allocationTracker({ watchAllGlobals: true });
  await tracker.startRecordingAllocations();
  await tracker.stopRecordingAllocations();
  tracker.stop();

  TrackedObjects.clear();
});

add_task(async function () {
  info("Test start and stop recording with tracked objects");

  const sandbox = Cu.Sandbox(window);
  const tracker = allocationTracker({ watchGlobal: sandbox });
  await tracker.startRecordingAllocations("leaks");

  Cu.evalInSandbox("this.foo = {};", sandbox, null, "sandbox.js", 1);

  const record = await tracker.stopRecordingAllocations("leaks");
  is(
    record.objectsWithStack,
    1,
    "We get only one leaked objects, the foo object of the sandbox."
  );
  ok(
    record.objectsWithoutStack > 10,
    "We get an handful of objects without stacks. Most likely created by Memory API itself."
  );

  is(
    record.leaks.length,
    2,
    "We get the one leak and the objects with missing stacks"
  );
  is(
    record.leaks[0].src,
    "UNKNOWN",
    "First item is the objects with missing stacks"
  );
  // In theory the two following values should be equal,
  // but they aren't always because of some dark matter around objects with missing stacks.
  // `count` is computed out of `takeCensus`, while `objectsWithoutStack` uses `drainAllocationsLog`
  // While the first go through the current GC graph, the second is a record of allocations over time,
  // this probably explain why there is some subtle difference
  ok(
    record.leaks[0].count <= record.objectsWithoutStack,
    "For now, the leak report intermittently assume there is less leaked objects than the summary"
  );
  is(record.leaks[1].src, "sandbox.js", "Second item if about our 'foo' leak");
  is(record.leaks[1].count, 1, "We leak one object on this file");
  is(record.leaks[1].lines.length, 1, "We leak from only one line");
  is(record.leaks[1].lines[0], "1: 1", "On first line, we leak one object");
  tracker.stop();

  TrackedObjects.clear();
});

add_task(async function () {
  info("Test that transient globals are not leaked");

  const tracker = allocationTracker({ watchAllGlobals: true });

  let sandboxBefore = Cu.Sandbox(window);
  // We need to allocate at least one object from the global to reproduce the leak
  Cu.evalInSandbox(
    "this.foo = {};",
    sandboxBefore,
    null,
    "sandbox-before.js",
    1
  );
  const weakBefore = Cu.getWeakReference(sandboxBefore);
  sandboxBefore = null;

  await tracker.startRecordingAllocations();

  ok(
    !weakBefore.get(),
    "Sandbox created before the record should have been freed by GCs done by startRecordingAllocations"
  );

  let sandboxDuring = Cu.Sandbox(window);
  // We need to allocate at least one object from the global to reproduce the leak
  Cu.evalInSandbox(
    "this.bar = {};",
    sandboxDuring,
    null,
    "sandbox-during.js",
    1
  );
  const weakDuring = Cu.getWeakReference(sandboxDuring);
  sandboxDuring = null;

  await tracker.stopRecordingAllocations();

  ok(
    !weakDuring.get(),
    "Sandbox should have been freed by GCs done by stopRecordingAllocations"
  );

  tracker.stop();
});
