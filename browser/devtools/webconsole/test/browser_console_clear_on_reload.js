/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Check that clear output on page reload works - bug 705921.

function test()
{
  const PREF = "devtools.webconsole.persistlog";
  const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";
  let hud = null;

  Services.prefs.setBoolPref(PREF, false);
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
        text: "test-console.html",
        category: CATEGORY_NETWORK,
      },
      {
        text: "foobarz2",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
      }],
    }).then(onConsoleMessageAfterReload);
  }

  function onConsoleMessageAfterReload()
  {
    is(hud.outputNode.textContent.indexOf("foobarz1"), -1,
       "foobarz1 has been removed from output");
    finishTest();
  }
}
