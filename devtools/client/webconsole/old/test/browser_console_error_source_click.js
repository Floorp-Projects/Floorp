/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that JS errors and CSS warnings open view source when their source link
// is clicked in the Browser Console. See bug 877778.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,<p>hello world from bug 877778 " +
                 "<button onclick='foobar.explode()' " +
                 "style='test-color: green-please'>click!</button>";

add_task(function* () {
  yield new Promise(resolve => {
    SpecialPowers.pushPrefEnv({"set": [
      ["devtools.browserconsole.filter.cssparser", true]
    ]}, resolve);
  });

  yield loadTab(TEST_URI);
  let hud = yield HUDService.toggleBrowserConsole();
  ok(hud, "browser console opened");

  // On e10s, the exception is triggered in child process
  // and is ignored by test harness
  if (!Services.appinfo.browserTabsRemoteAutostart) {
    expectUncaughtException();
  }

  info("generate exception and wait for the message");
  ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    let button = content.document.querySelector("button");
    button.click();
  });

  let results = yield waitForMessages({
    webconsole: hud,
    messages: [
      {
        text: "ReferenceError: foobar is not defined",
        category: CATEGORY_JS,
        severity: SEVERITY_ERROR,
      },
      {
        text: "Unknown property \u2018test-color\u2019",
        category: CATEGORY_CSS,
        severity: SEVERITY_WARNING,
      },
    ],
  });

  let viewSourceCalled = false;

  let viewSource = hud.viewSource;
  hud.viewSource = () => {
    viewSourceCalled = true;
  };

  for (let result of results) {
    viewSourceCalled = false;

    let msg = [...result.matched][0];
    ok(msg, "message element found for: " + result.text);
    ok(!msg.classList.contains("filtered-by-type"), "message element is not filtered");
    let selector = ".message .message-location .frame-link-source";
    let locationNode = msg.querySelector(selector);
    ok(locationNode, "message location element found");

    locationNode.click();

    ok(viewSourceCalled, "view source opened");
  }

  hud.viewSource = viewSource;

  yield finishTest();
});
