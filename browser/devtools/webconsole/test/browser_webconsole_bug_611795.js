/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = 'data:text/html;charset=utf-8,<div style="-moz-opacity:0;">' +
                 'test repeated css warnings</div><p style="-moz-opacity:0">' +
                 'hi</p>';
let hud;

/**
 * Unit test for bug 611795:
 * Repeated CSS messages get collapsed into one.
 */

let test = asyncTest(function* () {
  yield loadTab(TEST_URI);

  hud = yield openConsole();
  hud.jsterm.clearOutput(true);

  BrowserReload();
  yield loadBrowser(gBrowser.selectedBrowser);

  yield onContentLoaded();
  yield testConsoleLogRepeats();

  hud = null;
});

function onContentLoaded() {
  let cssWarning = "Unknown property '-moz-opacity'.  Declaration dropped.";

  return waitForMessages({
    webconsole: hud,
    messages: [{
      text: cssWarning,
      category: CATEGORY_CSS,
      severity: SEVERITY_WARNING,
      repeats: 2,
    }],
  });
}

function testConsoleLogRepeats() {
  let jsterm = hud.jsterm;

  jsterm.clearOutput();

  jsterm.setInputValue("for (let i = 0; i < 10; ++i) console.log('this is a " +
                       "line of reasonably long text that I will use to " +
                       "verify that the repeated text node is of an " +
                       "appropriate size.');");
  jsterm.execute();

  return waitForMessages({
    webconsole: hud,
    messages: [{
      text: "this is a line of reasonably long text",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
      repeats: 10,
    }],
  });
}
