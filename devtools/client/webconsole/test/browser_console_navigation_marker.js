/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that the navigation marker shows on page reload - bug 793996.

"use strict";

const PREF = "devtools.webconsole.persistlog";
const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console.html";

var hud;

add_task(function* () {
  Services.prefs.setBoolPref(PREF, true);

  let { browser } = yield loadTab(TEST_URI);
  hud = yield openConsole();

  yield consoleOpened();

  let loaded = loadBrowser(browser);
  BrowserReload();
  yield loaded;

  yield onReload();

  isnot(hud.outputNode.textContent.indexOf("foobarz1"), -1,
        "foobarz1 is still in the output");

  Services.prefs.clearUserPref(PREF);

  hud = null;
});

function consoleOpened() {
  ok(hud, "Web Console opened");

  hud.jsterm.clearOutput();

  ContentTask.spawn(gBrowser.selectedBrowser, null, function* () {
    content.console.log("foobarz1");
  });

  return waitForMessages({
    webconsole: hud,
    messages: [{
      text: "foobarz1",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  });
}

function onReload() {
  ContentTask.spawn(gBrowser.selectedBrowser, null, function* () {
    content.console.log("foobarz2");
  });

  return waitForMessages({
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
  });
}
