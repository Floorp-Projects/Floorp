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

  content.location = "data:text/html;charset=utf8,test Scratchpad pretty print.";
}

function runTests(sw)
{
  const sp = sw.Scratchpad;
  sp.setText("function main() { console.log(5); }");
  sp.prettyPrint().then(() => {
    const prettyText = sp.getText();
    ok(prettyText.includes("\n"));
    finish();
  }).then(null, error => {
    ok(false, error);
  });
}
