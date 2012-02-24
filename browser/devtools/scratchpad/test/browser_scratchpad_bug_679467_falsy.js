/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

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
  let sp = gScratchpadWindow.Scratchpad;
  verifyFalsies(sp);
  
  sp.setBrowserContext();
  verifyFalsies(sp);

  finish();
}

function verifyFalsies(sp)
{
  sp.setText("undefined");
  sp.display();
  is(sp.selectedText, "\n/*\nundefined\n*/", "'undefined' is displayed");

  sp.setText("false");
  sp.display();
  is(sp.selectedText, "\n/*\nfalse\n*/", "'false' is displayed");

  sp.setText("0");
  sp.display();
  is(sp.selectedText, "\n/*\n0\n*/", "'0' is displayed");

  sp.setText("null");
  sp.display();
  is(sp.selectedText, "\n/*\nnull\n*/", "'null' is displayed");

  sp.setText("NaN");
  sp.display();
  is(sp.selectedText, "\n/*\nNaN\n*/", "'NaN' is displayed");

  sp.setText("''");
  sp.display();
  is(sp.selectedText, "\n/*\n\n*/", "empty string is displayed");
}
