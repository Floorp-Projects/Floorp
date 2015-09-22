/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
    openScratchpad(runTests);
  }, true);

  content.location = "data:text/html;charset=utf8,test Scratchpad pretty print.";
}

var gTabsize;

function runTests(sw)
{
  gTabsize = Services.prefs.getIntPref("devtools.editor.tabsize");
  Services.prefs.setIntPref("devtools.editor.tabsize", 6);
  const space = " ".repeat(6);

  const sp = sw.Scratchpad;
  sp.setText("function main() { console.log(5); }");
  sp.prettyPrint().then(() => {
    const prettyText = sp.getText();
    ok(prettyText.includes(space));
    finish();
  }).then(null, error => {
    ok(false, error);
  });
}

registerCleanupFunction(function () {
  Services.prefs.setIntPref("devtools.editor.tabsize", gTabsize);
  gTabsize = null;
});
