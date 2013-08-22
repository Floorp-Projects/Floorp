/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Check that Ctrl-W closes the Browser Console and that Ctrl-W closes the
// current tab when using the Web Console - bug 871156.

function test()
{
  const TEST_URI = "data:text/html;charset=utf8,<title>bug871156</title>\n" +
                   "<p>hello world";
  let firstTab = gBrowser.selectedTab;
  Services.prefs.setBoolPref("browser.tabs.animate", false);

  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, consoleOpened);
  }, true);

  function consoleOpened(hud)
  {
    ok(hud, "Web Console opened");

    let tabClosed = promise.defer();
    let toolboxDestroyed = promise.defer();
    let tabSelected = promise.defer();

    let pageWindow = firstTab.linkedBrowser.contentWindow;
    let toolbox = gDevTools.getToolbox(hud.target);

    gBrowser.tabContainer.addEventListener("TabClose", function onTabClose() {
      gBrowser.tabContainer.removeEventListener("TabClose", onTabClose);
      info("tab closed");
      tabClosed.resolve(null);
    });

    gBrowser.tabContainer.addEventListener("TabSelect", function onTabSelect() {
      gBrowser.tabContainer.removeEventListener("TabSelect", onTabSelect);
      if (gBrowser.selectedTab == firstTab) {
        info("tab selected");
        tabSelected.resolve(null);
      }
    });

    toolbox.once("destroyed", () => {
      info("toolbox destroyed");
      toolboxDestroyed.resolve(null);
    });

    promise.all([tabClosed.promise, toolboxDestroyed.promise, tabSelected.promise ]).then(() => {
      info("promise.all resolved");
      waitForFocus(testBrowserConsole, pageWindow, true);
    });

    // Get out of the web console initialization.
    executeSoon(() => {
      EventUtils.synthesizeKey("w", { accelKey: true });
    });
  }

  function testBrowserConsole()
  {
    info("test the Browser Console");

    HUDService.toggleBrowserConsole().then((hud) => {
      ok(hud, "Browser Console opened");

      Services.obs.addObserver(function onDestroy() {
        Services.obs.removeObserver(onDestroy, "web-console-destroyed");
        ok(true, "the Browser Console closed");

        Services.prefs.clearUserPref("browser.tabs.animate");
        waitForFocus(finish, content, true);
      }, "web-console-destroyed", false);

      waitForFocus(() => {
        EventUtils.synthesizeKey("w", { accelKey: true }, hud.iframeWindow);
      }, hud.iframeWindow);
    });
  }
}
