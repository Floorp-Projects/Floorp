/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Check that the navigation marker shows on page reload - bug 793996.

function test()
{
  const PREF = "devtools.webconsole.persistlog";
  const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";
  let hud = null;
  let Messages = require("devtools/webconsole/console-output").Messages;

  Services.prefs.setBoolPref(PREF, true);
  registerCleanupFunction(() => Services.prefs.clearUserPref(PREF));

  addTab(TEST_URI);

  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, consoleOpened);
  }, true);

  function consoleOpened(aHud)
  {
    hud = aHud;
    ok(hud, "Web Console opened");

    hud.jsterm.clearOutput();
    content.console.log("foobarz1");
    waitForMessages({
      webconsole: hud,
      messages: [{
        text: "foobarz1",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
      }],
    }).then(onConsoleMessage);
  }

  function onConsoleMessage()
  {
    browser.addEventListener("load", onReload, true);
    content.location.reload();
  }

  function onReload()
  {
    browser.removeEventListener("load", onReload, true);

    content.console.log("foobarz2");

    waitForMessages({
      webconsole: hud,
      messages: [{
        name: "page reload",
        text: "test-console.html",
        category: CATEGORY_NETWORK,
        severity: SEVERITY_LOG,
      },
      {
        text: "foobarz2",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
      },
      {
        name: "navigation marker",
        text: "test-console.html",
        type: Messages.NavigationMarker,
      }],
    }).then(onConsoleMessageAfterReload);
  }

  function onConsoleMessageAfterReload()
  {
    isnot(hud.outputNode.textContent.indexOf("foobarz1"), -1,
          "foobarz1 is still in the output");
    finishTest();
  }
}
