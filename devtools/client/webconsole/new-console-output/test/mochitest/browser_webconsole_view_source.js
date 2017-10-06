/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that source URLs in the Web Console can be clicked to display the
// standard View Source window. As JS exceptions and console.log() messages always
// have their locations opened in Debugger, we need to test a security message in
// order to have it opened in the standard View Source window.

"use strict";

const TEST_URI = "https://example.com/browser/devtools/client/webconsole/" +
                 "test/test-mixedcontent-securityerrors.html";

add_task(function* () {
  yield actuallyTest();
});

add_task(function* () {
  Services.prefs.setBoolPref("devtools.debugger.new-debugger-frontend", false);
  yield actuallyTest();
  Services.prefs.clearUserPref("devtools.debugger.new-debugger-frontend");
});

var actuallyTest = Task.async(function*() {
  yield loadTab(TEST_URI);
  let hud = yield openConsole(null);
  info("console opened");

  let [result] = yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "Blocked loading mixed active content",
      category: CATEGORY_SECURITY,
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
  ok(true, "the view source tab was opened in response to clicking the location node");
  gBrowser.removeTab(tab);
});
