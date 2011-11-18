/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TESTCASE_URI = TEST_BASE + "simple.html";


function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();

  // launch Style Editor right when the tab is created (before load)
  // this checks that the Style Editor still launches correctly when it is opened
  // *while* the page is still loading
  launchStyleEditorChrome(function (aChrome) {
    isnot(gBrowser.selectedBrowser.contentWindow.document.readyState, "complete",
          "content document is still loading");

    if (!aChrome.isContentAttached) {
      aChrome.addChromeListener({
        onContentAttach: run
      });
    } else {
      run(aChrome);
    }
  });

  content.location = TESTCASE_URI;
}

function run(aChrome)
{
  is(aChrome.contentWindow.document.readyState, "complete",
     "content document is complete");

  finish();
}
