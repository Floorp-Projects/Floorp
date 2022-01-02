/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URI =
  "http://example.com/browser/dom/tests/browser/test_largeAllocation.html";
const TEST_URI_2 =
  "http://example.com/browser/dom/tests/browser/test_largeAllocation2.html";

function expectProcessCreated() {
  let os = Services.obs;
  let kill; // A kill function which will disable the promise.
  let promise = new Promise((resolve, reject) => {
    let topic = "ipc:content-created";
    function observer() {
      os.removeObserver(observer, topic);
      ok(true, "Expect process created");
      resolve();
    }
    os.addObserver(observer, topic);
    kill = () => {
      os.removeObserver(observer, topic);
      ok(true, "Expect process created killed");
      reject();
    };
  });
  promise.kill = kill;
  return promise;
}

function getPID(aBrowser) {
  return SpecialPowers.spawn(aBrowser, [], () => {
    return Services.appinfo.processID;
  });
}

function getInLAProc(aBrowser) {
  return SpecialPowers.spawn(aBrowser, [], () => {
    return Services.appinfo.remoteType == "webLargeAllocation";
  });
}

async function largeAllocSuccessTests() {
  // I'm terrible and put this set of tests into a single file, so I need a longer timeout
  requestLongerTimeout(4);

  // Check if we are on win32
  let isWin32 =
    /Windows/.test(navigator.userAgent) && !/x64/.test(navigator.userAgent);

  await SpecialPowers.pushPrefEnv({
    set: [
      // Enable the header if it is disabled
      ["dom.largeAllocationHeader.enabled", true],
      // Force-enable process creation with large-allocation, such that non
      // win32 builds can test the behavior.
      ["dom.largeAllocation.forceEnable", !isWin32],
      // Increase processCount.webLargeAllocation to avoid any races where
      // processes aren't being cleaned up quickly enough.
      ["dom.ipc.processCount.webLargeAllocation", 20],
    ],
  });

  // A toplevel tab should be able to navigate cross process!
  await BrowserTestUtils.withNewTab("about:blank", async function(aBrowser) {
    info("Starting test 0");
    let pid1 = await getPID(aBrowser);
    is(false, await getInLAProc(aBrowser));

    let ready = Promise.all([
      expectProcessCreated(),
      BrowserTestUtils.browserLoaded(aBrowser),
    ]);

    await SpecialPowers.spawn(aBrowser, [TEST_URI], TEST_URI => {
      content.document.location = TEST_URI;
    });

    // Wait for the new process to be created
    await ready;

    let pid2 = await getPID(aBrowser);

    isnot(
      pid1,
      pid2,
      "The pids should be different between the initial load and the new load"
    );
    is(true, await getInLAProc(aBrowser));
  });

  // When a Large-Allocation document is loaded in an iframe, the header should
  // be ignored, and the tab should stay in the current process.
  await BrowserTestUtils.withNewTab("about:blank", async function(aBrowser) {
    info("Starting test 1");
    let pid1 = await getPID(aBrowser);
    is(false, await getInLAProc(aBrowser));

    await SpecialPowers.spawn(aBrowser, [TEST_URI], TEST_URI => {
      // eslint-disable-next-line no-unsanitized/property
      content.document.body.innerHTML = `<iframe src='${TEST_URI}'></iframe>`;

      return new Promise(resolve => {
        content.document.body.querySelector("iframe").onload = () => {
          ok(true, "Iframe finished loading");
          resolve();
        };
      });
    });

    let pid2 = await getPID(aBrowser);

    is(pid1, pid2, "The PID should not have changed");
    is(false, await getInLAProc(aBrowser));
  });

  // If you have an opener cross process navigation shouldn't work
  await BrowserTestUtils.withNewTab("http://example.com", async function(
    aBrowser
  ) {
    info("Starting test 2");
    let pid1 = await getPID(aBrowser);
    is(false, await getInLAProc(aBrowser));

    await SpecialPowers.spawn(aBrowser, [TEST_URI], TEST_URI => {
      info(TEST_URI);
      content.document.body.innerHTML = "<button>CLICK ME</button>";

      return new Promise(resolve => {
        let w = content.window.open(TEST_URI, "_blank");
        w.onload = () => {
          ok(true, "Window finished loading");
          w.close();
          resolve();
        };
      });
    });

    let pid2 = await getPID(aBrowser);

    is(pid1, pid2, "The PID should not have changed");
    is(false, await getInLAProc(aBrowser));
  });

  // Load Large-Allocation twice with example.com load in between
  await BrowserTestUtils.withNewTab("about:blank", async function(aBrowser) {
    info("Starting test 3");
    let pid1 = await getPID(aBrowser);
    is(false, await getInLAProc(aBrowser));

    let ready = Promise.all([
      expectProcessCreated(),
      BrowserTestUtils.browserLoaded(aBrowser),
    ]);

    await SpecialPowers.spawn(aBrowser, [TEST_URI], TEST_URI => {
      content.document.location = TEST_URI;
    });

    await ready;

    let pid2 = await getPID(aBrowser);

    isnot(pid1, pid2);
    is(true, await getInLAProc(aBrowser));

    await SpecialPowers.spawn(aBrowser, [], () => {
      content.document.location = "http://example.com";
    });

    await BrowserTestUtils.browserLoaded(aBrowser);

    // We should have been kicked out of the large-allocation process by the
    // load, meaning we're back in a non-fresh process
    is(false, await getInLAProc(aBrowser));

    ready = Promise.all([
      expectProcessCreated(),
      BrowserTestUtils.browserLoaded(aBrowser),
    ]);

    await SpecialPowers.spawn(aBrowser, [TEST_URI], TEST_URI => {
      content.document.location = TEST_URI;
    });

    await ready;

    let pid4 = await getPID(aBrowser);

    isnot(pid1, pid4);
    isnot(pid2, pid4);
    is(true, await getInLAProc(aBrowser));
  });

  // Load Large-Allocation then example.com load, then back button press should load from bfcache.
  await BrowserTestUtils.withNewTab("about:blank", async function(aBrowser) {
    info("Starting test 4");
    let pid1 = await getPID(aBrowser);
    is(false, await getInLAProc(aBrowser));

    let ready = Promise.all([
      expectProcessCreated(),
      BrowserTestUtils.browserLoaded(aBrowser),
    ]);

    await SpecialPowers.spawn(aBrowser, [TEST_URI], TEST_URI => {
      content.document.location = TEST_URI;
    });

    await ready;

    let pid2 = await getPID(aBrowser);

    isnot(pid1, pid2, "PIDs 1 and 2 should not match");
    is(true, await getInLAProc(aBrowser));

    // Switch to about:blank, so we can navigate back
    await SpecialPowers.spawn(aBrowser, [], () => {
      content.document.location = "http://example.com";
    });

    await BrowserTestUtils.browserLoaded(aBrowser);

    let pid3 = await getPID(aBrowser);

    // We should have been kicked out of the large-allocation process by the
    // load, meaning we're back in a non-large-allocation process.
    is(false, await getInLAProc(aBrowser));

    ready = Promise.all([
      expectProcessCreated(),
      BrowserTestUtils.browserLoaded(aBrowser),
    ]);

    // Navigate back to the previous page. As the large alloation process was
    // left, it won't be in bfcache and will have to be loaded fresh.
    await SpecialPowers.spawn(aBrowser, [TEST_URI], TEST_URI => {
      content.window.history.back();
    });

    await ready;

    let pid4 = await getPID(aBrowser);

    isnot(pid1, pid4, "PID 4 shouldn't match PID 1");
    isnot(pid2, pid4, "PID 4 shouldn't match PID 2");
    isnot(pid3, pid4, "PID 4 shouldn't match PID 3");
    is(true, await getInLAProc(aBrowser));
  });

  // Two consecutive large-allocation loads should create two processes.
  await BrowserTestUtils.withNewTab("about:blank", async function(aBrowser) {
    info("Starting test 5");
    let pid1 = await getPID(aBrowser);
    is(false, await getInLAProc(aBrowser));

    let ready = Promise.all([
      expectProcessCreated(),
      BrowserTestUtils.browserLoaded(aBrowser),
    ]);

    await SpecialPowers.spawn(aBrowser, [TEST_URI], TEST_URI => {
      content.document.location = TEST_URI;
    });

    await ready;

    let pid2 = await getPID(aBrowser);

    isnot(pid1, pid2, "PIDs 1 and 2 should not match");
    is(true, await getInLAProc(aBrowser));

    ready = Promise.all([
      expectProcessCreated(),
      BrowserTestUtils.browserLoaded(aBrowser),
    ]);

    await SpecialPowers.spawn(aBrowser, [TEST_URI_2], TEST_URI_2 => {
      content.document.location = TEST_URI_2;
    });

    await ready;

    let pid3 = await getPID(aBrowser);

    isnot(pid1, pid3, "PIDs 1 and 3 should not match");
    isnot(pid2, pid3, "PIDs 2 and 3 should not match");
    is(true, await getInLAProc(aBrowser));
  });

  // Opening a window from the large-allocation window should prevent the process switch.
  await BrowserTestUtils.withNewTab("about:blank", async function(aBrowser) {
    info("Starting test 6");
    let pid1 = await getPID(aBrowser);
    is(false, await getInLAProc(aBrowser));

    let ready = Promise.all([
      expectProcessCreated(),
      BrowserTestUtils.browserLoaded(aBrowser),
    ]);

    await SpecialPowers.spawn(aBrowser, [TEST_URI], TEST_URI => {
      content.document.location = TEST_URI;
    });

    await ready;

    let pid2 = await getPID(aBrowser);

    isnot(pid1, pid2, "PIDs 1 and 2 should not match");
    is(true, await getInLAProc(aBrowser));

    let promiseTabOpened = BrowserTestUtils.waitForNewTab(
      gBrowser,
      "about:blank"
    );
    await SpecialPowers.spawn(aBrowser, [], () => {
      content.window.open("about:blank");
      content.document.location = "http://example.com";
    });

    await BrowserTestUtils.browserLoaded(aBrowser);

    let pid3 = await getPID(aBrowser);

    is(pid3, pid2, "PIDs 2 and 3 should match");
    is(true, await getInLAProc(aBrowser));

    BrowserTestUtils.removeTab(await promiseTabOpened);
  });

  // Opening a window from the large-allocation window should prevent the
  // process switch with reload.
  await BrowserTestUtils.withNewTab("about:blank", async function(aBrowser) {
    info("Starting test 6a");
    let pid1 = await getPID(aBrowser);
    is(false, await getInLAProc(aBrowser));

    let ready = Promise.all([
      expectProcessCreated(),
      BrowserTestUtils.browserLoaded(aBrowser),
    ]);

    await SpecialPowers.spawn(aBrowser, [TEST_URI], TEST_URI => {
      content.document.location = TEST_URI;
    });

    await ready;

    let pid2 = await getPID(aBrowser);

    isnot(pid1, pid2, "PIDs 1 and 2 should not match");
    is(true, await getInLAProc(aBrowser));

    let firstTab = gBrowser.selectedTab;
    let promiseTabOpened = BrowserTestUtils.waitForNewTab(
      gBrowser,
      "about:blank"
    );
    await SpecialPowers.spawn(aBrowser, [], () => {
      content.window.open("about:blank");
    });
    let newTab = await promiseTabOpened;

    if (firstTab != gBrowser.selectedTab) {
      firstTab = await BrowserTestUtils.switchTab(gBrowser, firstTab);
      aBrowser = firstTab.linkedBrowser;
    }
    let promiseLoad = BrowserTestUtils.browserLoaded(aBrowser);
    document.getElementById("reload-button").doCommand();
    await promiseLoad;

    let pid3 = await getPID(aBrowser);

    is(pid3, pid2, "PIDs 2 and 3 should match");
    is(true, await getInLAProc(aBrowser));

    BrowserTestUtils.removeTab(newTab);
  });

  // Try dragging the tab into a new window when not at the maximum number of
  // Large-Allocation processes.
  await BrowserTestUtils.withNewTab("about:blank", async function(aBrowser) {
    info("Starting test 7");

    let pid1 = await getPID(aBrowser);
    is(false, await getInLAProc(aBrowser));

    let ready = Promise.all([
      expectProcessCreated(),
      BrowserTestUtils.browserLoaded(aBrowser),
    ]);
    await SpecialPowers.spawn(aBrowser, [TEST_URI], TEST_URI => {
      content.document.location = TEST_URI;
    });

    await ready;

    let pid2 = await getPID(aBrowser);

    isnot(pid1, pid2, "PIDs 1 and 2 should not match");
    is(true, await getInLAProc(aBrowser));

    let newWindow = await BrowserTestUtils.openNewBrowserWindow();

    newWindow.gBrowser.adoptTab(
      gBrowser.getTabForBrowser(aBrowser),
      0,
      null,
      Services.scriptSecurityManager.getSystemPrincipal()
    );
    let newTab = newWindow.gBrowser.tabs[0];

    is(newTab.linkedBrowser.currentURI.spec, TEST_URI);
    is(newTab.linkedBrowser.remoteType, "webLargeAllocation");
    let pid3 = await getPID(newTab.linkedBrowser);

    is(pid2, pid3, "PIDs 2 and 3 should match");
    is(true, await getInLAProc(newTab.linkedBrowser));

    await BrowserTestUtils.closeWindow(newWindow);
  });

  // Try opening a new Large-Allocation document when at the max number of large
  // allocation processes.
  await BrowserTestUtils.withNewTab("about:blank", async function(aBrowser) {
    info("Starting test 8");
    await SpecialPowers.pushPrefEnv({
      set: [["dom.ipc.processCount.webLargeAllocation", 1]],
    });

    // Loading the first Large-Allocation tab should succeed as normal
    let pid1 = await getPID(aBrowser);
    is(false, await getInLAProc(aBrowser));

    let ready = Promise.all([
      expectProcessCreated(),
      BrowserTestUtils.browserLoaded(aBrowser),
    ]);

    await SpecialPowers.spawn(aBrowser, [TEST_URI], TEST_URI => {
      content.document.location = TEST_URI;
    });

    await ready;

    let pid2 = await getPID(aBrowser);

    isnot(pid1, pid2, "PIDs 1 and 2 should not match");
    is(true, await getInLAProc(aBrowser));

    await BrowserTestUtils.withNewTab("about:blank", async function(aBrowser) {
      // The second one should load in a non-LA proc because the
      // webLargeAllocation processes have been exhausted.
      is(false, await getInLAProc(aBrowser));

      let ready = Promise.all([BrowserTestUtils.browserLoaded(aBrowser)]);
      await SpecialPowers.spawn(aBrowser, [TEST_URI], TEST_URI => {
        content.document.location = TEST_URI;
      });
      await ready;

      is(false, await getInLAProc(aBrowser));
    });
  });

  // XXX: Important - reset the process count, as it was set to 1 by the
  // previous test.
  await SpecialPowers.pushPrefEnv({
    set: [["dom.ipc.processCount.webLargeAllocation", 20]],
  });

  // view-source tabs should not be considered to be in a large-allocation process.
  await BrowserTestUtils.withNewTab("about:blank", async function(aBrowser) {
    info("Starting test 9");
    let pid1 = await getPID(aBrowser);
    is(false, await getInLAProc(aBrowser));

    await SpecialPowers.spawn(aBrowser, [], () => {
      content.document.location = "view-source:http://example.com";
    });

    await BrowserTestUtils.browserLoaded(aBrowser);

    let pid2 = await getPID(aBrowser);

    is(pid1, pid2, "The PID should not have changed");
    is(false, await getInLAProc(aBrowser));
  });

  // Try dragging tab into new window when at the max number of large allocation
  // processes.
  await BrowserTestUtils.withNewTab("about:blank", async function(aBrowser) {
    info("Starting test 10");
    await SpecialPowers.pushPrefEnv({
      set: [["dom.ipc.processCount.webLargeAllocation", 1]],
    });

    let pid1 = await getPID(aBrowser);
    is(false, await getInLAProc(aBrowser));

    let ready = Promise.all([
      expectProcessCreated(),
      BrowserTestUtils.browserLoaded(aBrowser),
    ]);
    await SpecialPowers.spawn(aBrowser, [TEST_URI], TEST_URI => {
      content.document.location = TEST_URI;
    });

    await ready;

    let pid2 = await getPID(aBrowser);

    isnot(pid1, pid2, "PIDs 1 and 2 should not match");
    is(true, await getInLAProc(aBrowser));

    let newWindow = await BrowserTestUtils.openNewBrowserWindow();

    newWindow.gBrowser.adoptTab(gBrowser.getTabForBrowser(aBrowser), 0);
    let newTab = newWindow.gBrowser.tabs[0];

    is(newTab.linkedBrowser.currentURI.spec, TEST_URI);
    is(newTab.linkedBrowser.remoteType, "webLargeAllocation");
    let pid3 = await getPID(newTab.linkedBrowser);

    is(pid2, pid3, "PIDs 2 and 3 should match");
    is(true, await getInLAProc(newTab.linkedBrowser));

    await BrowserTestUtils.closeWindow(newWindow);
  });

  // XXX: Important - reset the process count, as it was set to 1 by the
  // previous test.
  await SpecialPowers.pushPrefEnv({
    set: [["dom.ipc.processCount.webLargeAllocation", 20]],
  });

  await BrowserTestUtils.withNewTab("about:blank", async function(aBrowser) {
    info("Starting test 11");

    let pid1 = await getPID(aBrowser);
    is(false, await getInLAProc(aBrowser));

    let ready = Promise.all([
      expectProcessCreated(),
      BrowserTestUtils.browserLoaded(aBrowser),
    ]);
    await SpecialPowers.spawn(aBrowser, [TEST_URI], TEST_URI => {
      content.document.location = TEST_URI;
    });

    await ready;

    let pid2 = await getPID(aBrowser);

    isnot(pid1, pid2, "PIDs 1 and 2 should not match");
    is(true, await getInLAProc(aBrowser));

    await Promise.all([
      SpecialPowers.spawn(aBrowser, [], () => {
        content.document.querySelector("#submit").click();
      }),
      BrowserTestUtils.browserLoaded(aBrowser),
    ]);

    let innerText = await SpecialPowers.spawn(aBrowser, [], () => {
      return content.document.body.innerText;
    });
    isnot(innerText, "FAIL", "We should not have sent a get request!");
    is(
      innerText,
      "textarea=default+value&button=submit",
      "The post data should be received by the callee"
    );
  });

  // XXX: Make sure to reset dom.ipc.processCount.webLargeAllocation if adding a
  // test after the above test.
}

async function largeAllocFailTests() {
  await BrowserTestUtils.withNewTab("http://example.com", async function(
    aBrowser
  ) {
    info("Starting test 1");
    let pid1 = await getPID(aBrowser);
    is(false, await getInLAProc(aBrowser));

    await SpecialPowers.spawn(aBrowser, [TEST_URI], TEST_URI => {
      content.document.location = TEST_URI;
    });

    await BrowserTestUtils.browserLoaded(aBrowser);

    let pid2 = await getPID(aBrowser);

    is(pid1, pid2, "The PID should not have changed");
    is(false, await getInLAProc(aBrowser));
  });
}
