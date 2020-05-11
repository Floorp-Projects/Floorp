"use strict";

// This test tends to trigger a race in the fullscreen time telemetry,
// where the fullscreen enter and fullscreen exit events (which use the
// same histogram ID) overlap. That causes TelemetryStopwatch to log an
// error.
SimpleTest.ignoreAllUncaughtExceptions(true);

function captureUnexpectedFullscreenChange() {
  ok(false, "Caught an unexpected fullscreen change");
}

const kPage = "http://example.org/browser/dom/html/test/dummy_page.html";

function waitForDocActivated(aBrowser) {
  return SpecialPowers.spawn(aBrowser, [], () => {
    return ContentTaskUtils.waitForCondition(
      () => docShell.isActive && content.document.hasFocus()
    );
  });
}

add_task(async function() {
  await pushPrefs(
    ["full-screen-api.transition-duration.enter", "0 0"],
    ["full-screen-api.transition-duration.leave", "0 0"]
  );

  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: kPage,
    waitForStateStop: true,
  });
  let browser = tab.linkedBrowser;

  // As requestFullscreen checks the active state of the docshell,
  // wait for the document to be activated, just to be sure that
  // the fullscreen request won't be denied.
  await SpecialPowers.spawn(browser, [], () => {
    return ContentTaskUtils.waitForCondition(
      () => docShell.isActive && content.document.hasFocus()
    );
  });

  let contextMenu = document.getElementById("contentAreaContextMenu");
  ok(contextMenu, "Got context menu");

  let state;
  info("Enter DOM fullscreen");
  let fullScreenChangedPromise = BrowserTestUtils.waitForContentEvent(
    browser,
    "fullscreenchange"
  );
  await SpecialPowers.spawn(browser, [], () => {
    content.document.body.requestFullscreen();
  });

  await fullScreenChangedPromise;
  state = await SpecialPowers.spawn(browser, [], () => {
    return !!content.document.fullscreenElement;
  });
  ok(state, "The content should have entered fullscreen");
  ok(document.fullscreenElement, "The chrome should also be in fullscreen");

  let removeContentEventListener = BrowserTestUtils.addContentEventListener(
    browser,
    "fullscreenchange",
    captureUnexpectedFullscreenChange
  );

  info("Open context menu");
  is(contextMenu.state, "closed", "Should not have opened context menu");

  let popupShownPromise = promiseWaitForEvent(window, "popupshown");

  EventUtils.synthesizeMouse(
    browser,
    screen.width / 2,
    screen.height / 2,
    { type: "contextmenu", button: 2 },
    window
  );
  await popupShownPromise;
  is(contextMenu.state, "open", "Should have opened context menu");

  info("Send the first escape");
  let popupHidePromise = promiseWaitForEvent(window, "popuphidden");
  EventUtils.synthesizeKey("KEY_Escape");
  await popupHidePromise;
  is(contextMenu.state, "closed", "Should have closed context menu");

  // Wait a small time to confirm that the first ESC key
  // does not exit fullscreen.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 1000));
  state = await SpecialPowers.spawn(browser, [], () => {
    return !!content.document.fullscreenElement;
  });
  ok(state, "The content should still be in fullscreen");
  ok(document.fullscreenElement, "The chrome should still be in fullscreen");

  removeContentEventListener();
  info("Send the second escape");
  let fullscreenExitPromise = BrowserTestUtils.waitForContentEvent(
    browser,
    "fullscreenchange"
  );
  EventUtils.synthesizeKey("KEY_Escape");
  await fullscreenExitPromise;
  ok(!document.fullscreenElement, "The chrome should have exited fullscreen");

  gBrowser.removeTab(tab);
});
