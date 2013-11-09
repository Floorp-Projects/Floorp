/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */


function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
    openScratchpad(testThrowOutput);
  }, true);

  content.location = "data:text/html;charset=utf8,<p>Test throw outputs in Scratchpad</p>";
}

function testThrowOutput()
{
  let scratchpad = gScratchpadWindow.Scratchpad, tests = [];

  let falsyValues = ["false", "0", "-0", "null", "undefined", "Infinity",
                      "-Infinity", "NaN"];
  falsyValues.forEach(function(value) {
    tests.push({
      method: "display",
      code: "throw " + value + ";",
      result: "throw " + value + ";\n/*\nException: " + value + "\n*/",
      label: "Correct exception message for '" + value + "' is shown"
    });
  });

  let server = Cu.import("resource://gre/modules/devtools/dbg-server.jsm", {})
               .DebuggerServer;

  let longLength = server.LONG_STRING_LENGTH + 1;
  let longString = new Array(longLength).join("a");
  let shortedString = longString.substring(0, server.LONG_STRING_INITIAL_LENGTH) +
                      "\u2026";

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
