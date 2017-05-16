/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedBrowser.addEventListener("load", function () {
    openScratchpad(runTests);
  }, {capture: true, once: true});

  content.location = "data:text/html;charset=utf8,<p>test long string in Scratchpad</p>";
}

function runTests()
{
  let sp = gScratchpadWindow.Scratchpad;

  sp.setText("'0'.repeat(10000)");

  sp.display().then(() => {
    is(sp.getText(), "'0'.repeat(10000)\n" +
                     "/*\n" + "0".repeat(10000) + "\n*/",
       "display()ing a long string works");
    finish();
  });
}
