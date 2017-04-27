"use strict";

// Make sure that BTU.withNewTab({ ..., forceNewProcess: true }) loads
// new tabs in their own process.
async function spawnNewAndTest(recur, pids) {
  await BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank", forceNewProcess: true },
                                    function* (browser) {
      // Make sure our new browser is in its own process.
      let newPid = browser.frameLoader.tabParent.osPid;
      ok(!pids.has(newPid), "new tab is in its own process");
      pids.add(newPid);

      if (recur) {
        yield spawnNewAndTest(recur - 1, pids);
      } else {
        yield BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank" }, function* (browser) {
          // This browser should share a PID with one of the existing tabs.
          // This is a todo because the process we end up using is actually
          // an extra process that gets started early in startup and not one
          // of the ones we use for the tabs.
          todo(pids.has(browser.frameLoader.tabParent.osPid),
               "we should be reusing processes if not asked to force the " +
               "tab into its own process");
        });
      }
  });
}

add_task(async function test() {
  let curPid = gBrowser.selectedBrowser.frameLoader.tabParent.osPid;
  let maxCount = Services.prefs.getIntPref("dom.ipc.processCount");

  // Use at least one more tab than max processes or at least 5 to make this
  // test interesting.
  await spawnNewAndTest(Math.max(maxCount + 1, 5), new Set([ curPid ]));
});
