/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(function() {
    openScratchpad(testThrowOutput);
  });

  gBrowser.loadURI("data:text/html;charset=utf8,<p>Test throw outputs in Scratchpad</p>");
}

function testThrowOutput() {
  const scratchpad = gScratchpadWindow.Scratchpad, tests = [];

  const falsyValues = ["false", "0", "-0", "null", "undefined", "Infinity",
                       "-Infinity", "NaN"];
  falsyValues.forEach(function(value) {
    tests.push({
      method: "display",
      code: "throw " + value + ";",
      result: "throw " + value + ";\n/*\nException: " + value + "\n*/",
      label: "Correct exception message for '" + value + "' is shown"
    });
  });

  const { DebuggerServer } = require("devtools/server/main");

  const longLength = DebuggerServer.LONG_STRING_LENGTH + 1;
  const longString = new Array(longLength).join("a");
  const shortedString = longString.substring(0,
    DebuggerServer.LONG_STRING_INITIAL_LENGTH) + "\u2026";

  tests.push({
    method: "display",
    code: "throw (new Array(" + longLength + ").join('a'));",
    result: "throw (new Array(" + longLength + ").join('a'));\n" +
            "/*\nException: " + shortedString + "\n*/",
    label: "Correct exception message for a longString is shown"
  });

  runAsyncTests(scratchpad, tests).then(function() {
    finish();
  });
}
