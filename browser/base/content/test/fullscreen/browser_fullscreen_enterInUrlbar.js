/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This test makes sure that when the user presses enter in the urlbar in full
// screen, the toolbars are hidden.  This should not be run on macOS because we
// don't hide the toolbars there.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.sys.mjs",
});

add_task(async function test() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    // Do the View:FullScreen command and wait for the transition.
    let onFullscreen = BrowserTestUtils.waitForEvent(window, "fullscreen");
    document.getElementById("View:FullScreen").doCommand();
    await onFullscreen;

    // Do the Browser:OpenLocation command to show the nav toolbox and focus
    // the urlbar.
    let onToolboxShown = TestUtils.topicObserved(
      "fullscreen-nav-toolbox",
      (subject, data) => data == "shown"
    );
    document.getElementById("Browser:OpenLocation").doCommand();
    info("Waiting for the nav toolbox to be shown");
    await onToolboxShown;

    // Enter a URL.
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "http://example.com/",
      waitForFocus: SimpleTest.waitForFocus,
      fireInputEvent: true,
    });

    // Press enter and wait for the nav toolbox to be hidden.
    let onToolboxHidden = TestUtils.topicObserved(
      "fullscreen-nav-toolbox",
      (subject, data) => data == "hidden"
    );
    EventUtils.synthesizeKey("KEY_Enter");
    info("Waiting for the nav toolbox to be hidden");
    await onToolboxHidden;

    Assert.ok(true, "Nav toolbox hidden");
  });
});
