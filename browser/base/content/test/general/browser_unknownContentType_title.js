const url =
  "data:text/html;charset=utf-8,%3C%21DOCTYPE%20html%3E%3Chtml%3E%3Chead%3E%3Ctitle%3ETest%20Page%3C%2Ftitle%3E%3C%2Fhead%3E%3C%2Fhtml%3E";
const unknown_url =
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.com/browser/browser/base/content/test/general/unknownContentType_file.pif";

function waitForNewWindow() {
  return new Promise(resolve => {
    let listener = win => {
      Services.obs.removeObserver(listener, "toplevel-window-ready");
      win.addEventListener("load", () => {
        resolve(win);
      });
    };

    Services.obs.addObserver(listener, "toplevel-window-ready");
  });
}

add_setup(async function () {
  let tmpDir = PathUtils.join(
    PathUtils.tempDir,
    "testsavedir" + Math.floor(Math.random() * 2 ** 32)
  );
  // Create this dir if it doesn't exist (ignores existing dirs)
  await IOUtils.makeDirectory(tmpDir);
  registerCleanupFunction(async function () {
    try {
      await IOUtils.remove(tmpDir, { recursive: true });
    } catch (e) {
      console.error(e);
    }
    Services.prefs.clearUserPref("browser.download.folderList");
    Services.prefs.clearUserPref("browser.download.dir");
  });
  Services.prefs.setIntPref("browser.download.folderList", 2);
  Services.prefs.setCharPref("browser.download.dir", tmpDir);
});

add_task(async function unknownContentType_title_with_pref_enabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.download.always_ask_before_handling_new_types", true]],
  });

  let tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, url));
  let browser = tab.linkedBrowser;
  await promiseTabLoaded(gBrowser.selectedTab);

  is(gBrowser.contentTitle, "Test Page", "Should have the right title.");

  BrowserTestUtils.startLoadingURIString(browser, unknown_url);
  let win = await waitForNewWindow();
  is(
    win.location.href,
    "chrome://mozapps/content/downloads/unknownContentType.xhtml",
    "Should have seen the unknown content dialog."
  );
  is(gBrowser.contentTitle, "Test Page", "Should still have the right title.");

  win.close();
  await promiseWaitForFocus(window);
  gBrowser.removeCurrentTab();
});

add_task(async function unknownContentType_title_with_pref_disabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.download.always_ask_before_handling_new_types", false]],
  });

  let tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, url));
  let browser = tab.linkedBrowser;
  await promiseTabLoaded(gBrowser.selectedTab);

  is(gBrowser.contentTitle, "Test Page", "Should have the right title.");

  BrowserTestUtils.startLoadingURIString(browser, unknown_url);
  // If the pref is disabled, then the downloads panel should open right away
  // since there is no UCT window prompt to block it.
  let waitForPanelShown = BrowserTestUtils.waitForCondition(() => {
    return DownloadsPanel.isPanelShowing;
  }).then(() => "panel-shown");

  let panelShown = await waitForPanelShown;
  is(panelShown, "panel-shown", "The downloads panel is shown");
  is(gBrowser.contentTitle, "Test Page", "Should still have the right title.");

  gBrowser.removeCurrentTab();
});
