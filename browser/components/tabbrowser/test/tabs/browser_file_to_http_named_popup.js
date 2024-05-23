/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */

const TEST_FILE = fileURL("dummy_page.html");
const TEST_HTTP = httpURL("dummy_page.html");

// Test for Bug 1634252
add_task(async function () {
  await BrowserTestUtils.withNewTab(TEST_FILE, async function (fileBrowser) {
    info("Tab ready");

    async function summonPopup(firstRun) {
      var winPromise;
      if (firstRun) {
        winPromise = BrowserTestUtils.waitForNewWindow({
          url: TEST_HTTP,
        });
      }

      await SpecialPowers.spawn(
        fileBrowser,
        [TEST_HTTP, firstRun],
        (target, firstRun_) => {
          var win = content.open(target, "named", "width=400,height=400");
          win.focus();
          ok(win, "window.open was successful");
          if (firstRun_) {
            content.document.firstWindow = win;
          } else {
            content.document.otherWindow = win;
          }
        }
      );

      if (firstRun) {
        // We should only wait for the window the first time, because only the
        // later times no new window should be created.
        info("Waiting for new window");
        var win = await winPromise;
        ok(win, "Got a window");
      }
    }

    info("Opening window");
    await summonPopup(true);
    info("Opening window again");
    await summonPopup(false);

    await SpecialPowers.spawn(fileBrowser, [], () => {
      ok(content.document.firstWindow, "Window is non-null");
      is(
        content.document.otherWindow,
        content.document.firstWindow,
        "Windows are the same"
      );

      content.document.firstWindow.close();
    });
  });
});
