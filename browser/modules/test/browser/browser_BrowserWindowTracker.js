"use strict";

ChromeUtils.import("resource:///modules/BrowserWindowTracker.jsm");
ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

const TEST_WINDOW = window;

async function withOpenWindows(amount, cont) {
  let windows = [];
  for (let i = 0; i < amount; ++i) {
    windows.push(await BrowserTestUtils.openNewBrowserWindow());
  }
  await cont(windows);
  await Promise.all(windows.map(window => BrowserTestUtils.closeWindow(window)));
}

add_task(async function test_getTopWindow() {
  await withOpenWindows(5, async function(windows) {
    // Without options passed in.
    let window = BrowserWindowTracker.getTopWindow();
    let expectedMostRecentIndex = windows.length - 1;
    Assert.equal(window, windows[expectedMostRecentIndex],
      "Last opened window should be the most recent one.");

    // Mess with the focused window things a bit.
    for (let idx of [3, 1]) {
      let promise = BrowserTestUtils.waitForEvent(windows[idx], "activate");
      Services.focus.focusedWindow = windows[idx];
      await promise;
      window = BrowserWindowTracker.getTopWindow();
      Assert.equal(window, windows[idx], "Lastly focused window should be the most recent one.");
      // For this test it's useful to keep the array of created windows in order.
      windows.splice(idx, 1);
      windows.push(window);
    }
    // Update the pointer to the most recent opened window.
    expectedMostRecentIndex = windows.length - 1;

    // With 'private' option.
    window = BrowserWindowTracker.getTopWindow({ private: true });
    Assert.equal(window, null, "No private windows opened yet.");
    window = BrowserWindowTracker.getTopWindow({ private: 1 });
    Assert.equal(window, null, "No private windows opened yet.");
    windows.push(await BrowserTestUtils.openNewBrowserWindow({ private: true }));
    ++expectedMostRecentIndex;
    window = BrowserWindowTracker.getTopWindow({ private: true });
    Assert.equal(window, windows[expectedMostRecentIndex], "Private window available.");
    window = BrowserWindowTracker.getTopWindow({ private: 1 });
    Assert.equal(window, windows[expectedMostRecentIndex], "Private window available.");
    // Private window checks seems to mysteriously fail on Linux in this test.
    if (AppConstants.platform != "linux") {
      window = BrowserWindowTracker.getTopWindow({ private: false });
      Assert.equal(window, windows[expectedMostRecentIndex - 1],
        "Private window available, but should not be returned.");
    }

    // With 'allowPopups' option.
    window = BrowserWindowTracker.getTopWindow({ allowPopups: true });
    Assert.equal(window, windows[expectedMostRecentIndex],
      "Window focused before the private window should be the most recent one.");
    window = BrowserWindowTracker.getTopWindow({ allowPopups: false });
    Assert.equal(window, windows[expectedMostRecentIndex],
      "Window focused before the private window should be the most recent one.");
    let popupWindowPromise = BrowserTestUtils.waitForNewWindow();
    ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
      let features = "location=no, personalbar=no, toolbar=no, scrollbars=no, menubar=no, status=no";
      content.window.open("about:blank", "_blank", features);
    });
    let popupWindow = await popupWindowPromise;
    window = BrowserWindowTracker.getTopWindow({ allowPopups: true });
    Assert.equal(window, popupWindow,
      "The popup window should be the most recent one, when requested.");
    window = BrowserWindowTracker.getTopWindow({ allowPopups: false });
    Assert.equal(window, windows[expectedMostRecentIndex],
      "Window focused before the popup window should be the most recent one.");
    popupWindow.close();
  });
});

add_task(async function test_orderedWindows() {
  await withOpenWindows(10, async function(windows) {
    let ordered = [...BrowserWindowTracker.orderedWindows].filter(w => w != TEST_WINDOW);
    Assert.deepEqual([9, 8, 7, 6, 5, 4, 3, 2, 1, 0], ordered.map(w => windows.indexOf(w)),
      "Order of opened windows should be as opened.");

    // Mess with the focused window things a bit.
    for (let idx of [4, 6, 1]) {
      let promise = BrowserTestUtils.waitForEvent(windows[idx], "activate");
      Services.focus.focusedWindow = windows[idx];
      await promise;
    }

    let ordered2 = [...BrowserWindowTracker.orderedWindows].filter(w => w != TEST_WINDOW);
    // After the shuffle, we expect window '1' to be the top-most window, because
    // it was the last one we called focus on. Then '6', the window we focused
    // before-last, followed by '4'. The order of the other windows remains
    // unchanged.
    let expected = [1, 6, 4, 9, 8, 7, 5, 3, 2, 0];
    Assert.deepEqual(expected, ordered2.map(w => windows.indexOf(w)),
      "After shuffle of focused windows, the order should've changed.");

    // Minimizing a window should put it at the end of the ordered list of windows.
    let promise = BrowserTestUtils.waitForEvent(windows[9], "sizemodechange");
    windows[9].minimize();
    await promise;

    let ordered3 = [...BrowserWindowTracker.orderedWindows].filter(w => w != TEST_WINDOW);
    // Test the end of the array of window indices, because Windows Debug builds
    // mysteriously swap the order of the first two windows.
    Assert.deepEqual([8, 7, 5, 3, 2, 0, 9], ordered3.map(w => windows.indexOf(w)).slice(3),
      "When a window is minimized, the order should've changed.");
  });
});
