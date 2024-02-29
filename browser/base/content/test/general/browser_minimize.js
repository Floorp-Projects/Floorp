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
  // there are 2 signifiers that the process is complete: the
  // sizemodechange event, and the browsing context becoming active
  // or inactive. There is another signifier, the
  // occlusionstatechange event, but whether or not that event
  // is sent is platform-dependent, so it's not very useful. The
  // safest way to check for stable state is to build promises
  // around sizemodechange and browsing context active and then
  // wait for them all to complete, and that's what we do here.
  info("Calling window.minimize");
  let promiseSizeModeChange = BrowserTestUtils.waitForEvent(
    window,
    "sizemodechange"
  ).then(
    () => ok(true, "Got sizemodechange."),
    () => ok(false, "Rejected sizemodechange.")
  );
  let promiseBrowserInactive = BrowserTestUtils.waitForCondition(
    () => !isActive(),
    "Docshell should be inactive."
  ).then(
    () => ok(true, "Got inactive."),
    () => ok(false, "Rejected inactive.")
  );
  window.minimize();
  await Promise.all([promiseSizeModeChange, promiseBrowserInactive]);
  ok(document.hidden, "Top level window should be hidden");

  // When we restore the window from minimization, we have the
  // same concerns as above, so prepare our promises.
  info("Calling window.restore");
  promiseSizeModeChange = BrowserTestUtils.waitForEvent(
    window,
    "sizemodechange"
  ).then(
    () => ok(true, "Got sizemodechange."),
    () => ok(false, "Rejected sizemodechange.")
  );
  let promiseBrowserActive = BrowserTestUtils.waitForCondition(
    () => isActive(),
    "Docshell should be active."
  ).then(
    () => ok(true, "Got active."),
    () => ok(false, "Rejected active.")
  );
  window.restore();

  // On Ubuntu `window.restore` doesn't seem to work, use a timer to make the
  // test fail faster and more cleanly than with a test timeout.
  await Promise.race([
    Promise.all([promiseSizeModeChange, promiseBrowserActive]),
    new Promise((resolve, reject) =>
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      setTimeout(() => {
        reject("timed out waiting for sizemodechange event");
      }, 5000)
    ),
  ]);
  ok(!document.hidden, "Top level window should be visible");
});
