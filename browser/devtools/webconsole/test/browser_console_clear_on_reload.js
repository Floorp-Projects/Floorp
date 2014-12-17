/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Check that clear output on page reload works - bug 705921.

"use strict";

let test = asyncTest(function*() {
  const PREF = "devtools.webconsole.persistlog";
  const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";

  Services.prefs.setBoolPref(PREF, false);
  registerCleanupFunction(() => Services.prefs.clearUserPref(PREF));

  yield loadTab(TEST_URI);

  let hud = yield openConsole();
  ok(hud, "Web Console opened");

  hud.jsterm.clearOutput();
  hud.jsterm.execute("console.log('foobarz1')");

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "foobarz1",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  });

  BrowserReload();
  yield loadBrowser(gBrowser.selectedBrowser);

  hud.jsterm.execute("console.log('foobarz2')");

  yield waitForMessages({
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
  });

  is(hud.outputNode.textContent.indexOf("foobarz1"), -1,
     "foobarz1 has been removed from output");
});
