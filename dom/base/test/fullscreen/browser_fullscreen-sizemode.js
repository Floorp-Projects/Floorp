/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const isMac = AppConstants.platform == "macosx";
const isWin = AppConstants.platform == "win";

async function waitForSizeMode(aWindow, aSizeMode) {
  await BrowserTestUtils.waitForEvent(aWindow, "sizemodechange", false, () => {
    return aWindow.windowState === aSizeMode;
  });
  const expectedHidden =
    aSizeMode == aWindow.STATE_MINIMIZED || aWindow.isFullyOccluded;
  if (aWindow.document.hidden != expectedHidden) {
    await BrowserTestUtils.waitForEvent(aWindow, "visibilitychange");
  }
  is(
    aWindow.document.hidden,
    expectedHidden,
    "Should be inactive if minimized or occluded"
  );
}

async function checkSizeModeAndFullscreenState(
  aWindow,
  aSizeMode,
  aFullscreen,
  aFullscreenEventShouldHaveFired,
  aStepFun
) {
  let promises = [];
  if (aWindow.windowState != aSizeMode) {
    promises.push(waitForSizeMode(aWindow, aSizeMode));
  }
  if (aFullscreenEventShouldHaveFired) {
    promises.push(
      BrowserTestUtils.waitForEvent(
        aWindow,
        aFullscreen ? "willenterfullscreen" : "willexitfullscreen"
      )
    );
    promises.push(BrowserTestUtils.waitForEvent(aWindow, "fullscreen"));
  }

  // Add listener for unexpected event.
  let unexpectedEventListener = aEvent => {
    ok(false, `should not receive ${aEvent.type} event`);
  };
  if (aFullscreenEventShouldHaveFired) {
    aWindow.addEventListener(
      aFullscreen ? "willexitfullscreen" : "willenterfullscreen",
      unexpectedEventListener
    );
  } else {
    aWindow.addEventListener("willenterfullscreen", unexpectedEventListener);
    aWindow.addEventListener("willexitfullscreen", unexpectedEventListener);
    aWindow.addEventListener("fullscreen", unexpectedEventListener);
  }

  let eventPromise = Promise.all(promises);
  aStepFun();
  await eventPromise;

  // Check SizeMode.
  is(
    aWindow.windowState,
    aSizeMode,
    "The new sizemode should have the expected value"
  );
  // Check Fullscreen state.
  is(
    aWindow.fullScreen,
    aFullscreen,
    `chrome window should ${aFullscreen ? "be" : "not be"} in fullscreen`
  );
  is(
    aWindow.document.documentElement.hasAttribute("inFullscreen"),
    aFullscreen,
    `chrome documentElement should ${
      aFullscreen ? "have" : "not have"
    } inFullscreen attribute`
  );

  // Remove listener for unexpected event.
  if (aFullscreenEventShouldHaveFired) {
    aWindow.removeEventListener(
      aFullscreen ? "willexitfullscreen" : "willenterfullscreen",
      unexpectedEventListener
    );
  } else {
    aWindow.removeEventListener("willenterfullscreen", unexpectedEventListener);
    aWindow.removeEventListener("willexitfullscreen", unexpectedEventListener);
    aWindow.removeEventListener("fullscreen", unexpectedEventListener);
  }
}

async function restoreWindowToNormal(aWindow) {
  while (aWindow.windowState != aWindow.STATE_NORMAL) {
    info(`Try to restore window with state ${aWindow.windowState} to normal`);
    let eventPromise = BrowserTestUtils.waitForEvent(aWindow, "sizemodechange");
    aWindow.restore();
    await eventPromise;
    info(`Window is now in state ${aWindow.windowState}`);
  }
}

add_task(async function test_fullscreen_restore() {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  await restoreWindowToNormal(win);

  info("Enter fullscreen");
  await checkSizeModeAndFullscreenState(
    win,
    win.STATE_FULLSCREEN,
    true,
    true,
    () => {
      win.fullScreen = true;
    }
  );

  info("Restore window");
  await checkSizeModeAndFullscreenState(
    win,
    win.STATE_NORMAL,
    false,
    true,
    () => {
      win.restore();
    }
  );

  await BrowserTestUtils.closeWindow(win);
});

// This test only enable on Windows because:
// - Test gets intermittent timeout on macOS, see bug 1828848.
// - Restoring a fullscreen window on GTK doesn't return it to the previous
//   sizemode, see bug 1828837.
if (isWin) {
  add_task(async function test_maximize_fullscreen_restore() {
    let win = await BrowserTestUtils.openNewBrowserWindow();
    await restoreWindowToNormal(win);

    info("Maximize window");
    await checkSizeModeAndFullscreenState(
      win,
      win.STATE_MAXIMIZED,
      false,
      false,
      () => {
        win.maximize();
      }
    );

    info("Enter fullscreen");
    await checkSizeModeAndFullscreenState(
      win,
      win.STATE_FULLSCREEN,
      true,
      true,
      () => {
        win.fullScreen = true;
      }
    );

    info("Restore window");
    await checkSizeModeAndFullscreenState(
      win,
      win.STATE_MAXIMIZED,
      false,
      true,
      () => {
        win.restore();
      }
    );

    await BrowserTestUtils.closeWindow(win);
  });
}

// Restoring a minimized window on macOS doesn't return it to the previous
// sizemode, see bug 1828706.
if (!isMac) {
  add_task(async function test_fullscreen_minimize_restore() {
    let win = await BrowserTestUtils.openNewBrowserWindow();
    await restoreWindowToNormal(win);

    info("Enter fullscreen");
    await checkSizeModeAndFullscreenState(
      win,
      win.STATE_FULLSCREEN,
      true,
      true,
      () => {
        win.fullScreen = true;
      }
    );

    info("Minimize window");
    await checkSizeModeAndFullscreenState(
      win,
      win.STATE_MINIMIZED,
      true,
      false,
      () => {
        win.minimize();
      }
    );

    info("Restore window");
    await checkSizeModeAndFullscreenState(
      win,
      win.STATE_FULLSCREEN,
      true,
      false,
      () => {
        win.restore();
      }
    );

    await BrowserTestUtils.closeWindow(win);
  });
}
