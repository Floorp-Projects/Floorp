/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Check that clear output on page reload works - bug 705921.
// Check that clear output and page reload remove the sidebar - bug 971967.

"use strict";

var test = asyncTest(function*() {
  const PREF = "devtools.webconsole.persistlog";
  const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                   "test/test-console.html";

  Services.prefs.setBoolPref(PREF, false);
  registerCleanupFunction(() => Services.prefs.clearUserPref(PREF));

  yield loadTab(TEST_URI);

  let hud = yield openConsole();
  ok(hud, "Web Console opened");

  yield openSidebar("fooObj", { name: "testProp", value: "testValue" });

  let sidebarClosed = hud.jsterm.once("sidebar-closed");
  hud.jsterm.clearOutput();
  yield sidebarClosed;

  hud.jsterm.execute("console.log('foobarz1')");

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "foobarz1",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  });

  yield openSidebar("fooObj", { name: "testProp", value: "testValue" });

  BrowserReload();

  sidebarClosed = hud.jsterm.once("sidebar-closed");
  loadBrowser(gBrowser.selectedBrowser);
  yield sidebarClosed;

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

  function* openSidebar(objName, expectedObj) {
    let msg = yield hud.jsterm.execute(objName);
    ok(msg, "output message found");

    let anchor = msg.querySelector("a");
    let body = msg.querySelector(".message-body");
    ok(anchor, "object anchor");
    ok(body, "message body");

    yield EventUtils.synthesizeMouse(anchor, 2, 2, {}, hud.iframeWindow);

    let vviewVar = yield hud.jsterm.once("variablesview-fetched");
    let vview = vviewVar._variablesView;
    ok(vview, "variables view object exists");

    yield findVariableViewProperties(vviewVar, [
      expectedObj,
    ], { webconsole: hud });
  }
});
