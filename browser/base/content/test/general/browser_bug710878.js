/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    waitForFocus(performTest, content);
  }, true);

  content.location = "data:text/html;charset=utf-8,<a href='%23xxx'><span>word1 <span> word2 </span></span><span> word3</span></a>";

  function performTest()
  {
    let doc = content.document;
    let link = doc.querySelector("a");;
    let text = gatherTextUnder(link);
    is(text, "word1 word2 word3", "Text under link is correctly computed.");
    gBrowser.removeCurrentTab();
    finish();
  }
}

