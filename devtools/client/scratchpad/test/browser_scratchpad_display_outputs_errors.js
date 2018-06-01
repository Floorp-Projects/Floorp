/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* Bug 690552 */

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(function() {
    openScratchpad(runTests, {"state": {"text": ""}});
  });

  gBrowser.loadURI("data:text/html,<p>test that exceptions are output as " +
                   "comments for 'display' and not sent to the console in Scratchpad");
}

function runTests() {
  const scratchpad = gScratchpadWindow.Scratchpad;

  const message = "\"Hello World!\"";
  const openComment = "\n/*\n";
  const closeComment = "\n*/";
  const error = "throw new Error(\"Ouch!\")";
  const syntaxError = "(";

  const tests = [{
    method: "display",
    code: message,
    result: message + openComment + "Hello World!" + closeComment,
    label: "message display output"
  },
  {
      method: "display",
      code: error,
      result: error + openComment + "Exception: Error: Ouch!\n@" +
            scratchpad.uniqueName + ":1:7" + closeComment,
      label: "error display output",
  },
  {
      method: "display",
      code: syntaxError,
      result: syntaxError + openComment + "Exception: SyntaxError: expected expression, got end of script\n@" +
            scratchpad.uniqueName + ":1" + closeComment,
      label: "syntaxError display output",
  },
  {
      method: "run",
      code: message,
      result: message,
      label: "message run output",
  },
  {
      method: "run",
      code: error,
      result: error + openComment + "Exception: Error: Ouch!\n@" +
            scratchpad.uniqueName + ":1:7" + closeComment,
      label: "error run output",
  },
  {
      method: "run",
      code: syntaxError,
      result: syntaxError + openComment + "Exception: SyntaxError: expected expression, got end of script\n@" +
            scratchpad.uniqueName + ":1" + closeComment,
      label: "syntaxError run output",
  }];

  runAsyncTests(scratchpad, tests).then(finish);
}
