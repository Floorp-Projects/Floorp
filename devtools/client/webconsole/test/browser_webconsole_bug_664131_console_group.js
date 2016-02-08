/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that console.group/groupEnd works as intended.

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Web Console test for " +
                 "bug 664131: Expand console object with group methods";

add_task(function* () {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();
  let jsterm = hud.jsterm;

  hud.jsterm.clearOutput();

  yield jsterm.execute("console.group('bug664131a')");

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "bug664131a",
      consoleGroup: 1,
    }],
  });

  yield jsterm.execute("console.log('bug664131a-inside')");

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "bug664131a-inside",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
      groupDepth: 1,
    }],
  });

  yield jsterm.execute('console.groupEnd("bug664131a")');
  yield jsterm.execute('console.log("bug664131-outside")');

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "bug664131-outside",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
      groupDepth: 0,
    }],
  });

  yield jsterm.execute('console.groupCollapsed("bug664131b")');

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "bug664131b",
      consoleGroup: 1,
    }],
  });

  // Test that clearing the console removes the indentation.
  hud.jsterm.clearOutput();
  yield jsterm.execute('console.log("bug664131-cleared")');

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "bug664131-cleared",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
      groupDepth: 0,
    }],
  });
});
