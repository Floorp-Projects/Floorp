const TEST_URLS_MAP = {
  "about:about": "About About",
  "about:license": "Licenses",
  "about:profiles": "About Profiles",
  "about:mozilla": "The Book of Mozilla, 15:1"
};
const TEST_URLS = Object.keys(TEST_URLS_MAP);
const TEST_LABELS = Object.values(TEST_URLS_MAP);

const Paths = SessionFile.Paths;
const BROKEN_WM_Z_ORDER = AppConstants.platform != "macosx" && AppConstants.platform != "win";

let source;
let state;

function promiseProvideWindow(url, features) {
  return new Promise(resolve => provideWindow(resolve, url, features));
}

add_task(async function init() {
  // Make sure that we start with only primary window.
  await promiseAllButPrimaryWindowClosed();

  let promises = [];
  for (let i = 0; i < 4; ++i) {
    let url = Object.keys(TEST_URLS_MAP)[i];
    let window = await promiseProvideWindow();
    BrowserTestUtils.loadURI(window.gBrowser, url);
    if (i == 2) {
      window.minimize();
    }
    // We want to get the lastest state from each window.
    await BrowserTestUtils.waitForLocationChange(window.gBrowser, url);
    await BrowserTestUtils.browserStopped(window.gBrowser.selectedBrowser, url);
    promises.push(TabStateFlusher.flushWindow(window));
  }

  // Wait until we get the lastest history from all windows.
  await Promise.all(promises);

  // Force save state and read the written state.
  source = await promiseRecoveryFileContents();
  state = JSON.parse(source);

  // Close all windows as we don't need them.
  await promiseAllButPrimaryWindowClosed();
});

add_task(async function test_z_indices_are_saved_correctly() {
  is(state.windows.length, 4, "Correct number of windows saved");

  // Check if we saved state in correct order of creation.
  for (let i = 0; i < TEST_URLS.length; ++i) {
    is(state.windows[i].tabs[0].entries[0].url, TEST_URLS[i],
       `Window #${i} is stored in correct creation order`);
  }

  // Check if we saved a valid zIndex (no null, no undefined or no 0).
  for (let win of state.windows) {
    ok(win.zIndex, "A valid zIndex is stored");
  }

  if (BROKEN_WM_Z_ORDER) {
    // Last window should have zIndex of 2.
    is(state.windows[3].zIndex, 2, "Currently using window has correct z-index");

    // Other windows should have zIndex of 1.
    is(state.windows[0].zIndex, 1, "Window #1 has correct z-index");
    is(state.windows[1].zIndex, 1, "Window #2 has correct z-index");

    // Minimized window should have zIndex of -1.
    is(state.windows[2].zIndex, -1, "Minimized window has correct z-index");
  } else {
    is(state.windows[2].zIndex, -1, "Minimzed window has correct z-index");
    ok(state.windows[0].zIndex != -1, "Window #1 shouldn't has z-index -1");
    ok(state.windows[1].zIndex > state.windows[0].zIndex, "Window #2 has correct z-index");
    ok(state.windows[3].zIndex > state.windows[1].zIndex, "Currently using window has correct z-index");
  }
});

add_task(async function test_windows_are_restored_in_reversed_z_index() {
  let windowsOpened = 1;
  let windows = [window];
  let windowsRestored = 0;
  let tabsRestoredLabels = [];

  // A defer promise that will be resolved once
  // we restored all tabs.
  let defer = {};
  defer.promise = new Promise((resolve, reject) => {
    defer.resolve = resolve;
    defer.reject = reject;
  });

  function allTabsRestored() {
    is(tabsRestoredLabels[0], TEST_LABELS[3], "First restored tab should be previous used tab");
    // We don't care about restoration order of windows in between the first and last window
    // when the OS has broken vm z-order.
    if (!BROKEN_WM_Z_ORDER) {
      is(tabsRestoredLabels[1], TEST_LABELS[1], "Second restored tab is correct");
      is(tabsRestoredLabels[2], TEST_LABELS[0], "Third restored tab is correct");
    }
    is(tabsRestoredLabels[3], TEST_LABELS[2], "Last restored tab should be in minimized window");

    // Finish the test.
    defer.resolve();
  }

  function onSSWindowRestored(aEvent) {
    if (++windowsRestored == 4) {
      // Remove the event listener from each window
      windows.forEach(function(win) {
        win.removeEventListener("SSWindowRestored", onSSWindowRestored, true);
      });
      executeSoon(allTabsRestored);
    }
    tabsRestoredLabels.push(aEvent.target.gBrowser.tabs[0].label);
  }

  // Used to add our listener to further windows so we can catch SSWindowRestored
  // coming from them when creating a multi-window state.
  function windowObserver(aSubject, aTopic, aData) {
    if (aTopic == "domwindowopened") {
      let newWindow = aSubject.QueryInterface(Ci.nsIDOMWindow);
      newWindow.addEventListener("load", function() {
        if (++windowsOpened == 3) {
          Services.ww.unregisterNotification(windowObserver);
        }

        // Track this window so we can remove the progress listener later.
        windows.push(newWindow);
        // Add the progress listener.
        newWindow.addEventListener("SSWindowRestored", onSSWindowRestored, true);
      }, { once: true });
    }
  }

  Services.ww.registerNotification(windowObserver);
  registerCleanupFunction(function() {
    windows.forEach(function(win) {
      win.removeEventListener("SSWindowRestored", onSSWindowRestored, true);
      if (win !== window) {
        BrowserTestUtils.closeWindow(win);
      }
    });
  });

  window.addEventListener("SSWindowRestored", onSSWindowRestored, true);

  // Restore states.
  ss.setBrowserState(source);

  await defer.promise;
});

