/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  registerCleanupFunction(function() {
    window.restore();
  });
  function isActive() {
    return gBrowser.selectedTab.linkedBrowser.docShellIsActive;
  }

  ok(isActive(), "Docshell should be active when starting the test");

  info("Calling window.minimize");
  let promiseSizeModeChange = BrowserTestUtils.waitForEvent(
    window,
    "sizemodechange"
  );
  window.minimize();
  await promiseSizeModeChange;
  ok(!isActive(), "Docshell should be Inactive");

  info("Calling window.restore");
  promiseSizeModeChange = BrowserTestUtils.waitForEvent(
    window,
    "sizemodechange"
  );
  window.restore();
  // On Ubuntu `window.restore` doesn't seem to work, use a timer to make the
  // test fail faster and more cleanly than with a test timeout.
  await Promise.race([
    promiseSizeModeChange,
    new Promise((resolve, reject) =>
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      setTimeout(() => {
        reject("timed out waiting for sizemodechange event");
      }, 5000)
    ),
  ]);
  // The sizemodechange event can sometimes be fired before the
  // occlusionstatechange event, especially in chaos mode.
  if (window.isFullyOccluded) {
    await BrowserTestUtils.waitForEvent(window, "occlusionstatechange");
  }
  ok(isActive(), "Docshell should be active again");
});
