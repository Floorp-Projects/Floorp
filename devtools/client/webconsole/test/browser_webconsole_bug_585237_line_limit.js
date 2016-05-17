/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the Web Console limits the number of lines displayed according to
// the user's preferences.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,test for bug 585237";

var outputNode;

add_task(function* () {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  outputNode = hud.outputNode;

  hud.jsterm.clearOutput();

  let prefBranch = Services.prefs.getBranch("devtools.hud.loglimit.");
  prefBranch.setIntPref("console", 20);

  for (let i = 0; i < 30; i++) {
    yield ContentTask.spawn(gBrowser.selectedBrowser, i, function (i) {
      // must change message to prevent repeats
      content.console.log("foo #" + i);
    });
  }

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "foo #29",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  });

  is(countMessageNodes(), 20, "there are 20 message nodes in the output " +
     "when the log limit is set to 20");

  yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function () {
    content.console.log("bar bug585237");
  });

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "bar bug585237",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  });

  is(countMessageNodes(), 20, "there are still 20 message nodes in the " +
     "output when adding one more");

  prefBranch.setIntPref("console", 30);
  for (let i = 0; i < 20; i++) {
    yield ContentTask.spawn(gBrowser.selectedBrowser, i, function (i) {
      // must change message to prevent repeats
      content.console.log("boo #" + i);
    });
  }

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "boo #19",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  });

  is(countMessageNodes(), 30, "there are 30 message nodes in the output " +
     "when the log limit is set to 30");

  prefBranch.clearUserPref("console");

  outputNode = null;
});

function countMessageNodes() {
  return outputNode.querySelectorAll(".message").length;
}
