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
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, consoleOpened);
  }, true);

  function consoleOpened(hud)
  {
    ok(hud, "Web Console opened");

    let tabClosed = false, toolboxDestroyed = false;

    gBrowser.tabContainer.addEventListener("TabClose", function onTabClose() {
      gBrowser.tabContainer.removeEventListener("TabClose", onTabClose);

      ok(true, "tab closed");

      tabClosed = true;
      if (toolboxDestroyed) {
        testBrowserConsole();
      }
    });

    let toolbox = gDevTools.getToolbox(hud.target);
    toolbox.once("destroyed", () => {
      ok(true, "toolbox destroyed");

      toolboxDestroyed = true;
      if (tabClosed) {
        testBrowserConsole();
      }
    });

    EventUtils.synthesizeKey("w", { accelKey: true }, hud.iframeWindow);
  }

  function testBrowserConsole()
  {
    info("test the Browser Console");

    HUDConsoleUI.toggleBrowserConsole().then((hud) => {
      ok(hud, "Browser Console opened");

      Services.obs.addObserver(function onDestroy() {
        Services.obs.removeObserver(onDestroy, "web-console-destroyed");
        ok(true, "the Browser Console closed");

        executeSoon(finishTest);
      }, "web-console-destroyed", false);

      EventUtils.synthesizeKey("w", { accelKey: true }, hud.iframeWindow);
    });
  }
}
