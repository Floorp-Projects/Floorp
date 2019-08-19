/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* Bug 679467 */

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(function() {
    openScratchpad(testFalsy);
  });

  BrowserTestUtils.loadURI(
    gBrowser,
    "data:text/html,<p>test falsy display() values in Scratchpad"
  );
}

function testFalsy() {
  const scratchpad = gScratchpadWindow.Scratchpad;
  verifyFalsies(scratchpad).then(function() {
    scratchpad.setBrowserContext();
    verifyFalsies(scratchpad).then(finish);
  });
}

function verifyFalsies(scratchpad) {
  const tests = [
    {
      method: "display",
      code: "undefined",
      result: "undefined\n/*\nundefined\n*/",
      label: "undefined is displayed",
    },
    {
      method: "display",
      code: "false",
      result: "false\n/*\nfalse\n*/",
      label: "false is displayed",
    },
    {
      method: "display",
      code: "0",
      result: "0\n/*\n0\n*/",
      label: "0 is displayed",
    },
    {
      method: "display",
      code: "null",
      result: "null\n/*\nnull\n*/",
      label: "null is displayed",
    },
    {
      method: "display",
      code: "NaN",
      result: "NaN\n/*\nNaN\n*/",
      label: "NaN is displayed",
    },
    {
      method: "display",
      code: "''",
      result: "''\n/*\n\n*/",
      label: "the empty string is displayed",
    },
  ];

  return runAsyncTests(scratchpad, tests);
}
