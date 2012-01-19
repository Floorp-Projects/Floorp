/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function browserLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", browserLoad, true);
    openScratchpad(runTests);
  }, true);

  content.location = "data:text/html,<p>test the Find feature in Scratchpad";
}

function runTests(aWindow, aScratchpad)
{
  let editor = aScratchpad.editor;
  let text = "foobar bug650345\nBug650345 bazbaz\nfoobar omg\ntest";
  editor.setText(text);

  let needle = "foobar";
  editor.setSelection(0, needle.length);

  let oldPrompt = Services.prompt;
  Services.prompt = {
    prompt: function() { return true; },
  };

  let findKey = "F";
  info("test Ctrl/Cmd-" + findKey + " (find)");
  EventUtils.synthesizeKey(findKey, {accelKey: true}, aWindow);
  let selection = editor.getSelection();
  let newIndex = text.indexOf(needle, needle.length);
  is(selection.start, newIndex, "selection.start is correct");
  is(selection.end, newIndex + needle.length, "selection.end is correct");

  info("test cmd_find");
  aWindow.goDoCommand("cmd_find");
  selection = editor.getSelection();
  is(selection.start, 0, "selection.start is correct");
  is(selection.end, needle.length, "selection.end is correct");

  let findNextKey = Services.appinfo.OS == "Darwin" ? "G" : "VK_F3";
  let findNextKeyOptions = Services.appinfo.OS == "Darwin" ?
                           {accelKey: true} : {};

  info("test " + findNextKey + " (findNext)");
  EventUtils.synthesizeKey(findNextKey, findNextKeyOptions, aWindow);
  selection = editor.getSelection();
  is(selection.start, newIndex, "selection.start is correct");
  is(selection.end, newIndex + needle.length, "selection.end is correct");

  info("test cmd_findAgain");
  aWindow.goDoCommand("cmd_findAgain");
  selection = editor.getSelection();
  is(selection.start, 0, "selection.start is correct");
  is(selection.end, needle.length, "selection.end is correct");

  let findPreviousKey = Services.appinfo.OS == "Darwin" ? "G" : "VK_F3";
  let findPreviousKeyOptions = Services.appinfo.OS == "Darwin" ?
                           {accelKey: true, shiftKey: true} : {shiftKey: true};

  info("test " + findPreviousKey + " (findPrevious)");
  EventUtils.synthesizeKey(findPreviousKey, findPreviousKeyOptions, aWindow);
  selection = editor.getSelection();
  is(selection.start, newIndex, "selection.start is correct");
  is(selection.end, newIndex + needle.length, "selection.end is correct");

  info("test cmd_findPrevious");
  aWindow.goDoCommand("cmd_findPrevious");
  selection = editor.getSelection();
  is(selection.start, 0, "selection.start is correct");
  is(selection.end, needle.length, "selection.end is correct");

  needle = "BAZbaz";
  newIndex = text.toLowerCase().indexOf(needle.toLowerCase());

  Services.prompt = {
    prompt: function(aWindow, aTitle, aMessage, aValue) {
      aValue.value = needle;
      return true;
    },
  };

  info("test Ctrl/Cmd-" + findKey + " (find) with a custom value");
  EventUtils.synthesizeKey(findKey, {accelKey: true}, aWindow);
  selection = editor.getSelection();
  is(selection.start, newIndex, "selection.start is correct");
  is(selection.end, newIndex + needle.length, "selection.end is correct");

  Services.prompt = oldPrompt;

  finish();
}

