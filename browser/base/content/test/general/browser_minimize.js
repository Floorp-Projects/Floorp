/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function () {
  registerCleanupFunction(function () {
    window.restore();
  });
  function isActive() {
    return gBrowser.selectedTab.linkedBrowser.docShellIsActive;
  }

  ok(isActive(), "Docshell should be active when starting the test");
  ok(!document.hidden, "Top level window should be visible");

  // When we show or hide the window (including by minimization),
  // there are 3 signifiers that the process is complete: the
  // sizemodechange event, the occlusionstatechange event, and the
  // browsing context becoming active or inactive. There is not
  // a strict ordering of these events. For example, the browsing
  // context is activated based on the occlusionstatechange event,
  // so ordering of the listeners will dictate whether the event
  // arrives before or after the browsing context has been made
  // active. The safest way to check for all these signals is to
  // build promises around them and then wait for them all to
  // complete, and that's what we do here.
  info("Calling window.minimize");
  let promiseSizeModeChange = BrowserTestUtils.waitForEvent(
    window,
    "sizemodechange"
  );
  let promiseOcclusionStateChange = BrowserTestUtils.waitForEvent(
    window,
    "occlusionstatechange"
  );
  let promiseBrowserInactive = BrowserTestUtils.waitForCondition(
    () => !isActive(),
    "Docshell should be inactive."
  );
  window.minimize();
  await Promise.all([
    promiseSizeModeChange,
    promiseOcclusionStateChange,
    promiseBrowserInactive,
  ]);
  ok(document.hidden, "Top level window should be hidden");

  // When we restore the window from minimization, we have the
  // same concerns as above, so prepare our 3 promises.
  info("Calling window.restore");
  promiseSizeModeChange = BrowserTestUtils.waitForEvent(
    window,
    "sizemodechange"
  );
  promiseOcclusionStateChange = BrowserTestUtils.waitForEvent(
    window,
    "occlusionstatechange"
  );
  let promiseBrowserActive = BrowserTestUtils.waitForCondition(
    () => isActive(),
    "Docshell should be active."
  );
  window.restore();

  // On Ubuntu `window.restore` doesn't seem to work, use a timer to make the
  // test fail faster and more cleanly than with a test timeout.
  await Promise.race([
    Promise.all([
      promiseSizeModeChange,
      promiseOcclusionStateChange,
      promiseBrowserActive,
    ]),
    new Promise((resolve, reject) =>
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      setTimeout(() => {
        reject("timed out waiting for sizemodechange event");
      }, 5000)
    ),
  ]);
  ok(!document.hidden, "Top level window should be visible");
});
