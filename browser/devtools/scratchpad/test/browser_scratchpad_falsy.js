/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* Bug 679467 */

function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
    openScratchpad(testFalsy);
  }, true);

  content.location = "data:text/html,<p>test falsy display() values in Scratchpad";
}

function testFalsy()
{
  let scratchpad = gScratchpadWindow.Scratchpad;
  verifyFalsies(scratchpad).then(function() {
    scratchpad.setBrowserContext();
    verifyFalsies(scratchpad).then(finish);
  });
}


function verifyFalsies(scratchpad)
{
  let tests = [{
    method: "display",
    code: "undefined",
    result: "undefined\n/*\nundefined\n*/",
    label: "undefined is displayed"
  },
  {
    method: "display",
    code: "false",
    result: "false\n/*\nfalse\n*/",
    label: "false is displayed"
  },
  {
    method: "display",
    code: "0",
    result: "0\n/*\n0\n*/",
    label: "0 is displayed"
  },
  {
    method: "display",
    code: "null",
    result: "null\n/*\nnull\n*/",
    label: "null is displayed"
  },
  {
    method: "display",
    code: "NaN",
    result: "NaN\n/*\nNaN\n*/",
    label: "NaN is displayed"
  },
  {
    method: "display",
    code: "''",
    result: "''\n/*\n\n*/",
    label: "the empty string is displayed"
  }];

  return runAsyncTests(scratchpad, tests);
}
