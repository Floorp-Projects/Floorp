/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Load the tracker very first in order to ensure tracking all objects created by DevTools.
// This is especially important for allocation sites. We need to catch the global the
// earliest possible in order to ensure that all allocation objects come with a stack.
//
// If we want to track DevTools module loader we should ensure loading Loader.jsm within
// the `testScript` Function. i.e. after having calling startRecordingAllocations.
let tracker;
{
  const { DevToolsLoader } = ChromeUtils.import(
    "resource://devtools/shared/loader/Loader.jsm"
  );
  const loader = new DevToolsLoader({
    invisibleToDebugger: true,
  });
  const { allocationTracker } = loader.require(
    "chrome://mochitests/content/browser/devtools/shared/test-helpers/allocation-tracker"
  );
  tracker = allocationTracker({ watchDevToolsGlobals: true });
}

// /!\ Be careful about imports/require
//
// Some tests may record the very first time we load a module.
// If we start loading them from here, we might only retrieve the already loaded
// module from the loader's cache. This would no longer highlight the cost
// of loading a new module from scratch.
//
// => Avoid loading devtools module as much as possible
// => If you really have to, lazy load them

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
XPCOMUtils.defineLazyGetter(this, "TrackedObjects", () => {
  return ChromeUtils.import(
    "resource://devtools/shared/test-helpers/tracked-objects.jsm"
  );
});

// So that PERFHERDER data can be extracted from the logs.
SimpleTest.requestCompleteLog();

// We have to disable testing mode, or various debug instructions are enabled.
// We especially want to disable redux store history, which would leak all the actions!
SpecialPowers.pushPrefEnv({
  set: [["devtools.testing", false]],
});

// Set DEBUG_DEVTOOLS_ALLOCATIONS=allocations|leaks in order print debug informations.
const env = Cc["@mozilla.org/process/environment;1"].getService(
  Ci.nsIEnvironment
);
const DEBUG_ALLOCATIONS = env.get("DEBUG_DEVTOOLS_ALLOCATIONS");

async function addTab(url) {
  const tab = BrowserTestUtils.addTab(gBrowser, url);
  gBrowser.selectedTab = tab;
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  return tab;
}

/**
 * This function will force some garbage collection before recording
 * data about allocated objects.
 *
 * This accept an optional boolean to also record the content process objects
 * of the current tab. That, in addition of objects from the parent process,
 * which are always recorded.
 *
 * This return same data object which is meant to be passed to `stopRecordingAllocations` as-is.
 *
 * See README.md file in this folder.
 */
async function startRecordingAllocations({
  alsoRecordContentProcess = false,
} = {}) {
  // Also start recording allocations in the content process, if requested
  if (alsoRecordContentProcess) {
    await SpecialPowers.spawn(
      gBrowser.selectedBrowser,
      [DEBUG_ALLOCATIONS],
      async debug_allocations => {
        const { DevToolsLoader } = ChromeUtils.import(
          "resource://devtools/shared/loader/Loader.jsm"
        );
        const loader = new DevToolsLoader({
          invisibleToDebugger: true,
        });
        const { allocationTracker } = loader.require(
          "chrome://mochitests/content/browser/devtools/shared/test-helpers/allocation-tracker"
        );
        // We watch all globals in the content process, (instead of only DevTools global in parent process)
        // because we may easily leak web page objects, which aren't in DevTools global.
        const tracker = allocationTracker({ watchAllGlobals: true });

        // /!\ HACK: save tracker and doGC on DevToolsLoader in order to be able to reuse
        // them in a following call to SpecialPowers.spawn
        DevToolsLoader.tracker = tracker;

        await tracker.startRecordingAllocations(debug_allocations);
      }
    );
    // Trigger a GC in the parent process as this additional ContentTask
    // seems to make harder to release objects created before we start recording.
    await tracker.doGC();
  }

  await tracker.startRecordingAllocations(DEBUG_ALLOCATIONS);
}

/**
 * See doc of startRecordingAllocations
 */
async function stopRecordingAllocations(
  recordName,
  { alsoRecordContentProcess = false } = {}
) {
  // Ensure that Memory API didn't ran out of buffers
  ok(!tracker.overflowed, "Allocation were all recorded in the parent process");

  // And finally, retrieve the record *after* having ran the test
  const parentProcessData = await tracker.stopRecordingAllocations(
    DEBUG_ALLOCATIONS
  );

  const objectNodeIds = TrackedObjects.getAllNodeIds();
  if (objectNodeIds.length) {
    tracker.traceObjects(objectNodeIds);
  }

  let contentProcessData = null;
  if (alsoRecordContentProcess) {
    contentProcessData = await SpecialPowers.spawn(
      gBrowser.selectedBrowser,
      [DEBUG_ALLOCATIONS],
      debug_allocations => {
        const { DevToolsLoader } = ChromeUtils.import(
          "resource://devtools/shared/loader/Loader.jsm"
        );
        const { tracker } = DevToolsLoader;
        ok(
          !tracker.overflowed,
          "Allocation were all recorded in the content process"
        );
        return tracker.stopRecordingAllocations(debug_allocations);
      }
    );
  }

  const trackedObjectsInContent = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => {
      const TrackedObjects = ChromeUtils.import(
        "resource://devtools/shared/test-helpers/tracked-objects.jsm"
      );
      const objectNodeIds = TrackedObjects.getAllNodeIds();
      if (objectNodeIds.length) {
        const { DevToolsLoader } = ChromeUtils.import(
          "resource://devtools/shared/loader/Loader.jsm"
        );
        const { tracker } = DevToolsLoader;
        // Record the heap snapshot from the content process,
        // and pass the record's filepath to the parent process
        // As only the parent process can read the file because
        // of sandbox restrictions made to content processes regarding file I/O.
        const snapshotFile = tracker.getSnapshotFile();
        return { snapshotFile, objectNodeIds };
      }
      return null;
    }
  );
  if (trackedObjectsInContent) {
    tracker.traceObjects(
      trackedObjectsInContent.objectNodeIds,
      trackedObjectsInContent.snapshotFile
    );
  }

  // Craft the JSON object required to save data in talos database
  info(
    `The ${recordName} test leaked ${parentProcessData.objectsWithStack} objects (${parentProcessData.objectsWithoutStack} with missing allocation site) in the parent process`
  );
  const PERFHERDER_DATA = {
    framework: {
      name: "devtools",
    },
    suites: [
      {
        name: recordName + ":parent-process",
        subtests: [
          {
            name: "objects-with-no-stacks",
            value: parentProcessData.objectsWithoutStack,
          },
          {
            name: "objects-with-stacks",
            value: parentProcessData.objectsWithStack,
          },
          {
            name: "memory",
            value: parentProcessData.memory,
          },
        ],
      },
    ],
  };
  if (alsoRecordContentProcess) {
    info(
      `The ${recordName} test leaked ${contentProcessData.objectsWithStack} objects (${contentProcessData.objectsWithoutStack} with missing allocation site) in the content process`
    );
    PERFHERDER_DATA.suites.push({
      name: recordName + ":content-process",
      subtests: [
        {
          name: "objects-with-no-stacks",
          value: contentProcessData.objectsWithoutStack,
        },
        {
          name: "objects-with-stacks",
          value: contentProcessData.objectsWithStack,
        },
        {
          name: "memory",
          value: contentProcessData.memory,
        },
      ],
    });
  }
  // Log it to stdout so that perfherder can collect this data.
  // This only works if we called `SimpleTest.requestCompleteLog()`!
  info("PERFHERDER_DATA: " + JSON.stringify(PERFHERDER_DATA));
}
