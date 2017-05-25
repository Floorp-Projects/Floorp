/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URI = "http://example.com/browser/dom/tests/browser/test_largeAllocation.html";
const TEST_URI_2 = "http://example.com/browser/dom/tests/browser/test_largeAllocation2.html";

function expectProcessCreated() {
  let os = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
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

function expectNoProcess() {
  let os = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
  let topic = "ipc:content-created";
  function observer() {
    ok(false, "A process was created!");
    os.removeObserver(observer, topic);
  }
  os.addObserver(observer, topic);

  return () => os.removeObserver(observer, topic);
}

function getPID(aBrowser) {
  return ContentTask.spawn(aBrowser, null, () => {
    const appinfo = Components.classes["@mozilla.org/xre/app-info;1"]
            .getService(Components.interfaces.nsIXULRuntime);
    return appinfo.processID;
  });
}

function getInLAProc(aBrowser) {
  return ContentTask.spawn(aBrowser, null, () => {
    return Services.appinfo.remoteType == "webLargeAllocation";
  });
}

function* largeAllocSuccessTests() {
  // I'm terrible and put this set of tests into a single file, so I need a longer timeout
  requestLongerTimeout(2);

  // Check if we are on win32
  let isWin32 = /Windows/.test(navigator.userAgent) && !/x64/.test(navigator.userAgent);

  yield SpecialPowers.pushPrefEnv({
    set: [
      // Enable the header if it is disabled
      ["dom.largeAllocationHeader.enabled", true],
      // Force-enable process creation with large-allocation, such that non
      // win32 builds can test the behavior.
      ["dom.largeAllocation.forceEnable", !isWin32],
      // Increase processCount.webLargeAllocation to avoid any races where
      // processes aren't being cleaned up quickly enough.
      ["dom.ipc.processCount.webLargeAllocation", 20]
    ]
  });

  // A toplevel tab should be able to navigate cross process!
  yield BrowserTestUtils.withNewTab("about:blank", function*(aBrowser) {
    info("Starting test 0");
    let pid1 = yield getPID(aBrowser);
    is(false, yield getInLAProc(aBrowser));

    let epc = expectProcessCreated();
    yield ContentTask.spawn(aBrowser, TEST_URI, TEST_URI => {
      content.document.location = TEST_URI;
    });

    // Wait for the new process to be created
    yield epc;

    let pid2 = yield getPID(aBrowser);

    isnot(pid1, pid2, "The pids should be different between the initial load and the new load");
    is(true, yield getInLAProc(aBrowser));
  });

  // When a Large-Allocation document is loaded in an iframe, the header should
  // be ignored, and the tab should stay in the current process.
  yield BrowserTestUtils.withNewTab("about:blank", function*(aBrowser) {
    info("Starting test 1");
    let pid1 = yield getPID(aBrowser);
    is(false, yield getInLAProc(aBrowser));

    // Fail the test if we create a process
    let stopExpectNoProcess = expectNoProcess();

    yield ContentTask.spawn(aBrowser, TEST_URI, TEST_URI => {
      content.document.body.innerHTML = `<iframe src='${TEST_URI}'></iframe>`;

      return new Promise(resolve => {
        content.document.body.querySelector('iframe').onload = () => {
          ok(true, "Iframe finished loading");
          resolve();
        };
      });
    });

    let pid2 = yield getPID(aBrowser);

    is(pid1, pid2, "The PID should not have changed");
    is(false, yield getInLAProc(aBrowser));

    stopExpectNoProcess();
  });

  // If you have an opener cross process navigation shouldn't work
  yield BrowserTestUtils.withNewTab("http://example.com", function*(aBrowser) {
    info("Starting test 2");
    let pid1 = yield getPID(aBrowser);
    is(false, yield getInLAProc(aBrowser));

    // Fail the test if we create a process
    let stopExpectNoProcess = expectNoProcess();

    let loaded = ContentTask.spawn(aBrowser, TEST_URI, TEST_URI => {
      content.document.body.innerHTML = '<button>CLICK ME</button>';

      return new Promise(resolve => {
        content.document.querySelector('button').onclick = e => {
          let w = content.window.open(TEST_URI, '_blank');
          w.onload = () => {
            ok(true, "Window finished loading");
            w.close();
            resolve();
          };
        };
      });
    });

    yield BrowserTestUtils.synthesizeMouseAtCenter("button", {}, aBrowser);

    yield loaded;

    let pid2 = yield getPID(aBrowser);

    is(pid1, pid2, "The PID should not have changed");
    is(false, yield getInLAProc(aBrowser));

    stopExpectNoProcess();
  });

  // Load Large-Allocation twice with about:blank load in between
  yield BrowserTestUtils.withNewTab("about:blank", function*(aBrowser) {
    info("Starting test 3");
    let pid1 = yield getPID(aBrowser);
    is(false, yield getInLAProc(aBrowser));

    let epc = expectProcessCreated();

    yield ContentTask.spawn(aBrowser, TEST_URI, TEST_URI => {
      content.document.location = TEST_URI;
    });

    yield epc;

    let pid2 = yield getPID(aBrowser);

    isnot(pid1, pid2);
    is(true, yield getInLAProc(aBrowser));

    yield BrowserTestUtils.browserLoaded(aBrowser);

    yield ContentTask.spawn(aBrowser, null, () => content.document.location = "about:blank");

    yield BrowserTestUtils.browserLoaded(aBrowser);

    let pid3 = yield getPID(aBrowser);

    // We should have been kicked out of the large-allocation process by the
    // load, meaning we're back in a non-fresh process
    is(false, yield getInLAProc(aBrowser));

    epc = expectProcessCreated();

    yield ContentTask.spawn(aBrowser, TEST_URI, TEST_URI => {
      content.document.location = TEST_URI;
    });

    yield epc;

    let pid4 = yield getPID(aBrowser);

    isnot(pid1, pid4);
    isnot(pid2, pid4);
    is(true, yield getInLAProc(aBrowser));
  });

  // Load Large-Allocation then about:blank load, then back button press should load from bfcache.
  yield BrowserTestUtils.withNewTab("about:blank", function*(aBrowser) {
    info("Starting test 4");
    let pid1 = yield getPID(aBrowser);
    is(false, yield getInLAProc(aBrowser));

    let epc = expectProcessCreated();

    yield ContentTask.spawn(aBrowser, TEST_URI, TEST_URI => {
      content.document.location = TEST_URI;
    });

    yield epc;

    let pid2 = yield getPID(aBrowser);

    isnot(pid1, pid2, "PIDs 1 and 2 should not match");
    is(true, yield getInLAProc(aBrowser));

    yield BrowserTestUtils.browserLoaded(aBrowser);

    // Switch to about:blank, so we can navigate back
    yield ContentTask.spawn(aBrowser, null, () => {
      content.document.location = "about:blank";
    });

    yield BrowserTestUtils.browserLoaded(aBrowser);

    let pid3 = yield getPID(aBrowser);

    // We should have been kicked out of the large-allocation process by the
    // load, meaning we're back in a non-large-allocation process.
    is(false, yield getInLAProc(aBrowser));

    epc = expectProcessCreated();

    // Navigate back to the previous page. As the large alloation process was
    // left, it won't be in bfcache and will have to be loaded fresh.
    yield ContentTask.spawn(aBrowser, TEST_URI, TEST_URI => {
      content.window.history.back();
    });

    yield epc;

    let pid4 = yield getPID(aBrowser);

    isnot(pid1, pid4, "PID 4 shouldn't match PID 1");
    isnot(pid2, pid4, "PID 4 shouldn't match PID 2");
    isnot(pid3, pid4, "PID 4 shouldn't match PID 3");
    is(true, yield getInLAProc(aBrowser));
  });

  // Two consecutive large-allocation loads should create two processes.
  yield BrowserTestUtils.withNewTab("about:blank", function*(aBrowser) {
    info("Starting test 5");
    let pid1 = yield getPID(aBrowser);
    is(false, yield getInLAProc(aBrowser));

    let ready = Promise.all([expectProcessCreated(),
                             BrowserTestUtils.browserLoaded(aBrowser)]);

    yield ContentTask.spawn(aBrowser, TEST_URI, TEST_URI => {
      content.document.location = TEST_URI;
    });

    yield ready;

    let pid2 = yield getPID(aBrowser);

    isnot(pid1, pid2, "PIDs 1 and 2 should not match");
    is(true, yield getInLAProc(aBrowser));

    let epc = expectProcessCreated();

    yield ContentTask.spawn(aBrowser, TEST_URI_2, TEST_URI_2 => {
      content.document.location = TEST_URI_2;
    });

    yield epc;

    // We just saw the creation of a new process. This is either the process we
    // are interested in, or, in a multi-e10s situation, the normal content
    // process which was created for the normal content to be loaded into as the
    // browsing context was booted out of the fresh process. If we discover that
    // this was not a fresh process, we'll need to wait for another process.
    // Start listening now.
    epc = expectProcessCreated();
    if (!(yield getInLAProc(aBrowser))) {
      yield epc;
    } else {
      epc.catch(() => {});
      epc.kill();
    }

    let pid3 = yield getPID(aBrowser);

    isnot(pid1, pid3, "PIDs 1 and 3 should not match");
    isnot(pid2, pid3, "PIDs 2 and 3 should not match");
    is(true, yield getInLAProc(aBrowser));
  });

  // Opening a window from the large-allocation window should prevent the process switch.
  yield BrowserTestUtils.withNewTab("about:blank", function*(aBrowser) {
    info("Starting test 6");
    let pid1 = yield getPID(aBrowser);
    is(false, yield getInLAProc(aBrowser));

    let ready = Promise.all([expectProcessCreated(),
                             BrowserTestUtils.browserLoaded(aBrowser)]);

    yield ContentTask.spawn(aBrowser, TEST_URI, TEST_URI => {
      content.document.location = TEST_URI;
    });

    yield ready;

    let pid2 = yield getPID(aBrowser);

    isnot(pid1, pid2, "PIDs 1 and 2 should not match");
    is(true, yield getInLAProc(aBrowser));

    let stopExpectNoProcess = expectNoProcess();

    yield ContentTask.spawn(aBrowser, null, () => {
      this.__newWindow = content.window.open("about:blank");
      content.document.location = "about:blank";
    });

    yield BrowserTestUtils.browserLoaded(aBrowser);

    let pid3 = yield getPID(aBrowser);

    is(pid3, pid2, "PIDs 2 and 3 should match");
    is(true, yield getInLAProc(aBrowser));

    stopExpectNoProcess();

    yield ContentTask.spawn(aBrowser, null, () => {
      ok(this.__newWindow, "The window should have been stored");
      this.__newWindow.close();
    });
  });

  // Opening a window from the large-allocation window should prevent the
  // process switch with reload.
  yield BrowserTestUtils.withNewTab("about:blank", function*(aBrowser) {
    info("Starting test 6a");
    let pid1 = yield getPID(aBrowser);
    is(false, yield getInLAProc(aBrowser));

    let ready = Promise.all([expectProcessCreated(),
                             BrowserTestUtils.browserLoaded(aBrowser)]);

    yield ContentTask.spawn(aBrowser, TEST_URI, TEST_URI => {
      content.document.location = TEST_URI;
    });

    yield ready;

    let pid2 = yield getPID(aBrowser);

    isnot(pid1, pid2, "PIDs 1 and 2 should not match");
    is(true, yield getInLAProc(aBrowser));

    let stopExpectNoProcess = expectNoProcess();

    let firstTab = gBrowser.selectedTab;
    let promiseTabOpened = BrowserTestUtils.waitForNewTab(gBrowser, "about:blank");
    yield ContentTask.spawn(aBrowser, null, () => {
      this.__newWindow = content.window.open("about:blank");
    });
    yield promiseTabOpened;

    if (firstTab != gBrowser.selectedTab) {
      firstTab = yield BrowserTestUtils.switchTab(gBrowser, firstTab);
      aBrowser = firstTab.linkedBrowser;
    }
    let promiseLoad = BrowserTestUtils.browserLoaded(aBrowser);
    document.getElementById("reload-button").doCommand();
    yield promiseLoad;

    let pid3 = yield getPID(aBrowser);

    is(pid3, pid2, "PIDs 2 and 3 should match");
    is(true, yield getInLAProc(aBrowser));

    stopExpectNoProcess();

    yield ContentTask.spawn(aBrowser, null, () => {
      ok(this.__newWindow, "The window should have been stored");
      this.__newWindow.close();
    });
  });

  // Try dragging the tab into a new window when not at the maximum number of
  // Large-Allocation processes.
  yield BrowserTestUtils.withNewTab("about:blank", function*(aBrowser) {
    info("Starting test 7");

    let pid1 = yield getPID(aBrowser);
    is(false, yield getInLAProc(aBrowser));

    let ready = Promise.all([expectProcessCreated(),
                             BrowserTestUtils.browserLoaded(aBrowser)]);
    yield ContentTask.spawn(aBrowser, TEST_URI, TEST_URI => {
      content.document.location = TEST_URI;
    });

    yield ready;

    let pid2 = yield getPID(aBrowser);

    isnot(pid1, pid2, "PIDs 1 and 2 should not match");
    is(true, yield getInLAProc(aBrowser));

    let newWindow = yield BrowserTestUtils.openNewBrowserWindow();

    newWindow.gBrowser.adoptTab(gBrowser.getTabForBrowser(aBrowser), 0);
    let newTab = newWindow.gBrowser.tabs[0];

    is(newTab.linkedBrowser.currentURI.spec, TEST_URI);
    is(newTab.linkedBrowser.remoteType, "webLargeAllocation");
    let pid3 = yield getPID(newTab.linkedBrowser);

    is(pid2, pid3, "PIDs 2 and 3 should match");
    is(true, yield getInLAProc(newTab.linkedBrowser));

    yield BrowserTestUtils.closeWindow(newWindow);
  });

  // Try opening a new Large-Allocation document when at the max number of large
  // allocation processes.
  yield BrowserTestUtils.withNewTab("about:blank", function*(aBrowser) {
    info("Starting test 8");
    yield SpecialPowers.pushPrefEnv({
      set: [
        ["dom.ipc.processCount.webLargeAllocation", 1]
      ],
    });

    // Loading the first Large-Allocation tab should succeed as normal
    let pid1 = yield getPID(aBrowser);
    is(false, yield getInLAProc(aBrowser));

    let ready = Promise.all([expectProcessCreated(),
                             BrowserTestUtils.browserLoaded(aBrowser)]);

    yield ContentTask.spawn(aBrowser, TEST_URI, TEST_URI => {
      content.document.location = TEST_URI;
    });

    yield ready;

    let pid2 = yield getPID(aBrowser);

    isnot(pid1, pid2, "PIDs 1 and 2 should not match");
    is(true, yield getInLAProc(aBrowser));

    yield BrowserTestUtils.withNewTab("about:blank", function*(aBrowser) {
      // The second one should load in a non-LA proc because the
      // webLargeAllocation processes have been exhausted.
      is(false, yield getInLAProc(aBrowser));

      let ready = Promise.all([BrowserTestUtils.browserLoaded(aBrowser)]);
      yield ContentTask.spawn(aBrowser, TEST_URI, TEST_URI => {
        content.document.location = TEST_URI;
      });
      yield ready;

      is(false, yield getInLAProc(aBrowser));
    });
  });

  // XXX: Important - reset the process count, as it was set to 1 by the
  // previous test.
  yield SpecialPowers.pushPrefEnv({
    set: [["dom.ipc.processCount.webLargeAllocation", 20]],
  });

  // view-source tabs should not be considered to be in a large-allocation process.
  yield BrowserTestUtils.withNewTab("about:blank", function*(aBrowser) {
    info("Starting test 9");
    let pid1 = yield getPID(aBrowser);
    is(false, yield getInLAProc(aBrowser));

    // Fail the test if we create a process
    let stopExpectNoProcess = expectNoProcess();

    yield ContentTask.spawn(aBrowser, null, () => {
      content.document.location = "view-source:http://example.com";
    });

    yield BrowserTestUtils.browserLoaded(aBrowser);

    let pid2 = yield getPID(aBrowser);

    is(pid1, pid2, "The PID should not have changed");
    is(false, yield getInLAProc(aBrowser));

    stopExpectNoProcess();
  });

  // Try dragging tab into new window when at the max number of large allocation
  // processes.
  yield BrowserTestUtils.withNewTab("about:blank", function*(aBrowser) {
    info("Starting test 10");
    yield SpecialPowers.pushPrefEnv({
      set: [
        ["dom.ipc.processCount.webLargeAllocation", 1]
      ],
    });

    let pid1 = yield getPID(aBrowser);
    is(false, yield getInLAProc(aBrowser));

    let ready = Promise.all([expectProcessCreated(),
                             BrowserTestUtils.browserLoaded(aBrowser)]);
    yield ContentTask.spawn(aBrowser, TEST_URI, TEST_URI => {
      content.document.location = TEST_URI;
    });

    yield ready;

    let pid2 = yield getPID(aBrowser);

    isnot(pid1, pid2, "PIDs 1 and 2 should not match");
    is(true, yield getInLAProc(aBrowser));

    let newWindow = yield BrowserTestUtils.openNewBrowserWindow();

    newWindow.gBrowser.adoptTab(gBrowser.getTabForBrowser(aBrowser), 0);
    let newTab = newWindow.gBrowser.tabs[0];

    is(newTab.linkedBrowser.currentURI.spec, TEST_URI);
    is(newTab.linkedBrowser.remoteType, "webLargeAllocation");
    let pid3 = yield getPID(newTab.linkedBrowser);

    is(pid2, pid3, "PIDs 2 and 3 should match");
    is(true, yield getInLAProc(newTab.linkedBrowser));

    yield BrowserTestUtils.closeWindow(newWindow);
  });

  // XXX: Important - reset the process count, as it was set to 1 by the
  // previous test.
  yield SpecialPowers.pushPrefEnv({
    set: [["dom.ipc.processCount.webLargeAllocation", 20]],
  });

  yield BrowserTestUtils.withNewTab("about:blank", function*(aBrowser) {
    info("Starting test 11");

    let pid1 = yield getPID(aBrowser);
    is(false, yield getInLAProc(aBrowser));

    let ready = Promise.all([expectProcessCreated(),
                             BrowserTestUtils.browserLoaded(aBrowser)]);
    yield ContentTask.spawn(aBrowser, TEST_URI, TEST_URI => {
      content.document.location = TEST_URI;
    });

    yield ready;

    let pid2 = yield getPID(aBrowser);

    isnot(pid1, pid2, "PIDs 1 and 2 should not match");
    is(true, yield getInLAProc(aBrowser));

    yield Promise.all([
      ContentTask.spawn(aBrowser, null, () => {
        content.document.querySelector("#submit").click();
      }),
      BrowserTestUtils.browserLoaded(aBrowser)
    ]);

    let innerText = yield ContentTask.spawn(aBrowser, null, () => {
      return content.document.body.innerText;
    });
    isnot(innerText, "FAIL", "We should not have sent a get request!");
    is(innerText, "textarea=default+value&button=submit",
       "The post data should be received by the callee");
  });

  // XXX: Make sure to reset dom.ipc.processCount.webLargeAllocation if adding a
  // test after the above test.
}

function* largeAllocFailTests() {
  yield BrowserTestUtils.withNewTab("http://example.com", function*(aBrowser) {
    info("Starting test 1");
    let pid1 = yield getPID(aBrowser);
    is(false, yield getInLAProc(aBrowser));

    // Fail the test if we create a process
    let stopExpectNoProcess = expectNoProcess();

    yield ContentTask.spawn(aBrowser, TEST_URI, TEST_URI => {
      content.document.location = TEST_URI;
    });

    yield BrowserTestUtils.browserLoaded(aBrowser);

    let pid2 = yield getPID(aBrowser);

    is(pid1, pid2, "The PID should not have changed");
    is(false, yield getInLAProc(aBrowser));

    stopExpectNoProcess();
  });
}
