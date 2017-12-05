/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-network.html";
const PREF = "devtools.webconsole.persistlog";

add_task(function* () {
  Services.prefs.setBoolPref(PREF, true);

  yield loadTab(TEST_URI);
  let hud = yield openConsole();

  let results = yield consoleOpened(hud);

  testScroll(results, hud);

  Services.prefs.clearUserPref(PREF);
});

function consoleOpened(hud) {
  let deferred = defer();

  for (let i = 0; i < 200; i++) {
    gBrowser.contentWindowAsCPOW.console.log("test message " + i);
  }

  hud.setFilterState("network", false);
  hud.setFilterState("networkinfo", false);

  hud.ui.filterBox.value = "test message";
  hud.ui.adjustVisibilityOnSearchStringChange();

  waitForMessages({
    webconsole: hud,
    messages: [{
      name: "console messages displayed",
      text: "test message 199",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  }).then(() => {
    waitForMessages({
      webconsole: hud,
      messages: [{
        text: "test-network.html",
        category: CATEGORY_NETWORK,
        severity: SEVERITY_LOG,
      }],
    }).then(deferred.resolve);

    gBrowser.reload();
  });

  return deferred.promise;
}

function testScroll([result], hud) {
  let scrollNode = hud.ui.outputWrapper;
  let msgNode = [...result.matched][0];
  ok(msgNode.classList.contains("filtered-by-type"),
    "network message is filtered by type");
  ok(msgNode.classList.contains("filtered-by-string"),
    "network message is filtered by string");

  ok(scrollNode.scrollTop > 0, "scroll location is not at the top");

  // Make sure the Web Console output is scrolled as near as possible to the
  // bottom.
  let nodeHeight = msgNode.clientHeight;
  ok(scrollNode.scrollTop >= scrollNode.scrollHeight - scrollNode.clientHeight -
     nodeHeight * 2, "scroll location is correct");

  hud.setFilterState("network", true);
  hud.setFilterState("networkinfo", true);
}
