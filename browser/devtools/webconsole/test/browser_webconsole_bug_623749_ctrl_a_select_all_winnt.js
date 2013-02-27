/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test for https://bugzilla.mozilla.org/show_bug.cgi?id=623749
// Map Control + A to Select All, In the web console input, on Windows

function test() {
  addTab("data:text/html;charset=utf-8,Test console for bug 623749");
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, runTest);
  }, true);
}

function runTest(HUD) {
  let jsterm = HUD.jsterm;
  jsterm.setInputValue("Ignore These Four Words");
  let inputNode = jsterm.inputNode;

  // Test select all with Control + A.
  EventUtils.synthesizeKey("a", { ctrlKey: true });
  let inputLength = inputNode.selectionEnd - inputNode.selectionStart;
  is(inputLength, inputNode.value.length, "Select all of input");

  // Test do nothing on Control + E.
  jsterm.setInputValue("Ignore These Four Words");
  inputNode.selectionStart = 0;
  EventUtils.synthesizeKey("e", { ctrlKey: true });
  is(inputNode.selectionStart, 0, "Control + E does not move to end of input");

  executeSoon(finishTest);
}
