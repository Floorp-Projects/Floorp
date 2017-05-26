"use strict";

const CONTENT_CREATED = "ipc:content-created";

// Make sure that BTU.withNewTab({ ..., forceNewProcess: true }) loads
// new tabs in their own process.
async function spawnNewAndTest(recur, pids) {
  let processCreated = TestUtils.topicObserved(CONTENT_CREATED);
  await BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank", forceNewProcess: true },
                                    function* (browser) {
      // Make sure our new browser is in its own process. The processCreated
      // promise should have already resolved by this point.
      yield processCreated;
      let newPid = browser.frameLoader.tabParent.osPid;
      ok(!pids.has(newPid), "new tab is in its own process");
      pids.add(newPid);

      if (recur) {
        yield spawnNewAndTest(recur - 1, pids);
      } else {
        let observer = () => {
          ok(false, "shouldn't have created a new process");
        };
        Services.obs.addObserver(observer, CONTENT_CREATED);

        yield BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank" }, function* (browser) {
          // If this new tab caused us to create a new process, the ok(false)
          // should have already happened. Therefore, if we get here, we've
          // passed. Simply remove the observer.
          Services.obs.removeObserver(observer, CONTENT_CREATED);
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
