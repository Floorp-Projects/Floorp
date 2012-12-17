/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const originalState = ss.getBrowserState();

/** Private Browsing Test for Bug 819510 **/
function test() {
  waitForExplicitFinish();

  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("browser.sessionstore.interval");
    ss.setBrowserState(originalState);
  });

  runNextTest();
}

let tests = [
  test_1,
  test_2,
  test_3,
];

const testState = {
  windows: [{
    tabs: [
      { entries: [{ url: "about:blank" }] },
    ]
  }]
};

function runNextTest() {
  // Set an empty state
  let windowsEnum = Services.wm.getEnumerator("navigator:browser");
  while (windowsEnum.hasMoreElements()) {
    let currentWindow = windowsEnum.getNext();
    if (currentWindow != window) {
      currentWindow.close();
    }
  }

  // Run the next test, or finish
  if (tests.length) {
    let currentTest = tests.shift();
    waitForBrowserState(testState, currentTest);
  }
  else {
    Services.prefs.clearUserPref("browser.sessionstore.interval");
    ss.setBrowserState(originalState);
    finish();
  }
}

// Test opening default mochitest-normal-private-normal-private windows
// (saving the state with last window being private)
function test_1() {
  testOnWindow(false, function(aWindow) {
    aWindow.gBrowser.addTab("http://www.example.com/1");
    testOnWindow(true, function(aWindow) {
      aWindow.gBrowser.addTab("http://www.example.com/2");
      testOnWindow(false, function(aWindow) {
        aWindow.gBrowser.addTab("http://www.example.com/3");
        testOnWindow(true, function(aWindow) {
          aWindow.gBrowser.addTab("http://www.example.com/4");

          let curState = JSON.parse(ss.getBrowserState());
          is (curState.windows.length, 5, "Browser has opened 5 windows");
          is (curState.windows[2].isPrivate, true, "Window is private");
          is (curState.windows[4].isPrivate, true, "Last window is private");
          is (curState.selectedWindow, 5, "Last window opened is the one selected");

          Services.obs.addObserver(function observe(aSubject, aTopic, aData) {
            Services.obs.removeObserver(observe, aTopic);
            aSubject.QueryInterface(Ci.nsISupportsString);
            let state = JSON.parse(aSubject.data);
            is(state.windows.length, 3,
              "sessionstore state: 3 windows in data being writted to disk");
            is (state.selectedWindow, 3,
              "Selected window is updated to match one of the saved windows");
            state.windows.forEach(function(win) {
              is(!win.isPrivate, true, "Saved window is not private");
            });
            is(state._closedWindows.length, 0,
              "sessionstore state: no closed windows in data being writted to disk");
            runNextTest();
          }, "sessionstore-state-write", false);

          Services.prefs.setIntPref("browser.sessionstore.interval", 0);
        });
      });
    });
  });
}

// Test opening default mochitest window + 2 private windows
function test_2() {
testOnWindow(true, function(aWindow) {
  aWindow.gBrowser.addTab("http://www.example.com/1");
    testOnWindow(true, function(aWindow) {
      aWindow.gBrowser.addTab("http://www.example.com/2");

      let curState = JSON.parse(ss.getBrowserState());
      is (curState.windows.length, 3, "Browser has opened 3 windows");
      is (curState.windows[1].isPrivate, true, "Window 1 is private");
      is (curState.windows[2].isPrivate, true, "Window 2 is private");
      is (curState.selectedWindow, 3, "Last window opened is the one selected");

      Services.obs.addObserver(function observe(aSubject, aTopic, aData) {
        Services.obs.removeObserver(observe, aTopic);
        aSubject.QueryInterface(Ci.nsISupportsString);
        let state = JSON.parse(aSubject.data);
        is(state.windows.length, 1,
          "sessionstore state: 1 windows in data being writted to disk");
        is (state.selectedWindow, 1,
          "Selected window is updated to match one of the saved windows");
        is(state._closedWindows.length, 0,
          "sessionstore state: no closed windows in data being writted to disk");
        runNextTest();
      }, "sessionstore-state-write", false);
      Services.prefs.setIntPref("browser.sessionstore.interval", 0);
  });
});
}

// Test opening default-normal-private-normal windows and closing a normal window
function test_3() {
  testOnWindow(false, function(normalWindow) {
    normalWindow.gBrowser.addTab("http://www.example.com/1");
    testOnWindow(true, function(aWindow) {
      aWindow.gBrowser.addTab("http://www.example.com/2");
      testOnWindow(false, function(aWindow) {
        aWindow.gBrowser.addTab("http://www.example.com/3");

        let curState = JSON.parse(ss.getBrowserState());
        is (curState.windows.length, 4, "Browser has opened 4 windows");
        is (curState.windows[2].isPrivate, true, "Window 2 is private");
        is (curState.selectedWindow, 4, "Last window opened is the one selected");

        normalWindow.close();
        Services.obs.addObserver(function observe(aSubject, aTopic, aData) {
          Services.obs.removeObserver(observe, aTopic);
          aSubject.QueryInterface(Ci.nsISupportsString);
          let state = JSON.parse(aSubject.data);
          is(state.windows.length, 2,
            "sessionstore state: 2 windows in data being writted to disk");
          is (state.selectedWindow, 2,
            "Selected window is updated to match one of the saved windows");
          state.windows.forEach(function(win) {
            is(!win.isPrivate, true, "Saved window is not private");
          });
          is(state._closedWindows.length, 1,
            "sessionstore state: 1 closed window in data being writted to disk");
          state._closedWindows.forEach(function(win) {
            is(!win.isPrivate, true, "Closed window is not private");
          });
          runNextTest();
        }, "sessionstore-state-write", false);

        Services.prefs.setIntPref("browser.sessionstore.interval", 0);
      });
    });
  });
}

function testOnWindow(aIsPrivate, aCallback) {
  let win = OpenBrowserWindow({private: aIsPrivate});
  win.addEventListener("load", function onLoad() {
    win.removeEventListener("load", onLoad, false);
    executeSoon(function() { aCallback(win); });
  }, false);
}
