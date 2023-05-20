"use strict";

const CONTENT_CREATED = "ipc:content-created";

// Make sure that BTU.withNewTab({ ..., forceNewProcess: true }) loads
// new tabs in their own process.
async function spawnNewAndTest(recur, pids) {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank", forceNewProcess: true },
    async function (browser) {
      // Make sure our new browser is in its own process.
      let newPid = browser.frameLoader.remoteTab.osPid;
      ok(!pids.has(newPid), "new tab is in its own process: " + recur);
      pids.add(newPid);

      if (recur) {
        await spawnNewAndTest(recur - 1, pids);
      } else {
        await BrowserTestUtils.withNewTab(
          { gBrowser, url: "about:blank" },
          function (lastBrowser) {
            let lastPid = lastBrowser.frameLoader.remoteTab.osPid;
            ok(pids.has(lastPid), "final tab cannot be in its own process");
          }
        );
      }
    }
  );
}

add_task(async function test() {
  let curPid = gBrowser.selectedBrowser.frameLoader.remoteTab.osPid;
  let maxCount = Services.prefs.getIntPref("dom.ipc.processCount");

  // Use at least one more tab than max processes or at least 5 to make this
  // test interesting.
  await spawnNewAndTest(Math.max(maxCount + 1, 5), new Set([curPid]));
});
