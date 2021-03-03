const TEST_URL =
  "data:text/html;charset=UTF-8,<div>Target allocations test</div>";

// Load the tracker very first in order to ensure tracking all objects created by DevTools.
// Loader.jsm shouldn't be loaded, not any other DevTools module until an explicit user action.
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

const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");

const {
  TabDescriptorFactory,
} = require("devtools/client/framework/tab-descriptor-factory");

async function doGC() {
  // In order to get stable results, we really have to do 3 GC attempts
  // *and* do wait for 1s between each GC.
  const numCycles = 3;
  for (let i = 0; i < numCycles; i++) {
    Cu.forceGC();
    Cu.forceCC();
    await new Promise(resolve => Cu.schedulePreciseShrinkingGC(resolve));

    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(resolve => setTimeout(resolve, 1000));
  }
}

async function addTab(url) {
  const tab = BrowserTestUtils.addTab(gBrowser, url);
  gBrowser.selectedTab = tab;
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  return tab;
}

async function testScript(tab) {
  const descriptor = await TabDescriptorFactory.createDescriptorForTab(tab);
  const target = await descriptor.getTarget();
  await target.attach();

  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 1000));

  await target.destroy();
}

add_task(async function() {
  const tab = await addTab(TEST_URL);

  // Run the test scenario first before recording in order to load all the
  // modules. Otherwise they get reported as "still allocated" objects,
  // whereas we do expect them to be kept in memory as they are loaded via
  // the main DevTools loader, which keeps the module loaded until the
  // shutdown of Firefox
  await testScript(tab);

  // Do a first pass of GC, to ensure all to-be-freed objects from the first run
  // are really freed.
  await doGC();

  // Then, record what was already allocated, which should not be declared
  // as potential leaks. For ex, there is all the modules already loaded
  // in the main DevTools loader.
  const totalBefore = tracker.stillAllocatedObjects();

  // Now, run the test script. This time, we record this run.
  await testScript(tab);

  // After that, re-do some GCs in order to free all what is to-be-freed.
  await doGC();

  // Ensure that Memory API didn't ran out of buffers
  ok(!tracker.overflowed, "Allocation were all recorded");

  // And finally, retrieve the number of objects that are still allocated.
  const totalAfter = tracker.stillAllocatedObjects();

  gBrowser.removeTab(tab);

  const PERFHERDER_DATA = {
    framework: {
      name: "devtools",
    },
    suites: [
      {
        name: "total-after-gc",
        value: totalAfter - totalBefore,
        subtests: [
          {
            name: "before",
            value: totalBefore,
          },
          {
            name: "after",
            value: totalAfter - totalBefore,
          },
        ],
      },
    ],
  };
  info("PERFHERDER_DATA: " + JSON.stringify(PERFHERDER_DATA));
  ok(true, "Test succeeded");
});
