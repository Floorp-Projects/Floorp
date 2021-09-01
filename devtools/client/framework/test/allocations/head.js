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
  // Also start recording allocations in the content process if requested
  let contentProcessObjects = null;
  let contentProcessMemory = null;
  if (alsoRecordContentProcess) {
    contentProcessMemory = await SpecialPowers.spawn(
      gBrowser.selectedBrowser,
      [],
      async () => {
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

        await tracker.doGC();

        return tracker.getAllocatedMemory();
      }
    );
    contentProcessObjects = await SpecialPowers.spawn(
      gBrowser.selectedBrowser,
      [],
      () => {
        const { DevToolsLoader } = ChromeUtils.import(
          "resource://devtools/shared/Loader.jsm"
        );
        const { tracker } = DevToolsLoader;
        return tracker.stillAllocatedObjects();
      }
    );
  }

  // Do a first pass of GC, to ensure all to-be-freed objects from the first run
  // are really freed.
  await tracker.doGC();

  // Then, record what was already allocated, which should not be declared
  // as potential leaks. For ex, there is all the modules already loaded
  // in the main DevTools loader.
  const parentProcessMemory = tracker.getAllocatedMemory();
  const parentProcessObjects = tracker.stillAllocatedObjects();

  return {
    parentProcessObjects,
    parentProcessMemory,
    contentProcessObjects,
    contentProcessMemory,
  };
}

/**
 * See doc of startRecordingAllocations
 */
async function stopRecordingAllocations(dataOnStart, recordName) {
  // Before computing allocations, re-do some GCs in order to free all what is to-be-freed.
  await tracker.doGC();

  // Ensure that Memory API didn't ran out of buffers
  ok(!tracker.overflowed, "Allocation were all recorded");

  // And finally, retrieve the number of objects that are still allocated.
  const parentProcessMemory = tracker.getAllocatedMemory();
  const parentProcessObjects = tracker.stillAllocatedObjects();

  let contentProcessMemory = null;
  let contentProcessObjects = null;
  if (dataOnStart.contentProcessObjects) {
    contentProcessMemory = await SpecialPowers.spawn(
      gBrowser.selectedBrowser,
      [],
      async () => {
        const { DevToolsLoader } = ChromeUtils.import(
          "resource://devtools/shared/Loader.jsm"
        );
        const { tracker } = DevToolsLoader;

        await tracker.doGC();

        return tracker.getAllocatedMemory();
      }
    );
    contentProcessObjects = await SpecialPowers.spawn(
      gBrowser.selectedBrowser,
      [],
      () => {
        const { DevToolsLoader } = ChromeUtils.import(
          "resource://devtools/shared/Loader.jsm"
        );
        const { tracker } = DevToolsLoader;
        return tracker.stillAllocatedObjects();
      }
    );
  }

  // Craft the JSON object required to save data in talos database
  const diffNoStackInParent =
    parentProcessObjects.objectsWithoutStack -
    dataOnStart.parentProcessObjects.objectsWithoutStack;
  const diffWithStackInParent =
    parentProcessObjects.objectsWithStack -
    dataOnStart.parentProcessObjects.objectsWithStack;
  info(
    `The ${recordName} test leaked ${diffWithStackInParent} objects (${diffNoStackInParent} with missing allocation site) in the parent process`
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
            value: diffNoStackInParent,
          },
          {
            name: "objects-with-stacks",
            value: diffWithStackInParent,
          },
          {
            name: "memory",
            value: parentProcessMemory - dataOnStart.parentProcessMemory,
          },
        ],
      },
    ],
  };
  if (contentProcessObjects) {
    const diffNoStackInContent =
      contentProcessObjects.objectsWithoutStack -
      dataOnStart.contentProcessObjects.objectsWithoutStack;
    const diffWithStackInContent =
      contentProcessObjects.objectsWithStack -
      dataOnStart.contentProcessObjects.objectsWithStack;
    info(
      `The ${recordName} test leaked ${diffWithStackInContent} objects (${diffNoStackInContent} with missing allocation site) in the content process`
    );
    PERFHERDER_DATA.suites.push({
      name: recordName + ":content-process",
      subtests: [
        {
          name: "objects-with-no-stacks",
          value: diffNoStackInContent,
        },
        {
          name: "objects-with-stacks",
          value: diffWithStackInContent,
        },
        {
          name: "memory",
          value: contentProcessMemory - dataOnStart.contentProcessMemory,
        },
      ],
    });
  }
  // Log it to stdout so that perfherder can collect this data.
  // This only works if we called `SimpleTest.requestCompleteLog()`!
  info("PERFHERDER_DATA: " + JSON.stringify(PERFHERDER_DATA));
}
