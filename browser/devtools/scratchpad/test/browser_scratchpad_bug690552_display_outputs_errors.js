/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function browserLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", browserLoad, true);
    openScratchpad(runTests, {"state":{"text":""}});
  }, true);

  content.location = "data:text/html,<p>test that exceptions our output as " +
      "comments for 'display' and not sent to the console in Scratchpad";
}

function runTests()
{
  var scratchpad = gScratchpadWindow.Scratchpad;

  var message = "\"Hello World!\""
  var openComment = "\n/*\n";
  var closeComment = "\n*/";
  var error = "throw new Error(\"Ouch!\")";
  let messageArray = {};
  let count = {};

  scratchpad.setText(message);
  scratchpad.display();
  is(scratchpad.getText(),
      message + openComment + "Hello World!" + closeComment,
      "message display output");

  scratchpad.setText(error);
  scratchpad.display();
  is(scratchpad.getText(), 
      error + openComment + "Exception: Ouch!\n@Scratchpad:1" + closeComment,
      "error display output");

  scratchpad.setText(message);
  scratchpad.run();
  is(scratchpad.getText(), message, "message run output");

  scratchpad.setText(error);
  scratchpad.run();
  is(scratchpad.getText(), 
      error + openComment + "Exception: Ouch!\n@Scratchpad:1" + closeComment,
      "error display output");

  finish();
}
