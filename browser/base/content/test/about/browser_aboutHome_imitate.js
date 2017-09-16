/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

ignoreAllUncaughtExceptions();

add_task(async function() {
  info("Make sure that a page can't imitate about:home");

  await BrowserTestUtils.withNewTab({ gBrowser, url: "about:home" }, async function(browser) {
    let promise = BrowserTestUtils.browserLoaded(browser);
    browser.loadURI("https://example.com/browser/browser/base/content/test/about/test_bug959531.html");
    await promise;

    await ContentTask.spawn(browser, null, async function() {
      let button = content.document.getElementById("settings");
      ok(button, "Found settings button in test page");
      button.click();
    });

    await new Promise(resolve => {
      // It may take a few turns of the event loop before the window
      // is displayed, so we wait.
      function check(n) {
        let win = Services.wm.getMostRecentWindow("Browser:Preferences");
        ok(!win, "Preferences window not showing");
        if (win) {
          win.close();
        }

        if (n > 0) {
          executeSoon(() => check(n - 1));
        } else {
          resolve();
        }
      }

      check(5);
    });
  });
});
