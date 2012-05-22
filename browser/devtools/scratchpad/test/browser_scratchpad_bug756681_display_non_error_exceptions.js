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

  content.location = "data:text/html, test that exceptions are output as " +
      "comments correctly in Scratchpad";
}

function runTests()
{
  var scratchpad = gScratchpadWindow.Scratchpad;

  var message = "\"Hello World!\""
  var openComment = "\n/*\n";
  var closeComment = "\n*/";
  var error1 = "throw new Error(\"Ouch!\")";
  var error2 = "throw \"A thrown string\"";
  var error3 = "throw {}";
  var error4 = "document.body.appendChild(document.body)";
  let messageArray = {};
  let count = {};

  // Display message
  scratchpad.setText(message);
  scratchpad.display();
  is(scratchpad.getText(),
      message + openComment + "Hello World!" + closeComment,
      "message display output");

  // Display error1, throw new Error("Ouch")
  scratchpad.setText(error1);
  scratchpad.display();
  is(scratchpad.getText(),
      error1 + openComment + "Exception: Ouch!\n@Scratchpad:1" + closeComment,
      "error display output");

  // Display error2, throw "A thrown string"
  scratchpad.setText(error2);
  scratchpad.display();
  is(scratchpad.getText(),
      error2 + openComment + "Exception: A thrown string" + closeComment,
      "thrown string display output");

  // Display error3, throw {}
  scratchpad.setText(error3);
  scratchpad.display();
  is(scratchpad.getText(),
      error3 + openComment + "Exception: [object Object]" + closeComment,
      "thrown object display output");

  // Display error4, document.body.appendChild(document.body)
  scratchpad.setText(error4);
  scratchpad.display();
  is(scratchpad.getText(),
      error4 + openComment + "Exception: Node cannot be inserted " +
      "at the specified point in the hierarchy\n@1" + closeComment,
      "Alternative format error display output");

  // Run message
  scratchpad.setText(message);
  scratchpad.run();
  is(scratchpad.getText(), message, "message run output");

  // Run error1, throw new Error("Ouch")
  scratchpad.setText(error1);
  scratchpad.run();
  is(scratchpad.getText(),
      error1 + openComment + "Exception: Ouch!\n@Scratchpad:1" + closeComment,
      "error run output");

  // Run error2, throw "A thrown string"
  scratchpad.setText(error2);
  scratchpad.run();
  is(scratchpad.getText(),
      error2 + openComment + "Exception: A thrown string" + closeComment,
      "thrown string run output");

  // Run error3, throw {}
  scratchpad.setText(error3);
  scratchpad.run();
  is(scratchpad.getText(),
      error3 + openComment + "Exception: [object Object]" + closeComment,
      "thrown object run output");

  // Run error4, document.body.appendChild(document.body)
  scratchpad.setText(error4);
  scratchpad.run();
  is(scratchpad.getText(),
      error4 + openComment + "Exception: Node cannot be inserted " +
      "at the specified point in the hierarchy\n@1" + closeComment,
      "Alternative format error run output");

  finish();
}
