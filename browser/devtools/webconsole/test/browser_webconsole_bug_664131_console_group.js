/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that console.group/groupEnd works as intended.
const TEST_URI = "data:text/html;charset=utf-8,Web Console test for bug 664131: Expand console object with group methods";

function test() {
  Task.spawn(runner).then(finishTest);

  function* runner() {
    let {tab} = yield loadTab(TEST_URI);
    let hud = yield openConsole(tab);
    let outputNode = hud.outputNode;

    hud.jsterm.clearOutput();

    content.console.group("bug664131a");

    yield waitForMessages({
      webconsole: hud,
      messages: [{
        text: "bug664131a",
        consoleGroup: 1,
      }],
    });

    content.console.log("bug664131a-inside");

    yield waitForMessages({
      webconsole: hud,
      messages: [{
        text: "bug664131a-inside",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
        groupDepth: 1,
      }],
    });

    content.console.groupEnd("bug664131a");
    content.console.log("bug664131-outside");

    yield waitForMessages({
      webconsole: hud,
      messages: [{
        text: "bug664131-outside",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
        groupDepth: 0,
      }],
    });

    content.console.groupCollapsed("bug664131b");

    yield waitForMessages({
      webconsole: hud,
      messages: [{
        text: "bug664131b",
        consoleGroup: 1,
      }],
    });

    // Test that clearing the console removes the indentation.
    hud.jsterm.clearOutput();
    content.console.log("bug664131-cleared");

    yield waitForMessages({
      webconsole: hud,
      messages: [{
        text: "bug664131-cleared",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
        groupDepth: 0,
      }],
    });
  }
}
