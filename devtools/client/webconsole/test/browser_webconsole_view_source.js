/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that source URLs in the Web Console can be clicked to display the
// standard View Source window.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-error.html";

add_task(function* () {
  yield loadTab(TEST_URI);
  let hud = yield openConsole(null);
  info("console opened");


  // On e10s, the exception is triggered in child process
  // and is ignored by test harness
  if (!Services.appinfo.browserTabsRemoteAutostart) {
    expectUncaughtException();
  }

  ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    let button = content.document.querySelector("button");
    ok(button, "we have the button on the page");
    button.click();
  });

  let [result] = yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "fooBazBaz is not defined",
      category: CATEGORY_JS,
      severity: SEVERITY_ERROR,
    }],
  });

  let msg = [...result.matched][0];
  ok(msg, "error message");
  let locationNode = msg.querySelector(".message-location .frame-link-filename");
  ok(locationNode, "location node");

  let onTabOpen = waitForTab();

  EventUtils.sendMouseEvent({ type: "click" }, locationNode);

  let tab = yield onTabOpen;
  ok(true, "the view source tab was opened in response to clicking " +
           "the location node");
  gBrowser.removeTab(tab);
});
