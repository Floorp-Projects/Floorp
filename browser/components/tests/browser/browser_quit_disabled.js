/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_appMenu_quit_disabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.quitShortcut.disabled", true]],
  });

  let win = await BrowserTestUtils.openNewBrowserWindow();
  let doc = win.document;

  let menuButton = doc.getElementById("PanelUI-menu-button");
  menuButton.click();
  await BrowserTestUtils.waitForEvent(win.PanelUI.mainView, "ViewShown");

  let quitButton = doc.querySelector(`[key="key_quitApplication"]`);
  is(quitButton, null, "No quit button with shortcut key");

  await BrowserTestUtils.closeWindow(win);

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_quit_shortcut_disabled() {
  async function testQuitShortcut(shouldQuit) {
    let win = await BrowserTestUtils.openNewBrowserWindow();

    let quitRequested = false;
    let observer = {
      observe(subject, topic, data) {
        is(topic, "quit-application-requested", "Right observer topic");
        ok(shouldQuit, "Quit shortcut should NOT have worked");

        // Don't actually quit the browser when testing.
        let cancelQuit = subject.QueryInterface(Ci.nsISupportsPRBool);
        cancelQuit.data = true;

        quitRequested = true;
      },
    };
    Services.obs.addObserver(observer, "quit-application-requested");

    let modifiers = { accelKey: true };
    if (AppConstants.platform == "win") {
      modifiers.shiftKey = true;
    }
    EventUtils.synthesizeKey("q", modifiers, win);

    await BrowserTestUtils.closeWindow(win);
    Services.obs.removeObserver(observer, "quit-application-requested");

    is(quitRequested, shouldQuit, "Expected quit state");
  }

  // Quit shortcut should work when pref is not set.
  await testQuitShortcut(true);

  await SpecialPowers.pushPrefEnv({
    set: [["browser.quitShortcut.disabled", true]],
  });
  await testQuitShortcut(false);
});
