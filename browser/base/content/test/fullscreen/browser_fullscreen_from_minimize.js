/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This test checks whether fullscreen windows can transition to minimized windows,
// and back again. This is sometimes not directly supported by the OS widgets. For
// example, in macOS, the minimize button is greyed-out in the title bar of
// fullscreen windows, making this transition impossible for users to initiate.
// Still, web APIs do allow arbitrary combinations of window calls, and this test
// exercises some of those combinations.

const restoreWindowToNormal = async () => {
  // Get the window to normal state by calling window.restore(). This may take
  // multiple attempts since a call to restore could bring the window to either
  // NORMAL or MAXIMIZED state.
  while (window.windowState != window.STATE_NORMAL) {
    info(
      `Calling window.restore(), to try to reach "normal" state ${window.STATE_NORMAL}.`
    );
    let promiseSizeModeChange = BrowserTestUtils.waitForEvent(
      window,
      "sizemodechange"
    );
    window.restore();
    await promiseSizeModeChange;
    info(`Window reached state ${window.windowState}.`);
  }
};

add_task(async function () {
  registerCleanupFunction(function () {
    window.restore();
  });

  // We reuse these variables to create new promises for each transition.
  let promiseSizeModeChange;
  let promiseFullscreen;

  await restoreWindowToNormal();
  ok(!window.fullScreen, "Window should not be fullscreen at start of test.");

  // Get to fullscreen.
  info("Requesting fullscreen.");
  promiseFullscreen = document.documentElement.requestFullscreen();
  await promiseFullscreen;
  ok(window.fullScreen, "Window should be fullscreen before being minimized.");

  // Transition between fullscreen and minimize states.
  info("Requesting minimize on a fullscreen window.");
  promiseSizeModeChange = BrowserTestUtils.waitForEvent(
    window,
    "sizemodechange"
  );
  window.minimize();
  await promiseSizeModeChange;
  is(
    window.windowState,
    window.STATE_MINIMIZED,
    "Window should be minimized after fullscreen."
  );

  // Whether or not the previous transition worked, restore the window
  // and then minimize it.
  await restoreWindowToNormal();

  info("Requesting minimize on a normal window.");
  promiseSizeModeChange = BrowserTestUtils.waitForEvent(
    window,
    "sizemodechange"
  );
  window.minimize();
  await promiseSizeModeChange;
  is(
    window.windowState,
    window.STATE_MINIMIZED,
    "Window should be minimized before fullscreen."
  );

  info("Requesting fullscreen on a minimized window.");
  promiseFullscreen = document.documentElement.requestFullscreen();
  await promiseFullscreen;
  ok(window.fullScreen, "Window should be fullscreen after being minimized.");
});
