/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* Bug 756681 */

function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(function () {
    openScratchpad(runTests, {"state":{"text":""}});
  });

  gBrowser.loadURI("data:text/html, test that exceptions are output as " +
                   "comments correctly in Scratchpad");
}

function runTests()
{
  var scratchpad = gScratchpadWindow.Scratchpad;

  var message = "\"Hello World!\"";
  var openComment = "\n/*\n";
  var closeComment = "\n*/";
  var error1 = "throw new Error(\"Ouch!\")";
  var error2 = "throw \"A thrown string\"";
  var error3 = "throw {}";
  var error4 = "document.body.appendChild(document.body)";

  let tests = [{
    // Display message
    method: "display",
    code: message,
    result: message + openComment + "Hello World!" + closeComment,
    label: "message display output"
  },
    {
    // Display error1, throw new Error("Ouch")
      method: "display",
      code: error1,
      result: error1 + openComment +
            "Exception: Error: Ouch!\n@" + scratchpad.uniqueName + ":1:7" + closeComment,
      label: "error display output"
    },
    {
    // Display error2, throw "A thrown string"
      method: "display",
      code: error2,
      result: error2 + openComment + "Exception: A thrown string" + closeComment,
      label: "thrown string display output"
    },
    {
    // Display error3, throw {}
      method: "display",
      code: error3,
      result: error3 + openComment + "Exception: [object Object]" + closeComment,
      label: "thrown object display output"
    },
    {
    // Display error4, document.body.appendChild(document.body)
      method: "display",
      code: error4,
      result: error4 + openComment + "Exception: HierarchyRequestError: Node cannot be inserted " +
            "at the specified point in the hierarchy\n@" +
            scratchpad.uniqueName + ":1:0" + closeComment,
      label: "Alternative format error display output"
    },
    {
    // Run message
      method: "run",
      code: message,
      result: message,
      label: "message run output"
    },
    {
    // Run error1, throw new Error("Ouch")
      method: "run",
      code: error1,
      result: error1 + openComment +
            "Exception: Error: Ouch!\n@" + scratchpad.uniqueName + ":1:7" + closeComment,
      label: "error run output"
    },
    {
    // Run error2, throw "A thrown string"
      method: "run",
      code: error2,
      result: error2 + openComment + "Exception: A thrown string" + closeComment,
      label: "thrown string run output"
    },
    {
    // Run error3, throw {}
      method: "run",
      code: error3,
      result: error3 + openComment + "Exception: [object Object]" + closeComment,
      label: "thrown object run output"
    },
    {
    // Run error4, document.body.appendChild(document.body)
      method: "run",
      code: error4,
      result: error4 + openComment + "Exception: HierarchyRequestError: Node cannot be inserted " +
            "at the specified point in the hierarchy\n@" +
            scratchpad.uniqueName + ":1:0" + closeComment,
      label: "Alternative format error run output"
    }];

  runAsyncTests(scratchpad, tests).then(finish);
}
