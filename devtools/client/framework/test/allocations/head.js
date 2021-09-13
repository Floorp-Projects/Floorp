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
    "resource://devtools/shared/Loader.jsm"
  );
  const loader = new DevToolsLoader({
    invisibleToDebugger: true,
  });
  const { allocationTracker } = loader.require(
    "chrome://mochitests/content/browser/devtools/shared/test-helpers/allocation-tracker"
  );
  tracker = allocationTracker({ watchDevToolsGlobals: true });
}

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

const TrackedObjects = ChromeUtils.import(
  "resource://devtools/shared/test-helpers/tracked-objects.jsm"
);

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
          "resource://devtools/shared/Loader.jsm"
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

  // Only do that from the parent process for now,
  // as traceObjects doesn't work from the content process.
  // It throws because the content process isn't allowed to read the snapshot files.
  const objectNodeIds = TrackedObjects.getAllNodeIds();
  if (objectNodeIds.length > 0) {
    tracker.traceObjects(objectNodeIds);
  }

  let contentProcessData = null;
  if (alsoRecordContentProcess) {
    contentProcessData = await SpecialPowers.spawn(
      gBrowser.selectedBrowser,
      [DEBUG_ALLOCATIONS],
      debug_allocations => {
        const { DevToolsLoader } = ChromeUtils.import(
          "resource://devtools/shared/Loader.jsm"
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
